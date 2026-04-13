# Minimum required CMake version.
cmake_minimum_required(VERSION 3.6)

# Project name and settings.
project(egolib-library CXX)
message("building Egolib Library")
set_project_default_properties()

set(SOURCE_FILES "")

# Include directories for project.
include_directories(${PROJECT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Explicit per-subsystem source lists.
#
# Grouped by logical subsystem so that ownership is visible in the build
# system. Each section is ordered alphabetically within the group.
# When adding new files, place them in the appropriate section.
# ---------------------------------------------------------------------------

# -- AI ----------------------------------------------------------------------
set(EGOLIB_AI_SOURCES
    src/egolib/AI/AStar.cpp
    src/egolib/AI/AStar.hpp
    src/egolib/AI/LineOfSight.cpp
    src/egolib/AI/LineOfSight.hpp
    src/egolib/AI/State.hpp
    src/egolib/AI/WaypointList.c
    src/egolib/AI/WaypointList.h
)

# -- Audio -------------------------------------------------------------------
set(EGOLIB_AUDIO_SOURCES
    src/egolib/Audio/AudioSystem.cpp
    src/egolib/Audio/AudioSystem.hpp
)

# -- Configuration -----------------------------------------------------------
set(EGOLIB_CONFIGURATION_SOURCES
    src/egolib/Configuration/Configuration.hpp
    src/egolib/Configuration/Enum.hpp
    src/egolib/Configuration/Number.hpp
    src/egolib/Configuration/StringOrBool.hpp
    src/egolib/Configuration/Variable.hpp
)

# -- Console -----------------------------------------------------------------
set(EGOLIB_CONSOLE_SOURCES
    src/egolib/Console/Console.cpp
    src/egolib/Console/Console.hpp
)

# -- Core --------------------------------------------------------------------
set(EGOLIB_CORE_SOURCES
    src/egolib/Core/QuadTree.hpp
    src/egolib/Core/StringUtilities.hpp
    src/egolib/Core/System.cpp
    src/egolib/Core/System.hpp
    src/egolib/Core/ThreadPool.hpp
)

# -- Entities ----------------------------------------------------------------
set(EGOLIB_ENTITIES_SOURCES
    src/egolib/Entities/Common.cpp
    src/egolib/Entities/Common.hpp
    src/egolib/Entities/Enchant.cpp
    src/egolib/Entities/Enchant.hpp
    src/egolib/Entities/_Include.hpp
    src/egolib/Entities/Object.cpp
    src/egolib/Entities/ObjectHandler.cpp
    src/egolib/Entities/ObjectHandler.hpp
    src/egolib/Entities/Object.hpp
    src/egolib/Entities/Particle.cpp
    src/egolib/Entities/ParticleHandler.cpp
    src/egolib/Entities/ParticleHandler.hpp
    src/egolib/Entities/Particle.hpp
)

# -- Extensions --------------------------------------------------------------
set(EGOLIB_EXTENSIONS_SOURCES
    src/egolib/Extensions/ogl_extensions.c
    src/egolib/Extensions/ogl_extensions.h
)

# -- FileFormats -------------------------------------------------------------
set(EGOLIB_FILEFORMATS_SOURCES
    src/egolib/FileFormats/ConfigFile/configfile.c
    src/egolib/FileFormats/ConfigFile/configfile.h
    src/egolib/FileFormats/ConfigFile/ConfigFileToken.cpp
    src/egolib/FileFormats/ConfigFile/ConfigFileToken.hpp
    src/egolib/FileFormats/ConfigFile/ConfigFileTokenKind.cpp
    src/egolib/FileFormats/ConfigFile/ConfigFileTokenKind.hpp
    src/egolib/FileFormats/Globals.cpp
    src/egolib/FileFormats/Globals.hpp
    src/egolib/FileFormats/id_md2.c
    src/egolib/FileFormats/id_md2.h
    src/egolib/FileFormats/map_file.c
    src/egolib/FileFormats/map_file.h
    src/egolib/FileFormats/map_file-v1.c
    src/egolib/FileFormats/map_file-v1.h
    src/egolib/FileFormats/map_file-v2.c
    src/egolib/FileFormats/map_file-v2.h
    src/egolib/FileFormats/map_file-v3.c
    src/egolib/FileFormats/map_file-v3.h
    src/egolib/FileFormats/map_file-v4.c
    src/egolib/FileFormats/map_file-v4.h
    src/egolib/FileFormats/map_fx.hpp
    src/egolib/FileFormats/MapTileDefinitionsDictionary.cpp
    src/egolib/FileFormats/MapTileDefinitionsDictionary.hpp
    src/egolib/FileFormats/map_tile_dictionary.c
    src/egolib/FileFormats/map_tile_dictionary.h
    src/egolib/FileFormats/SpawnFile/spawn_file.c
    src/egolib/FileFormats/SpawnFile/spawn_file.h
    src/egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.cpp
    src/egolib/FileFormats/SpawnFile/SpawnFileReaderImpl.hpp
    src/egolib/FileFormats/SpawnFile/SpawnFileToken.cpp
    src/egolib/FileFormats/SpawnFile/SpawnFileToken.hpp
    src/egolib/FileFormats/SpawnFile/SpawnFileTokenKind.cpp
    src/egolib/FileFormats/SpawnFile/SpawnFileTokenKind.hpp
    src/egolib/FileFormats/SpawnFile/SpawnName.cpp
    src/egolib/FileFormats/SpawnFile/SpawnName.hpp
    src/egolib/FileFormats/template.c
    src/egolib/FileFormats/template.h
    src/egolib/FileFormats/wawalite_file.c
    src/egolib/FileFormats/wawalite_file.h
)

# -- Graphics (engine level) -------------------------------------------------
set(EGOLIB_GRAPHICS_SOURCES
    src/egolib/Graphics/Animation2D.hpp
    src/egolib/Graphics/Camera.hpp
    src/egolib/Graphics/descriptor_factory.hpp
    src/egolib/Graphics/Display.cpp
    src/egolib/Graphics/Display.hpp
    src/egolib/Graphics/DisplayMode.cpp
    src/egolib/Graphics/DisplayMode.hpp
    src/egolib/Graphics/Font.cpp
    src/egolib/Graphics/Font.hpp
    src/egolib/Graphics/FontManager.cpp
    src/egolib/Graphics/FontManager.hpp
    src/egolib/Graphics/GraphicsContext.cpp
    src/egolib/Graphics/GraphicsContext.hpp
    src/egolib/Graphics/GraphicsSystem.cpp
    src/egolib/Graphics/GraphicsSystem.hpp
    src/egolib/Graphics/GraphicsSystemNew.cpp
    src/egolib/Graphics/GraphicsSystemNew.hpp
    src/egolib/Graphics/GraphicsWindow.cpp
    src/egolib/Graphics/GraphicsWindow.hpp
    src/egolib/Graphics/IndexFormat.cpp
    src/egolib/Graphics/IndexFormat.hpp
    src/egolib/Graphics/MD2Model.cpp
    src/egolib/Graphics/MD2Model.hpp
    src/egolib/Graphics/ModelDescriptor.cpp
    src/egolib/Graphics/ModelDescriptor.hpp
    src/egolib/Graphics/PixelFormat.cpp
    src/egolib/Graphics/PixelFormat.hpp
    src/egolib/Graphics/SDL/Display.cpp
    src/egolib/Graphics/SDL/Display.hpp
    src/egolib/Graphics/SDL/DisplayMode.cpp
    src/egolib/Graphics/SDL/DisplayMode.hpp
    src/egolib/Graphics/SDL/GraphicsContext.cpp
    src/egolib/Graphics/SDL/GraphicsContext.hpp
    src/egolib/Graphics/SDL/GraphicsSystemNew.cpp
    src/egolib/Graphics/SDL/GraphicsSystemNew.hpp
    src/egolib/Graphics/SDL/GraphicsWindow.cpp
    src/egolib/Graphics/SDL/GraphicsWindow.hpp
    src/egolib/Graphics/SDL/Utilities.cpp
    src/egolib/Graphics/SDL/Utilities.hpp
    src/egolib/Graphics/TextureManager.cpp
    src/egolib/Graphics/TextureManager.hpp
    src/egolib/Graphics/VertexFormat.cpp
    src/egolib/Graphics/VertexFormat.hpp
    src/egolib/Graphics/Viewport.cpp
    src/egolib/Graphics/Viewport.hpp
)

# -- Grid --------------------------------------------------------------------
set(EGOLIB_GRID_SOURCES
    src/egolib/Grid/Index.hpp
    src/egolib/Grid/Info.hpp
    src/egolib/Grid/Rect.hpp
)

# -- Image -------------------------------------------------------------------
set(EGOLIB_IMAGE_SOURCES
    src/egolib/Image/convert.hpp
    src/egolib/Image/Image.cpp
    src/egolib/Image/Image.hpp
    src/egolib/Image/ImageLoader.cpp
    src/egolib/Image/ImageLoader.hpp
    src/egolib/Image/ImageLoader_SDL.cpp
    src/egolib/Image/ImageLoader_SDL.hpp
    src/egolib/Image/ImageLoader_SDL_image.cpp
    src/egolib/Image/ImageLoader_SDL_image.hpp
    src/egolib/Image/ImageManager.cpp
    src/egolib/Image/ImageManager.hpp
    src/egolib/Image/SDL_Image_Extensions.c
    src/egolib/Image/SDL_Image_Extensions.h
)

# -- InputControl ------------------------------------------------------------
set(EGOLIB_INPUTCONTROL_SOURCES
    src/egolib/InputControl/ControlSettingsFile.cpp
    src/egolib/InputControl/ControlSettingsFile.hpp
    src/egolib/InputControl/InputDevice.cpp
    src/egolib/InputControl/InputDevice.hpp
    src/egolib/InputControl/InputSystem.cpp
    src/egolib/InputControl/InputSystem.hpp
    src/egolib/InputControl/ModifierKeys.hpp
)

# -- Integrations ------------------------------------------------------------
set(EGOLIB_INTEGRATIONS_SOURCES
    src/egolib/integrations/color.hpp
    src/egolib/integrations/events.hpp
    src/egolib/integrations/filesystem.hpp
    src/egolib/integrations/math.hpp
    src/egolib/integrations/signal.hpp
    src/egolib/integrations/video.hpp
)

# -- Log ---------------------------------------------------------------------
set(EGOLIB_LOG_SOURCES
    src/egolib/Log/ConsoleColor.cpp
    src/egolib/Log/ConsoleColor.hpp
    src/egolib/Log/DefaultTarget.cpp
    src/egolib/Log/DefaultTarget.hpp
    src/egolib/Log/EndOfEntry.cpp
    src/egolib/Log/EndOfEntry.hpp
    src/egolib/Log/EndOfLine.cpp
    src/egolib/Log/EndOfLine.hpp
    src/egolib/Log/Entry.cpp
    src/egolib/Log/Entry.hpp
    src/egolib/Log/_Include.cpp
    src/egolib/Log/_Include.hpp
    src/egolib/Log/Level.cpp
    src/egolib/Log/Level.hpp
    src/egolib/Log/Target.cpp
    src/egolib/Log/Target.hpp
)

# -- Logic -------------------------------------------------------------------
set(EGOLIB_LOGIC_SOURCES
    src/egolib/Logic/Action.hpp
    src/egolib/Logic/Attribute.hpp
    src/egolib/Logic/Damage.hpp
    src/egolib/Logic/FeedbackType.hpp
    src/egolib/Logic/GameDifficulty.hpp
    src/egolib/Logic/Gender.hpp
    src/egolib/Logic/MissileTreatment.cpp
    src/egolib/Logic/MissileTreatment.hpp
    src/egolib/Logic/ObjectSlot.hpp
    src/egolib/Logic/Perk.cpp
    src/egolib/Logic/Perk.hpp
    src/egolib/Logic/PerkHandler.cpp
    src/egolib/Logic/PerkHandler.hpp
    src/egolib/Logic/Team.cpp
    src/egolib/Logic/Team.hpp
    src/egolib/Logic/TreasureTables.cpp
    src/egolib/Logic/TreasureTables.hpp
)

# -- Math --------------------------------------------------------------------
set(EGOLIB_MATH_SOURCES
    src/egolib/Math/euler_angle.hpp
    src/egolib/Math/_Include.hpp
    src/egolib/Math/Math.hpp
    src/egolib/Math/Random.cpp
    src/egolib/Math/Random.hpp
    src/egolib/Math/Standard.cpp
    src/egolib/Math/Standard.hpp
    src/egolib/Math/VectorProjection.hpp
    src/egolib/Math/VectorRejection.hpp
)

# -- Mesh --------------------------------------------------------------------
set(EGOLIB_MESH_SOURCES
    src/egolib/Mesh/Info.cpp
    src/egolib/Mesh/Info.hpp
    src/egolib/Mesh/TileFX.cpp
    src/egolib/Mesh/TileFX.hpp
)

# -- Platform ----------------------------------------------------------------
set(EGOLIB_PLATFORM_SOURCES
    src/egolib/platform.h
    src/egolib/Platform/NSFileManager+DirectoryLocations.h
)

# -- Profiles ----------------------------------------------------------------
set(EGOLIB_PROFILES_SOURCES
    src/egolib/Profiles/AbstractProfile.hpp
    src/egolib/Profiles/_AbstractProfileSystem.hpp
    src/egolib/Profiles/EnchantProfile.cpp
    src/egolib/Profiles/EnchantProfile.hpp
    src/egolib/Profiles/EnchantProfileWriter.cpp
    src/egolib/Profiles/EnchantProfileWriter.hpp
    src/egolib/Profiles/GenderProfile.hpp
    src/egolib/Profiles/_Include.hpp
    src/egolib/Profiles/LocalParticleProfileRef.cpp
    src/egolib/Profiles/LocalParticleProfileRef.hpp
    src/egolib/Profiles/ModuleProfile.cpp
    src/egolib/Profiles/ModuleProfile.hpp
    src/egolib/Profiles/ObjectProfile.cpp
    src/egolib/Profiles/ObjectProfile.hpp
    src/egolib/Profiles/ParticleProfile.cpp
    src/egolib/Profiles/ParticleProfile.hpp
    src/egolib/Profiles/ParticleProfileWriter.cpp
    src/egolib/Profiles/ParticleProfileWriter.hpp
    src/egolib/Profiles/ProfileSystem.cpp
    src/egolib/Profiles/ProfileSystem.hpp
    src/egolib/Profiles/RandomName.cpp
    src/egolib/Profiles/RandomName.hpp
)

# -- Renderer ----------------------------------------------------------------
set(EGOLIB_RENDERER_SOURCES
    src/egolib/Renderer/DeferredTexture.cpp
    src/egolib/Renderer/DeferredTexture.hpp
    src/egolib/Renderer/OpenGL/AccumulationBuffer.cpp
    src/egolib/Renderer/OpenGL/AccumulationBuffer.hpp
    src/egolib/Renderer/OpenGL/ColourBuffer.cpp
    src/egolib/Renderer/OpenGL/ColourBuffer.hpp
    src/egolib/Renderer/OpenGL/DefaultTexture.cpp
    src/egolib/Renderer/OpenGL/DefaultTexture.hpp
    src/egolib/Renderer/OpenGL/DepthBuffer.cpp
    src/egolib/Renderer/OpenGL/DepthBuffer.hpp
    src/egolib/Renderer/OpenGL/Renderer.cpp
    src/egolib/Renderer/OpenGL/Renderer.hpp
    src/egolib/Renderer/OpenGL/RendererInfo.cpp
    src/egolib/Renderer/OpenGL/RendererInfo.hpp
    src/egolib/Renderer/OpenGL/StencilBuffer.cpp
    src/egolib/Renderer/OpenGL/StencilBuffer.hpp
    src/egolib/Renderer/OpenGL/Texture.cpp
    src/egolib/Renderer/OpenGL/Texture.hpp
    src/egolib/Renderer/OpenGL/TextureUnit.cpp
    src/egolib/Renderer/OpenGL/TextureUnit.hpp
    src/egolib/Renderer/OpenGL/Utilities.cpp
    src/egolib/Renderer/OpenGL/Utilities.hpp
    src/egolib/Renderer/Renderer.cpp
    src/egolib/Renderer/Renderer.hpp
    src/egolib/Renderer/RendererInfo.cpp
    src/egolib/Renderer/RendererInfo.hpp
    src/egolib/Renderer/Texture.cpp
    src/egolib/Renderer/Texture.hpp
)

# -- Script ------------------------------------------------------------------
set(EGOLIB_SCRIPT_SOURCES
    src/egolib/Script/Buffer.cpp
    src/egolib/Script/Buffer.hpp
    src/egolib/Script/CLogEntry.cpp
    src/egolib/Script/CLogEntry.hpp
    src/egolib/Script/Constant.cpp
    src/egolib/Script/Constant.hpp
    src/egolib/Script/ConstantPool.cpp
    src/egolib/Script/ConstantPool.hpp
    src/egolib/Script/DDLToken.cpp
    src/egolib/Script/DDLToken.hpp
    src/egolib/Script/DDLTokenDecoder.cpp
    src/egolib/Script/DDLTokenDecoder.hpp
    src/egolib/Script/DDLTokenKind.cpp
    src/egolib/Script/DDLTokenKind.hpp
    src/egolib/Script/EnumDescriptor.hpp
    src/egolib/Script/Errors.cpp
    src/egolib/Script/Errors.hpp
    src/egolib/Script/Interpreter/Configuration.cpp
    src/egolib/Script/Interpreter/Configuration.hpp
    src/egolib/Script/Interpreter/InvalidCastException.cpp
    src/egolib/Script/Interpreter/InvalidCastException.hpp
    src/egolib/Script/Interpreter/SafeCast.cpp
    src/egolib/Script/Interpreter/SafeCast.hpp
    src/egolib/Script/Interpreter/Tag.cpp
    src/egolib/Script/Interpreter/Tag.hpp
    src/egolib/Script/Interpreter/TaggedValue.cpp
    src/egolib/Script/Interpreter/TaggedValue.hpp
    src/egolib/Script/IRuntimeStatistics.cpp
    src/egolib/Script/IRuntimeStatistics.hpp
    src/egolib/Script/OpcodeInfo.cpp
    src/egolib/Script/OpcodeInfo.hpp
    src/egolib/Script/PDLToken.cpp
    src/egolib/Script/PDLToken.hpp
    src/egolib/Script/PDLTokenKind.cpp
    src/egolib/Script/PDLTokenKind.hpp
    src/egolib/Script/Scanner.cpp
    src/egolib/Script/Scanner.hpp
    src/egolib/Script/script.c
    src/egolib/Script/script.h
    src/egolib/Script/TextFile.cpp
    src/egolib/Script/TextFile.hpp
    src/egolib/Script/TextInputFile.cpp
    src/egolib/Script/TextInputFile.hpp
    src/egolib/Script/Traits.hpp
)

# -- Time --------------------------------------------------------------------
set(EGOLIB_TIME_SOURCES
    src/egolib/Time/LocalTime.cpp
    src/egolib/Time/LocalTime.hpp
    src/egolib/Time/SlidingWindow.hpp
    src/egolib/Time/Stopwatch.hpp
    src/egolib/Time/Time.cpp
    src/egolib/Time/Time.hpp
    src/egolib/Time/Unit.hpp
)

# -- VFS ---------------------------------------------------------------------
set(EGOLIB_VFS_SOURCES
    src/egolib/vfs.c
    src/egolib/vfs.h
    src/egolib/VFS/FsPath.cpp
    src/egolib/VFS/FsPath.hpp
    src/egolib/VFS/internal.hpp
    src/egolib/VFS/VfsPath.cpp
    src/egolib/VFS/VfsPath.hpp
)

# -- Top-level egolib files (not yet assigned to a subsystem) ----------------
set(EGOLIB_TOPLEVEL_SOURCES
    src/egolib/App.cpp
    src/egolib/App.hpp
    src/egolib/bbox.c
    src/egolib/bbox.h
    src/egolib/Clock.cpp
    src/egolib/Clock.hpp
    src/egolib/Debug.cpp
    src/egolib/Debug.hpp
    src/egolib/egoboo_setup.c
    src/egolib/egoboo_setup.h
    src/egolib/egolib_config.h
    src/egolib/egolib.h
    src/egolib/endian.h
    src/egolib/file_common.c
    src/egolib/file_common.h
    src/egolib/fileutil.c
    src/egolib/fileutil.h
    src/egolib/Float.cpp
    src/egolib/Float.hpp
    src/egolib/font_bmp.c
    src/egolib/font_bmp.h
    src/egolib/frustum.c
    src/egolib/frustum.h
    src/egolib/geometry.c
    src/egolib/geometry.h
    src/egolib/IDSZ.cpp
    src/egolib/IDSZ.hpp
    src/egolib/map_functions.c
    src/egolib/map_functions.h
    src/egolib/_math.c
    src/egolib/_math.h
    src/egolib/Ref.hpp
    src/egolib/strutil.c
    src/egolib/strutil.h
    src/egolib/TrigonometricTable.hpp
    src/egolib/typedef.c
    src/egolib/typedef.h
)

# -- game/Core ---------------------------------------------------------------
set(EGOLIB_GAME_CORE_SOURCES
    src/egolib/game/Core/ContentRuntimeBootstrap.cpp
    src/egolib/game/Core/ContentRuntimeBootstrap.hpp
    src/egolib/game/Core/EngineContext.cpp
    src/egolib/game/Core/EngineContext.hpp
    src/egolib/game/Core/GameEngine.cpp
    src/egolib/game/Core/GameEngine.hpp
    src/egolib/game/Core/GameSessionContext.cpp
    src/egolib/game/Core/GameSessionContext.hpp
)

# -- game/GameStates ---------------------------------------------------------
set(EGOLIB_GAME_GAMESTATES_SOURCES
    src/egolib/game/GameStates/AudioOptionsScreen.cpp
    src/egolib/game/GameStates/AudioOptionsScreen.hpp
    src/egolib/game/GameStates/DebugFontRenderingState.cpp
    src/egolib/game/GameStates/DebugFontRenderingState.hpp
    src/egolib/game/GameStates/DebugMainMenuState.cpp
    src/egolib/game/GameStates/DebugMainMenuState.hpp
    src/egolib/game/GameStates/DebugModuleLoadingState.cpp
    src/egolib/game/GameStates/DebugModuleLoadingState.hpp
    src/egolib/game/GameStates/DebugObjectLoadingState.cpp
    src/egolib/game/GameStates/DebugObjectLoadingState.hpp
    src/egolib/game/GameStates/DebugParticlesScreen.cpp
    src/egolib/game/GameStates/DebugParticlesScreen.hpp
    src/egolib/game/GameStates/GameState.cpp
    src/egolib/game/GameStates/GameState.hpp
    src/egolib/game/GameStates/InGameMenuState.cpp
    src/egolib/game/GameStates/InGameMenuState.hpp
    src/egolib/game/GameStates/InputOptionsScreen.cpp
    src/egolib/game/GameStates/InputOptionsScreen.hpp
    src/egolib/game/GameStates/LoadingState.cpp
    src/egolib/game/GameStates/LoadingState.hpp
    src/egolib/game/GameStates/LoadPlayerElement.cpp
    src/egolib/game/GameStates/LoadPlayerElement.hpp
    src/egolib/game/GameStates/MainMenuState.cpp
    src/egolib/game/GameStates/MainMenuState.hpp
    src/egolib/game/GameStates/MapEditorSelectModuleState.cpp
    src/egolib/game/GameStates/MapEditorSelectModuleState.hpp
    src/egolib/game/GameStates/MapEditorState.cpp
    src/egolib/game/GameStates/MapEditorState.hpp
    src/egolib/game/GameStates/OptionsScreen.cpp
    src/egolib/game/GameStates/OptionsScreen.hpp
    src/egolib/game/GameStates/PlayingState.cpp
    src/egolib/game/GameStates/PlayingState.hpp
    src/egolib/game/GameStates/SelectCharacterState.cpp
    src/egolib/game/GameStates/SelectCharacterState.hpp
    src/egolib/game/GameStates/SelectModuleState.cpp
    src/egolib/game/GameStates/SelectModuleState.hpp
    src/egolib/game/GameStates/SelectPlayersState.cpp
    src/egolib/game/GameStates/SelectPlayersState.hpp
    src/egolib/game/GameStates/VictoryScreen.cpp
    src/egolib/game/GameStates/VictoryScreen.hpp
    src/egolib/game/GameStates/VideoOptionsScreen.cpp
    src/egolib/game/GameStates/VideoOptionsScreen.hpp
)

# -- game/Graphics -----------------------------------------------------------
set(EGOLIB_GAME_GRAPHICS_SOURCES
    src/egolib/game/Graphics/Billboard.cpp
    src/egolib/game/Graphics/Billboard.hpp
    src/egolib/game/Graphics/BillboardSystem.cpp
    src/egolib/game/Graphics/BillboardSystem.hpp
    src/egolib/game/Graphics/Camera.cpp
    src/egolib/game/Graphics/Camera.hpp
    src/egolib/game/Graphics/CameraSystem.cpp
    src/egolib/game/Graphics/CameraSystem.hpp
    src/egolib/game/Graphics/DefaultMd2ModelRenderer.cpp
    src/egolib/game/Graphics/DefaultMd2ModelRenderer.hpp
    src/egolib/game/Graphics/EntityList.cpp
    src/egolib/game/Graphics/EntityList.hpp
    src/egolib/game/Graphics/Md2ModelRenderer.cpp
    src/egolib/game/Graphics/Md2ModelRenderer.hpp
    src/egolib/game/Graphics/ObjectGraphics.cpp
    src/egolib/game/Graphics/ObjectGraphics.hpp
    src/egolib/game/Graphics/ParticleGraphics.cpp
    src/egolib/game/Graphics/ParticleGraphics.hpp
    src/egolib/game/Graphics/RenderPass.cpp
    src/egolib/game/Graphics/RenderPass.hpp
    src/egolib/game/Graphics/RenderPasses.cpp
    src/egolib/game/Graphics/RenderPasses.hpp
    src/egolib/game/Graphics/RenderPasses/BackgroundRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/BackgroundRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/EntityReflectionsRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/EntityReflectionsRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/EntityShadowsRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/EntityShadowsRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/ForegroundRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/ForegroundRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/HeightmapRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/HeightmapRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/NonOpaqueEntitiesRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/NonOpaqueEntitiesRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/NonReflectiveTilesRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/NonReflectiveTilesRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/OpaqueEntitiesRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/OpaqueEntitiesRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/ReflectiveTilesFirstRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/ReflectiveTilesFirstRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/ReflectiveTilesSecondRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/ReflectiveTilesSecondRenderPass.hpp
    src/egolib/game/Graphics/RenderPasses/WaterTilesRenderPass.cpp
    src/egolib/game/Graphics/RenderPasses/WaterTilesRenderPass.hpp
    src/egolib/game/Graphics/TextureAtlasManager.cpp
    src/egolib/game/Graphics/TextureAtlasManager.hpp
    src/egolib/game/Graphics/TileList.cpp
    src/egolib/game/Graphics/TileList.hpp
    src/egolib/game/Graphics/Vertex.hpp
)

# -- game/GUI ----------------------------------------------------------------
set(EGOLIB_GAME_GUI_SOURCES
    src/egolib/game/GUI/Button.cpp
    src/egolib/game/GUI/Button.hpp
    src/egolib/game/GUI/CharacterStatus.cpp
    src/egolib/game/GUI/CharacterStatus.hpp
    src/egolib/game/GUI/CharacterWindow.cpp
    src/egolib/game/GUI/CharacterWindow.hpp
    src/egolib/game/GUI/Component.cpp
    src/egolib/game/GUI/Component.hpp
    src/egolib/game/GUI/Container.cpp
    src/egolib/game/GUI/Container.hpp
    src/egolib/game/GUI/DrawingContext.cpp
    src/egolib/game/GUI/DrawingContext.hpp
    src/egolib/game/GUI/IconButton.cpp
    src/egolib/game/GUI/IconButton.hpp
    src/egolib/game/GUI/Image.cpp
    src/egolib/game/GUI/Image.hpp
    src/egolib/game/GUI/InputListener.cpp
    src/egolib/game/GUI/InputListener.hpp
    src/egolib/game/GUI/InternalDebugWindow.cpp
    src/egolib/game/GUI/InternalDebugWindow.hpp
    src/egolib/game/GUI/InternalWindow.cpp
    src/egolib/game/GUI/InternalWindow.hpp
    src/egolib/game/GUI/InventorySlot.cpp
    src/egolib/game/GUI/InventorySlot.hpp
    src/egolib/game/GUI/JoinBounds.hpp
    src/egolib/game/GUI/Label.cpp
    src/egolib/game/GUI/Label.hpp
    src/egolib/game/GUI/Layout.cpp
    src/egolib/game/GUI/Layout.hpp
    src/egolib/game/GUI/LevelUpWindow.cpp
    src/egolib/game/GUI/LevelUpWindow.hpp
    src/egolib/game/GUI/Material.cpp
    src/egolib/game/GUI/Material.hpp
    src/egolib/game/GUI/MessageLog.cpp
    src/egolib/game/GUI/MessageLog.hpp
    src/egolib/game/GUI/MiniMap.cpp
    src/egolib/game/GUI/MiniMap.hpp
    src/egolib/game/GUI/ModuleSelector.cpp
    src/egolib/game/GUI/ModuleSelector.hpp
    src/egolib/game/GUI/OptionsButton.cpp
    src/egolib/game/GUI/OptionsButton.hpp
    src/egolib/game/GUI/Panel.cpp
    src/egolib/game/GUI/Panel.hpp
    src/egolib/game/GUI/ProgressBar.cpp
    src/egolib/game/GUI/ProgressBar.hpp
    src/egolib/game/GUI/ScrollableList.cpp
    src/egolib/game/GUI/ScrollableList.hpp
    src/egolib/game/GUI/Slider.cpp
    src/egolib/game/GUI/Slider.hpp
    src/egolib/game/GUI/TabPanel.cpp
    src/egolib/game/GUI/TabPanel.hpp
    src/egolib/game/GUI/UIManager.cpp
    src/egolib/game/GUI/UIManager.hpp
)

# -- game/Logic --------------------------------------------------------------
set(EGOLIB_GAME_LOGIC_SOURCES
    src/egolib/game/Logic/Player.cpp
    src/egolib/game/Logic/Player.hpp
    src/egolib/game/Logic/QuestLog.cpp
    src/egolib/game/Logic/QuestLog.hpp
)

# -- game/Module -------------------------------------------------------------
set(EGOLIB_GAME_MODULE_SOURCES
    src/egolib/game/Module/AnimatedTiles.cpp
    src/egolib/game/Module/AnimatedTiles.hpp
    src/egolib/game/Module/damagetile_instance.h
    src/egolib/game/Module/Fog.cpp
    src/egolib/game/Module/Fog.hpp
    src/egolib/game/Module/Module.cpp
    src/egolib/game/Module/Module.hpp
    src/egolib/game/Module/module_spawn.c
    src/egolib/game/Module/module_spawn.h
    src/egolib/game/Module/Passage.cpp
    src/egolib/game/Module/Passage.hpp
    src/egolib/game/Module/Water.cpp
    src/egolib/game/Module/Water.hpp
    src/egolib/game/Module/Weather.cpp
    src/egolib/game/Module/Weather.hpp
)

# -- game/Physics ------------------------------------------------------------
set(EGOLIB_GAME_PHYSICS_SOURCES
    src/egolib/game/Physics/Collidable.cpp
    src/egolib/game/Physics/Collidable.hpp
    src/egolib/game/Physics/CollisionSystem.cpp
    src/egolib/game/Physics/CollisionSystem.hpp
    src/egolib/game/Physics/ObjectPhysics.cpp
    src/egolib/game/Physics/ObjectPhysics.hpp
    src/egolib/game/Physics/particle_collision.c
    src/egolib/game/Physics/particle_collision.h
    src/egolib/game/Physics/ParticlePhysics.cpp
    src/egolib/game/Physics/ParticlePhysics.hpp
    src/egolib/game/Physics/PhysicalConstants.hpp
)

# -- game/ top-level files ---------------------------------------------------
set(EGOLIB_GAME_TOPLEVEL_SOURCES
    src/egolib/game/CharacterMatrix.c
    src/egolib/game/CharacterMatrix.h
    src/egolib/game/egoboo.c
    src/egolib/game/egoboo_doxygen.h
    src/egolib/game/egoboo.h
    src/egolib/game/game.c
    src/egolib/game/game.h
    src/egolib/game/graphic.c
    src/egolib/game/graphic_fan.c
    src/egolib/game/graphic_fan.h
    src/egolib/game/graphic.h
    src/egolib/game/graphic_mad.c
    src/egolib/game/graphic_mad.h
    src/egolib/game/graphic_prt.c
    src/egolib/game/graphic_prt.h
    src/egolib/game/Inventory.cpp
    src/egolib/game/Inventory.hpp
    src/egolib/game/lighting.c
    src/egolib/game/lighting.h
    src/egolib/game/link.c
    src/egolib/game/link.h
    src/egolib/game/mesh.c
    src/egolib/game/mesh.h
    src/egolib/game/physics.c
    src/egolib/game/physics.h
    src/egolib/game/renderer_3d.c
    src/egolib/game/renderer_3d.h
    src/egolib/game/script_compile.c
    src/egolib/game/script_compile.h
    src/egolib/game/script_functions.c
    src/egolib/game/script_functions.h
    src/egolib/game/script_implementation.c
    src/egolib/game/script_implementation.h
    src/egolib/game/script_variables.c
    src/egolib/game/script_variables.h
    src/egolib/game/Shop.cpp
    src/egolib/game/Shop.hpp
)

# ---------------------------------------------------------------------------
# Assemble the complete source list from all subsystems.
# ---------------------------------------------------------------------------
list(APPEND SOURCE_FILES
    ${EGOLIB_AI_SOURCES}
    ${EGOLIB_AUDIO_SOURCES}
    ${EGOLIB_CONFIGURATION_SOURCES}
    ${EGOLIB_CONSOLE_SOURCES}
    ${EGOLIB_CORE_SOURCES}
    ${EGOLIB_ENTITIES_SOURCES}
    ${EGOLIB_EXTENSIONS_SOURCES}
    ${EGOLIB_FILEFORMATS_SOURCES}
    ${EGOLIB_GRAPHICS_SOURCES}
    ${EGOLIB_GRID_SOURCES}
    ${EGOLIB_IMAGE_SOURCES}
    ${EGOLIB_INPUTCONTROL_SOURCES}
    ${EGOLIB_INTEGRATIONS_SOURCES}
    ${EGOLIB_LOG_SOURCES}
    ${EGOLIB_LOGIC_SOURCES}
    ${EGOLIB_MATH_SOURCES}
    ${EGOLIB_MESH_SOURCES}
    ${EGOLIB_PLATFORM_SOURCES}
    ${EGOLIB_PROFILES_SOURCES}
    ${EGOLIB_RENDERER_SOURCES}
    ${EGOLIB_SCRIPT_SOURCES}
    ${EGOLIB_TIME_SOURCES}
    ${EGOLIB_VFS_SOURCES}
    ${EGOLIB_TOPLEVEL_SOURCES}
    ${EGOLIB_GAME_CORE_SOURCES}
    ${EGOLIB_GAME_GAMESTATES_SOURCES}
    ${EGOLIB_GAME_GRAPHICS_SOURCES}
    ${EGOLIB_GAME_GUI_SOURCES}
    ${EGOLIB_GAME_LOGIC_SOURCES}
    ${EGOLIB_GAME_MODULE_SOURCES}
    ${EGOLIB_GAME_PHYSICS_SOURCES}
    ${EGOLIB_GAME_TOPLEVEL_SOURCES}
)

# Mark C files to be compiled as C++.
set(C_SOURCE_FILES "")
foreach(_src ${SOURCE_FILES})
    if(_src MATCHES "\\.[ch]$")
        list(APPEND C_SOURCE_FILES ${_src})
    endif()
endforeach()
SET_SOURCE_FILES_PROPERTIES(${C_SOURCE_FILES} PROPERTIES LANGUAGE CXX)

# Mark header files as header-only.
set(HEADER_FILES "")
foreach(_src ${SOURCE_FILES})
    if(_src MATCHES "\\.(h|hpp)$")
        list(APPEND HEADER_FILES ${_src})
    endif()
endforeach()
set_source_files_properties(${HEADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(${HEADER_FILES} PROPERTIES LANGUAGE CXX)

# Add OS specific files.
if (UNIX AND NOT APPLE)
  list(APPEND SOURCE_FILES src/egolib/Platform/file_linux.c)
  SET_SOURCE_FILES_PROPERTIES(src/egolib/Platform/file_linux.c PROPERTIES LANGUAGE CXX)
endif()

if (APPLE)
  file(GLOB_RECURSE MM_FILES ${PROJECT_SOURCE_DIR}/src/*.mm)
  list(APPEND SOURCE_FILES ${MM_FILES})
  SET_SOURCE_FILES_PROPERTIES(${MM_FILES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

if (WIN32)
  list(APPEND SOURCE_FILES src/egolib/Platform/file_win.c)
  SET_SOURCE_FILES_PROPERTIES(src/egolib/Platform/file_win.c PROPERTIES LANGUAGE CXX)
endif()

# Define product.
add_library(egolib-library STATIC ${SOURCE_FILES})
target_include_directories(egolib-library INTERFACE "${PROJECT_SOURCE_DIR}/src/")
target_link_libraries(egolib-library idlib-game-engine-library idlib-document-library idlib-math-geometry-library idlib-hll-library idlib-parsing-expression-library idlib-color-library idlib-filesystem-library)
if (WIN32)
  if (${IDLIB_CXX_COMPILER_ID} EQUAL ${IDLIB_CXX_COMPILER_ID_MSVC})
    target_link_libraries(egolib-library Shlwapi.lib)
  else()
    target_link_libraries(egolib-library shlwapi)
  endif()
endif()

# == Doxygen ==================================================================================== #

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    ADD_CUSTOM_TARGET(egolib-doc ${DOXYGEN_EXECUTABLE} COMMENT "Building documentation")
ELSE(DOXYGEN_FOUND)
    MESSAGE(STATUS "Doxygen not found. You won't be able to build documentation.")
ENDIF(DOXYGEN_FOUND)
