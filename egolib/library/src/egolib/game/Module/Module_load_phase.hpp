#pragma once

class GameModule;

namespace module_loading
{

class ModuleLoadPhase
{
public:
    explicit ModuleLoadPhase(GameModule& module);

    void run();

private:
    void initializeRuntime();
    void initializeTeamsAndTextures();
    void initializeSharedAssets();
    void loadEnvironment();
    void loadContent();
    void finalizeInitialization();

private:
    GameModule& _module;
};

} // namespace module_loading
