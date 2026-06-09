---
name: foundation-closure-map-file
description: Dependency closure analysis of {map_functions, Log, FileFormats/map_file, Mesh/Info, Math} — confirms set is game/-layer-free and can form egolib-foundation link target
metadata:
  type: project
---

# Foundation Closure: map_functions + Log + FileFormats/map_file + Mesh/Info + Math

**Fact:** The set {map_functions, Log, FileFormats/map_file*, Mesh/Info, Math} is dependency-closed below the game/ layer. Zero upward edges to game/ exist in any header or implementation file within this set.

**Why:** This was verified by tracing the complete transitive include graph of each subsystem. Key path: `map_file.h` → `vfs.h` → `VFS/{FsPath,VfsPath,internal}.hpp` → `idlib` / `platform.h` only. No game/ headers appear anywhere.

**How to apply:** This set can be carved into an `egolib-foundation` static library without introducing circular dependencies. The physics nucleus (MeshLookupTables, ObjectPhysics, CollisionSystem, particle_collision) already links against these via `twist_to_normal`/`vec_to_facing` and can join the same target.

## Header-layer includes (all clean)

| File | Includes | Status |
|------|----------|--------|
| `map_functions.h` | `Math/_Include.hpp` | clean |
| `map_file.h` | `typedef.h`, `vfs.h`, `_math.h`, `Math/_Include.hpp`, `map_tile_dictionary.h`, `Mesh/Info.hpp` | clean |
| `map_tile_dictionary.h` | `typedef.h`, `FileFormats/map_fx.hpp` | clean |
| `Mesh/Info.hpp` | `Grid/Rect.hpp` → `Grid/Index.hpp` → `typedef.h` | clean |
| `Math/_Include.hpp` | `idlib/math.hpp`, `Log/_Include.hpp`, `integrations/math.hpp` | clean |
| `Log/_Include.hpp` | `Log/Entry.hpp`, `Log/Target.hpp`, `Log/Level.hpp` → `platform.h` | clean |
| `vfs.h` | `egolib_config.h`, `VFS/FsPath.hpp`, `VFS/VfsPath.hpp`, `integrations/filesystem.hpp` → `idlib` | clean |

## TU-level nuance: map_file.c includes fileutil.h gratuitously

`/egolib/library/src/egolib/FileFormats/map_file.c:38` includes `egolib/fileutil.h`, which transitively brings in `Profiles/_Include.hpp` and `Renderer/Renderer.hpp`. However:
1. `map_file.c` calls no symbols from `fileutil.h` (verified by grep) — it only uses `vfs.h` symbols already included via `map_file.h`.
2. `Profiles/_Include.hpp` public headers do NOT include any `game/` headers (game/ refs are in `.cpp` TUs only: `ObjectProfile_internal.h` in private header; `ProfileSystem.cpp`).
3. `Renderer/Renderer.hpp` is also clean.

**This is a gratuitous include — cleanup candidate — but NOT a hard-blocker for the foundation library.**

## Confirmed clean subsystems

- **Log**: all headers only pull `platform.h` and standard library. TU implementations are also clean.
- **Math**: only `idlib/math.hpp`, `Log/_Include.hpp`, `integrations/math.hpp` (→ idlib). TU implementations clean.
- **Mesh/Info**: only `Grid/Rect.hpp` → `Grid/Index.hpp` → `typedef.h`. TU uses `FileFormats/map_fx.hpp` (no includes).
- **Grid**: `Grid/Index.hpp` → `typedef.h` only.
- **VFS**: no game/ anywhere.
- **FileFormats/map_file headers**: no game/ anywhere.
- **FileFormats/map_file-v{1,2,3,4}.c**: only `Log/_Include.hpp`, `strutil.h`, `_math.h`, `Math/_Include.hpp`.

## Profiles game/ boundary (reference)

Profiles game/ references are confined to:
- `Profiles/ObjectProfile_internal.h:28` — `#include "egolib/game/Core/GameEngine.hpp"` (private TU header only)
- `Profiles/ProfileSystem.cpp:29-33` — game/GameStates, game/Core/GameSessionContext, game/game.h, game/script_compile.h

These are NOT reachable through `Profiles/_Include.hpp` → public headers.
