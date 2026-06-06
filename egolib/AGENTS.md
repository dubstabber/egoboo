# AGENTS.md

Additional instructions for work under `egolib/`.

## Refactor posture

- `egolib` is the main runtime library and the highest-risk code area in the repository.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, update `refactoring-documents/`.

## High-risk hotspots

Read the relevant audit docs before working in these areas:

- `library/src/egolib/game/script_functions_{systems,target,state,spawn,action,movement,bitwise}.c` (split from the former `script_functions.c`; `_systems.c` is the largest TU at ~3200 lines)
- `library/src/egolib/vfs.c` (~2460 lines)
- `library/src/egolib/game/Physics/particle_collision.c` (~1530 lines)
- `library/src/egolib/game/Graphics/ObjectGraphics.cpp` (~1490 lines)
- `library/src/egolib/game/mesh.c` (~1370 lines)
- `library/src/egolib/Script/script.c` (~1370 lines)
- `library/src/egolib/fileutil.c` (~1330 lines)
- `library/src/egolib/Entities/Object.hpp` (~1620 lines, monolithic interface)

Architecturally central but now small after split passes: `game/game.c` (~550), `Entities/Object.cpp` (~200), `game/Module/Module.cpp` (~200).

## Global-state constraints

- The three former mutable globals (`_gameEngine`, `_currentModule`, `update_wld`) are fully retired. Module access routes through `GameSessionContext`; engine access through `EngineContext`. The remaining coupling hotspot is ~912 singleton `::get()` call sites.
- Avoid introducing new hidden global dependencies.
- If you touch code that affects VFS setup, module loading, object profile loading, or script compilation, validate beyond compilation.

## Validation expectations

- Never use more than 4 parallel jobs for CMake builds on this machine.
- Prefer targeted CMake builds for touched targets.
- If your change can affect content loading, module loading, VFS behavior, object profiles, or scripts, run at least:
  - `./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod`
- For shared loading-path or VFS changes, run the full validator.

## Linux and portability

- Preserve current Linux/Fedora behavior unless the task explicitly revisits portability policy.
- If you touch filesystem, SDL, OpenGL, or PhysFS paths, check `doc/build-linux.md` and document any new operational assumptions.

## Safety

- Never use `backup-copy/` as a write target or patch source.
- Never manually edit `build/`.
