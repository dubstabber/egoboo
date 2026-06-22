# Codebase Health Status

Current-state health snapshot for the Egoboo workspace. This document is the
canonical place for volatile size, archive, and test-count numbers; other
Markdown files should link here instead of carrying duplicate copies.

Snapshot date: 2026-06-22. Measurements below were taken from the live tree and
the existing `build/products/x64/lib/libegolib-*.a` archives.

## Executive Summary

Egoboo remains a mixed C/C++ runtime in active modernization. The largest wins
since the April 2026 baseline are still intact:

- The former mutable runtime globals `_gameEngine`, `_currentModule`, and
  `update_wld` are retired from active code. Only four `update_wld` text/comment
  artifacts remain.
- `egolib` is split into nine static archives with an acyclic intended
  dependency direction.
- Former runtime monoliths have been split aggressively. There are now no
  production runtime files over 1,000 lines under `egolib/library/src` or
  `egoboo/src`.
- The test suite is substantially larger than the April baseline and currently
  configures 897 ctest cases.
- The content validator has a stable known legacy-content baseline: 42 modules,
  10 warnings, 245 errors.

The main remaining debt is not raw file size anymore. It is interface and
ownership coupling: `Object` is still broad by interface, session and engine
access still route through context singletons, script dispatch remains
procedural, and the Windows runtime path remains unstable under Wine.

## Key Metrics

| Metric | Current value | Notes |
| --- | ---: | --- |
| `egolib` archives | 9 | `foundation-base`, `physics`, `renderer`, `gui`, `library`, `game-graphics`, `hud-widgets`, `scriptvm`, `gamestates` |
| Archive members | 162 / 6 / 28 / 24 / 76 / 21 / 6 / 33 / 19 | In the archive order above, measured with `ar t` |
| Runtime source files | 767 | `egolib/library/src` + `egoboo/src`; 103 `.c`, 274 `.cpp`, 73 `.h`, 317 `.hpp` |
| Runtime source lines | 128,227 | Same scope as above |
| Test files / lines | 50 / 23,648 | `egolib/tests`, source/header files only |
| ctest cases | 897 | `ctest --test-dir build -N` |
| ctest baseline | 897 / 897 | Last recorded green baseline in the pass log; use `ctest -j20 --output-on-failure` |
| `::get()` call sites | 623 | `rg "::get\\(" egolib/library/src`; includes intentional context seams |
| `EngineContext::get()` | 435 | Dominant intentional engine seam |
| `GameSessionContext::get()` | 133 | Dominant intentional session seam |
| `TODO`/`FIXME`/`HACK` markers | 59 | `egolib/library/src` + `egoboo/src` |
| `throw` references | 605 | Broad grep count, not semantic classification |
| Object role interfaces | 18 | 20 `Entities/I*.hpp` files total, including 2 service interfaces |

## Link Layout

The live archive member counts are:

| Archive | Members | Role |
| --- | ---: | --- |
| `egolib-foundation-base` | 162 | Dependency-closed base: math, logging, VFS, file formats, profiles data/model loading, script compiler pieces, low-level services |
| `egolib-physics` | 6 | Collision nucleus and physics primitives |
| `egolib-renderer` | 28 | SDL windowing and OpenGL renderer backend |
| `egolib-gui` | 24 | Generic GUI toolkit and abstract `GameState` base |
| `egolib-library` | 76 | Core gameplay remainder: entities, session/module runtime, lower service holders, game physics, object graphics |
| `egolib-game-graphics` | 21 | 3D scene-rendering layer: camera, billboard, texture atlas, render passes, GFX bootstrap |
| `egolib-hud-widgets` | 6 | Game-coupled in-game HUD widgets |
| `egolib-scriptvm` | 33 | EgoScript VM and `script_functions_*` dispatch family |
| `egolib-gamestates` | 19 | Concrete game state screens |

Intended direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

When touching `egolib/library/CMakeLists.txt` or moving sources between these
archives, re-run the live-archive `nm` back-edge check. Measure the `.a` files,
not `CMakeFiles/*.dir`, because object directories can retain stale `.o` files
from earlier carves.

## File Size Status

Production runtime source no longer has a >1,000-line file. Current largest
runtime files:

| File | Lines |
| --- | ---: |
| `egolib/library/src/egolib/Entities/Object.hpp` | 971 |
| `egolib/library/src/egolib/Profiles/ObjectProfile.hpp` | 808 |
| `egolib/library/src/egolib/FileFormats/wawalite_file.h` | 736 |
| `egolib/library/src/egolib/Script/script.h` | 685 |
| `egolib/library/src/egolib/Audio/AudioSystem.cpp` | 675 |
| `egolib/library/src/egolib/map_functions.c` | 668 |
| `egolib/library/src/egolib/bbox.c` | 660 |
| `egolib/library/src/egolib/vfs.c` | 658 |
| `egolib/library/src/egolib/Renderer/OpenGL/Renderer.cpp` | 658 |
| `egolib/library/src/egolib/game/Physics/CollisionSystem.cpp` | 646 |

Large non-runtime files still exist and are intentional test/tool hotspots:

| File | Lines |
| --- | ---: |
| `egolib/tests/egolib/tests/ScriptSystemsFunctions.cpp` | 3,211 |
| `egolib/tests/egolib/tests/ObjectAccessors.cpp` | 2,846 |
| `egolib/tests/egolib/tests/ScriptStateFunctions.cpp` | 1,915 |
| `tools/egoboo-content-validator.cpp` | 1,550 |
| `egolib/tests/egolib/tests/ScriptTargetFunctions.cpp` | 1,256 |
| `egolib/tests/egolib/tests/ScriptActionFunctions.cpp` | 1,231 |
| `egolib/tests/egolib/tests/ContentParsers.cpp` | 1,230 |

Current file-size priority: do not chase production file size mechanically.
Further value is in reducing interface breadth and cross-archive coupling.

## Global State And Singleton Coupling

Former globals:

| Global | Current state |
| --- | --- |
| `_gameEngine` | 0 active references; engine access routes through `EngineContext` |
| `_currentModule` | 0 active references; module access routes through `GameSessionContext` and `GameModule` surfaces |
| `update_wld` | variable removed; four text/comment/debug-label artifacts remain |

The remaining coupling hotspot is service-locator access. `EngineContext::get()`
and `GameSessionContext::get()` are intentional seams for now, but they still
flatten dependency visibility. Broader constructor injection does not exist.

Actionable direct singleton calls should remain small and justified. When adding
or moving code, prefer existing service interfaces and `active*()` seams over
new hidden global access.

## Testing And Validation

Recommended default verification:

```bash
cmake --build build -j20
ctest --test-dir build -j20 --output-on-failure
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

The test runner is parallel-safe in the current harness. Each test process gets
its own `EGOBOO_USER_DIR` through per-PID isolation.

Full validator baseline, last rechecked 2026-06-22:

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Warnings | 10 |
| Errors | 245 |

The full validator exits nonzero because the shipped legacy content has
pre-existing integrity errors. Treat parser crashes, new error categories, or
baseline count changes as suspicious; do not treat the current 245 legacy
content errors as a new regression by themselves.

## Build And Platform Status

| Target | Build status | Runtime status | Canonical doc |
| --- | --- | --- | --- |
| Linux native | Works | Primary development path | `doc/build-linux.md` |
| Linux-hosted Windows cross-build | Works | Wine path remains a debugging/compatibility path | `doc/build-windows.md` |
| Native Windows open-source toolchain | Future target | Not first-class yet | not documented |
| MSVC / Visual Studio | Legacy only | Deprecated | `doc/legacy/` |
| macOS | Not maintained | Deprecated | `doc/legacy/` |

The Wine helper still uses compatibility defaults for mipmaps and audio. Do not
treat the Wine path as a completed Windows runtime port.

## Current Priorities

1. Keep the nine-archive DAG acyclic when moving sources.
2. Reduce interface and ownership coupling, especially around `Object`,
   `GameSessionContext`, and `EngineContext`.
3. Keep script dispatch splits and parser/model-loader work behavior-preserving
   and covered by focused tests.
4. Stabilize the open-source Windows path without reintroducing Visual
   Studio-only requirements.
5. Maintain docs by updating this file first for volatile numbers, then linking
   to it from roadmap/audit documents.
