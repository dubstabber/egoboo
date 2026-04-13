#include "egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/FileFormats/wawalite_file.h"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/Logic/TreasureTables.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Module/module_spawn.h"
#include "egolib/game/game.h"
#include "egolib/game/script_compile.h"
#include "egolib/vfs.h"

#include <SDL.h>

#include <algorithm>
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

class MinimalRuntime
{
public:
    explicit MinimalRuntime(const std::string& binaryPath)
    {
        if (vfs_init(binaryPath.c_str(), nullptr))
        {
            throw std::runtime_error("unable to initialize virtual file system");
        }

        setup_init_base_vfs_paths();
        Log::initialize("/debug/content-validator.log", Log::Level::Warning);

        // Avoid null model access in lightweight profile validation.
        egoboo_config_t::get().graphic_gouraudShading_enable.setValue(false);

        Random::setSeed(0);
        Ego::ImageManager::initialize();
        Ego::Perks::PerkHandler::initialize();
        ProfileSystem::initialize();
    }

    ~MinimalRuntime()
    {
        ProfileSystem::uninitialize();
        Ego::Perks::PerkHandler::uninitialize();
        Ego::ImageManager::uninitialize();
        setup_clear_module_vfs_paths();
        setup_clear_base_vfs_paths();
        Log::uninitialize();
    }
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
                    const std::vector<ModuleResult>& results)
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

    std::cout << "\"schema_version\":1,";

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

    std::cout << "}" << std::endl;
}

void printUsage(const char* executableName)
{
    std::cout
        << "Usage: " << executableName << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir <path>   Use the given Egoboo data directory.\n"
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
                    ModuleSummary& summary)
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

            spawn_file_info_t normalized = entry;
            convert_spawn_file_load_name(normalized, treasureTables);

            const std::string virtualPath = objectVirtualPath(normalized.spawn_comment);
            if (!vfs_exists(virtualPath))
            {
                reporter.error(moduleName,
                               "missing_spawn_object",
                               spawnPath,
                               "referenced object `" + normalized.spawn_comment + "` was not found on mp_objects",
                               &summary,
                               spawnPath,
                               normalized.spawn_comment,
                               virtualPath);
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
        int exitCode = EXIT_FAILURE;

        {
            StreamSilencer silencer(options.jsonOutput);
            MinimalRuntime runtime(argv[0]);

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
                result.ok = validateModule(module, reporter, options, result.summary);
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
            emitJsonReport(options, reporter, results);
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
