#include "egolib/FileFormats/SpawnFile/SpawnName.hpp"
#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/Logic/TreasureTables.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/game.h"
#include "egolib/game/script_compile.h"
#include "egolib/file_common.h"
#include "egolib/vfs.h"

#include <SDL.h>

#include <dirent.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

struct Options
{
    bool verbose = false;
    bool skipScripts = false;
    bool emitReconciliation = false;
    bool jsonOutput = false;
    bool hasModuleFilter = false;
    bool hasDataDir = false;
    std::string moduleFilter;
    std::string dataDir;
};

struct ModuleSummary
{
    size_t localObjects = 0;
    size_t uniqueObjects = 0;
    size_t spawnEntries = 0;
    size_t warnings = 0;
    size_t errors = 0;
    std::map<std::string, size_t> categoryCounts;
};

struct ModuleResult
{
    std::string moduleName;
    bool ok = false;
    ModuleSummary summary;
};

struct ValidationEvent
{
    std::string module;
    std::string severity;
    std::string category;
    std::string subject;
    std::string message;
    std::string sourcePath;
    std::string spawnName;
    std::string resolvedVirtualPath;
};

struct ReconciliationKey
{
    std::string module;
    std::string sourcePath;
    std::string spawnName;
    std::string resolvedVirtualPath;

    bool operator<(const ReconciliationKey& other) const
    {
        return std::tie(module, sourcePath, spawnName, resolvedVirtualPath)
             < std::tie(other.module, other.sourcePath, other.spawnName, other.resolvedVirtualPath);
    }
};

struct ReachableObject
{
    std::string module;
    std::string objectName;
    std::string resolvedVirtualPath;
    std::string originPath;
    std::string sourceKind;
};

struct ReconciliationRowKey
{
    std::string module;
    std::string sourcePath;
    std::string rawLoadName;
    std::string normalizedName;
    std::string resolvedVirtualPath;
    std::string canonicalKey;

    bool operator<(const ReconciliationRowKey& other) const
    {
        return std::tie(module, sourcePath, rawLoadName, normalizedName, resolvedVirtualPath, canonicalKey)
             < std::tie(other.module,
                        other.sourcePath,
                        other.rawLoadName,
                        other.normalizedName,
                        other.resolvedVirtualPath,
                        other.canonicalKey);
    }
};

struct ReconciliationRow
{
    std::string module;
    std::string sourcePath;
    std::string rawLoadName;
    std::string normalizedName;
    std::string resolvedVirtualPath;
    size_t occurrenceCount = 0;
    std::string canonicalKey;
    std::vector<std::string> candidateNames;
};

struct ReconciliationReport
{
    std::vector<ReachableObject> reachableObjects;
    std::map<ReconciliationRowKey, ReconciliationRow> rows;
};

class Reporter
{
public:
    Reporter(bool verbose, bool jsonOutput) :
        verbose(verbose),
        jsonOutput(jsonOutput)
    {}

    void info(const std::string& message) const
    {
        if (verbose && !jsonOutput)
        {
            std::cout << message << std::endl;
        }
    }

    void error(const std::string& moduleName,
               const std::string& category,
               const std::string& subject,
               const std::string& message,
               ModuleSummary* summary = nullptr,
               const std::string& sourcePath = "",
               const std::string& spawnName = "",
               const std::string& resolvedVirtualPath = "")
    {
        record(true, moduleName, category, subject, message, summary, sourcePath, spawnName, resolvedVirtualPath);
    }

    void warning(const std::string& moduleName,
                 const std::string& category,
                 const std::string& subject,
                 const std::string& message,
                 ModuleSummary* summary = nullptr,
                 const std::string& sourcePath = "",
                 const std::string& spawnName = "",
                 const std::string& resolvedVirtualPath = "")
    {
        record(false, moduleName, category, subject, message, summary, sourcePath, spawnName, resolvedVirtualPath);
    }

    const std::vector<ValidationEvent>& getEvents() const
    {
        return events;
    }

    const std::map<std::string, size_t>& getCategoryCounts() const
    {
        return categoryCounts;
    }

    const std::map<std::string, size_t>& getErrorCategoryCounts() const
    {
        return errorCategoryCounts;
    }

    const std::map<std::string, size_t>& getWarningCategoryCounts() const
    {
        return warningCategoryCounts;
    }

    const std::map<ReconciliationKey, size_t>& getUnresolvedSpawnCounts() const
    {
        return unresolvedSpawnCounts;
    }

    size_t errors = 0;
    size_t warnings = 0;

private:
    void record(bool isError,
                const std::string& moduleName,
                const std::string& category,
                const std::string& subject,
                const std::string& message,
                ModuleSummary* summary,
                const std::string& sourcePath,
                const std::string& spawnName,
                const std::string& resolvedVirtualPath)
    {
        if (isError)
        {
            ++errors;
        }
        else
        {
            ++warnings;
        }

        if (summary)
        {
            if (isError)
            {
                ++summary->errors;
            }
            else
            {
                ++summary->warnings;
            }
            ++summary->categoryCounts[category];
        }

        ++categoryCounts[category];
        if (isError)
        {
            ++errorCategoryCounts[category];
        }
        else
        {
            ++warningCategoryCounts[category];
        }

        ValidationEvent event;
        event.module = moduleName;
        event.severity = isError ? "error" : "warning";
        event.category = category;
        event.subject = subject;
        event.message = message;
        event.sourcePath = sourcePath.empty() ? subject : sourcePath;
        event.spawnName = spawnName;
        event.resolvedVirtualPath = resolvedVirtualPath;
        events.push_back(event);

        if (category == "missing_spawn_object")
        {
            ReconciliationKey key;
            key.module = moduleName;
            key.sourcePath = event.sourcePath;
            key.spawnName = spawnName;
            key.resolvedVirtualPath = resolvedVirtualPath;
            ++unresolvedSpawnCounts[key];
        }

        if (!jsonOutput)
        {
            std::cerr << (isError ? "error" : "warning")
                      << " [" << moduleName << "] "
                      << subject << ": "
                      << message << std::endl;
        }
    }

    bool verbose;
    bool jsonOutput;
    std::vector<ValidationEvent> events;
    std::map<std::string, size_t> categoryCounts;
    std::map<std::string, size_t> errorCategoryCounts;
    std::map<std::string, size_t> warningCategoryCounts;
    std::map<ReconciliationKey, size_t> unresolvedSpawnCounts;
};

class StreamSilencer
{
public:
    explicit StreamSilencer(bool active) :
        active(active)
    {
        if (!active)
        {
            return;
        }

        nullFd = open("/dev/null", O_WRONLY);
        if (nullFd == -1)
        {
            throw std::runtime_error("unable to open /dev/null");
        }

        stdoutCopy = dup(STDOUT_FILENO);
        stderrCopy = dup(STDERR_FILENO);
        if (stdoutCopy == -1 || stderrCopy == -1)
        {
            throw std::runtime_error("unable to duplicate process streams");
        }

        if (dup2(nullFd, STDOUT_FILENO) == -1 || dup2(nullFd, STDERR_FILENO) == -1)
        {
            throw std::runtime_error("unable to redirect process streams");
        }
    }

    StreamSilencer(const StreamSilencer&) = delete;
    StreamSilencer& operator=(const StreamSilencer&) = delete;

    ~StreamSilencer()
    {
        if (!active)
        {
            return;
        }

        if (stdoutCopy != -1)
        {
            dup2(stdoutCopy, STDOUT_FILENO);
            close(stdoutCopy);
        }

        if (stderrCopy != -1)
        {
            dup2(stderrCopy, STDERR_FILENO);
            close(stderrCopy);
        }

        if (nullFd != -1)
        {
            close(nullFd);
        }
    }

private:
    bool active = false;
    int stdoutCopy = -1;
    int stderrCopy = -1;
    int nullFd = -1;
};

std::string baseName(const std::string& path)
{
    const auto position = path.find_last_of('/');
    return (std::string::npos == position) ? path : path.substr(position + 1);
}

std::string objectVirtualPath(const std::string& anyObjectPath)
{
    return "mp_objects/" + baseName(anyObjectPath);
}

std::string joinSystemPath(const std::string& left, const std::string& right)
{
    if (left.empty())
    {
        return right;
    }
    if (right.empty())
    {
        return left;
    }

    if ('/' == left.back())
    {
        return left + right;
    }
    return left + "/" + right;
}

std::string lowerCase(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool hasObjectDirectorySuffix(const std::string& entryName)
{
    const std::string lower = lowerCase(entryName);
    return lower.size() >= 4 && 0 == lower.compare(lower.size() - 4, 4, ".obj");
}

std::vector<std::string> listObjectDirectories(const std::string& rootPath)
{
    std::vector<std::string> directories;

    DIR* directory = opendir(rootPath.c_str());
    if (!directory)
    {
        return directories;
    }

    dirent* entry = nullptr;
    while ((entry = readdir(directory)) != nullptr)
    {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || !hasObjectDirectorySuffix(name))
        {
            continue;
        }

        const std::string fullPath = joinSystemPath(rootPath, name);
        if (fs_fileIsDirectory(fullPath))
        {
            directories.push_back(name);
        }
    }

    closedir(directory);
    std::sort(directories.begin(), directories.end());
    return directories;
}

struct InventoryRoot
{
    std::string physicalRoot;
    std::string sourceKind;
};

struct ReachabilityIndex
{
    std::map<std::string, ReachableObject> reachableByName;
    std::map<std::string, std::vector<std::string>> candidateNamesByCanonicalKey;
};

std::vector<InventoryRoot> buildInventoryRoots(const ModuleProfile& module)
{
    const std::string moduleDir = baseName(module.getPath());
    const std::string moduleObjects = "modules/" + moduleDir + "/objects";

    std::vector<InventoryRoot> roots;
    roots.push_back({joinSystemPath(fs_getDataDirectory(), moduleObjects), "module_data"});
    roots.push_back({joinSystemPath(fs_getUserDirectory(), moduleObjects), "module_user"});

    static const std::pair<const char*, const char*> globalRoots[] = {
        {"basicdat/globalobjects/items", "global_items"},
        {"basicdat/globalobjects/magic", "global_magic"},
        {"basicdat/globalobjects/magic_item", "global_magic_item"},
        {"basicdat/globalobjects/misc", "global_misc"},
        {"basicdat/globalobjects/monsters", "global_monsters"},
        {"basicdat/globalobjects/players", "global_players"},
        {"basicdat/globalobjects/potions", "global_potions"},
        {"basicdat/globalobjects/unique", "global_unique"},
        {"basicdat/globalobjects/weapons", "global_weapons"},
        {"basicdat/globalobjects/work_in_progress", "global_work_in_progress"},
        {"basicdat/globalobjects/traps", "global_traps"},
        {"basicdat/globalobjects/pets", "global_pets"},
        {"basicdat/globalobjects/scrolls", "global_scrolls"},
        {"basicdat/globalobjects/armor", "global_armor"},
    };

    for (const auto& root : globalRoots)
    {
        roots.push_back({joinSystemPath(fs_getDataDirectory(), root.first), root.second});
    }

    return roots;
}

ReachabilityIndex buildReachabilityIndex(const ModuleProfile& module,
                                         ReconciliationReport* reconciliationReport)
{
    ReachabilityIndex index;
    const std::string moduleName = module.getFolderName();

    for (const auto& root : buildInventoryRoots(module))
    {
        for (const auto& directoryName : listObjectDirectories(root.physicalRoot))
        {
            const std::string objectName = lowerCase(directoryName);
            if (index.reachableByName.find(objectName) != index.reachableByName.end())
            {
                continue;
            }

            ReachableObject reachable;
            reachable.module = moduleName;
            reachable.objectName = objectName;
            reachable.resolvedVirtualPath = objectVirtualPath(objectName);
            reachable.originPath = joinSystemPath(root.physicalRoot, directoryName);
            reachable.sourceKind = root.sourceKind;

            index.candidateNamesByCanonicalKey[Ego::SpawnFile::buildReconciliationKey(objectName)].push_back(objectName);
            index.reachableByName.emplace(objectName, reachable);

            if (reconciliationReport)
            {
                reconciliationReport->reachableObjects.push_back(reachable);
            }
        }
    }

    return index;
}

void appendReconciliationRow(ReconciliationReport* reconciliationReport,
                             const std::string& moduleName,
                             const std::string& sourcePath,
                             const std::string& rawLoadName,
                             const std::string& normalizedName,
                             const std::string& resolvedVirtualPath,
                             const ReachabilityIndex& reachabilityIndex)
{
    if (!reconciliationReport)
    {
        return;
    }

    const std::string canonicalKey = Ego::SpawnFile::buildReconciliationKey(normalizedName);
    ReconciliationRowKey key;
    key.module = moduleName;
    key.sourcePath = sourcePath;
    key.rawLoadName = rawLoadName;
    key.normalizedName = normalizedName;
    key.resolvedVirtualPath = resolvedVirtualPath;
    key.canonicalKey = canonicalKey;

    auto insertion = reconciliationReport->rows.emplace(key, ReconciliationRow());
    ReconciliationRow& row = insertion.first->second;
    if (insertion.second)
    {
        row.module = moduleName;
        row.sourcePath = sourcePath;
        row.rawLoadName = rawLoadName;
        row.normalizedName = normalizedName;
        row.resolvedVirtualPath = resolvedVirtualPath;
        row.canonicalKey = canonicalKey;

        const auto candidates = reachabilityIndex.candidateNamesByCanonicalKey.find(canonicalKey);
        if (candidates != reachabilityIndex.candidateNamesByCanonicalKey.end())
        {
            row.candidateNames = candidates->second;
        }
    }

    ++row.occurrenceCount;
}

void writeJsonString(std::ostream& output, const std::string& value)
{
    output << '"';
    for (unsigned char character : value)
    {
        switch (character)
        {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    const char digits[] = "0123456789abcdef";
                    output << "\\u00"
                           << digits[(character >> 4) & 0x0f]
                           << digits[character & 0x0f];
                }
                else
                {
                    output << static_cast<char>(character);
                }
                break;
        }
    }
    output << '"';
}

void writeJsonCategoryCounts(std::ostream& output, const std::map<std::string, size_t>& counts)
{
    output << "{";
    bool first = true;
    for (const auto& entry : counts)
    {
        if (!first)
        {
            output << ",";
        }
        first = false;
        writeJsonString(output, entry.first);
        output << ":" << entry.second;
    }
    output << "}";
}

void emitJsonReport(const Options& options,
                    const Reporter& reporter,
                    const std::vector<ModuleResult>& results,
                    const ReconciliationReport* reconciliationReport)
{
    size_t passingModules = 0;
    size_t failingModules = 0;
    for (const auto& result : results)
    {
        if (result.ok)
        {
            ++passingModules;
        }
        else
        {
            ++failingModules;
        }
    }

    std::cout << "{";

    std::cout << "\"schema_version\":" << (options.emitReconciliation ? 2 : 1) << ",";

    std::cout << "\"options\":{";
    std::cout << "\"verbose\":" << (options.verbose ? "true" : "false") << ",";
    std::cout << "\"skip_scripts\":" << (options.skipScripts ? "true" : "false") << ",";
    std::cout << "\"module_filter\":";
    if (options.hasModuleFilter)
    {
        writeJsonString(std::cout, options.moduleFilter);
    }
    else
    {
        std::cout << "null";
    }
    std::cout << ",";
    std::cout << "\"data_dir\":";
    if (options.hasDataDir)
    {
        writeJsonString(std::cout, options.dataDir);
    }
    else
    {
        std::cout << "null";
    }
    std::cout << "},";

    std::cout << "\"summary\":{";
    std::cout << "\"validated_modules\":" << results.size() << ",";
    std::cout << "\"passing_modules\":" << passingModules << ",";
    std::cout << "\"failing_modules\":" << failingModules << ",";
    std::cout << "\"warnings\":" << reporter.warnings << ",";
    std::cout << "\"errors\":" << reporter.errors << ",";
    std::cout << "\"category_counts\":";
    writeJsonCategoryCounts(std::cout, reporter.getCategoryCounts());
    std::cout << ",";
    std::cout << "\"error_category_counts\":";
    writeJsonCategoryCounts(std::cout, reporter.getErrorCategoryCounts());
    std::cout << ",";
    std::cout << "\"warning_category_counts\":";
    writeJsonCategoryCounts(std::cout, reporter.getWarningCategoryCounts());
    std::cout << "},";

    std::cout << "\"modules\":[";
    for (size_t index = 0; index < results.size(); ++index)
    {
        if (index > 0)
        {
            std::cout << ",";
        }

        const auto& result = results[index];
        std::cout << "{";
        std::cout << "\"module\":";
        writeJsonString(std::cout, result.moduleName);
        std::cout << ",";
        std::cout << "\"ok\":" << (result.ok ? "true" : "false") << ",";
        std::cout << "\"local_objects\":" << result.summary.localObjects << ",";
        std::cout << "\"unique_objects\":" << result.summary.uniqueObjects << ",";
        std::cout << "\"spawn_entries\":" << result.summary.spawnEntries << ",";
        std::cout << "\"warnings\":" << result.summary.warnings << ",";
        std::cout << "\"errors\":" << result.summary.errors << ",";
        std::cout << "\"category_counts\":";
        writeJsonCategoryCounts(std::cout, result.summary.categoryCounts);
        std::cout << "}";
    }
    std::cout << "],";

    std::cout << "\"events\":[";
    const auto& events = reporter.getEvents();
    for (size_t index = 0; index < events.size(); ++index)
    {
        if (index > 0)
        {
            std::cout << ",";
        }

        const auto& event = events[index];
        std::cout << "{";
        std::cout << "\"module\":";
        writeJsonString(std::cout, event.module);
        std::cout << ",";
        std::cout << "\"severity\":";
        writeJsonString(std::cout, event.severity);
        std::cout << ",";
        std::cout << "\"category\":";
        writeJsonString(std::cout, event.category);
        std::cout << ",";
        std::cout << "\"subject\":";
        writeJsonString(std::cout, event.subject);
        std::cout << ",";
        std::cout << "\"message\":";
        writeJsonString(std::cout, event.message);
        std::cout << ",";
        std::cout << "\"source_path\":";
        writeJsonString(std::cout, event.sourcePath);
        std::cout << ",";
        std::cout << "\"spawn_name\":";
        if (event.spawnName.empty())
        {
            std::cout << "null";
        }
        else
        {
            writeJsonString(std::cout, event.spawnName);
        }
        std::cout << ",";
        std::cout << "\"resolved_virtual_path\":";
        if (event.resolvedVirtualPath.empty())
        {
            std::cout << "null";
        }
        else
        {
            writeJsonString(std::cout, event.resolvedVirtualPath);
        }
        std::cout << "}";
    }
    std::cout << "],";

    std::cout << "\"unresolved_spawn_references\":[";
    bool firstUnresolved = true;
    for (const auto& entry : reporter.getUnresolvedSpawnCounts())
    {
        if (!firstUnresolved)
        {
            std::cout << ",";
        }
        firstUnresolved = false;

        std::cout << "{";
        std::cout << "\"module\":";
        writeJsonString(std::cout, entry.first.module);
        std::cout << ",";
        std::cout << "\"source_path\":";
        writeJsonString(std::cout, entry.first.sourcePath);
        std::cout << ",";
        std::cout << "\"spawn_name\":";
        writeJsonString(std::cout, entry.first.spawnName);
        std::cout << ",";
        std::cout << "\"resolved_virtual_path\":";
        writeJsonString(std::cout, entry.first.resolvedVirtualPath);
        std::cout << ",";
        std::cout << "\"occurrence_count\":" << entry.second;
        std::cout << "}";
    }
    std::cout << "]";

    if (options.emitReconciliation)
    {
        std::cout << ",\"reachable_objects\":[";
        bool firstReachable = true;
        for (const auto& reachable : reconciliationReport->reachableObjects)
        {
            if (!firstReachable)
            {
                std::cout << ",";
            }
            firstReachable = false;

            std::cout << "{";
            std::cout << "\"module\":";
            writeJsonString(std::cout, reachable.module);
            std::cout << ",";
            std::cout << "\"object_name\":";
            writeJsonString(std::cout, reachable.objectName);
            std::cout << ",";
            std::cout << "\"resolved_virtual_path\":";
            writeJsonString(std::cout, reachable.resolvedVirtualPath);
            std::cout << ",";
            std::cout << "\"origin_path\":";
            writeJsonString(std::cout, reachable.originPath);
            std::cout << ",";
            std::cout << "\"source_kind\":";
            writeJsonString(std::cout, reachable.sourceKind);
            std::cout << "}";
        }
        std::cout << "]";

        std::cout << ",\"reconciliation_rows\":[";
        bool firstRow = true;
        for (const auto& entry : reconciliationReport->rows)
        {
            const ReconciliationRow& row = entry.second;
            if (!firstRow)
            {
                std::cout << ",";
            }
            firstRow = false;

            std::cout << "{";
            std::cout << "\"module\":";
            writeJsonString(std::cout, row.module);
            std::cout << ",";
            std::cout << "\"source_path\":";
            writeJsonString(std::cout, row.sourcePath);
            std::cout << ",";
            std::cout << "\"raw_load_name\":";
            writeJsonString(std::cout, row.rawLoadName);
            std::cout << ",";
            std::cout << "\"normalized_name\":";
            writeJsonString(std::cout, row.normalizedName);
            std::cout << ",";
            std::cout << "\"resolved_virtual_path\":";
            writeJsonString(std::cout, row.resolvedVirtualPath);
            std::cout << ",";
            std::cout << "\"occurrence_count\":" << row.occurrenceCount << ",";
            std::cout << "\"canonical_key\":";
            writeJsonString(std::cout, row.canonicalKey);
            std::cout << ",";
            std::cout << "\"candidate_names\":[";
            for (size_t index = 0; index < row.candidateNames.size(); ++index)
            {
                if (index > 0)
                {
                    std::cout << ",";
                }
                writeJsonString(std::cout, row.candidateNames[index]);
            }
            std::cout << "]";
            std::cout << "}";
        }
        std::cout << "]";
    }

    std::cout << "}" << std::endl;
}

void printUsage(const char* executableName)
{
    std::cout
        << "Usage: " << executableName << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir <path>   Use the given Egoboo data directory.\n"
        << "  --emit-reconciliation  Add reachable-object and reconciliation JSON arrays.\n"
        << "  --json              Emit a machine-readable JSON report.\n"
        << "  --module <name>     Validate a single module by folder name or VFS path.\n"
        << "  --skip-scripts      Skip object script compilation checks.\n"
        << "  --verbose           Print progress while validating.\n"
        << "  --help              Show this help text.\n";
}

Options parseArguments(int argc, char** argv)
{
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];

        if ("--help" == argument)
        {
            printUsage(argv[0]);
            std::exit(EXIT_SUCCESS);
        }
        else if ("--verbose" == argument)
        {
            options.verbose = true;
        }
        else if ("--skip-scripts" == argument)
        {
            options.skipScripts = true;
        }
        else if ("--json" == argument)
        {
            options.jsonOutput = true;
        }
        else if ("--emit-reconciliation" == argument)
        {
            options.emitReconciliation = true;
        }
        else if ("--module" == argument)
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("missing value for --module");
            }
            options.hasModuleFilter = true;
            options.moduleFilter = argv[++i];
        }
        else if ("--data-dir" == argument)
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("missing value for --data-dir");
            }
            options.hasDataDir = true;
            options.dataDir = argv[++i];
        }
        else
        {
            throw std::runtime_error("unrecognized argument `" + argument + "`");
        }
    }

    if (options.emitReconciliation && !options.jsonOutput)
    {
        throw std::runtime_error("--emit-reconciliation requires --json");
    }

    return options;
}

bool validateObjectProfile(const std::string& moduleName,
                           const std::string& virtualObjectPath,
                           Reporter& reporter,
                           const Options& options,
                           std::unordered_set<std::string>& validatedObjects,
                           ModuleSummary& summary)
{
    const auto insertion = validatedObjects.insert(virtualObjectPath);
    if (!insertion.second)
    {
        return true;
    }

    ++summary.uniqueObjects;

    bool success = true;

    const std::string dataPath = virtualObjectPath + "/data.txt";
    const std::string modelPath = virtualObjectPath + "/tris.md2";
    const std::string scriptPath = virtualObjectPath + "/script.txt";

    if (!vfs_exists(dataPath))
    {
        reporter.error(moduleName, "missing_required_file", dataPath, "missing required object data file", &summary, dataPath);
        return false;
    }

    if (!vfs_exists(modelPath))
    {
        reporter.error(moduleName, "missing_required_file", modelPath, "missing required object model", &summary, modelPath);
        success = false;
    }

    try
    {
        auto profile = ObjectProfile::loadFromFile(virtualObjectPath, ObjectProfileRef(1), true);
        if (!profile)
        {
            reporter.error(moduleName, "parse_failure", virtualObjectPath, "unable to parse object profile", &summary, virtualObjectPath);
            return false;
        }

        if (!options.skipScripts)
        {
            if (!vfs_exists(scriptPath))
            {
                reporter.warning(moduleName,
                                 "script_missing",
                                 scriptPath,
                                 "missing object script; runtime will fall back to mp_data/script.txt",
                                 &summary,
                                 scriptPath);
            }

            script_info_t script;
            parser_state_t& parser = parser_state_t::get();
            if (rv_success != load_ai_script_vfs(parser, scriptPath, profile.get(), script))
            {
                reporter.error(moduleName,
                               "script_compile_failure",
                               scriptPath,
                               "unable to compile object script or fallback script",
                               &summary,
                               scriptPath);
                success = false;
            }
            else if (script.getName() != scriptPath)
            {
                reporter.warning(moduleName,
                                 "script_fallback",
                                 scriptPath,
                                 "object script failed and runtime fell back to `" + script.getName() + "`",
                                 &summary,
                                 scriptPath);
            }
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, "parse_failure", virtualObjectPath, ex.what(), &summary, virtualObjectPath);
        success = false;
    }

    return success;
}

bool shouldValidateSpawnObject(const spawn_file_info_t& entry, uint8_t importAmount)
{
    if (!entry.do_spawn)
    {
        return true;
    }

    if (entry.slot < 0)
    {
        return true;
    }

    return entry.slot > static_cast<int>(importAmount) * MAX_IMPORT_PER_PLAYER;
}

bool validateModule(const std::shared_ptr<ModuleProfile>& module,
                    Reporter& reporter,
                    const Options& options,
                    ModuleSummary& summary,
                    ReconciliationReport* reconciliationReport)
{
    const std::string moduleName = module->getFolderName();
    bool success = true;

    reporter.info("validating module `" + moduleName + "`");

    if (!setup_init_module_vfs_paths(module->getPath()))
    {
        reporter.error(moduleName,
                       "runtime_setup_failure",
                       module->getPath(),
                       "unable to configure module VFS paths",
                       &summary,
                       module->getPath());
        return false;
    }

    const ReachabilityIndex reachabilityIndex = buildReachabilityIndex(*module, reconciliationReport);
    const std::string menuPath = "mp_data/menu.txt";
    const std::string spawnPath = "mp_data/spawn.txt";
    const std::string mapPath = "mp_data/level.mpd";
    const std::string environmentPath = "mp_data/wawalite.txt";
    const std::string treasurePath = "mp_data/randomtreasure.txt";

    if (!vfs_exists(menuPath))
    {
        reporter.error(moduleName, "missing_required_file", menuPath, "missing required module metadata file", &summary, menuPath);
        success = false;
    }

    if (!vfs_exists(spawnPath))
    {
        reporter.error(moduleName, "missing_required_file", spawnPath, "missing required spawn file", &summary, spawnPath);
        success = false;
    }

    if (!vfs_exists(mapPath))
    {
        reporter.error(moduleName, "missing_required_file", mapPath, "missing required module mesh", &summary, mapPath);
        success = false;
    }

    if (!vfs_exists(environmentPath))
    {
        reporter.error(moduleName, "missing_required_file", environmentPath, "missing required environment file", &summary, environmentPath);
        success = false;
    }

    if (!success)
    {
        return false;
    }

    map_t map;
    if (!map.load(mapPath))
    {
        reporter.error(moduleName, "parse_failure", mapPath, "unable to parse module mesh", &summary, mapPath);
        success = false;
    }
    else
    {
        reporter.info("  mesh ok");
    }

    try
    {
        wawalite_data_t wawalite;
        if (!wawalite_data_read(environmentPath, &wawalite))
        {
            reporter.error(moduleName, "parse_failure", environmentPath, "unable to parse environment data", &summary, environmentPath);
            success = false;
        }
        else
        {
            reporter.info("  wawalite ok");
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, "parse_failure", environmentPath, ex.what(), &summary, environmentPath);
        success = false;
    }

    std::vector<spawn_file_info_t> spawnEntries;
    try
    {
        spawnEntries = SpawnFileReaderImpl().read(spawnPath);
        summary.spawnEntries = spawnEntries.size();
        reporter.info("  spawn ok");
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, "parse_failure", spawnPath, ex.what(), &summary, spawnPath);
        success = false;
    }

    if (!success)
    {
        return false;
    }

    std::unordered_set<std::string> validatedObjects;

    try
    {
        SearchContext moduleObjects(Ego::VfsPath(module->getPath() + "/objects"), Ego::Extension("obj"), VFS_SEARCH_DIR);
        while (moduleObjects.hasData())
        {
            ++summary.localObjects;
            const std::string virtualPath = objectVirtualPath(moduleObjects.getData().string());
            success = validateObjectProfile(moduleName, virtualPath, reporter, options, validatedObjects, summary) && success;
            moduleObjects.nextData();
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName,
                       "scan_failure",
                       module->getPath() + "/objects",
                       ex.what(),
                       &summary,
                       module->getPath() + "/objects");
        success = false;
    }

    try
    {
        Ego::TreasureTables treasureTables(treasurePath);
        for (const auto& entry : spawnEntries)
        {
            if (!shouldValidateSpawnObject(entry, module->getImportAmount()))
            {
                continue;
            }

            const std::string rawLoadName = entry.spawn_comment;
            const std::string normalizedName = Ego::SpawnFile::resolveSpawnLoadName(rawLoadName, treasureTables);

            const std::string virtualPath = objectVirtualPath(normalizedName);
            if (!vfs_exists(virtualPath))
            {
                reporter.error(moduleName,
                               "missing_spawn_object",
                               spawnPath,
                               "referenced object `" + normalizedName + "` was not found on mp_objects",
                               &summary,
                               spawnPath,
                               normalizedName,
                               virtualPath);
                appendReconciliationRow(reconciliationReport,
                                        moduleName,
                                        spawnPath,
                                        rawLoadName,
                                        normalizedName,
                                        virtualPath,
                                        reachabilityIndex);
                success = false;
                continue;
            }

            success = validateObjectProfile(moduleName, virtualPath, reporter, options, validatedObjects, summary) && success;
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, "parse_failure", treasurePath, ex.what(), &summary, treasurePath);
        success = false;
    }

    return success;
}

bool matchesModuleFilter(const ModuleProfile& module, const Options& options)
{
    if (!options.hasModuleFilter)
    {
        return true;
    }

    return module.getFolderName() == options.moduleFilter
        || module.getPath() == options.moduleFilter;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseArguments(argc, argv);

        if (options.hasDataDir)
        {
            if (SDL_setenv("EGOBOO_DATA_DIR", options.dataDir.c_str(), 1) != 0)
            {
                throw std::runtime_error("unable to set EGOBOO_DATA_DIR");
            }
        }

        Reporter reporter(options.verbose, options.jsonOutput);
        std::vector<ModuleResult> results;
        ReconciliationReport reconciliationReport;
        int exitCode = EXIT_FAILURE;

        {
            StreamSilencer silencer(options.jsonOutput);
            ContentRuntimeBootstrap::Options bootstrapOptions;
            bootstrapOptions.initializeVirtualFileSystem = true;
            bootstrapOptions.initializeBaseVfsPaths = true;
            bootstrapOptions.initializeLogging = true;
            bootstrapOptions.configureLightweightProfileLoading = true;
            bootstrapOptions.initializeImageManager = true;
            bootstrapOptions.initializePerkHandler = true;
            bootstrapOptions.initializeProfileSystem = true;
            bootstrapOptions.clearModuleVfsPathsOnShutdown = true;
            bootstrapOptions.clearBaseVfsPathsOnShutdown = true;
            bootstrapOptions.seedRandom = true;
            bootstrapOptions.randomSeed = 0;
            bootstrapOptions.binaryPath = argv[0];
            bootstrapOptions.logPath = "/debug/content-validator.log";
            bootstrapOptions.logLevel = Log::Level::Warning;
            ContentRuntimeBootstrap runtime(bootstrapOptions);

            ProfileSystem::get().loadModuleProfiles();

            std::vector<std::shared_ptr<ModuleProfile>> selectedModules;
            for (const auto& module : ProfileSystem::get().getModuleProfiles())
            {
                if (module && matchesModuleFilter(*module, options))
                {
                    selectedModules.push_back(module);
                }
            }

            if (selectedModules.empty())
            {
                throw std::runtime_error("no modules matched the requested filter");
            }

            std::sort(selectedModules.begin(),
                      selectedModules.end(),
                      [](const std::shared_ptr<ModuleProfile>& left, const std::shared_ptr<ModuleProfile>& right)
                      {
                          return left->getFolderName() < right->getFolderName();
                      });

            results.reserve(selectedModules.size());

            for (const auto& module : selectedModules)
            {
                ModuleResult result;
                result.moduleName = module->getFolderName();
                result.ok = validateModule(module,
                                           reporter,
                                           options,
                                           result.summary,
                                           options.emitReconciliation ? &reconciliationReport : nullptr);
                results.push_back(result);

                if (!options.jsonOutput)
                {
                    std::cout << (result.ok ? "[ok]   " : "[fail] ")
                              << result.moduleName
                              << " local_objects=" << result.summary.localObjects
                              << " unique_objects=" << result.summary.uniqueObjects
                              << " spawn_entries=" << result.summary.spawnEntries
                              << " warnings=" << result.summary.warnings
                              << " errors=" << result.summary.errors
                              << std::endl;
                }
            }

            exitCode = reporter.errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        if (options.jsonOutput)
        {
            emitJsonReport(options,
                           reporter,
                           results,
                           options.emitReconciliation ? &reconciliationReport : nullptr);
        }
        else
        {
            std::cout << "validated modules=" << results.size()
                      << " warnings=" << reporter.warnings
                      << " errors=" << reporter.errors
                      << std::endl;
        }

        return exitCode;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "fatal: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "fatal: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }
}
