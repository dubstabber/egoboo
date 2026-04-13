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

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct Options
{
    bool verbose = false;
    bool skipScripts = false;
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
};

class Reporter
{
public:
    explicit Reporter(bool verbose) :
        verbose(verbose),
        errors(0),
        warnings(0)
    {}

    void info(const std::string& message) const
    {
        if (verbose)
        {
            std::cout << message << std::endl;
        }
    }

    void error(const std::string& moduleName, const std::string& subject, const std::string& message)
    {
        ++errors;
        std::cerr << "error"
                  << " [" << moduleName << "] "
                  << subject << ": "
                  << message << std::endl;
    }

    void warning(const std::string& moduleName, const std::string& subject, const std::string& message)
    {
        ++warnings;
        std::cerr << "warning"
                  << " [" << moduleName << "] "
                  << subject << ": "
                  << message << std::endl;
    }

    size_t errors;
    size_t warnings;

private:
    bool verbose;
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

void printUsage(const char* executableName)
{
    std::cout
        << "Usage: " << executableName << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --data-dir <path>   Use the given Egoboo data directory.\n"
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
        reporter.error(moduleName, dataPath, "missing required object data file");
        return false;
    }

    if (!vfs_exists(modelPath))
    {
        reporter.error(moduleName, modelPath, "missing required object model");
        success = false;
    }

    try
    {
        auto profile = ObjectProfile::loadFromFile(virtualObjectPath, ObjectProfileRef(1), true);
        if (!profile)
        {
            reporter.error(moduleName, virtualObjectPath, "unable to parse object profile");
            return false;
        }

        if (!options.skipScripts)
        {
            if (!vfs_exists(scriptPath))
            {
                reporter.warning(moduleName, scriptPath, "missing object script; runtime will fall back to mp_data/script.txt");
            }

            script_info_t script;
            parser_state_t& parser = parser_state_t::get();
            if (rv_success != load_ai_script_vfs(parser, scriptPath, profile.get(), script))
            {
                reporter.error(moduleName, scriptPath, "unable to compile object script or fallback script");
                success = false;
            }
            else if (script.getName() != scriptPath)
            {
                reporter.warning(moduleName, scriptPath, "object script failed and runtime fell back to `" + script.getName() + "`");
            }
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, virtualObjectPath, ex.what());
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
        reporter.error(moduleName, module->getPath(), "unable to configure module VFS paths");
        return false;
    }

    const std::string menuPath = "mp_data/menu.txt";
    const std::string spawnPath = "mp_data/spawn.txt";
    const std::string mapPath = "mp_data/level.mpd";
    const std::string environmentPath = "mp_data/wawalite.txt";
    const std::string treasurePath = "mp_data/randomtreasure.txt";

    if (!vfs_exists(menuPath))
    {
        reporter.error(moduleName, menuPath, "missing required module metadata file");
        success = false;
    }

    if (!vfs_exists(spawnPath))
    {
        reporter.error(moduleName, spawnPath, "missing required spawn file");
        success = false;
    }

    if (!vfs_exists(mapPath))
    {
        reporter.error(moduleName, mapPath, "missing required module mesh");
        success = false;
    }

    if (!vfs_exists(environmentPath))
    {
        reporter.error(moduleName, environmentPath, "missing required environment file");
        success = false;
    }

    if (!success)
    {
        return false;
    }

    map_t map;
    if (!map.load(mapPath))
    {
        reporter.error(moduleName, mapPath, "unable to parse module mesh");
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
            reporter.error(moduleName, environmentPath, "unable to parse environment data");
            success = false;
        }
        else
        {
            reporter.info("  wawalite ok");
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, environmentPath, ex.what());
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
        reporter.error(moduleName, spawnPath, ex.what());
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
        reporter.error(moduleName, module->getPath() + "/objects", ex.what());
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
                reporter.error(moduleName, spawnPath, "referenced object `" + normalized.spawn_comment + "` was not found on mp_objects");
                success = false;
                continue;
            }

            success = validateObjectProfile(moduleName, virtualPath, reporter, options, validatedObjects, summary) && success;
        }
    }
    catch (const std::exception& ex)
    {
        reporter.error(moduleName, treasurePath, ex.what());
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

        MinimalRuntime runtime(argv[0]);
        Reporter reporter(options.verbose);

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

        size_t validatedModules = 0;
        for (const auto& module : selectedModules)
        {
            const size_t errorsBefore = reporter.errors;
            const size_t warningsBefore = reporter.warnings;

            ModuleSummary summary;
            const bool ok = validateModule(module, reporter, options, summary);
            ++validatedModules;

            std::cout << (ok ? "[ok]   " : "[fail] ")
                      << module->getFolderName()
                      << " local_objects=" << summary.localObjects
                      << " unique_objects=" << summary.uniqueObjects
                      << " spawn_entries=" << summary.spawnEntries
                      << " warnings=" << (reporter.warnings - warningsBefore)
                      << " errors=" << (reporter.errors - errorsBefore)
                      << std::endl;
        }

        std::cout << "validated modules=" << validatedModules
                  << " warnings=" << reporter.warnings
                  << " errors=" << reporter.errors
                  << std::endl;

        return reporter.errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
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
