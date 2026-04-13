# Refactoring Documents

This directory is the baseline architecture and refactoring audit for the current Egoboo workspace as inspected on 2026-04-12.

The goal of this first pass is not to redesign the project in the abstract. It is to document what is actually running today, what is clearly stale, where the major coupling points are, and how to approach a long refactor without breaking the game beyond recovery.

## Scope of this audit

- Active runtime code: `egoboo/`, `egolib/`, `idlib/`, `idlib-game-engine/`
- Content pipeline: `data/`
- Build and tooling: root `CMakeLists.txt`, `README*`, `utilities/`, `run-egoboo.sh`
- Local workspace drift that already affects build/run behavior

Out of scope for the active architecture model:

- `backup-copy/` - local archive copy, currently 2.2G and untracked
- `build/` - generated build output, currently 1.7G and untracked
- Vendored third-party code under `external/` except where it affects build portability

## Fast findings

- The executable is tiny. Almost all runtime behavior lives inside `egolib`, which currently mixes engine services, gameplay logic, rendering, file formats, GUI, and legacy C code in one static library.
- The runtime depends heavily on global singletons and global mutable state, especially `_gameEngine`, `_currentModule`, and `update_wld`.
- The virtual file system is not a thin wrapper. It actively rewrites where content comes from by mounting module and global directories onto logical paths like `mp_data`, `mp_modules`, and `mp_objects`.
- The content model is directory-shaped and convention-driven: `menu.txt`, `spawn.txt`, `data.txt`, `script.txt`, `message.txt`, `partN.txt`, `enchant.txt`, `level.mpd`, `tris.md2`, plus bitmap and audio assets.
- There are several abandoned or partial modernization attempts already in the tree: `doc/ego2xml/`, `utilities/migrator/`, and `egolib/library/src/egolib/game/Lua/`.
- Build documentation is inconsistent with the real build. Root docs describe CMake, `README.Linux` still describes `make all`, and the local workspace now relies on `run-egoboo.sh` plus Fedora-specific source edits.
- Automated tests exist mainly for utility code. They do not currently protect gameplay, module loading, scripting, or content compatibility.

## Snapshot metrics

| Metric | Value |
| --- | ---: |
| Runtime source files in `egolib`/`egoboo`/`cartman` (`*.c`, `*.cpp`, `*.h`, `*.hpp`) | 591 |
| C files | 56 |
| C++ implementation files | 210 |
| Header files (`.h`) | 64 |
| Header files (`.hpp`) | 261 |
| Largest translation unit | `egolib/library/src/egolib/game/script_functions.c` (8153 lines) |
| `_currentModule` references | 592 |
| `_gameEngine` references | 266 |
| `update_wld` references | 65 |
| `TODO`/`FIXME`/`HACK` style matches in active code | 73 |
| Modules under `data/modules` | 42 |
| Object directories under `data/` | 968 |
| `data.txt` files | 946 |
| `script.txt` files | 951 |
| `enchant.txt` files | 206 |
| `level.mpd` files | 43 |
| `tris.md2` files | 953 |

## Documents in this folder

- `01-repository-and-build-audit.md`
- `02-runtime-architecture.md`
- `03-data-and-content-audit.md`
- `04-refactoring-strategy.md`
- `05-playtesting-and-bug-hunt-plan.md`
- `06-validator-baseline.md`
- `07-historical-docs-audit.md`
- `08-spawn-format-spec.md`
- `09-data-format-spec.md`
- `10-spawn-reconciliation-pass.md`
- `11-runtime-context-extraction-pass.md`
- `12-ui-game-state-session-access-pass.md`
- `13-gameplay-runtime-shell-context-pass.md`
- `14-graphics-runtime-shell-context-pass.md`
- `15-entity-physics-runtime-context-pass.md`
- `16-scripting-runtime-shell-context-pass.md`

## Recommended reading order

1. Read `01-repository-and-build-audit.md` to understand what parts of the repo are active and what local drift already exists.
2. Read `02-runtime-architecture.md` to see how the executable, engine, module runtime, and game state flow are currently wired together.
3. Read `03-data-and-content-audit.md` before touching any content format or scripting change.
4. Read `06-validator-baseline.md` before assuming the current content set is internally consistent.
5. Read `07-historical-docs-audit.md` before planning content-format or scripting migrations.
6. Read `08-spawn-format-spec.md` before changing spawn-name resolution, dependency handling, or validator alias reporting.
7. Read `09-data-format-spec.md` before changing object profile parsing, slot semantics, or save/import behavior.
8. Read `04-refactoring-strategy.md` before starting code motion.
9. Read `11-runtime-context-extraction-pass.md`, `12-ui-game-state-session-access-pass.md`, `13-gameplay-runtime-shell-context-pass.md`, `14-graphics-runtime-shell-context-pass.md`, `15-entity-physics-runtime-context-pass.md`, and `16-scripting-runtime-shell-context-pass.md` before continuing session/global-access cleanup work.
10. Use `05-playtesting-and-bug-hunt-plan.md` to turn future changes into repeatable validation work.

## Immediate recommendation

Do not start by rewriting systems wholesale. The first safe wins are:

1. Freeze and document the current build/run path for Linux.
2. Establish source-of-truth boundaries for active code versus archive/generated/vendor content.
3. Add non-UI validation tooling for content loading and module parsing.
4. Introduce a runtime context abstraction around `_currentModule` and `_gameEngine` before attempting deeper subsystem extraction.

Items 1 and 3 now have a first implementation in this workspace:

- canonical Linux doc: `doc/build-linux.md`
- content validation baseline: `06-validator-baseline.md`
- historical legacy-doc audit: `07-historical-docs-audit.md`
- spawn format compatibility spec: `08-spawn-format-spec.md`
- data format compatibility spec: `09-data-format-spec.md`
