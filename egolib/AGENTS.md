# AGENTS.md

Additional instructions for work under `egolib/`.

## Refactor posture

- `egolib` is the main runtime library and the highest-risk code area in the repository.
- Prefer seam creation, file-splitting, and dependency reduction over speculative rewrites.
- Preserve observable behavior unless the task explicitly calls for behavior change.
- If an edit changes runtime ownership, loading flow, or subsystem boundaries, update `refactoring-documents/`.

## High-risk hotspots

The live source-size, link-layout, and global-state figures live in root `AGENTS.md` and
`refactoring-documents/CODEBASE-HEALTH-STATUS.md`. This file deliberately keeps no parallel copy of those
numbers. Production runtime files are currently below 1,000 lines, so treat interface breadth and coupling
as the risk, not raw line count. Before working in script dispatch (`game/script_functions_*.c`),
`Entities/Object.hpp`, physics collision, VFS, object profiles, model loading, or game graphics, read those
docs first.

## Global-state constraints

- The three former mutable globals (`_gameEngine`, `_currentModule`, `update_wld`) are fully retired. Module access routes through `GameSessionContext`; engine access through `EngineContext`. (Current `::get()` figures: see root `AGENTS.md` "Global State".)
- Avoid introducing new hidden global dependencies.
- If you touch code that affects VFS setup, module loading, object profile loading, or script compilation, validate beyond compilation.

## Validation expectations

- The build is parallel-safe on this machine (i7-13700HX, 24 threads) — use `-j20`.
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
