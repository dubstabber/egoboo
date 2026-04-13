#pragma once

#include "egolib/Log/_Include.hpp"
#include "idlib/non_copyable.hpp"

#include <cstdint>
#include <string>

class ContentRuntimeBootstrap : private idlib::non_copyable
{
public:
    struct Options
    {
        bool initializeVirtualFileSystem = false;
        bool initializeBaseVfsPaths = false;
        bool initializeLogging = false;
        bool configureLightweightProfileLoading = false;
        bool initializeImageManager = false;
        bool initializePerkHandler = true;
        bool initializeProfileSystem = true;
        bool clearModuleVfsPathsOnShutdown = false;
        bool clearBaseVfsPathsOnShutdown = false;
        bool seedRandom = false;
        uint32_t randomSeed = 0;
        std::string binaryPath;
        std::string egobooPath;
        std::string logPath = "/debug/log.txt";
        Log::Level logLevel = Log::Level::Warning;
    };

    explicit ContentRuntimeBootstrap(const Options& options);
    ~ContentRuntimeBootstrap();

private:
    Options _options;
    bool _baseVfsPathsInitialized = false;
    bool _loggingInitialized = false;
    bool _imageManagerInitialized = false;
    bool _perkHandlerInitialized = false;
    bool _profileSystemInitialized = false;
};
