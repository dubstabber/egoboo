#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"

#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/Math/Random.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/vfs.h"

#include <stdexcept>

ContentRuntimeBootstrap::ContentRuntimeBootstrap(const Options& options) :
    _options(options)
{
    if (_options.initializeVirtualFileSystem)
    {
        const char* egobooPath = _options.egobooPath.empty() ? nullptr : _options.egobooPath.c_str();
        if (vfs_init(_options.binaryPath.c_str(), egobooPath))
        {
            throw std::runtime_error("unable to initialize virtual file system");
        }
    }

    if (_options.initializeBaseVfsPaths)
    {
        setup_init_base_vfs_paths();
        _baseVfsPathsInitialized = true;
    }

    if (_options.initializeLogging)
    {
        Log::initialize(_options.logPath, _options.logLevel);
        _loggingInitialized = true;
    }

    if (_options.configureLightweightProfileLoading)
    {
        egoboo_config_t::get().graphic_gouraudShading_enable.setValue(false);
    }

    if (_options.seedRandom)
    {
        Random::setSeed(_options.randomSeed);
    }

    if (_options.initializeImageManager)
    {
        Ego::ImageManager::initialize();
        _imageManagerInitialized = true;
    }

    if (_options.initializePerkHandler)
    {
        Ego::Perks::PerkHandler::initialize();
        EngineContext::get().installPerkHandler(Ego::Perks::PerkHandler::get());
        _perkHandlerInitialized = true;
    }

    if (_options.initializeProfileSystem)
    {
        ProfileSystem::initialize();
        _profileSystemInitialized = true;
    }
}

ContentRuntimeBootstrap::~ContentRuntimeBootstrap()
{
    if (_profileSystemInitialized)
    {
        ProfileSystem::uninitialize();
    }

    if (_perkHandlerInitialized)
    {
        EngineContext::get().clearPerkHandler();
        Ego::Perks::PerkHandler::uninitialize();
    }

    if (_imageManagerInitialized)
    {
        Ego::ImageManager::uninitialize();
    }

    if (_options.clearModuleVfsPathsOnShutdown)
    {
        setup_clear_module_vfs_paths();
    }

    if (_options.clearBaseVfsPathsOnShutdown && _baseVfsPathsInitialized)
    {
        setup_clear_base_vfs_paths();
    }

    if (_loggingInitialized)
    {
        Log::uninitialize();
    }
}
