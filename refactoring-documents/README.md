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
- Windows build direction is still inconsistent. A MinGW-based path exists, but the maintained project direction should be native Windows compilation with fully open-source tooling rather than Visual Studio-specific workflows or Wine-only validation.
- The Linux-hosted Windows path is not healthy yet. `debug-output.txt` shows a current Wine-run startup failure involving font atlas initialization and a later crash during audio loading.
- The current codebase still has many bugs, incomplete features, and portability warnings. The refactor goal is not just cleaner structure; it is a more usable and maintainable game.
- Automated tests exist mainly for utility code. They do not currently protect gameplay, module loading, scripting, or content compatibility.

## Snapshot metrics

| Metric                                                                               |                                                            Value |
| ------------------------------------------------------------------------------------ | ---------------------------------------------------------------: |
| Runtime source files in `egolib`/`egoboo`/`cartman` (`*.c`, `*.cpp`, `*.h`, `*.hpp`) |                                                              591 |
| C files                                                                              |                                                               56 |
| C++ implementation files                                                             |                                                              210 |
| Header files (`.h`)                                                                  |                                                               64 |
| Header files (`.hpp`)                                                                |                                                              261 |
| Largest translation unit                                                             | `egolib/library/src/egolib/game/script_functions.c` (8153 lines) |
| `_currentModule` references                                                          |                                                              592 |
| `_gameEngine` references                                                             |                                                              266 |
| `update_wld` references                                                              |                                                               65 |
| `TODO`/`FIXME`/`HACK` style matches in active code                                   |                                                               73 |
| Modules under `data/modules`                                                         |                                                               42 |
| Object directories under `data/`                                                     |                                                              968 |
| `data.txt` files                                                                     |                                                              946 |
| `script.txt` files                                                                   |                                                              951 |
| `enchant.txt` files                                                                  |                                                              206 |
| `level.mpd` files                                                                    |                                                               43 |
| `tris.md2` files                                                                     |                                                              953 |

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
- `20-inventory-and-commerce-runtime-context-pass.md`
- `21-presentation-engine-context-pass.md`
- `17-codebase-health-assessment.md`
- `18-modularization-analysis.md`
- `19-new-refactoring-plan.md`
- `22-module-runtime-ownership-plan.md`
- `23-session-state-ownership-pass.md`
- `24-spawn-reconciliation-remediation-pass.md`
- `25-entity-layer-decomposition-plan.md`
- `26-audio-session-leaf-cleanup-pass.md`
- `27-environment-state-ownership-pass.md`
- `28-module-translation-unit-split-pass.md`
- `29-module-boundary-coverage-pass.md`
- `30-module-spawn-planning-pass.md`
- `31-module-spawn-realization-pass.md`
- `32-project-health-and-solid-assessment.md`
- `33-maintainability-improvement-plan.md`
- `34-module-player-binding-policy-pass.md`
- `35-module-startup-equipment-hook-pass.md`
- `36-module-player-startup-boundary-pass.md`
- `37-module-player-quest-hydration-pass.md`
- `38-module-local-player-bookkeeping-pass.md`
- `39-session-local-player-count-access-pass.md`
- `40-module-spawn-local-player-count-access-pass.md`
- `41-game-loop-local-player-status-pass.md`

## Recommended reading order

1. Read `01-repository-and-build-audit.md` to understand what parts of the repo are active and what local drift already exists.
2. Read `02-runtime-architecture.md` to see how the executable, engine, module runtime, and game state flow are currently wired together.
3. Read `03-data-and-content-audit.md` before touching any content format or scripting change.
4. Read `06-validator-baseline.md` before assuming the current content set is internally consistent.
5. Read `07-historical-docs-audit.md` before planning content-format or scripting migrations.
6. Read `08-spawn-format-spec.md` before changing spawn-name resolution, dependency handling, or validator alias reporting.
7. Read `09-data-format-spec.md` before changing object profile parsing, slot semantics, or save/import behavior.
8. Read `04-refactoring-strategy.md` before starting code motion.
9. Read `11-runtime-context-extraction-pass.md`, `12-ui-game-state-session-access-pass.md`, `13-gameplay-runtime-shell-context-pass.md`, `14-graphics-runtime-shell-context-pass.md`, `15-entity-physics-runtime-context-pass.md`, `16-scripting-runtime-shell-context-pass.md`, `20-inventory-and-commerce-runtime-context-pass.md`, `21-presentation-engine-context-pass.md`, `22-module-runtime-ownership-plan.md`, `23-session-state-ownership-pass.md`, and `24-spawn-reconciliation-remediation-pass.md` before continuing runtime-ownership or content-reconciliation work.
10. Use `05-playtesting-and-bug-hunt-plan.md` to turn future changes into repeatable validation work.
11. Read `17-codebase-health-assessment.md` for a quantitative quality snapshot with metrics and a scorecard.
12. Read `18-modularization-analysis.md` to understand current module boundaries, coupling, and a target decomposition.
13. Read `19-new-refactoring-plan.md` for the prioritized, actionable refactoring roadmap.
14. Read `22-module-runtime-ownership-plan.md`, `23-session-state-ownership-pass.md`, and `24-spawn-reconciliation-remediation-pass.md` for the latest completed runtime-ownership work and the current spawn-reconciliation remediation state.
15. Read `25-entity-layer-decomposition-plan.md` for the completed entity/profile decomposition pass covering Object.cpp, ObjectProfile.cpp, and Particle.cpp.
16. Read `26-audio-session-leaf-cleanup-pass.md` for the small deferred audio/session leaf cleanup that followed the session-state pass.
17. Read `27-environment-state-ownership-pass.md` for the completed weather/fog/animated-tile ownership cleanup and the next recommendation after that module-runtime seam.
18. Read `28-module-translation-unit-split-pass.md` for the completed `GameModule` file split and the next recommendation after the module-runtime decomposition.
19. Read `29-module-boundary-coverage-pass.md` for the added smoke coverage around the split module bootstrap/loading seams and the next recommended extraction target.
20. Read `30-module-spawn-planning-pass.md` for the extracted `spawn.txt` planning seam and the next follow-on recommendation around live spawn realization.
21. Read `31-module-spawn-realization-pass.md` for the extracted live spawn helper, its characterization coverage, and the remaining player-binding follow-on seam.
22. Read `32-project-health-and-solid-assessment.md` for the comprehensive design-quality assessment covering SOLID principles, design patterns, code cleanliness, and C++ modernization state.
23. Read `33-maintainability-improvement-plan.md` for the tiered improvement plan that maps design-quality findings into actionable work items with dependency ordering.
24. Read `34-module-player-binding-policy-pass.md` for the narrowed spawn player-binding policy seam and the next recommendation around startup-equipment side effects.
25. Read `35-module-startup-equipment-hook-pass.md` for the isolated startup-equipment identification hook and the remaining player-startup side-effect follow-on.
26. Read `36-module-player-startup-boundary-pass.md` for the extracted module-side player-startup helper boundary and the remaining quest-log hydration follow-on.
27. Read `37-module-player-quest-hydration-pass.md` for the shared quest-hydration helper extraction and the remaining local-player bookkeeping follow-on.
28. Read `38-module-local-player-bookkeeping-pass.md` for the isolated local-player bookkeeping helper and the next recommendation around `local_stats.player_count` consumer migration.
29. Read `39-session-local-player-count-access-pass.md` for the first read-side migration away from raw `local_stats.player_count` reads and the next follow-on recommendation.
30. Read `40-module-spawn-local-player-count-access-pass.md` for the spawn-time local-player-count accessor cleanup and the narrowed follow-on recommendation around `game_loop.c`.
31. Read `41-game-loop-local-player-status-pass.md` for the extracted local-player-status helper in `game_loop.c` and the next follow-on recommendation after the last gameplay-loop `local_stats.player_count` read is removed.
32. Read `42-session-local-player-status-ownership-pass.md` for the session-owned local-player status surface, the preserved legacy mirrors, and the next compatibility-cleanup recommendation.

## Immediate recommendation

Do not start by rewriting systems wholesale. The first safe wins are:

1. Freeze and document the current build/run path for Linux.
2. Establish source-of-truth boundaries for active code versus archive/generated/vendor content.
3. Add non-UI validation tooling for content loading and module parsing.
4. Introduce a runtime context abstraction around `_currentModule` and `_gameEngine` before attempting deeper subsystem extraction.
5. Make the Windows target explicit: native compilation should be supported in the future, the toolchain should remain fully open source, and Visual Studio-specific build guidance should be retired from the maintained path.
6. Make Linux-native, native-Windows, and Linux-hosted Windows builds converge toward one coherent cross-platform workflow.
7. Reduce routine C++ warnings as part of the long-term maintainability and portability push rather than accepting them as normal.

Items 1 and 3 now have a first implementation in this workspace:

- canonical Linux doc: `doc/build-linux.md`
- content validation baseline: `06-validator-baseline.md`
- historical legacy-doc audit: `07-historical-docs-audit.md`
- spawn format compatibility spec: `08-spawn-format-spec.md`
- data format compatibility spec: `09-data-format-spec.md`
