# Refactoring Documents

This directory is the baseline architecture and refactoring audit for the current Egoboo workspace. Original baseline: 2026-04-12. Last index refresh: 2026-04-17.

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
- The runtime historically depended heavily on global singletons and global mutable state, especially `_gameEngine`, `_currentModule`, and `update_wld`. The active ownership seams for `_currentModule` and `_gameEngine` are now retired in-repo; only a small amount of `update_wld` terminology residue remains in comments and debug labels.
- The virtual file system is not a thin wrapper. It actively rewrites where content comes from by mounting module and global directories onto logical paths like `mp_data`, `mp_modules`, and `mp_objects`.
- The content model is directory-shaped and convention-driven: `menu.txt`, `spawn.txt`, `data.txt`, `script.txt`, `message.txt`, `partN.txt`, `enchant.txt`, `level.mpd`, `tris.md2`, plus bitmap and audio assets.
- There are several abandoned or partial modernization attempts already in the tree: `doc/ego2xml/`, `utilities/migrator/`, and `egolib/library/src/egolib/game/Lua/`.
- Build documentation has been reconciled. `README.md` and `doc/build-linux.md` / `doc/build-windows.md` are the source of truth; `README.Linux` is now a short stub that redirects to `doc/build-linux.md`. The Fedora-specific source edits previously tracked as uncommitted workspace drift have since landed as repo commits (see `b97717e48`).
- Windows build direction is still inconsistent. A MinGW-based path exists, but the maintained project direction should be native Windows compilation with fully open-source tooling rather than Visual Studio-specific workflows or Wine-only validation.
- The Linux-hosted Windows path is not healthy yet. `debug-output.txt` shows a current Wine-run startup failure involving font atlas initialization and a later crash during audio loading.
- The current codebase still has many bugs, incomplete features, and portability warnings. The refactor goal is not just cleaner structure; it is a more usable and maintainable game.
- Automated tests exist mainly for utility code. They do not currently protect gameplay, module loading, scripting, or content compatibility.

## Snapshot metrics

Refreshed 2026-04-17. The 2026-04-12 baseline values are preserved in `01-repository-and-build-audit.md`.

| Metric                                                                               |                                                      Value |
| ------------------------------------------------------------------------------------ | ---------------------------------------------------------: |
| Runtime source files in `egolib`/`egoboo`/`cartman` (`*.c`, `*.cpp`, `*.h`, `*.hpp`) |                                                        656 |
| C files                                                                              |                                                         70 |
| C++ implementation files                                                             |                                                        244 |
| Header files (`.h`)                                                                  |                                                         71 |
| Header files (`.hpp`)                                                                |                                                        271 |
| Largest translation unit                                                             |             `egolib/library/src/egolib/vfs.c` (2445 lines) |
| `_currentModule` references (in runtime code)                                        |                                                          0 |
| `_gameEngine` references (in runtime code)                                           |                                                          0 |
| `update_wld` references (in runtime code)                                            |                                                          3 |
| `TODO`/`FIXME`/`HACK` style matches in `egolib` active code                          |                                                         63 |
| Modules under `data/modules`                                                         |                                                         42 |
| Object directories under `data/` (`data.txt`-bearing)                                |                                                        953 |
| `data.txt` files                                                                     |                                                        953 |
| `script.txt` files                                                                   |                                                        958 |
| `enchant.txt` files                                                                  |                                                        207 |
| `level.mpd` files                                                                    |                                                         42 |
| `tris.md2` files                                                                     |                                                        956 |

The `_currentModule` / `_gameEngine` / `update_wld` reference counts dropped from the 2026-04-12 baseline (592 / 266 / 65) as the runtime-context extraction passes (`11-`…`51-`) migrated consumers onto the session and engine accessor surfaces. The raw `_currentModule` and `_gameEngine` seams are now retired in-repo. The remaining `update_wld` mentions are limited to `script.c`, `ObjectGraphics.hpp`, and `Particle.hpp`, where they survive only as legacy wording in a debug label and comments.

The `.c` file count rose from 56 → 70 because `script_functions.c` (formerly an 8153-line single TU) was split into seven domain-specific files (`script_functions_{action,bitwise,movement,spawn,state,systems,target}.c`). This is a deliberate decomposition, not a regression in C→C++ progress.

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
- `17-codebase-health-assessment.md`
- `18-modularization-analysis.md`
- `19-new-refactoring-plan.md`
- `20-inventory-and-commerce-runtime-context-pass.md`
- `21-presentation-engine-context-pass.md`
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
- `42-session-local-player-status-ownership-pass.md`
- `43-local-player-status-compatibility-quarantine-pass.md`
- `44-local-player-perception-ownership-pass.md`
- `45-local-player-enemy-sense-ownership-pass.md`
- `46-cross-platform-and-third-party-independence-status.md`
- `47-local-player-respawn-cooldown-ownership-pass.md`
- `48-local-stats-legacy-boundary-pass.md`
- `49-local-stats-accessor-shim-pass.md`
- `50-local-stats-export-retirement-pass.md`
- `51-engine-context-ownership-pass.md`
- `52-object-field-encapsulation-pass.md`
- `53-object-flag-encapsulation-pass.md`
- `54-object-attachment-platform-encapsulation-pass.md`
- `55-object-runtime-timer-status-encapsulation-pass.md`
- `56-object-movement-collision-mask-encapsulation-pass.md`
- `57-object-appearance-profile-encapsulation-pass.md`
- `58-object-stats-ammo-gender-encapsulation-pass.md`
- `59-object-orientation-encapsulation-pass.md`
- `60-object-bumper-collision-volume-encapsulation-pass.md`
- `61-object-inst-transitional-boundary-pass.md`
- `62-object-graphics-escape-hatch-retirement-pass.md`
- `63-object-graphics-tint-reflection-policy-pass.md`
- `64-object-graphics-profile-animation-reset-pass.md`
- `65-object-graphics-animation-control-policy-pass.md`
- `66-object-graphics-animation-transition-pass.md`
- `67-object-graphics-animation-state-bookkeeping-pass.md`
- `68-object-graphics-frame-publication-pass.md`
- `69-object-graphics-update-animation-publication-pass.md`

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
33. Read `43-local-player-status-compatibility-quarantine-pass.md` for the final quarantine of the local-player status mirrors and the next recommendation around broader legacy-global cleanup.
34. Read `44-local-player-perception-ownership-pass.md` for the session-owned local-player perception surface, preserved legacy mirrors, and the remaining legacy-global follow-on seams.
35. Read `45-local-player-enemy-sense-ownership-pass.md` for the session-owned minimap enemy-sense surface, preserved legacy mirrors, and the remaining respawn-timing follow-on seam.
36. Read `46-cross-platform-and-third-party-independence-status.md` for the current cross-platform build, Visual Studio retirement, and third-party dependency self-containment snapshot with the remaining gap list.
37. Read `47-local-player-respawn-cooldown-ownership-pass.md` for the session-owned respawn cooldown surface, the preserved `local_stats.revivetimer` compatibility mirror, and the new post-cleanup recommendation for the remaining legacy ABI boundary.
38. Read `48-local-stats-legacy-boundary-pass.md` for the audit-backed narrowing of the `local_stats` declaration surface and the remaining question around out-of-repo compatibility consumers.
39. Read `49-local-stats-accessor-shim-pass.md` for the explicit accessor-based quarantine around the exported `local_stats` global and the remaining decision point around external consumers.
40. Read `50-local-stats-export-retirement-pass.md` for the retirement of the raw `local_stats` export, the accessor-only legacy mirror boundary, and the return to the final in-repo engine ownership cleanup seam.
41. Read `51-engine-context-ownership-pass.md` for the retirement of the raw `_gameEngine` export, the `EngineContext` ownership seam, and the clarification that remaining `update_wld` mentions are terminology residue rather than an active global-state boundary.
42. Read `52-object-field-encapsulation-pass.md` for the first bounded `Object` field-accessor cleanup covering team, held/equipment, jump, size-transition, and damage-type state.
43. Read `53-object-flag-encapsulation-pass.md` for the follow-on encapsulation of player-binding, mutable flag, and sparkle state plus the recommendation to tackle attachment/platform coupling next.
44. Read `54-object-attachment-platform-encapsulation-pass.md` for the completed attachment, inventory-placement, and platform-capability accessor cleanup and the follow-on recommendation after the highest-traffic `Object` state is sealed behind accessors.
45. Read `55-object-runtime-timer-status-encapsulation-pass.md` for the follow-on encapsulation of cooldown timers, confusion state, dismount bookkeeping, water-state, and icon-display state plus the narrowed recommendation before any role-interface extraction.
46. Read `56-object-movement-collision-mask-encapsulation-pass.md` for the next bounded encapsulation of `stoppedby`, `turnmode`, and bump-list linkage before moving into either the appearance cluster or stats/ammo scalar cleanup.
47. Read `57-object-appearance-profile-encapsulation-pass.md` for the bounded encapsulation of skin/base-model/overlay/shadow scalar state and the recommendation to finish the remaining stats/ammo/gender scalar cleanup before broader interface work.
48. Read `58-object-stats-ammo-gender-encapsulation-pass.md` for the bounded encapsulation of the remaining stats/ammo/gender scalar surface and the branching recommendation between orientation and `inst`.
49. Read `59-object-orientation-encapsulation-pass.md` for the bounded encapsulation of `ori` / `ori_old`, the reason that seam landed before `inst`, and the next follow-on recommendation around bumper/CV or render-facing cleanup.
50. Read `60-object-bumper-collision-volume-encapsulation-pass.md` for the bounded encapsulation of `Object` bumper/collision-volume state, the grouped collision publish path in `ObjectPhysics`, and the narrowed follow-on recommendation around `inst`.
51. Read `61-object-inst-transitional-boundary-pass.md` for the transitional `Object::inst` boundary, the temporary `graphics()` escape hatch for render/matrix code, and the remaining follow-on recommendation after object/render callers stop depending on a public graphics instance.
52. Read `62-object-graphics-escape-hatch-retirement-pass.md` for the retirement of the public `Object::graphics()` escape hatch, the new stable render-facing `Object` helpers, and the narrowed follow-on recommendation inside `ObjectGraphics`.
53. Read `63-object-graphics-tint-reflection-policy-pass.md` for the extracted tint/reflection render-policy seam inside `ObjectGraphics`, the preserved public `Object` render-facing surface, and the narrowed follow-on recommendation around model-reset and animation-reset responsibilities.
54. Read `64-object-graphics-profile-animation-reset-pass.md` for the split between profile/model reset and initial animation policy inside `ObjectGraphics`, the preserved `Object` caller contract, and the narrowed follow-on recommendation around broader animation/control policy cleanup.
55. Read `65-object-graphics-animation-control-policy-pass.md` for the bounded extraction of `updateAnimationRate()` policy, the preserved caller contract inside `ObjectGraphics`, and the narrowed follow-on recommendation around end-of-animation transition cleanup.
56. Read `66-object-graphics-animation-transition-pass.md` for the bounded extraction of `incrementFrame()` end-of-animation transition policy, the preserved caller contract inside `ObjectGraphics`, and the narrowed follow-on recommendation around animation-state mutation and frame bookkeeping cleanup.
57. Read `67-object-graphics-animation-state-bookkeeping-pass.md` for the bounded cleanup of action mutation versus frame-bookkeeping responsibilities inside `ObjectGraphics` and the narrowed follow-on recommendation around animation-state publication.
58. Read `68-object-graphics-frame-publication-pass.md` for the explicit frame-publication helper cleanup around `setFrameFull()` / `removeInterpolation()` and the narrowed follow-on recommendation around `updateAnimation()` and cache-publication sequencing.
59. Read `69-object-graphics-update-animation-publication-pass.md` for the bounded `updateAnimation()` interpolation/publication helper extraction, the preserved step-order characterization coverage, and the narrowed follow-on recommendation around cache-validity bookkeeping.

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
