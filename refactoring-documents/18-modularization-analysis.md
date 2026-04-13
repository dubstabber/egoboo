# Modularization Analysis

This document analyzes the current modular structure of the Egoboo codebase, identifies logical subsystem boundaries, measures coupling between them, and proposes a target module decomposition.

## 1. Current Physical Structure

### Top-level build targets

| Target | Type | Role | Lines |
| --- | --- | --- | ---: |
| `idlib` | Static library (submodule) | Foundation utilities: math, color, filesystem, parsing, signals, types | ~33,000 |
| `idlib-game-engine` | Static library (submodule) | Game engine utilities: OpenGL (GLEW), PhysFS, googletest | ~5,000 |
| `egolib-library` | Static library | All runtime code: engine, gameplay, rendering, formats, scripting, GUI | ~83,000 |
| `egoboo` | Executable | Thin entry point (90 lines) | 90 |
| `cartman` | Not built | Map editor (disconnected from main build) | ~6,000 |
| `egoboo-content-validator` | Executable (tool) | Content validation tool | ~1,200 |

### The core problem

**`egolib-library` is one monolithic static library containing everything.** It has no internal module boundaries expressed in the build system. All 606 source files are globbed into a single compilation target via `GLOB_RECURSE`. There is no way to build, test, or reason about subsystems independently.

## 2. Logical Subsystem Map Inside egolib

Despite the flat build, the source tree does have directory-based logical groupings. Here they are ranked by size:

| Subsystem | Directory | Lines | Files | Concern |
| --- | --- | ---: | ---: | --- |
| **Game core** | `game/` (top-level `.c`/`.h`) | 25,259 | 26 | Game loop, session, rendering, physics, script |
| **Game Graphics** | `game/Graphics/` | 7,185 | 49 | Object/particle/tile rendering |
| **Game GUI** | `game/GUI/` | 5,625 | 53 | Menu screens, HUD, widgets |
| **Game States** | `game/GameStates/` | 5,197 | 42 | State machine: menus, playing, loading |
| **Game Physics** | `game/Physics/` | 5,064 | 11 | Collision, particle physics, movement |
| **Game Module** | `game/Module/` | 2,611 | 15 | Module loading, water, weather, passages |
| **Game Core (engine)** | `game/Core/` | 1,297 | 8 | GameEngine, bootstrap |
| **Entities** | `Entities/` | 8,212 | 13 | Object, Particle, Enchantment, ObjectHandler |
| **Graphics (engine)** | `Graphics/` | 5,835 | 45 | Font, MD2, textures, billboard, render utilities |
| **Script** | `Script/` | 5,721 | 44 | Script runtime, operators, interpreter |
| **Profiles** | `Profiles/` | 5,537 | 22 | ObjectProfile, ParticleProfile, EnchantProfile, ModuleProfile |
| **FileFormats** | `FileFormats/` | 5,135 | 39 | Spawn, map tile, wawalite, configfile parsers |
| **Renderer** | `Renderer/` | 4,089 | 28 | OpenGL abstraction, texture, blend state |
| **Image** | `Image/` | 1,998 | 13 | Image loading, SDL surface |
| **Logic** | `Logic/` | 1,649 | 17 | Perks, teams, damage types, gender |
| **Log** | `Log/` | 1,138 | 16 | Logging infrastructure |
| **Math** | `Math/` | 1,118 | 9 | Game-specific math helpers |
| **Time** | `Time/` | 1,028 | 7 | Clock, timer |
| **Core** | `Core/` | 942 | 5 | System init, singleton |
| **Audio** | `Audio/` | 892 | 2 | Audio system |
| **AI** | `AI/` | 878 | 7 | AI state machine |
| **InputControl** | `InputControl/` | 651 | 7 | Input devices |
| **Console** | `Console/` | 640 | 2 | Debug console |
| **Mesh** | `Mesh/` | 565 | 4 | Mesh info |
| **VFS** | `VFS/` | 409 | 5 | VFS abstraction (note: real VFS is `vfs.c` at 2,445 lines) |
| **Platform** | `Platform/` | 397 | 3 | Platform filesystem |
| **Configuration** | `Configuration/` | 384 | 5 | Config variables |
| **Grid** | `Grid/` | 288 | 3 | Grid helpers |
| **Extensions** | `Extensions/` | 257 | 2 | SDL extensions |
| **Network** | `Network/` | 0 | 1 | Placeholder only |
| **Lua** | `game/Lua/` | 0 | 0 | Abandoned |
| **Top-level loose files** | `egolib/` root | ~9,500 | 22 | vfs.c, fileutil.c, bbox.c, egoboo_setup.c, etc. |

## 3. Dependency Flow Analysis

### Ideal dependency direction (top to bottom)

```
egoboo (executable)
  └─ egolib (runtime library)
       ├─ Game States (state machine)
       │    ├─ Game GUI (menus, HUD)
       │    └─ Playing session
       │         ├─ Game Module (module lifecycle)
       │         ├─ Entities (Object, Particle)
       │         ├─ Game Physics (collision)
       │         ├─ Script (AI execution)
       │         └─ Game Graphics (rendering)
       ├─ Profiles (data definitions)
       ├─ FileFormats (parsers)
       ├─ Graphics engine (OpenGL, textures)
       ├─ Renderer (abstraction)
       ├─ Audio, Input, Image
       ├─ VFS, Platform, Log, Time
       └─ Core (system init)
  └─ idlib-game-engine
  └─ idlib
```

### Actual dependency violations

The actual dependency graph is nearly flat because of global state. Key violations:

1. **Entities → Game Module → Entities (circular)**: `Object.hpp` includes `Module/Module.hpp`, and `Module.cpp` creates and manages Objects.
2. **Script → Everything**: `script_functions.c` includes 23 headers spanning Entities, Profiles, Physics, Graphics, GUI, Module, and game state.
3. **GUI → Game internals**: GUI screens directly read `_currentModule`, `_gameEngine`, player state, and inventory.
4. **Profiles → Runtime singletons**: `ObjectProfile::loadDataFile()` accesses `PerkHandler` and `ImageManager` singletons during parsing.
5. **FileFormats → Runtime services**: Content parsing is entangled with VFS mount state.
6. **Graphics → Game state**: Rendering code directly accesses `_currentModule` for entity data.

### The "gravity well" files

These files pull everything together and are the hardest to isolate:

| File | Includes | Included by | Role |
| --- | ---: | --- | --- |
| `egolib.h` | 57 | Many C files | Uber-header |
| `game.h` | 8 | Most game code | Game-wide declarations + globals |
| `game/Core/GameEngine.hpp` | 3 | Entry point + states | God orchestrator |
| `Entities/Object.hpp` | 12 | Physics, script, graphics, module | Core entity |

## 4. Coupling Metrics

### Global access points

| Global | References | Subsystems touching it |
| --- | ---: | --- |
| `_currentModule` | 126 | Entities, Physics, Graphics, GUI, Script, Module, game loop |
| `_gameEngine` | 176 | States, GUI, Module, game loop, Audio, Input |
| Singleton `::get()` | 1,239 | Every subsystem |
| `update_wld` | 65 | Physics, Script, Entities, game loop |
| `extern` globals | 79 | Weather, fog, tiles, clocks, import list |

### Singleton abuse

1,239 singleton access calls across the codebase means an average of **2 singleton calls per source file**. Key singletons:

- `AudioSystem::get()`
- `ProfileSystem::get()`
- `ObjectHandler::get()` (via `_currentModule`)
- `PerkHandler::get()`
- `ImageManager::get()`
- Camera access through `_gameEngine`

## 5. Boundary Violations Classification

### Category A: Structural (can be fixed by refactoring headers and interfaces)

- `egolib.h` uber-header usage in C files
- `game.h` exporting 60+ free functions and globals
- Missing forward declarations causing unnecessary includes

### Category B: Ownership (requires runtime context extraction)

- `_currentModule` as a global entry point to the active session
- `_gameEngine` as a global entry point to the engine
- Module loading constructing everything in one monolithic path

### Category C: Architectural (requires subsystem redesign)

- Entity ↔ Module circular dependency
- Script dispatch knowing about every other subsystem
- Profile loading depending on runtime singletons
- GUI states directly manipulating game state

## 6. idlib Module Structure

`idlib` itself is well-modularized into 11 sub-libraries:

| Module | Lines | Purpose |
| --- | ---: | --- |
| `idlib-math` | 9,770 | Math library |
| `idlib-filesystem` | 4,914 | Filesystem abstraction |
| `idlib-color` | 3,713 | Color types |
| `idlib-numeric` | 3,086 | Numeric utilities |
| `idlib-math-geometry` | 2,729 | Geometry primitives |
| `idlib-parsing-expression` | 2,141 | PEG parser |
| `idlib-hll` | 2,138 | Higher-level language support |
| `idlib-type` | 2,032 | Type utilities |
| `idlib-signal` | 1,203 | Signal/slot system |
| `idlib-document` | 1,050 | Document parsing |
| `idlib-chrono` | 333 | Time utilities |

This is the correct modular pattern that `egolib` should eventually follow.

## 7. Namespace Consistency

The project uses the `Ego` namespace for newer C++ code, with sub-namespaces like:

- `Ego::Core`, `Ego::Graphics`, `Ego::GUI`, `Ego::Math`, `Ego::OpenGL`
- `Ego::Physics`, `Ego::Script`, `Ego::Configuration`, `Ego::Events`
- `Ego::SpawnFile`, `Ego::Internal`

However, many top-level types and functions live **outside any namespace** — particularly all C code and many C++ types in `game/`, `Entities/`, and `Profiles/`. This inconsistency makes symbol discovery and autocompletion harder.

## 8. Target Module Decomposition

The following is the recommended decomposition of `egolib` into internal logical modules. This does not require splitting into separate libraries immediately — it can start with explicit `CMakeLists.txt` source lists and interface boundaries.

### Module 1: `ego-platform` (foundation)

- `Core/`, `Platform/`, `VFS/`, `vfs.c`, `Log/`, `Time/`, `Configuration/`
- Dependencies: `idlib`, `idlib-game-engine`
- ~5,500 lines

### Module 2: `ego-io` (media and input)

- `Audio/`, `Image/`, `InputControl/`, `Extensions/`, `Console/`
- Dependencies: `ego-platform`
- ~4,400 lines

### Module 3: `ego-renderer` (graphics backend)

- `Renderer/`, `Graphics/` (engine-level, not game-level)
- Dependencies: `ego-platform`, `ego-io`
- ~9,900 lines

### Module 4: `ego-formats` (content parsing)

- `FileFormats/`, `fileutil.c`, `file_common.c`
- Dependencies: `ego-platform`
- ~6,500 lines

### Module 5: `ego-content` (profiles and data definitions)

- `Profiles/`, `Logic/` (perks, teams, damage types)
- Dependencies: `ego-platform`, `ego-formats`
- ~7,200 lines

### Module 6: `ego-entities` (runtime entity model)

- `Entities/`, `AI/`, `Script/`, `game/script_*.c`
- Dependencies: `ego-content`, `ego-platform`
- ~28,000 lines (largest — would benefit from further split later)

### Module 7: `ego-gameplay` (session and module runtime)

- `game/Module/`, `game/Physics/`, `game/game.c`, `game/Core/`, `game/Logic/`
- Dependencies: `ego-entities`, `ego-content`, `ego-renderer`
- ~14,000 lines

### Module 8: `ego-presentation` (GUI and game graphics)

- `game/GUI/`, `game/GameStates/`, `game/Graphics/`, `game/graphic*.c`
- Dependencies: `ego-gameplay`, `ego-renderer`
- ~20,000 lines

### Dependency graph

```
ego-presentation
  └─ ego-gameplay
       ├─ ego-entities
       │    ├─ ego-content
       │    │    ├─ ego-formats
       │    │    └─ ego-platform
       │    └─ ego-platform
       ├─ ego-content
       └─ ego-renderer
            ├─ ego-io
            └─ ego-platform
```

## 9. Migration Path

### Step 1: Explicit source lists

Replace `GLOB_RECURSE` in `egolib/library/CMakeLists.txt` with explicit per-directory source lists. This does not change the build output, but makes subsystem ownership visible.

### Step 2: Header dependency audit

Create a script or tool that maps which headers each subsystem directory includes from other directories. Use this to identify and reduce cross-boundary includes.

### Step 3: Interface headers

For each proposed module, create a single public interface header (e.g., `ego-platform.hpp`) and migrate external consumers to use only that interface.

### Step 4: Internal CMake targets

Create `OBJECT` library targets for each module within the same `egolib` build. This enforces dependency direction at compile time without changing the final link.

### Step 5: Test targets per module

Create test executables per module that only link the module under test plus its dependencies. This is where the real maintainability payoff happens.

## 10. Key Findings

1. **`egolib` is a monolith.** There is one build target containing 83,000+ lines with no internal boundaries.
2. **Coupling is pervasive.** Every subsystem can reach every other through globals and singletons.
3. **The dependency direction is mostly correct** in the directory structure but **violated at runtime** through global state.
4. **`idlib` is well-modularized** and serves as a good reference for how `egolib` should eventually look.
5. **The biggest blockers** to modularization are `_currentModule`, `_gameEngine`, and `egolib.h`.
6. **The script system** (`script_functions.c`) is the single worst coupling hotspot — it touches every other subsystem and is 8,183 lines.
7. **Circular dependencies** between Entities and Module must be broken before real module extraction.
