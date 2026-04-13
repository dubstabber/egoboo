# AGENTS.md

Additional instructions for work under `egolib/`.

## Refactor posture

- `egolib` is the main runtime library and the highest-risk code area in the repository.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, update `refactoring-documents/`.

## High-risk hotspots

Read the relevant audit docs before working in these areas:

- `library/src/egolib/game/script_functions.c`
- `library/src/egolib/game/game.c`
- `library/src/egolib/game/graphic.c`
- `library/src/egolib/vfs.c`
- `library/src/egolib/Entities/Object.cpp`
- `library/src/egolib/game/Module/Module.cpp`

These files are large, central, and coupled to legacy global state.

## Global-state constraints

- Be careful around `_gameEngine`, `_currentModule`, and `update_wld`.
- Avoid introducing new hidden global dependencies.
- If you touch code that affects VFS setup, module loading, object profile loading, or script compilation, validate beyond compilation.

## Validation expectations

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
