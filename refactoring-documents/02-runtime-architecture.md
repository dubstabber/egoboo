# Runtime Architecture

## 1. Boot path

The executable itself is not the architecture center.

Runtime startup is:

1. `egoboo/src/game/Main.cpp`
2. `Ego::Core::System::initialize(argv[0])`
3. `EngineContext::get().setEngine(std::make_unique<GameEngine>())` — the engine is installed in the `EngineContext` (the former `_gameEngine` global is retired, 0 references)
4. `engine().start()`, then `EngineContext::get().clearEngine()` on shutdown

This means the executable layer currently does almost nothing except install the engine into the `EngineContext` and hand off control.

## 2. System initialization

`egolib/library/src/egolib/Core/System.cpp` initializes:

- VFS and search paths
- logging
- configuration (`setup.txt`)
- SDL timer/events
- SDL video/audio/input services

Important architectural point:

- Filesystem and configuration setup happen before most gameplay systems exist.
- VFS mount points are part of core runtime identity, not just an IO detail.

## 3. GameEngine as central orchestrator

`GameEngine` in `egolib/library/src/egolib/game/Core/GameEngine.*` is the main runtime coordinator.

### Responsibilities currently held by `GameEngine`

- startup and shutdown sequencing
- fixed-rate update/render loop
- SDL event polling
- game state stack management
- preload UI rendering
- subsystem initialization order
- screenshot handling
- saved-character and module profile loading

### Why this matters

This is a classic "god orchestrator" shape:

- many systems are initialized here because they cannot initialize themselves safely
- shutdown order is manual and fragile
- newer systems coexist with legacy systems that still require imperative setup calls

The code itself says this explicitly in places:

- "crappy old systems do not pull their configuration"
- "TODO: REMOVE THIS"
- preload text rendering is described as a "small hacky function"

## 4. Main loop structure

The runtime loop is a fixed update / fixed render loop with frame skipping:

- target FPS: 60
- target UPS: 50
- max frameskip: 10

Flow per loop:

1. panic-button check (`Ctrl+Q`)
2. zero or more update frames
3. one render frame if due
4. sleep if idle
5. FPS/UPS estimation

This is reasonable in principle, but the implementation is tightly bound to global state and manual timing variables.

## 5. Game states

The game uses a stack of `GameState` instances:

- `MainMenuState`
- `PlayingState`
- options and selection states
- debug loading states
- victory and in-game menu states

The state stack itself is not the main problem. The problem is that states do not receive an isolated session object. They instead rely on globals and singleton-style services.

## 6. Global runtime state

The historical shape of the runtime was defined by three large mutable globals — `_currentModule`, `_gameEngine`, `update_wld` — reached by hundreds of call sites. Those boundaries have been dismantled. Current state (see `CODEBASE-HEALTH-STATUS.md` §4 for the authoritative numbers):

- `_currentModule` — 0 direct references in active runtime code; all consumers go through `GameSessionContext` / `GameModule` accessor surfaces.
- `_gameEngine` — 0 direct references in active runtime code; remaining mentions are in commented-out documentation.
- `update_wld` — 3 residue references in `Script/script.c`, `game/Graphics/ObjectGraphics.hpp`, and `Entities/Particle.hpp` as a legacy debug label, not active global coupling.

Secondary runtime globals that still exist:

- `clock_chr_stat`, `clock_enc_stat`
- `overrideslots`
- `g_importList`
- weather/fog/animated tile globals in `game.h`

### Remaining coupling risk

The raw-global boundary is gone, but coupling was migrated, not eliminated. Subsystems now reach into session/engine context singletons (`GameSessionContext::get()`, `EngineContext::get()`) rather than `_currentModule` directly, and the broader singleton count has fallen to ~863 `::get()` call sites (from ~1,150). A service-interface layer is partially in place — 15 services are now seamed through `EngineContext` (`IAudioSystem`, `ICameraSystem`, `IInputSystem`, `IPerkHandler`, `IImageManager`, `IFontManager`, `IGraphicsSystem`, `ITextureManager`, `IParticleHandler`, `IProfileSystem`, `IGFX`, `IBillboardSystem`, `ITextureAtlasManager`, plus config and logging) — but the context wrappers remain the dominant DIP boundary until the remaining `::get()` call sites migrate onto them (roadmap T1.3).

## 7. Module runtime

`GameModule` in `egolib/library/src/egolib/game/Module/Module.cpp` is the live gameplay session model for one loaded module.

### What `GameModule` currently owns

- module profile metadata
- object handler and player list
- team list
- water and damage tile state
- passages
- mesh
- tile and water textures
- import/export validity
- random seed

### Load sequence

Module creation performs all of this in one constructor path:

1. reconfigure module VFS mount points
2. seed randomness
3. initialize teams
4. load textures
5. load global sounds and particles
6. load `wawalite.txt`
7. load object profiles
8. load mesh
9. load passages
10. load alliances
11. spawn objects from `spawn.txt`
12. compile/load profile AI scripts

This is a very large amount of behavior for one constructor-driven lifecycle.

## 8. Data and gameplay are still entangled inside runtime code

The runtime still expects content conventions directly:

- slot numbers from `data.txt`
- object directories with hardcoded filenames
- spawn entries that rely on legacy slot semantics
- module VFS overlays
- script loading from `script.txt`
- implicit asset enumeration such as `sound0..29`, `part0..29`

This means "content format refactor" is also "runtime architecture refactor".

## 9. Mixed old and new subsystems

`egolib` contains both older C-style systems and newer C++-style systems:

- old-style C files such as `game.c`, `mesh.c`, and the seven `script_functions_*.c` files (split out of the former 8,183-line `script_functions.c`)
- newer C++ areas such as `GameModule`, GUI classes, render passes, players, camera system, and parts of profiles

The result is not merely mixed language style. It is mixed ownership style:

- some code uses classes and RAII
- some code still depends on procedural sequencing and cross-subsystem reach
- newer code still reaches for concrete singletons rather than role interfaces

## 10. Major subsystem map

### Core/runtime services

- `Core/`
- `Configuration/`
- `Log/`
- `VFS/`
- platform-specific filesystem code

### Gameplay runtime

- `game/`
- `Entities/`
- `Profiles/`
- `Logic/`
- `Inventory`
- `Shop`

### Presentation/runtime IO

- `Graphics/`
- `Renderer/`
- `Image/`
- `Audio/`
- `InputControl/`
- `game/GUI/`

### File and content formats

- `FileFormats/`
- `Script/`

## 11. Architectural pain points worth fixing first

### Pain point 1: singleton-mediated dependency graph

Raw-global reach into `_currentModule` / `_gameEngine` is gone, but ~863 `::get()` call sites still flatten the effective dependency graph. Most "dependencies" in `egolib` are implicit access to concrete singletons, not declared constructor parameters.

### Pain point 2: initialization order as architecture

The order in `GameEngine::initialize()` is effectively the architecture spec. If the order is wrong, the game breaks.

### Pain point 3: engine/game split is incomplete

`egolib` contains both platform/runtime services and game-specific behavior. The project already wants "separating technology from game logic", but that line is still blurred.

### Pain point 4: gameplay logic is spread across multiple styles

Gameplay is not only in `game/`. It is spread across:

- `game/`
- `Entities/`
- `Profiles/`
- `FileFormats/`
- `Script/`
- GUI states that directly manipulate runtime state

### Pain point 5: content-driven semantics are not formalized

Many rules live partly in file formats, partly in runtime code, and partly in old docs.

## 12. Target architecture direction

The next architecture should aim for these explicit boundaries:

### Boundary A: platform/runtime services

- logging
- filesystem abstraction
- window/audio/input bootstrap
- renderer backend

### Boundary B: content repositories

- module metadata
- object definitions
- particle definitions
- script assets
- mesh/environment data

### Boundary C: game session runtime

- active module instance
- players
- objects
- team state
- quest/runtime state

### Boundary D: presentation

- menus
- HUD
- camera
- render passes

### Boundary E: script or rules execution

- current EgoScript compatibility layer
- future scripting engine adapter

The first architectural move — replacing raw global access with explicit `GameSessionContext` / `EngineContext` wrappers — has been executed (see passes 11–51 in `71-completed-passes-log.md`). The next frontier is to replace those context singletons with constructor-injected service interfaces so subsystems receive their dependencies instead of reaching for a concrete wrapper.
