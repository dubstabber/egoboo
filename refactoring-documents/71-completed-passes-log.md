# Completed Passes Log

Chronological summary of the numbered refactoring passes (10 through 250) and Tier 2 build tasks completed between 2026-04-13 and 2026-06-22. Passes 10 through 69 each had their own per-pass document before 2026-04-18; those documents were consolidated into this log to reduce directory clutter. Later passes append directly here. Full per-pass detail (scope constraints, acceptance commands, follow-on recommendations) remains in git history.

For the current-state snapshot, read `CODEBASE-HEALTH-STATUS.md`. For the forward plan that builds on these passes, read `19-refactoring-roadmap.md`.

Note: this is a historical log. Older entries preserve the verification commands
and counts used at the time; for new work, use the current build/test guidance in
root `AGENTS.md` and `CODEBASE-HEALTH-STATUS.md`.

---

## Theme 1 — Spawn format and validator (2026-04-13 → 04-15)

### Pass 10 — Spawn reconciliation (2026-04-13)

Extended `egoboo-content-validator` with a deterministic object-reconciliation report per module: raw load token, normalized object name, resolved `mp_objects/...` virtual path, and a reachable-object inventory based on current VFS mount order. Surfaced canonical-key candidate matches for likely aliases like `g'nome.obj` vs `gnome.obj`. Baseline confirmed at 42 modules, 33 failing, 277 `missing_spawn_object` errors.

### Pass 24 — Spawn reconciliation remediation (2026-04-15)

Validator JSON schema bumped to v3. Each reconciliation row now carries `placeholder_like` and `suggested_matches[]` with `object_name`, `resolved_virtual_path`, `source_kind`, `origin_path`, `match_reason`, `score`. Ranking policy: exact canonical-key matches, then prefix/suffix/contains, then edit-distance ≤3.

---

## Theme 2 — Runtime context wrappers (2026-04-13 → 04-15)

All passes in this theme share a common shape: extract `GameSessionContext` / `EngineContext` accessors and replace raw `_currentModule` / `_gameEngine` / `update_wld` reads in one bounded caller set per pass.

### Pass 11 — Runtime context extraction (2026-04-13)

Introduced `GameSessionContext` for active-module, import-list, slot-override, world-update, character-stat-clock, and enchant-stat-clock ownership. Migrated `game_begin_module`, `game_quit_module`, `game_finish_module`, `LoadingState`, `DebugModuleLoadingState`, `MapEditorState`, `GameEngine` frame-count / shutdown, and validator bootstrap. Shared `ContentRuntimeBootstrap` between runtime and validator.

### Pass 12 — UI / game-state session access (2026-04-13)

Removed direct `_currentModule` reads from `PlayingState`, `InGameMenuState`, `VictoryScreen`, `CharacterStatus`, `CharacterWindow`, `LevelUpWindow`, `MiniMap`. Introduced `tryActiveModule()` for destructors and debug watches that can outlive the active module.

### Pass 13 — Gameplay runtime shell context (2026-04-13)

Removed `_currentModule` / `_gameEngine` / `update_wld` reads from `game.c` gameplay-shell helpers (import/export flow, player/session helpers, particle/object helper entrypoints, status/minimap/message-log access). Introduced narrow `EngineContext` wrapper.

### Pass 14 — Graphics runtime shell context (2026-04-13)

Removed global-state reads from `graphic.c` (HUD, overlay, debug/status, cursor, passage debug, tile/entity list assembly, flashing, object/particle instance update gates). Routed through `GameSessionContext` / `EngineContext`.

### Pass 15 — Entity / physics runtime context (2026-04-13)

Migrated `Entities/Object.*`, `Entities/Particle.cpp`, `Entities/ParticleHandler.cpp`, `game/Physics/ObjectPhysics.cpp`, `game/Physics/ParticlePhysics.cpp`, `game/Physics/particle_collision.c`, `game/Logic/Player.cpp` off raw globals. Moved header-level access behind out-of-line implementations where needed.

### Pass 16 — Scripting runtime shell context (2026-04-13)

Removed global reads from `game/script_functions.c`, `Script/script.c`, `game/script_implementation.c`, `game/script_variables.c`. Routed active-module, object-handler, team, passage, mesh, player, and world-update access through the session seam.

### Pass 20 — Inventory and commerce runtime context (2026-04-15)

Removed `_currentModule` reads from `game/CharacterMatrix.c`, `game/Inventory.cpp`, `game/Shop.cpp` (stack merging, inventory swap/remove, shop buy/sell/steal, held-item matrix updates).

### Pass 21 — Presentation engine context (2026-04-15)

Removed direct `_gameEngine` reads from all of `game/GUI/*` and `game/GameStates/*`. Extended `EngineContext` with UI-manager accessors. Presentation code now calls `engine()` / `uiManager()` through the context wrapper.

### Pass 23 — Session-state ownership (2026-04-15)

`GameSessionContext` took direct ownership of active `GameModule`, import-list, slot-override, world-update count, character/enchant stat clocks. Removed legacy storage exports from `game.h`, `game.c`, `game_export.c`, `game_loop.c`, `Module.hpp`, `GameEngine.hpp`. Migrated remaining direct reads in `Logic/Team.cpp`, `Entities/Enchant.cpp`, `game/link.c`, `game/mesh.c`, `Profiles/ProfileSystem.cpp`, `game/GUI/InventorySlot.cpp`, `game/GUI/CharacterStatus.cpp`, `game/Graphics/ObjectGraphics.cpp`.

---

## Theme 3 — Module runtime ownership (2026-04-16)

### Pass 22 — Module runtime ownership plan execution (2026-04-15 → 04-16)

Checkpoint plan from `22-module-runtime-ownership-plan.md` (consolidated into `19-refactoring-roadmap.md`) executed across passes 23, 26, 27, 28. All checkpoints landed.

### Pass 26 — Audio session leaf cleanup (2026-04-16)

Added `GameSessionContext::tryObjectHandler()` nullable accessor. `Audio/AudioSystem.cpp` now resolves looping-sound owner objects through the session accessor; no longer needs the `GameModule` header include.

### Pass 27 — Environment state ownership (2026-04-16)

Moved weather, fog, and animated-tile state ownership under `GameModule`. `upload_wawalite(...)` uploads into explicit state objects instead of file-scope globals. Removed `g_weatherState`, `fog`, `g_animatedTilesState` exports. Active consumers (`Module.cpp`, `graphic_fan.c`, `script_functions_state.c`, `script_functions_systems.c`) routed through session accessors.

### Pass 28 — Module translation-unit split (2026-04-16)

Split `GameModule` into seven files under `game/Module/`: `Module.cpp` (105 lines, accessors only), `Module_bootstrap.cpp`, `Module_loading.cpp`, `Module_spawn.cpp`, `Module_spawn_plan.cpp`, `Module_spawn_realization.cpp`, `Module_update.cpp`. CMakeLists updated with explicit source listing.

### Pass 29 — Module boundary coverage (2026-04-16)

Added `egolib/tests/egolib/tests/ModuleLoadSmoke.cpp` spawn-slot boundary test, spawn-resolution characterization test for `test.mod`, and wawalite environment assertions. Pinned module-owned water/weather/fog/animated-tile state against parsed `wawalite.txt`.

### Pass 30 — Module spawn planning (2026-04-16)

Extracted planning half of `spawnAllObjects()` into `Module_spawn_plan.cpp`: parse `spawn.txt` → normalized entries with reserved concrete profile slots. Planning logic now smaller, testable, side-effect-free.

### Pass 31 — Module spawn realization (2026-04-16)

Extracted live-object realization into `Module_spawn_realization.cpp`: attach-none matrix setup, inventory attachment/merge-termination, left/right grip attachment, startup-equipment identification, local-player binding (single-player and import-based). Added direct characterization tests for attachment and player-binding branches.

---

## Theme 4 — Player startup and binding (2026-04-16)

### Pass 34 — Module player-binding policy (2026-04-16)

Isolated the player-binding decision from `realizeSpawnEntry()` into a narrower policy seam. Kept `ops.addPlayer(...)` plumbing in `GameModule::spawnObjectFromFileEntry()` unchanged. Preserved no-op behavior when import-slot matching fails.

### Pass 35 — Module startup-equipment hook (2026-04-16)

Extracted startup-equipment identification (curse clearing, name-known marking for local-player child items) from `realizeSpawnEntry()` into its own hook. Hook still runs after attachment/XP adjustment, before player-binding side effects.

### Pass 36 — Module player-startup boundary (2026-04-16)

Extracted `module_player_startup` helper inside `GameModule` for: player-list registration, player-index assignment, quest-log hydration, local-player flag updates, optional spawned-player identification. Kept `GameModule::addPlayer()` public signature unchanged.

### Pass 37 — Module player quest hydration (2026-04-16)

Extracted shared quest-hydration helper used by both module startup and menu-time player loading. Preserved silent missing-`quest.txt` behavior.

### Pass 38 — Module local-player bookkeeping (2026-04-16)

Extracted `applySuccessfulLocalPlayerBookkeeping()`: `islocalplayer`, `local_stats.player_count++`, `noplayers=false`, optional `nameknown=true`. `finalizeLocalPlayerStartup()` became the small orchestrator that hydrates quests and applies bookkeeping.

---

## Theme 5 — Local-player session ownership (2026-04-16 → 04-17)

### Pass 39 — Session local-player count access (2026-04-16)

Added `GameSessionContext::localPlayerCount()` accessor. Migrated first read consumers: camera setup in `DebugModuleLoadingState`, single-player fast-turn gating in `Player::updateLatches()`, single-player autoturn gating in `Camera::readInput()`.

### Pass 40 — Module spawn local-player count access (2026-04-16)

Migrated `GameModule::spawnObjectFromFileEntry()` off raw `local_stats.player_count` — zero-import spawn-time device-slot selection now flows through the session accessor.

### Pass 41 — Game-loop local-player status (2026-04-16)

Migrated `MainLoop::updateLocalStats()` to derive `allpladead` from aggregating registered local-player liveness rather than mirrored `local_stats.player_count`. Added helper for dead/alive counts excluding null/terminated objects.

### Pass 42 — Session local-player status ownership (2026-04-16)

Added `GameSessionContext` read surface: `localPlayerStatus()`, `hasLocalPlayers()`, `allLocalPlayersDead()`. Added publication helpers: `publishLocalPlayerCount()`, `publishLocalPlayerStatus()`, `resetLocalPlayerState()`. Migrated respawn gating in `MainLoop::readPlayerInput()` and `draw_game_status()`.

### Pass 43 — Local-player status compatibility quarantine (2026-04-16)

Quarantined `local_stats` mirror writes for `player_count` / `noplayers` / `allpladead` behind a file-local compatibility bridge in `GameSessionContext.cpp`. Removed `syncLegacyLocalPlayerState()` from the public session type surface. Annotated `local_stats_t` as legacy compatibility state.

### Pass 44 — Local-player perception ownership (2026-04-16)

Added `LocalPlayerPerceptionState` + `collectLocalPlayerPerception()` for `seeinvis_level/mag`, `seedark_level/mag`, `seekurse_level`, `grog_level`, `daze_level`. Migrated `Graphics/Camera.cpp`, `Graphics/ObjectGraphics.cpp`, `graphic_lighting.c`, `graphic_scene.c` to read session-owned perception. Preserved map-editor invisibility-reveal by routing override through session.

### Pass 45 — Local-player enemy-sense ownership (2026-04-16)

Added `EnemySenseState` (`sense_enemies_team`, `sense_enemies_idsz`). Migrated production writers (perk-driven minimap reveals in `Object_update.cpp`, scripted enemy-blip publication in `script_functions_systems.c`) and `MiniMap::draw()` consumer to session seam. `game_reset_players()` clears session state and mirrors.

### Pass 47 — Local-player respawn cooldown ownership (2026-04-17)

Moved `local_stats.revivetimer` ownership into `GameSessionContext` with `publishRespawnCooldown()` / `tickRespawnCooldown()` / `resetRespawnCooldown()`. Migrated death handling in `Object_combat.cpp`, countdown in `MainLoop::updateLocalStats()`, respawn gate in `MainLoop::readPlayerInput()`.

---

## Theme 6 — Local-stats retirement and engine context (2026-04-17)

### Pass 48 — Local-stats legacy boundary (2026-04-17)

Moved `local_stats_t` / `local_stats` ABI declarations out of the broad `egolib/game/egoboo.h` umbrella into dedicated `egolib/game/LegacyLocalStats.hpp`. No behavior change.

### Pass 49 — Local-stats accessor shim (2026-04-17)

Added inline accessor shim for legacy-mirror access. Updated focused compatibility assertions in `ModulePlayerStartup.cpp` to read through the shim instead of the raw global name.

### Pass 50 — Local-stats export retirement (2026-04-17)

Replaced exported file-scope `local_stats` definition with file-local compatibility storage plus the accessor definitions. Raw variable name no longer advertised or exported; compatibility mirror data shape preserved.

### Pass 51 — Engine context ownership (2026-04-17)

Removed raw `extern std::unique_ptr<GameEngine> _gameEngine` declaration from `GameEngine.hpp`. `Main.cpp` now installs, starts, and clears the engine through `EngineContext`. Added focused `EngineContext` tests (empty-state, installation, double-install rejection, clearing). `update_wld` confirmed as terminology residue only, not live coupling.

---

## Theme 7 — Object and ObjectGraphics encapsulation (2026-04-17)

All passes in this theme share a shape: move a cluster of `Object.hpp` public fields behind private accessors, migrate non-`Object` callers, extend `egolib/tests/egolib/tests/ObjectAccessors.cpp` with regression coverage.

### Pass 52 — Object field encapsulation (2026-04-17)

First cluster: held/equipped object refs, current/base team refs, jump state, size-transition state, damage-target/reaffirm damage types, damage threshold.

### Pass 53 — Object flag encapsulation (2026-04-17)

Player registration, knowledge / kurse / item / shop-item / crushable flags, sparkle state.

### Pass 54 — Object attachment and platform encapsulation (2026-04-17)

Holder and inventory-placement refs, platform capability flags, platform holding-weight bookkeeping.

### Pass 55 — Object runtime timer / status encapsulation (2026-04-17)

Cooldown timers, confusion state, dismount bookkeeping, in-water state, draw-icon flag.

### Pass 56 — Object movement / collision-mask encapsulation (2026-04-17)

`stoppedby`, `turnmode`, `bumplist_next` — collision/pathfinding/LOS masks and movement turn mode.

### Pass 57 — Object appearance / profile encapsulation (2026-04-17)

`skin`, `skin_stt`, `basemodel_ref`, `is_overlay`, `shadow_size_*` (baseline / saved / current).

### Pass 58 — Object stats, ammo, gender encapsulation (2026-04-17)

`gender`, `experience`, `experiencelevel`, `ammomax`, `ammo`.

### Pass 59 — Object orientation encapsulation (2026-04-17)

`ori` and `ori_old` — across spawn, physics, scripts, combat, particles, enchantment, matrix-update.

### Pass 60 — Object bumper / collision-volume encapsulation (2026-04-17)

`bump_stt`, `bump`, `bump_save`, `bump_1`, `chr_max_cv`, `chr_min_cv`, `slot_cv`. Added narrow write-side helpers so spawn/bootstrap and `ObjectPhysics` publish collision state without re-exposing fields. Rewired `ObjectPhysics::updateCollisionSize()` to publish through accessors.

### Pass 61 — Object `inst` transitional boundary (2026-04-17)

Added matrix, vertex, and lighting helper forwarding on `Object`. Sealed public `ObjectGraphics` data surface (alpha/light/sheen, color shift, texture offsets, matrix-cache copy/validity). Introduced temporary `graphics()` / `graphics() const` escape hatch for render-facing callers.

### Pass 62 — ObjectGraphics escape-hatch retirement (2026-04-17)

Removed the public `Object::graphics()` / `graphics() const` escape hatch. Stable render-facing `Object` helpers (`hasModelDescriptor()`, `getReflectionAlpha()`, `getTint(...)`) cover the remaining renderer callers (`graphic_mad.c`, `graphic_prt.c`, `ParticleGraphics.cpp`).

### Pass 63 — ObjectGraphics tint / reflection policy (2026-04-17)

Extracted tint/reflection policy helpers inside `ObjectGraphics`: reflection-tint computation, local-player perception overrides, final render tint encoding for `CHR_ALPHA`, `CHR_LIGHT`, `CHR_PHONG`.

### Pass 64 — ObjectGraphics profile / animation reset (2026-04-17)

Split `setObjectProfile()` into profile/model-reset phase and initial-animation policy phase. Kept `setObjectProfile()` as the only public profile-application entrypoint.

### Pass 65 — ObjectGraphics animation control policy (2026-04-17)

Extracted `updateAnimationRate()` into `applyIdleAnimationPolicy()` and `applyMovementAnimationPolicy()`. Preserved caller contract and mutation order.

### Pass 66 — ObjectGraphics animation transition (2026-04-17)

Extracted `incrementFrame()` end-of-animation transition policy and mounted-loop resolution (`resolveMountedLoopAnimation()`). Preserved frame publication order and child-instance invalidation.

### Pass 67 — ObjectGraphics animation state bookkeeping (2026-04-17)

Split action mutation vs frame bookkeeping: `normalizeCurrentAnimationForFrameMutation()`, `commitFrameState()`, `invalidateChildInstancesIfCacheInvalid()`, `restartMovementAnimation()`. Action helpers own `_currentAnimation` / `_nextAnimation` / `_canBeInterrupted`; frame helpers own `_sourceFrameIndex` / `_targetFrameIndex` / `_animationProgressInteger` / `_animationProgress`.

### Pass 68 — ObjectGraphics frame publication (2026-04-17)

Explicit frame-publication helper cleanup around `setFrameFull()` and `removeInterpolation()`. Behavior unchanged; contract narrowed.

### Pass 69 — ObjectGraphics updateAnimation publication (2026-04-17)

Extracted `publishInterpolationState(...)` and `applyPublishedInterpolationStep()`. Rewrote `updateAnimation()` to delegate quarter-step and residual-progress publication. Routed `incrementFrame()` child invalidation through `invalidateChildInstancesIfCacheInvalid()`.

### Pass 72 — Object AI helper seam (2026-04-18)

Moved the stateful `Object` AI helper methods (`addAIOrder`, `markAIChanged`, `recordAIBump`, `resetAIState`, `spawnAIState`) and the transitional `aiStateForScript()` bridge out of `Object.hpp` and into the split implementation. Kept raw `ai_state_t` access quarantined to the existing script seam.

Extended `ObjectAccessors.cpp` with characterization coverage for AI order publication, changed-state publication, bump-alert throttling, and the reset/spawn defaults that script execution depends on.

### Pass 73 — Object AI accessor closure (2026-04-18)

Moved the remaining header-inline `Object` AI accessor block (`alert/state/content/timer/poof/owner-child-target/last-hit` accessors) out of `Object.hpp` and into the split implementation beside the existing AI helper seam. Kept `aiStateForScript()` public as the only raw `ai_state_t` bridge for the legacy script runtime.

### Pass 74 — Object enchant / temp-attribute seam closure (2026-04-18)

Replaced the remaining mutable enchant-list and temp-attribute map leaks on `Object` with narrow helpers: read-only enchant observation (`hasActiveEnchants()`, `getFirstActiveEnchant()`, const list access, explicit `addActiveEnchant(...)`) plus explicit temp-attribute mutation helpers (`has/get/set/adjust/clear`). Migrated `Enchant.cpp`, `CharacterWindow`, `game_loop.c`, `particle_collision.c`, and `script_functions_systems.c`.

Extended `ObjectAccessors.cpp` with temp-attribute regression coverage and an end-to-end enchant publication test using a real enchant-backed profile.

### Pass 75 — Object inventory seam closure (2026-04-18)

Removed the public `Inventory& getInventory()` escape hatch from `Object` and replaced it with narrow inventory observation/mutation helpers (`getInventoryItem(s)`, first-free-slot, `setInventoryItem(...)`, `removeInventoryItem(...)`). Migrated `Inventory.cpp`, `Object_interaction.cpp`, spawn attachment, and script inventory callers to the narrowed surface.

Extended `ObjectAccessors.cpp` with direct inventory-mutation coverage and a characterization test for `Inventory::add_item`, `swap_item`, and `remove_item` through the narrowed `Object` seam.

### Pass 76 — Object team seam closure (2026-04-18)

Changed `Object::getTeam()` to a read-only surface and routed write-side team actions through explicit `Object` intent methods (`becomeTeamLeader`, `callTeamForHelp`, `giveTeamExperience`). Moved team leadership and morale bookkeeping in `setTeam()` / `respawn()` behind internal helpers.

Extended `ObjectAccessors.cpp` with regression coverage for team leadership assignment, team-change morale transfer, team call-for-help publication, and respawn-time leader reclamation. `aiStateForScript()` remains the only intentional mutable legacy bridge on `Object`.

### Pass 77 — `IInventoryHolder` role extraction start (2026-04-18)

Introduced the first `Object` role interface, `IInventoryHolder`, and made `Object` implement it without changing runtime behavior. Refactored `Inventory.cpp` so the core add/find/swap/remove logic operates on the role surface, while the existing `ObjectRef` entrypoints remain as compatibility wrappers.

Extended `ObjectAccessors.cpp` with a direct interface-based inventory regression that exercises add/swap/remove through `IInventoryHolder`. Updated the roadmap and health docs to reflect that inventory/team seam closure is complete and role extraction is now the active Tier 1 frontier.

### Pass 78 — `IRenderable` role extraction start (2026-04-18)

Introduced the second `Object` role interface, `IRenderable`, and made `Object` implement it as a read-only render-facing seam over the existing `ObjectGraphics` forwarding surface. Migrated `graphic_mad.c` and the opaque/non-opaque/reflection render passes to consume the role instead of `Object` directly, while leaving mixed render/physics helpers for a later `IPhysical` pass.

Extended `ObjectAccessors.cpp` with an interface-based render regression covering render policy, tint, model-descriptor visibility, and matrix/reflection access through `IRenderable`.

### Pass 79 — `IScriptable` role seam (2026-04-18)

Introduced `IScriptable` as the third new T1.2 role interface and made `Object` implement the existing script-visible AI/publication surface through that seam. Narrowed selected spawn/shop/module/object-lifecycle helpers and regression tests to the role interface instead of the concrete `Object` type.

Kept `aiStateForScript()` quarantined on `Object` for `Script/script.c`; this pass continues role extraction without changing the raw legacy script runtime.

Extended `ObjectAccessors.cpp` with a bridge-equivalence regression and added `ModuleUpdate.cpp` coverage pinning `GameModule::updateAllObjects()` to the public `getAIPoofTime()` termination boundary.

### Pass 80 — `IDamageable` role seam (2026-04-18)

Introduced `IDamageable` as the fourth T1.2 `Object` role interface and made `Object` implement the existing combat-facing damage, healing, invincibility, damage-timer, and damage-type surface through that seam. Narrowed bounded combat callers in `Particle_combat.cpp`, `particle_collision.c`, `Module_update.cpp`, and selected script-system damage helpers to use the role interface where they only need damage behavior, while leaving mixed physics, AI, and rendering flows on concrete `Object`.

Extended `ObjectAccessors.cpp` with a focused interface regression covering damage-timer parity, damage-type/reduction access, and bounded `damage(...)`, `heal(...)`, and `kill(...)` calls through `IDamageable`. Build and `test.mod` validator checks remained clean for this pass.

### Pass 81 — `IPhysical` role seam (2026-04-18)

Introduced `IPhysical` as the fifth T1.2 `Object` role interface and made `Object` implement the read-only physical surface for bumper state, collision volumes, and orientation. Narrowed bounded geometry readers in `ParticlePhysics.cpp`, `particle_collision.c`, and `game.c` to consume the role where they only need collision-shape or facing data, while leaving mixed platform, movement-mask, and writer-side physics flows on concrete `Object`.

Extended `ObjectAccessors.cpp` with an interface parity regression covering bump-state, collision-volume, and orientation access through `IPhysical`. Build, targeted accessor tests, and the `test.mod` validator remained the acceptance bar for this pass.

### Pass 82 — Conservative T1.2 caller migration (2026-04-18)

Migrated low-risk gameplay helpers onto existing `Object` role seams without changing public APIs: `game.c` now resolves owner/target escape-code substitutions through `IScriptable`, and `Shop.cpp` routes shopkeeper order/target publication through the same role instead of the concrete `Object` surface.

Added focused regressions for game-text owner/target expansion and shop buy / no-afford / theft-publication behavior. Kept `aiStateForScript()` quarantined; no `Script/script.c` changes in this pass.

### Pass 83 — Conservative `IScriptable` gameplay migration (2026-04-18)

Migrated another bounded set of gameplay helpers onto `IScriptable` without widening the role surface: `game_loop.c`, `script_functions_spawn.c`, `script_functions_target.c`, and `ParticleHandler.cpp` now route alert/timer/owner/target/content writes through the existing script-facing seam where those helpers only need AI publication behavior.

Added focused gameplay-loop and particle regressions for cleaned-up / crushed alert handling and defence-ping publication, while keeping `Object::aiStateForScript()` explicitly quarantined for the legacy script runtime.

### Pass 84 — Conservative target-helper role migration (2026-04-18)

Migrated a narrower target-query / target-order slice in `script_functions_target.c` onto existing role seams: `scr_IfTargetKilled()` now reads liveness through `IDamageable`, while `scr_OrderTarget()`, `scr_GetTargetState()`, `scr_GetTargetContent()`, and `scr_GetTargetDamageType()` consume `IScriptable` instead of the concrete `Object` surface.

Added direct script-helper regressions covering target order publication, target state/content/damage-type queries, and killed-target detection. Kept `aiStateForScript()` quarantined to the legacy script runtime; no `Script/script.c` changes in this pass.

### Pass 85 — Conservative AI-publication role migration (2026-04-18)

Migrated pure AI alert/state publication callers in `game_combat.c`, `Inventory.cpp`, `Passage.cpp`, `Team.cpp`, `ObjectPhysics.cpp`, and the hit/publication slice of `particle_collision.c` onto `IScriptable` without widening the role surface or touching `Script/script.c`.

Added focused gameplay regressions for latch-attack publication, team call-for-help, passage crush alerts, item-grab publication, and kursed put-away rejection. `aiStateForScript()` remains quarantined as the only raw legacy bridge.

### Pass 86 — Script-owned raw AI bridge (2026-04-18)

Removed `Object::aiStateForScript()` from the public `Object` surface and replaced it with the narrower `Ego::Script::runtimeState(...)` friend helper owned by the Script subsystem. `Script/script.c` now reaches raw `ai_state_t` through that Script-local seam instead of through `Object`.

Updated `ObjectAccessors`, `ScriptTargetFunctions`, and `ShopInteractions` to assert script-private order/waypoint state through the new helper while keeping public AI publication checks on `IScriptable` and the existing accessors.

### Pass 87 — Conservative physics/combat role migration (2026-04-18)

Migrated the remaining bounded combat/physics caller pockets in `game_combat.c`, `ObjectPhysics.cpp`, and `particle_collision.c` onto existing `IScriptable` / `IDamageable` seams where those helpers only needed AI publication, AI-owned movement limits, or damage-state reads. Kept mixed perk/profile/inventory/position logic on concrete `Object`.

Extended `GameplayAlertPublication.cpp` with a stacked-weapon `character_swipe()` regression that pins `ALERTIF_THROWN` publication on the spawned thrown copy. Build, targeted tests, and the `test.mod` validator remained the acceptance bar.

### Pass 88 — Engine-owned audio service seam (2026-04-18)

Introduced `IAudioSystem` and published the live audio service through `EngineContext`, while keeping `AudioSystem::get()` as the bootstrap seam. `GameEngine` now installs/clears the active audio service during initialize/uninitialize.

Migrated engine-owned presentation callers in game states, menu/theme helpers, and GUI widgets/windows to `EngineContext::get().audioSystem()` instead of `AudioSystem::get()`. Added `EngineContext` regression coverage for audio-service installation, lookup, and teardown behavior.

### Pass 89 — Engine-owned perk service seam (2026-04-18)

Second T1.3 service-interface pass. Introduced `Ego::Perks::IPerkHandler` at `egolib/library/src/egolib/Logic/IPerkHandler.hpp` and made `PerkHandler` implement it. Extended `EngineContext` with `installPerkHandler` / `clearPerkHandler` / `tryPerkHandler` / `perkHandler` accessors mirroring the audio seam, and wired install/clear into `ContentRuntimeBootstrap` beside the existing `PerkHandler::initialize` / `uninitialize` calls so the runtime *and* the validator tool publish the service through the same seam.

Migrated all eight `Ego::Perks::PerkHandler::get()` call sites outside `Logic/Perks` — `ObjectProfile_{load,export}.cpp`, `CharacterWindow.cpp`, `LevelUpWindow.cpp`, `Object_attributes.cpp` — to `EngineContext::get().perkHandler()`. `PerkHandler::get()` remains as the bootstrap seam inside the subsystem. Added five `EngineContext` regression tests covering install, double-install rejection, clear, throw-when-missing, and `clearEngine()` cascade for the perk handler.

### Pass 90 — Engine-published image service seam (2026-04-19)

Third T1.3 service-interface pass. Introduced `Ego::IImageManager` at `egolib/library/src/egolib/Image/IImageManager.hpp` and made `ImageManager` implement it. Extended `EngineContext` with `installImageManager` / `clearImageManager` / `tryImageManager` / `imageManager` accessors, with lifecycle publication owned by `App`/`GFX` for runtime and `ContentRuntimeBootstrap` for validator/tests.

Migrated all non-`Image/` external `ImageManager::get()` callers — `fileutil.c`, `TextureManager.cpp`, `Font.cpp`, `GraphicsWindow.cpp`, `DefaultTexture.cpp`, `TextureAtlasManager.cpp`, `UIManager.cpp` — to `EngineContext::get().imageManager()`. Replaced external loader iteration with narrow basename-probing helpers on the new image-service seam, kept `ImageManager::get()` as the bootstrap seam inside the subsystem, and added five `EngineContext` regressions covering image-service install, double-install rejection, clear, throw-when-missing, and `clearEngine()` cascade behavior.

### Pass 91 — Conservative role-pure helper migration (2026-04-19)

Continued T1.2 without widening any role interfaces. Migrated bounded AI-publication helpers in `game_combat.c`, `Module/Passage.cpp`, `ParticleHandler.cpp`, and `particle_collision.c` onto `IScriptable`-owned helper paths, and kept physics/damage helper selection on the existing `IPhysical` / `IDamageable` seams.

Extended targeted regression coverage with a `spawnDefencePing(...)` null-attacker check while keeping the existing gameplay-alert, module-update, and validator acceptance bar unchanged. The Script-owned `runtimeState(...)` bridge remained the only raw `ai_state_t` seam.

### Pass 92 — Engine-published particle service seam (2026-04-19)

Fourth T1.3 service-interface pass. Introduced `IParticleHandler` at `egolib/library/src/egolib/Entities/IParticleHandler.hpp` and made `ParticleHandler` implement it. Extended `EngineContext` with `installParticleHandler` / `clearParticleHandler` / `tryParticleHandler` / `particleHandler` accessors, with runtime publication owned by `GameEngine` around the existing `ParticleHandler::initialize` / `uninitialize` lifecycle. Particle-heavy test fixtures now install and clear the same service around direct singleton initialization so engine-less tests keep the same runtime contract.

Reworked particle iteration into an interface-owned RAII iterator bound to the installed handler rather than to `ParticleHandler::get()`, then migrated all non-`Entities/Particle_*` external particle callers across gameplay, graphics/render passes, module update/spawn/weather, physics/collision, debug UI, and object-side helpers to `EngineContext::get().particleHandler()`. `ParticleHandler::get()` remains only as the subsystem-local bootstrap seam inside `ParticleHandler.*`, `Entities/Particle_*`, `GameEngine` install/clear, and the real-handler test fixture setup.

Added five `EngineContext` regression tests covering particle-service install, double-install rejection, clear, throw-when-missing, and `clearEngine()` cascade behavior. Build, targeted test coverage, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 93 — Engine-published profile service cleanup (2026-04-19)

Completed the fifth T1.3 service-interface pass by finishing the already-landed `IProfileSystem` seam. Migrated the validator and the remaining non-lifecycle profile-loading callers in `ObjectProfile_load.cpp` from `ProfileSystem::get()` to `EngineContext::get().profileSystem()`, and removed the last dead `LOADED_PIP(...)` singleton-style helper from `ProfileSystem.hpp`.

Kept `ProfileSystem::get()` as a subsystem-local lifecycle seam inside `Profiles/`, specifically for teardown paths like `ObjectProfile` destruction that can run after `EngineContext::clearProfileSystem()`. Build, targeted test coverage, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 94 — Engine-routed log target cleanup (2026-04-19)

Completed the sixth T1.3 singleton/service cleanup without introducing a new interface type. Added `Log::tryActiveTarget()` / `activeTarget()` so runtime code prefers the installed `EngineContext` log target and otherwise falls back to the default logging target, while preserving `Log::get()` as the logging subsystem's bootstrap/lifecycle seam.

Migrated the remaining non-subsystem `Log::get()` callers in `Core/System.cpp`, `Math/Standard.hpp`, `fileutil.h`, and `Profiles/_AbstractProfileSystem.hpp` onto the new routing helper. Extended `EngineContext` regression coverage to pin the new helper's no-target, installed-target, and clear-target behavior. Build, targeted tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 95 — Script helper role sweep (2026-04-19)

Continued T1.2 inside the legacy script helper layer by migrating role-pure state, holder-alert, and invictus/damage-timer helpers in `script_functions_spawn.c` and `script_functions_state.c` onto `IScriptable` / `IDamageable`. Added the missing `IDamageable::setInvincible(...)` write-side seam so invictus helpers no longer need the concrete `Object` type.

Added `ScriptStateFunctions.cpp` coverage for state publication, child-state/content publication, holder-blocked target propagation, damage-timer publication, and invictus toggling. Kept `Script/script.c` untouched and preserved `Ego::Script::runtimeState(...)` as the only raw script-runtime bridge.

### Pass 96 — Engine-published config bootstrap seam (2026-04-19)

Started the `egoboo_config_t` Tier 1.3 seam by extending `EngineContext` with install/clear/try/throwing config accessors and publishing the active config from `Core::System`. Migrated bootstrap/lifecycle callers in `GameEngine`, `LoadingState`, and `MapEditorState` onto the installed config seam instead of reaching directly for `egoboo_config_t::get()`.

Kept non-game lightweight runtimes working by letting `ContentRuntimeBootstrap` conditionally publish the singleton-backed config only when no system-owned config is already installed, then moved its Gouraud-shading setup tweak onto `EngineContext::get().config()`. Added `EngineContext` regressions covering config install, double-install rejection, clear, throw-when-missing, and the requirement that `clearEngine()` must not clear the installed config.

### Pass 97 — Read-mostly config caller migration (2026-04-19)

Continued the `egoboo_config_t` Tier 1.3 seam by migrating the first read-mostly render/content caller cluster onto `EngineContext::get().config()`: SDL window/context setup, renderer-info subscriptions, deferred HD-texture selection, heightmap/debug render toggles, object-profile Gouraud-light fixup, module unlock developer gate, and fog/water module environment toggles.

Added focused regression coverage for installed-config reads and updates in `ConfigReadMostly.cpp`, including `RendererInfo` subscription behavior plus fog/water toggle checks. Kept `ContentParsers`, `ModuleLoadSmoke`, and the `test.mod` validator as the acceptance bar for the content-loading side of the pass.

### Pass 98 — Game-state / GUI read-mostly config migration (2026-04-19)

Continued the `egoboo_config_t` Tier 1.3 seam by migrating the remaining read-mostly game-state / GUI callers onto `EngineContext::get().config()`: `PlayingState`, `MapEditorState`, `MainMenuState`, and `MessageLog` no longer reach directly for `egoboo_config_t::get()` for debug gating, cursor/grab settings, status-bar visibility, or HUD message timing / limits.

Extended `ConfigReadMostly.cpp` with focused `MessageLog` coverage that pins installed-config reads and updates for HUD message duration and simultaneous-message limits without widening the runtime-facing GUI API.

### Pass 99 — Gameplay / render read-mostly config migration (2026-04-19)

Continued the `egoboo_config_t` Tier 1.3 seam by migrating the remaining non-subsystem gameplay, render, script, engine-event, and module-finalization readers onto `EngineContext::get().config()`: combat difficulty/feedback gates, respawn and HUD/debug checks, fog-script toggles, footfall FX gating, script-visible difficulty reads, console-event gating, and module slot-usage debug logging no longer reach directly for `egoboo_config_t::get()`.

Extended `ConfigReadMostly.cpp` and `ScriptStateFunctions.cpp` with focused coverage for installed-config difficulty reads and fog-script toggle behavior. Subsystem-local bootstrap/lifecycle seams and the write-heavy audio/video options screens remain deferred.

### Pass 100 — Write-heavy config seam closure (2026-04-19)

Finished the cross-cutting `egoboo_config_t` Tier 1.3 migration by routing the write-heavy audio/video options flow and SDL graphics fallback requirements through `EngineContext::get().config()`. Extracted the option mutation/label logic into internal `OptionsConfigActions` helpers so screen behavior stays testable without constructing the full UI stack, while preserving live side effects like audio-volume updates, channel allocation, fullscreen toggles, and setup-file saves.

Added focused regression coverage in `ConfigMutations.cpp` for installed-config audio/video option mutations, save callbacks, resolution selection, and SDL aliasing/fullscreen requirement reset-relax behavior. Remaining direct `egoboo_config_t::get()` callers are now confined to subsystem-local bootstrap/lifecycle or singleton-definition code (`AudioSystem`, `ImageManager`, `ParticleHandler`, `Core::System`, `ContentRuntimeBootstrap`, `egoboo_setup.c`).

### Pass 101 — Conservative target-role migration (2026-04-19)

Continued Tier 1.2 inside `script_functions_target.c` by routing the already role-pure target order/state/damage helpers through local `IScriptable` / `IDamageable` casts instead of ad hoc concrete `Object` use, while keeping `Script/script.c` and the Script-owned raw-runtime bridge untouched.

Also migrated the left/right-hand target setters onto `IInventoryHolder` and extended `ScriptTargetFunctions.cpp` with direct coverage pinning target-hand selection through the landed inventory-holder seam.

### Pass 102 — Conservative systems-role migration (2026-04-19)

Continued Tier 1.2 inside `script_functions_systems.c` by routing the bounded item-cost, ammo-restock, damage, kill, and heal helpers through local `IInventoryHolder` / `IDamageable` adapters instead of concrete `Object` access, while preserving the legacy owner-selection semantics in those helpers.

Added `ScriptSystemsFunctions.cpp` coverage for held/inventory item costing, target-hand plus actor-inventory ammo restock ordering, bounded damage/kill/heal behavior, and `[HEAL]` enchant termination after `HealTarget()`. Build, targeted script/gameplay tests, and the `test.mod` validator remained the acceptance bar.

### Pass 103 — Conservative spawn-helper role migration (2026-04-19)

Continued Tier 1.2 inside `script_functions_spawn.c` by routing the remaining role-pure child-state, child-content, and target-poof publication helpers through the existing `IScriptable` seam instead of ad hoc concrete `Object` handling. Kept `Script/script.c` and the Script-owned raw-runtime bridge untouched.

Extended `ScriptStateFunctions.cpp` with direct regressions for self-poof deferral, immediate non-self poof publication plus retargeting, and missing-child failure behavior for child-state/content helpers. Build, targeted script-state tests, and the `test.mod` validator remained the acceptance bar.

### Pass 104 — Conservative script-role resolver sweep (2026-04-19)

Continued Tier 1.2 by adding shared `ObjectRef` to role-resolver helpers in the split script-function infrastructure and migrating the next bounded target/state/spawn/systems helpers onto existing `IScriptable`, `IDamageable`, `IInventoryHolder`, and `IRenderable` seams. Kept `Script/script.c`, enchant-removal flows, mana/stat economics, and other non-role domains out of scope.

Extended `ScriptStateFunctions.cpp` and `ScriptTargetFunctions.cpp` with coverage for renderable invisibility, inventory-holder unarmed/rider targeting, and the resolver-backed role paths. Build, targeted script tests, and the `test.mod` validator remained the acceptance bar.

### Pass 105 — Conservative target-query role sweep (2026-04-19)

Continued Tier 1.2 inside `script_functions_target.c` by introducing the read-only `ITargetInfo` seam for target-query helpers and migrating the bounded target ID, team, status, mount/platform, visibility, timer, and weapon predicates off ad hoc concrete-`Object` reads. Kept quest, spell, facing-geometry, and raw script-runtime work out of scope.

Extended `ScriptTargetFunctions.cpp` with direct coverage for the new target-query role paths across inventory-equipped item resolution, team predicates, hurt/mana checks, mount detection, and weapon classification. Build, focused script-target tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 106 — Conservative held-item role sweep (2026-04-19)

Continued Tier 1.2 inside `script_functions_state.c` by routing the bounded held-item predicate block (`IfHoldingItemID`, ranged/melee/shield checks, and `IfHeldInLeftHand`) through local `ITargetInfo` / `IInventoryHolder` adapters instead of concrete `Object` hand access. Kept latch-selection semantics, ammo gating, and holder-slot behavior unchanged.

Extended `ScriptStateFunctions.cpp` with direct coverage for held-item IDSZ detection, ranged ammo gating with left-hand fallback, right-hand preference for melee/shield detection, and holder-left-hand detection through the landed inventory-holder seam. Build, focused script-state tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 107 — Conservative target-query role completion (2026-04-19)

Continued Tier 1.2 inside `script_functions_target.c` by extending the read-only query seams with target-player and physical-position accessors, then routing the remaining bounded target quest/owner/facing predicates through `ITargetInfo` and `IPhysical` instead of concrete `Object` reads. Kept team-leader/sissy lookups and all mutation-heavy script helpers out of scope.

Extended `ScriptTargetFunctions.cpp` with direct coverage for target quest lookup, owner detection, and facing predicates through the landed target-info and physical seams. Build, focused script-target tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 108 — Gameplay audio service migration (2026-04-19)

Continued Tier 1.3 by widening `IAudioSystem` with the runtime-facing spatial, looped-sound, hearing-distance, and per-frame update operations, then migrating gameplay/runtime callers onto the installed `EngineContext` audio service. Kept audio bootstrap, config upload/download, global sound loading, and profile-time sound asset loading on `AudioSystem::get()` as subsystem-local lifecycle seams.

Added focused regression coverage in `ScriptActionFunctions.cpp` for script-driven sound/music routing and `GameSessionContext::quitModule()` fade-all behavior through the installed audio seam. Build, targeted script-action tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 109 — Self-query state role sweep (2026-04-19)

Continued Tier 1.2 inside `script_functions_state.c` by extending the read-only `ITargetInfo` seam with self-query accessors for name-known, equipped, ammo, skin, water-tile, and stealth state, then routing the remaining bounded self-query predicates (`IfGrogged`, `IfDazed`, `IfArmorIs`, `IfNameIsKnown`, `IfKursed`, `IfOverWater`, `IfAmmoOut`, `IfEquipped`, `IfStealthed`) off concrete `Object` reads.

Extended `ObjectAccessors.cpp` and `ScriptStateFunctions.cpp` with direct coverage for the widened target-info seam and the migrated self-query predicates. Build, focused accessor/script-state tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 110 — Character-state role seam for script systems (2026-04-19)

Continued Tier 1.2 inside `script_functions_systems.c` by introducing `ICharacterState` for ammo, mana, kurse, timer, perk, attribute, and enchant-removal mutation, then routing the bounded mutable helpers (`CostTargetMana`, `IncreaseAmmo`, `CostAmmo`, `Give*ToTarget` stat helpers, `HealTarget`, `PumpTarget`, `GrogTarget`, `DazeTarget`, `DispelTargetEnchantID`, `KurseTarget`, `SetTargetAmmo`, and `GiveSkillToTarget`) through the landed role surfaces instead of ad hoc concrete-`Object` mutation. Kept quest, team, money, armor, enchant-construction, and `Script/script.c` raw-runtime work out of scope.

Added `ObjectAccessors.cpp` coverage for the new `ICharacterState` seam plus `ScriptSystemsFunctions.cpp` coverage for mana/ammo/kurse mutation and attribute/timer/enchant/perk helpers. Build, the focused `ScriptSystemsFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 111 — Conservative systems-role follow-on (2026-04-19)

Continued Tier 1.2 with a narrow `script_functions_systems.c` follow-on: routed `UnkurseTarget` through `ICharacterState`, kept `AddBlipAllEnemies` on conservative target-resolution helpers while avoiding direct team-object reads, and rewired `TargetDamageSelf` onto the existing target-query and damageable seams without changing attacker attribution.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for unkurse failure semantics, enemy-sense publication/reset, and target-attributed self-damage. Build, the focused script-systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 112 — Conservative inventory-state cleanup (2026-04-19)

Continued Tier 1.2 inside `script_functions_systems.c` by routing `UnkurseTargetInventory` through the landed `IInventoryHolder` and `ICharacterState` seams instead of direct concrete-`Object` handling. Kept the current mixed semantics intact: target held items are uncursed, while the pocket loop still operates on the actor inventory rather than retconning the helper's legacy behavior.

Extended `ScriptSystemsFunctions.cpp` with a focused regression pinning the preserved actor-pocket behavior alongside the target-held-item cleanup. Build, the focused script-systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 113 — Conservative systems-query role sweep (2026-04-19)

Continued Tier 1.2 inside `script_functions_systems.c` by widening the read-only `ITargetInfo` seam with team and grog/daze susceptibility queries, then routing `AddBlipAllEnemies`, `TargetDamageSelf`, `GrogTarget`, and `DazeTarget` off concrete target-object reads where those helpers only needed bounded query data plus the already-landed mutable role surfaces. Kept `IDamageable::damage(...)` attribution unchanged by preserving the existing shared attacker-object lookup while moving attacker-team reads onto the role seam.

Extended `ObjectAccessors.cpp` and the existing focused script-systems coverage to pin the widened target-info surface and the migrated enemy-sense, target-damage, and grog/daze helper behavior. Build, the focused script-systems / accessor test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 114 — Subsystem-local config seam cleanup (2026-04-19)

Finished the remaining Tier 1.3 config cleanup inside subsystem-local runtime code by replacing `egoboo_config_t::get()` reads in `AudioSystem.cpp` and `ImageManager.cpp` with the installed `EngineContext` config seam, while preserving `Core/System.cpp` and `ContentRuntimeBootstrap.cpp` as the explicit config-ownership edges. Moved the `ParticleHandler` constructor out of the header and onto the same installed-config seam so particle display-limit initialization no longer reaches for the singleton config directly.

Extended `ConfigReadMostly.cpp` with focused regressions covering installed-config image-loader registration and particle display-limit initialization. Build, focused config/engine-context tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 115 — Backstab last-attacker role closure (2026-04-19)

Continued Tier 1.2 inside `script_functions_state.c` by routing `scr_IfBackstabbed()` through the landed `IScriptable::getAILastAttacker()` seam instead of reading the raw attacker ref from `ai_state_t`. Kept the existing behind-angle, physical-damage, and terminated-attacker semantics unchanged while avoiding any new role widening.

Extended `ScriptStateFunctions.cpp` with focused regressions covering the scriptable last-attacker source of truth plus missing/terminated attacker failure behavior. Build, the focused script-state test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 116 — Inventory/ammo leaf closure for script systems (2026-04-19)

Continued Tier 1.2 inside `script_functions_systems.c` by localizing the remaining single-use ammo-restock helper and routing the bounded inventory/ammo leaf helpers (`CostTargetItemID`, `RestockTargetAmmoIDAll`, `RestockTargetAmmoIDFirst`) through the landed `IInventoryHolder` and `ICharacterState` seams instead of bouncing back through a shared raw-`ObjectRef` helper in `script_implementation.c`. Kept the legacy traversal and mixed semantics intact: target hands are still checked before target or actor inventory exactly as before, and the actor-pocket vs target-hand behavior remains unchanged.

Extended `ScriptSystemsFunctions.cpp` with a focused no-match regression for the localized restock path alongside the existing held-item, actor-inventory, and traversal-order coverage. Build, the focused script-systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 117 — Team and wallet role seams for script systems (2026-04-19)

Continued Tier 1.2 by introducing `ITeamMember` and `IWallet` as narrow policy seams on `Object`, then routing the bounded team/money helpers in `script_functions_systems.c` (`Join*Team`, `BecomeLeader`, team-XP helpers, and wallet transfer/drop/set helpers) through local role resolvers instead of direct concrete-`Object` calls. Kept module-owned leader lookup and whole-team XP policy on `GameModule::getTeamList()`, and left quest/profile/armor/class coupling out of scope.

Extended `ObjectAccessors.cpp` and `ScriptSystemsFunctions.cpp` with direct regression coverage for the new team/wallet role surfaces, including team-copy/join behavior, leader publication, team-wide XP routing, wallet transfer clamping, and drop/set semantics. Build, focused accessor/script-system tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 118 — Conservative target-helper role closure (2026-04-19)

Continued Tier 1.2 inside `script_functions_target.c` by routing the remaining bounded self-target selection helpers (`WhoeverAttacked`, `WhoeverBumped`, `WhoeverWasHit`, `LastItemUsed`, holder reads, and team-backed leader/sissy selection) through the landed `IScriptable`, `ITargetInfo`, and `IPhysical` seams instead of concrete-`Object` or raw AI-state reads. Kept quest/profile/armor-policy helpers and `Script/script.c` out of scope.

Extended `ScriptTargetFunctions.cpp` with focused regressions for scriptable self-state target selection and holder resolution through the target-info seam. Build, the focused script-target test slice, and the `test.mod` validator remained the acceptance bar for the pass.

---

## Theme 8 — Error-handling policy and first C++ `egolib_rv` retirement (2026-04-19)

### Pass 119 — Error-handling policy and camera render seam (2026-04-19)

Started Tier 1.4 by adding `doc/error-handling-policy.md` and documenting the forward policy: exceptions for exceptional or invalid-call paths, ordinary return values for expected boundary outcomes, and no new silent failure. Retired `egolib_rv` from the smallest public C++ seam by changing `CameraSystem::renderAll()` to `void` and treating a missing render callback as `idlib::argument_null_error` instead of a legacy status return.

Kept broader `gfx_rv` / tri-state graphics paths (`ObjectGraphics`, `CharacterMatrix`, and other C-era render helpers) out of scope. Acceptance for this pass is the build plus focused runtime render smoke, since `CameraSystem` construction still depends on the live graphics system and does not currently expose a lightweight unit-test seam.

### Pass 120 — Conservative appearance-profile caller sweep (2026-04-20)

Returned to Tier 1.2 by removing the remaining file-local `IAppearanceProfile` wrapper helpers from `script_functions_systems.c` and `script_functions_target.c`, routing the bounded armor/appearance helpers through the already-landed appearance seam without widening `Object` or touching `Script/script.c`.

Preserved current script-visible behavior exactly, including the legacy actor-profile semantics in `scr_IfTargetIsDressedUp()` and `scr_IfTargetIsASpell()`. Acceptance for this pass is the build, the focused `ScriptSystemsFunctions` / `ScriptTargetFunctions` coverage for armor and appearance helpers, and the `test.mod` validator smoke.

### Pass 121 — Enchantment lifecycle role seam (2026-04-20)

Continued Tier 1.2 by introducing `IEnchantable` as a narrow `Object` role for enchant application, observation, and removal, then routing the bounded enchant lifecycle helpers in `script_functions_systems.c` (`EnchantTarget`, `EnchantChild`, `UndoEnchant`, `SetEnchantBoostValues`, `DisenchantTarget`, `DisenchantAll`) through that seam instead of direct concrete-`Object` calls.

Repaired the stale enchant accessor fixture to use a valid enchant-backed object and extended accessor/script-system coverage to pin add/disenchant publication, last-spawned tracking, boost-value mutation, and target/child/all disenchant semantics. Build, focused enchant tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 122 — Module-owned team helper closure (2026-04-20)

Continued Tier 1.2 by adding narrow `GameModule` team helpers for leader lookup, call-for-help lookup, and team-XP publication, then routing the remaining script-helper `activeModule().getTeamList()` reads in `script_functions_systems.c` and `script_functions_target.c` through that seam instead of direct module-team indexing.

Extended focused script-system and script-target coverage to pin leader-alive behavior, team-owned leader/sissy ref lookup, and `Team::TEAM_GOOD` XP publication through the new module seam. Build, focused script tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 123 — Residual target/state role cleanup (2026-04-20)

Continued Tier 1.2 by routing the remaining actor-side query reads in `script_functions_target.c` and `script_functions_state.c` through the landed `IScriptable`, `ITargetInfo`, `IPhysical`, and `IInventoryHolder` seams instead of concrete `Object` reads. Unified the target-facing predicates around a shared read-only facing helper and kept the existing missing-ref, missing-holder, and missing-leader failure behavior intact.

Extended focused script-target and script-state coverage with deterministic failure cases for missing attacker refs, missing holders, missing holder-block alerts, and non-left-hand holder attachment. Build, the full `ScriptTargetFunctionsFixture` / `ScriptStateFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 124 — Movement-control role seam for script helpers (2026-04-20)

Continued Tier 1.2 by introducing `IMovementControl` as a narrow write-side `Object` role for turn mode, latch publication, movement velocity, teleport, bump sizing, reload/shadow mutation, and fly-height publication. Migrated the motion-pure helpers in `script_functions_movement.c` onto `IMovementControl` plus the existing read-only `IPhysical` seam instead of direct concrete-`Object` access, while leaving waypoint storage, `ai_state_t` speed scaling, and animation-frame control out of scope.

Extended `ObjectAccessors.cpp` and added focused `ScriptMovementFunctions.cpp` coverage for movement-role parity, turn-mode and latch publication, teleport success/missing-target failure, target-velocity clamp behavior, reload timers, and shadow/fly-height mutation. Build, the focused movement/accessor tests, and the `test.mod` validator are the acceptance bar for this pass.

### Pass 125 — Movement animation-frame helper closure (2026-04-20)

Continued Tier 1.2 with the smallest remaining `script_functions_movement.c` follow-on by extracting the encoded-frame behavior behind a file-local helper and routing `scr_SetFrame()` through that seam instead of carrying inline animation bookkeeping. Kept `IMovementControl`, waypoint storage, `ai_state_t` speed scaling, and broader action-helper cleanup out of scope.

Extended `ScriptMovementFunctions.cpp` with focused characterization coverage for the encoded `ACTION_DA` frame publication path, pinning the published target frame plus interpolation-step bookkeeping. Build, the focused movement test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 126 — Attachment-handle alias seam closure (2026-04-20)

Closed the remaining holder/platform alias-style `Object` handle returns by adding `getAttachedPlatformRef()`, removing the public `getHolder()` / `getAttachedPlatform()` shared-pointer accessors, and migrating the bounded matrix, physics, render, and lifecycle callers onto ref-based lookups instead of direct handle returns.

Extended `ObjectAccessors.cpp` with focused coverage for the new platform-ref accessor plus missing holder/platform refs staying null-like through runtime lookups. Build, the focused accessor test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 127 — Conservative action-helper role migration (2026-04-20)

Continued Tier 1.2 inside `script_functions_action.c` by introducing `IAnimationControl` for bounded action resolution and animation control, extending the existing team/target query roles with the small helper surface that action scripts still needed, and routing the conservative action/control helpers off concrete `Object` use. Kept flash/color/light/sparkle/name-known mutation, billboard/UI, and broader action-file cleanup out of scope.

Extended `ObjectAccessors.cpp` and `ScriptActionFunctions.cpp` with focused coverage for animation-role parity, team call-for-help publication, holder/attachment role queries, action-start success/blocking behavior, dead-target rejection, interpolation reset, hand-band action correction, and direct-player versus holder-player charge-bar routing. Build, focused action/accessor tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 128 — Action visual/identity role sweep (2026-04-20)

Continued Tier 1.2 inside `script_functions_action.c` by introducing `IVisualControl` for bounded shift/light/alpha/name/ammo/sparkle mutation, then routing the remaining non-UI visual/identity helpers off direct concrete-`Object` writes. Kept billboard, screenshot, charge-bar, and flash-helper seams out of scope.

Extended `ObjectAccessors.cpp` and `ScriptActionFunctions.cpp` with focused coverage for visual-role parity plus script-visible shift clamping, name/ammo flag publication, sparkle semantics, and the `test.mod` validator smoke.

### Pass 129 — Action presentation helper follow-on (2026-04-20)

Continued Tier 1.2 inside `script_functions_action.c` by routing the remaining bounded screenshot and billboard helpers through file-local presentation helpers, replacing the direct `engine().getUIManager()` dereference with the installed `EngineContext` UI seam and isolating billboard creation behind a local helper without introducing a broader graphics-service pass.

Extended `ScriptActionFunctions.cpp` with focused coverage for missing-UI screenshot failure and invalid-message billboard rejection. Build, focused action tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 130 — Residual systems-role sweep (2026-04-20)

Continued Tier 1.2 inside `script_functions_systems.c` by centralizing the remaining role-pure target/self/owner object resolution behind file-local helpers, then migrating the bounded damage/heal/enchant/stat helpers off repeated ad hoc `ObjectRef` lookups while preserving the legacy success/failure semantics that still hang off `SCRIPT_FUNCTION_BEGIN()`.

Extended `ScriptSystemsFunctions.cpp` with a held-weapon `KillTarget` regression that pins the supported kill path when the script self is a wielded item. Build, focused systems tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 131 — Item-query role seam (2026-04-20)

Continued Tier 1.2 by introducing read-only `IItemInfo` plus additive `IInventoryHolder` ref-based inventory accessors, then routing the bounded held-item and restock/item-cost helpers in `script_functions_state.c` and `script_functions_systems.c` off concrete inventory-item `Object` handles for classification checks. Kept attachment lifecycle, morph/class policy, quest/player flows, and combat/enchant shared-ownership attribution out of scope.

Extended `ObjectAccessors.cpp`, `ScriptStateFunctions.cpp`, and `ScriptSystemsFunctions.cpp` with focused coverage for item-role parity, inventory ref accessors, missing-held-item failure behavior, and no-match restock behavior. Build, the focused accessor/state/systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 132 — Backstab perk-query role closure (2026-04-20)

Continued Tier 1.2 by widening `ICharacterState` with a read-only `hasPerk(...)` query and routing `scr_IfBackstabbed()` in `script_functions_state.c` through `IScriptable`, `IInventoryHolder`, and `ICharacterState` instead of a raw attacker `Object` lookup. Kept the existing missing/terminated attacker, behind-angle, and physical-damage gating unchanged.

Extended `ObjectAccessors.cpp` to pin the new perk-query role surface while keeping the existing focused `ScriptStateFunctions.cpp` backstab regressions as the behavior lock. Build, the focused `ObjectAccessors` / `ScriptStateFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 133 — Spawn lifecycle role seam (2026-04-20)

Continued Tier 1.2 inside `script_functions_spawn.c` by introducing `ILifecycleControl` for bounded respawn, detach, drop, crush/item, damage-threshold, and stealth control, then routing the lifecycle-pure spawn helpers off direct concrete-`Object` use. Added `Object::respawnInPlace()` for the in-place target-respawn path and kept the broader morph/size and child post-spawn initialization flows out of scope.

Also rewired `scr_IdentifyTarget()` onto the landed `ICharacterState`, `ITargetInfo`, and `IVisualControl` seams instead of direct target-object access, preserving the legacy ammo-known, name-known, and usage-known semantics. Extended `ObjectAccessors.cpp` and `ScriptStateFunctions.cpp` with focused lifecycle-role coverage for respawn-in-place, key/item drops, self/target respawn, target identification, bounded mutation helpers, and stealth entry/exit behavior. Build, the focused `ObjectAccessors` / `ScriptStateFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 134 — Spawn morph/resize role follow-on (2026-04-20)

Continued Tier 1.2 inside `script_functions_spawn.c` by introducing `IMorphControl` for bounded base-model, polymorph, fat-target, and resize-timer control, then routing `scr_MorphToTarget()` and `scr_SetTargetSize()` off direct concrete-`Object` mutation while preserving their legacy missing-target and resize semantics.

Extended `ObjectAccessors.cpp` and `ScriptStateFunctions.cpp` with focused morph/resize coverage for role-surface parity, target-driven morphing, target-size scaling, and missing-target failure behavior. Build, the focused `ObjectAccessors` / `ScriptStateFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 135 — Morph base-model publication seam (2026-04-20)

Continued Tier 1.2 inside `script_functions_systems.c` by widening `IMorphControl` with base-model publication and routing `scr_ChangeTargetClass()` through the morph role instead of direct concrete-`Object` mutation. Preserved the existing loaded-profile gate plus the permanent-export semantics of changing both current and base model.

Extended `ObjectAccessors.cpp` and `ScriptSystemsFunctions.cpp` with focused coverage for morph-role base-model publication and `ChangeTargetClass` success/failure behavior. Build, the focused accessor/systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 136 — Spell conversion seam closure (2026-04-20)

Continued Tier 1.2 inside `script_functions_systems.c` by routing `scr_BecomeSpell()` and `scr_BecomeSpellbook()` through the landed `IEnchantable`, `IMorphControl`, and `IAnimationControl` seams for their mutation paths, while keeping the current profile/spell-effect reads on the existing local compatibility variables instead of widening another public role.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for spell conversion: enchant cleanup, morph-to-spell / morph-to-spellbook publication, script-state reset, preserved base-model behavior, and dropped-animation publication for the spellbook conversion path. Build, the focused systems test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 137 — Systems helper consolidation follow-on (2026-04-20)

Continued Tier 1.2 inside `script_functions_systems.c` by centralizing the remaining quest-log, damage/heal source-resolution, appearance-payment, and skill-mapping helper flows behind file-local helpers, while keeping all public role surfaces unchanged and leaving broader handle-seam closure out of scope. Migrated `scr_AddQuest*`, `scr_BeatQuestAllPlayers`, `scr_SetQuestLevel`, `scr_GiveExperienceToTarget`, `scr_DamageTarget`, `scr_HealSelf`, `scr_HealTarget`, `scr_GiveLifeToTarget`, `scr_TargetPayForArmor`, `scr_TargetDamageSelf`, and `scr_GiveSkillToTarget` onto those local helpers.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for all-player quest beating/progress publication, resolved-target XP grants, and the preserved legacy no-op/success behavior for unknown skill IDSZ mappings. Build, the full `ScriptSystemsFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar for the pass.

## Theme 9 — Particle service follow-on (2026-04-20)

### Pass 138 — Particle runtime service cleanup (2026-04-20)

Continued Tier 1.3 by removing the remaining runtime-facing `ParticleHandler::get()` calls from `Particle_core.cpp`, `Particle_spawn.cpp`, `Particle_update.cpp`, and `Particle_combat.cpp`, routing parent-particle lookup and continuous/end-spawn behavior through the installed `EngineContext` particle service instead. Kept `ParticleHandler::get()` as the subsystem-local bootstrap/lifecycle seam in `ParticleHandler.*` and `GameEngine` install/clear ownership.

Extended `ModuleUpdate.cpp` with focused regressions for parent-particle owner fallback during initialization, recursive owner resolution through the installed particle service, and end-spawn routing through the installed handler. Build, targeted `ModuleUpdate` / `EngineContext` coverage, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 139 — Team-target helper closure (2026-04-20)

Continued Tier 1.2 by closing the remaining direct team/leader target-selection pocket in `script_functions_target.c` and the matching leader-alive check in `script_functions_systems.c`, routing all four helpers through the existing `GameModule::getTeamLeaderRef()` / `getTeamCallerForHelpRef()` seam behind file-local resolver helpers. Preserved the legacy failure behavior: invalid teams, missing leaders/callers, and missing leader targets still fail without mutating `self.target`.

Extended `ScriptTargetFunctions.cpp` and `ScriptSystemsFunctions.cpp` with focused invalid-team regressions on top of the existing leader/caller fixture coverage. Build, focused target/systems tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 140 — Spawn lifecycle tail closure (2026-04-20)

Continued Tier 1.2 inside `script_functions_spawn.c` by widening `IMovementControl` and `ILifecycleControl` just enough to cover the remaining child post-spawn mutation pocket, then routing `scr_SpawnCharacter()` and the held-item drop rider-buck flow off concrete write-side `Object` access for velocity, jump/move, kursed inheritance, and dismount publication. Kept `spawnObject()`, `hasSafePosition()`, `requestTerminate()`, and the local `Object::SIZETIME` compatibility use unchanged so the pass stayed bounded to the spawn-tail seam.

Extended `ObjectAccessors.cpp` and `ScriptStateFunctions.cpp` with focused coverage for the widened movement/lifecycle role surfaces, successful child-state publication, and the preserved unsafe-position spawn behavior where the child stays invalid after termination is requested. Build, the focused accessor/state test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 141 — Exact-position spawn role closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_spawn.c` by routing `scr_SpawnCharacterXYZ()` and `scr_SpawnExactCharacterXYZ()` through the same role-based child-publication helper used by `scr_SpawnCharacter()`, covering kurse inheritance, script-owned spawn-state publication, and dismount publication without widening any public role surfaces. Kept exact-position spawn semantics unchanged: no `hasSafePosition()` gate and no injected launch velocity on the XYZ variants.

Extended `ScriptStateFunctions.cpp` with focused regressions for both exact-position spawn variants, pinning requested profile/facing/position plus the shared child-state publication contract. Build, the focused `ScriptStateFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 142 — Narrow systems role sweep follow-on (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by widening `ICharacterState` for XP grants, adding the missing terminate hook on `ILifecycleControl`, and routing the remaining bounded target-inventory helpers (`GiveExperienceToTarget`, `CostTargetItemID`, `UnkurseTargetInventory`) through the landed role seams instead of direct concrete-object mutation. Kept the current gameplay semantics intact, including the legacy actor-pocket behavior in `UnkurseTargetInventory` and the existing self-inventory removal path in `CostTargetItemID`.

Extended `ObjectAccessors.cpp` and `ScriptSystemsFunctions.cpp` with focused regressions for `ICharacterState` XP parity, lifecycle termination through the role seam, role-surface XP grants to targets, inventory-item poof coverage, and the preserved unkurse actor-pocket quirk. Build, targeted accessor/systems tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 143 — Residual enchant-all role closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by routing `scr_DisenchantAll()` through the landed `IEnchantable` seam instead of direct concrete-`Object` mutation, while preserving the same full object sweep, null-skip guard, and unconditional success behavior. Kept the pass intentionally narrow: no role-surface widening, no changes to the target/child enchant helpers, and no changes to the remaining damage-attribution or profile-coupling pockets.

Extended `ScriptSystemsFunctions.cpp` by tightening the mixed-object global disenchant regression to assert that the previously enchanted object's enchantment is terminated after the sweep. Build, the focused `ScriptSystemsFunctions` test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 144 — Residual damage-attribution role closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by consolidating the remaining self-damage, kill-source, and retaliation-source attribution flows behind file-local helpers, then routing `scr_DamageTarget()`, `scr_KillTarget()`, and `scr_TargetDamageSelf()` through those helpers without widening any public role surfaces. Preserved the current combat semantics: damage type and team attribution stay unchanged, kill attribution still prefers a non-mount holder via `resolvedKillSourceRef(...)`, and the scripts still return false when the resolved source or target cannot be obtained.

Extended `ScriptSystemsFunctions.cpp` with a focused missing-target retaliation regression alongside the existing damage/kill attribution coverage. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 145 — Systems self-profile compatibility quarantine (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by consolidating the remaining self-profile compatibility reads behind one file-local resolver that exposes self profile ref, self enchant ref, spell-effect skin, and base-model/current-profile comparison state. Routed `scr_BecomeSpellbook()`, `scr_EnchantTarget()`, `scr_EnchantChild()`, and `scr_IfCharacterWasABook()` through that helper while keeping mutation on the landed `IEnchantable`, `IMorphControl`, and `IAnimationControl` seams and preserving the existing false-return behavior when owner or spawner resolution fails.

Extended `ScriptSystemsFunctions.cpp` with a focused regression pinning the `IfCharacterWasABook()` base-model/current-profile semantics. Explicitly deferred the remaining message/logging coupling in `scr_FollowLink()` and `scr_EnableListenSkill()`, passage/mesh/module helper follow-ons, and any `Script/script.c` changes. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 146 — FollowLink and deprecation-message quarantine (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by isolating the remaining `scr_FollowLink()` and `scr_EnableListenSkill()` compatibility behavior behind file-local helpers that consume the already-resolved script actor/profile context instead of re-resolving the actor from `ai_state_t`. Kept the current follow-link and deprecation contract unchanged: invalid message IDs still fail quietly, failed module links still publish `"That's too scary for <name>"`, and `EnableListenSkill` remains a logged no-op.

Preserved the existing focused `ScriptSystemsFunctions.cpp` coverage for successful follow-link resolution, scary-message publication, no-active-playing-state fallback, and deprecated `EnableListenSkill` logging. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 147 — Passage helper quarantine follow-on (2026-04-21)

Continued Tier 1.2 by isolating the remaining direct `activeModule().getPassageByID(...)` pockets behind file-local helpers in `script_functions_systems.c`, `script_functions_action.c`, and `script_functions_target.c`. Routed the bounded passage open/close/open-state, flash/shop/music, and passage-occupant target-selection helpers through those local seams while preserving the existing silent-failure and `returncode` behavior for invalid passage IDs.

Extended `ScriptSystemsFunctions.cpp`, `ScriptActionFunctions.cpp`, and `ScriptTargetFunctions.cpp` with focused regressions covering passage open/close parity, shop-owner publication, missing-passage success/failure quirks, music assignment/clearing, and passage-based target selection. Build, the focused script-function test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 148 — Script runtime team/order quarantine (2026-04-21)

Continued Tier 1.2 inside `Script/script.c` by isolating the remaining leader-variable and team-order runtime pocket behind file-local helpers that consume existing `ITargetInfo`, `IScriptable`, and `GameModule::getTeamLeaderRef()` seams instead of direct team-list indexing and concrete team checks. Preserved the runtime contract: missing leaders still fall back to self for operand resolution, invalid caller refs fail quietly, same-team order publication still includes the caller, and terminated listeners are skipped.

Extended `ScriptRuntime.cpp` with focused regressions for resolved leader-variable reads, invalid-caller quiet no-op behavior, and terminated same-team listeners. Build, the focused `ScriptRuntime` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 149 — Residual systems profile/armor policy quarantine (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by tightening the remaining self-profile and target-armor compatibility helpers into smaller file-local policy structs and resolvers. Routed `scr_GetTargetArmorPrice()`, `scr_ChangeTargetArmor()`, `scr_TargetPayForArmor()`, `scr_IfCharacterWasABook()`, `scr_BecomeSpellbook()`, `scr_EnchantTarget()`, `scr_EnchantChild()`, `scr_FollowLink()`, and `scr_EnableListenSkill()` through the renamed helper layer without widening any public `Object` role surfaces or changing the existing success/failure semantics.

Kept the focused `ScriptSystemsFunctions.cpp` armor, spellbook, enchant, follow-link, and deprecation-message regressions as the behavior lock. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 150 — Systems module/UI helper quarantine (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by isolating the remaining module-environment, playing-state UI, and end-text compatibility helpers behind file-local wrappers instead of scattering direct `activeModule()`, `GameSessionContext::fog()`, `PlayingState`, and `g_endText` calls across the script opcodes. Routed `scr_Set/GetWaterLevel()`, fog top/bottom/color helpers, tile get/set, minimap/status-monitor helpers, module beat/export/pits helpers, good-team XP publication, and end-text clear/append through those local seams without widening any public `Object` role surfaces or changing the current success/failure semantics.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for water/fog/module-flag publication and end-text clear/append behavior. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 151 — Residual systems quest/tile/pit helper quarantine (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by isolating the remaining quest-player iteration, module-IDSZ append, actor-tile mutation, enemy-sense publication, and pit-fall compatibility pockets behind file-local helpers instead of leaving direct `activeModule()` and `GameSessionContext` calls spread across the opcode bodies. Routed `updatePlayerQuestLogs(...)`, `scr_AddIDSZ()`, `scr_ChangeTile()`, `scr_AddBlipAllEnemies()`, and `scr_PitsFall()` through those local seams without widening any public `Object` role surfaces or changing the current quiet-failure and branch behavior.

Kept the focused `ScriptSystemsFunctions.cpp` regressions for tile helper parity, pit teleport/kill behavior, quest helper player resolution, all-player quest updates, enemy-sense publication/reset, nearby module-environment coverage, and the `test.mod` validator as the acceptance bar for the pass.

### Pass 152 — Script runtime helper quarantine (2026-04-21)

Continued Tier 1.2 inside `Script/script.c` by isolating the remaining held-item, operand-context, leader-resolution, and order-recipient pockets behind file-local helpers that consume the existing `IInventoryHolder`, `ITargetInfo`, `IScriptable`, `IPhysical`, `ICharacterState`, and `IWallet` seams instead of scattering direct casts and mixed lookup logic across the runtime file. Kept `Ego::Script::runtimeState(...)`, `scr_run_chr_script(...)`, and the current waypoint / invisible-target / order-publication behavior unchanged.

Extended `ScriptRuntime.cpp` with a focused regression covering mount rider-velocity publication through the left-hand held-item seam. Build, the focused `ScriptRuntime` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 153 — Residual `Object` handle seam closure (2026-04-21)

Continued Tier 1.2 by replacing the remaining public alias-style `Object` shared-handle helpers with ref-based seams: attachment, mount, and visibility checks now consume `ObjectRef`, while public `toSharedPointer()` and `isWieldingItemIDSZ(...)` escape hatches are gone. Kept ownership-bearing damage/heal/enchant APIs unchanged by resolving self handles locally inside the few implementation sites that still need concrete shared ownership.

Updated the matching gameplay/runtime/test callers (`Script/script.c`, spawn/inventory/collision/shop/targeting paths, and focused script/gameplay regressions) to use refs or bounded bool queries instead of public shared-object aliases. Build, focused script/gameplay tests, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 154 — Systems ownership-pocket cleanup (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by collapsing the last ownership-bearing damage/heal/kill/enchant compatibility paths behind file-local resolver structs instead of repeating ad hoc `tryObjectShared(...)` lookups in opcode bodies. Kept the existing `IDamageable` and `IEnchantable` ownership-bearing signatures intact, and routed `DamageTarget`, `KillTarget`, `HealSelf`, `GiveLifeToTarget`, `HealTarget`, `TargetDamageSelf`, `EnchantTarget`, and `EnchantChild` through the new local helper layer without widening any public role seams.

Also replaced the mixed null-check/object-handle sweep in `DisenchantAll` with a small local object-iteration helper that delegates directly through the landed `IEnchantable` role. Kept the focused `ScriptSystemsFunctions` coverage for damage/kill, heal cleanup, retaliation attribution, enchant owner/spawner failure, and mixed-object disenchant behavior plus the `test.mod` validator as the acceptance bar for the pass.

### Pass 155 — Script runtime object-handle quarantine (2026-04-21)

Continued Tier 1.2 inside `Script/script.c` by isolating the remaining direct `objectHandler()` / `shared_ptr<Object>` runtime pockets behind file-local helpers for object-ref iteration, spawn-object resolution, and bump-target validation. Routed `issue_order(...)`, `issue_special_order(...)`, `ai_state_t::set_bumplast(...)`, and `ai_state_t::spawn(...)` through those helpers without widening any public role seams or changing order, spawn, or bump semantics.

Extended `ScriptRuntime.cpp` with focused regressions for invalid spawn refs and invalid bump targets on top of the existing order-publication, valid-spawn, and bump-throttling coverage. Build, the focused `ScriptRuntime` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 156 — Script runtime liveness-helper closure (2026-04-21)

Continued Tier 1.2 inside `Script/script.c` by replacing the remaining direct runtime `objectHandler().exists(...)` checks with a file-local ref-liveness helper and routing order publication through a ref-yielding iteration helper instead of open-coded object-handle reads at each call site. Kept `ai_state_t::get_wp(...)`, `ai_state_t::ensure_wp(...)`, `issue_order(...)`, and `issue_special_order(...)` behavior unchanged while narrowing the runtime TU's remaining object-handler coupling.

Kept the existing focused `ScriptRuntime.cpp` coverage for invalid actor refs, waypoint velocity publication, same-team order publication, special-IDSZ order filtering, invalid spawn refs, and invalid bump targets as the behavior lock. Build, the focused `ScriptRuntime` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 157 — Script systems object-handler seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by isolating the remaining direct playing-state/minimap object-handle adapters behind file-local helpers and replacing the residual `DisenchantAll` concrete-object sweep with a ref-yielding iterator plus `IEnchantable` role lookup. Kept the ownership-bearing damage/heal/enchant APIs unchanged and preserved the current active-playing-state behavior while making the show-map and status-monitor helpers safe no-ops when no playing state is installed.

Extended `ScriptSystemsFunctions.cpp` with focused coverage for `ShowMap`/`ShowYouAreHere` active-state publication, no-active-playing-state no-op behavior for the UI adapters, and `AddStat` status-monitor publication through the new helper layer. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 158 — Target-selection object-handler seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_target.c` by routing the remaining setter-style target-selection helpers through the shared file-local resolved-target helper instead of open-coded `objectHandler().exists(...)` checks. Kept passage-target helpers, query-only predicates, and all other script translation units unchanged so the pass stayed focused on quiet-failure target publication semantics.

Extended `ScriptTargetFunctions.cpp` with focused regressions for owner-target resolution, self-ref rejection in `SetTargetToLastItemUsed`, no-match nearest/distant/weapon search behavior, and invalid-target failure for `SetTargetToLowestTarget`. Build, the focused `ScriptTargetFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 159 — Target-query liveness seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_target.c` by replacing the last direct target-query `objectHandler().exists(...)` checks with one file-local liveness helper shared by target publication and equipped-item lookup. Routed `trySetResolvedTarget(...)` and `scr_IfTargetHasItemIDEquipped()` through that helper without widening any public `Object` or role surfaces and preserved the current quiet-failure semantics for invalid refs.

Extended `ScriptTargetFunctions.cpp` with a focused regression for missing equipped-item matches staying false without mutating the current target. Build, the focused `ScriptTargetFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 160 — Script runtime iterator seam closure (2026-04-21)

Continued Tier 1.2 inside `Script/script.c` by replacing the last direct runtime `objectHandler().iterator()` dependency in `forEachLiveRuntimeObjectRef(...)` with the session-owned object-handler seam. Kept the helper ref-yielding only, so `issue_order(...)` and `issue_special_order(...)` still publish through file-local recipient resolution without reintroducing shared-object exposure or widening any public role surface.

Kept the existing focused `ScriptRuntime.cpp` coverage for same-team order publication, special-IDSZ filtering, invalid spawn refs, and invalid bump targets as the behavior lock. Build, the focused `ScriptRuntime` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 161 — Script spawn seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_spawn.c` by adding file-local ref-liveness, owner-resolution, spawn publication, particle-owner, and respawn-toggle helpers, then routing the remaining cleanup, particle, character-spawn, attach, and respawn opcodes through that layer instead of open-coded `objectHandler()` and module calls. Kept the current spawn and attach semantics intact: invalid refs still fail quietly, unsafe spawned children still terminate without publishing `self.child`, holder and self owner fallbacks remain unchanged, and ownership-bearing spawn/particle APIs still use the existing shared-ownership signatures internally where required.

Extended `ScriptStateFunctions.cpp` with focused regressions for cleanup listener iteration, particle owner fallback and attachment placement, attached-character inventory and grip behavior, and module respawn toggling. Build, the focused `ScriptStateFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 162 — Script state liveness seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_state.c` by adding one file-local ref-liveness helper and routing the remaining holder/target publication and held-slot predicates through it instead of open-coded `objectHandler().exists(...)` checks. Kept `IfSitting`, `IfHolderBlocked`, and `IfUnarmed` behavior unchanged: invalid or terminated refs still fail quietly, `self.target` stays untouched on failure, and terminated held-item refs still count as unarmed.

Extended `ScriptStateFunctions.cpp` with a focused regression for terminated held-item refs on top of the existing `IfHolderBlocked` and `IfUnarmed` coverage. Build, the focused `ScriptStateFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 163 — Residual action/state seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_action.c` and `script_functions_state.c` by isolating the remaining direct live-object sweep, holder-liveness, and module-IDSZ lookup pockets behind file-local helpers instead of leaving open-coded `objectHandler()` and module-name access in the opcode bodies. Routed `scr_MakeSimilarNamesKnown()`, `scr_CorrectActionForHand()`, and `scr_IfModuleHasIDSZ()` through those local seams while preserving the existing action-band and quiet-failure behavior and correcting the module predicate to consult the message-selected module name as documented.

Extended `ScriptActionFunctions.cpp` and `ScriptStateFunctions.cpp` with focused regressions for missing-holder no-op action correction, preserved non-match name visibility behavior, valid `test.mod` IDSZ lookup, and invalid module/message rejection. Build, the focused script action/state test slices, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 164 — Residual systems self-seam closure (2026-04-21)

Continued Tier 1.2 inside `script_functions_systems.c` by localizing the remaining concrete-self opcode writes and self-ref publication behind file-local helpers instead of reading `pchr` directly in the opcode bodies. Routed `scr_SetDamageType()`, `scr_Equip()`, `scr_ShowBlipXY()`, and `scr_PumpTarget()` through the new self helpers while preserving the current UI no-op behavior, target-liveness gating, and mana-source semantics.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for the preserved map/UI adapter behavior, self damage-type and equipped-state mutation, and `PumpTarget` preserving self-ref mana-source attribution. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 165 — Desired-velocity movement seam closure (2026-04-21)

Continued Tier 1.2 by widening `IMovementControl` with desired-velocity read/write accessors, migrating the remaining non-`Object` callers in `Script/script.c`, `Logic/Player.cpp`, and `Graphics/ObjectGraphics.cpp` onto that role, and moving `Object`'s desired-velocity override behind the role-only surface instead of leaving it as a public concrete seam.

Extended `ObjectAccessors.cpp` and `ScriptRuntime.cpp` with movement-role desired-velocity coverage for clamping, waypoint publication, mount rider propagation, and invalid-actor no-op behavior. Build, the focused accessor/runtime slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 166 — Systems self-compatibility quarantine (2026-04-22)

Continued Tier 1.2 inside `script_functions_systems.c` by replacing the remaining scattered self-object/self-profile/self-wallet/team/end-text compatibility helpers with one file-local self-compatibility context used by the self-only opcode cluster. Routed spellbook/follow-link, self team/money/armor writes, and end-text compatibility paths through that shared helper layer without widening any public role surfaces or changing legacy success/failure behavior.

Extended `ScriptSystemsFunctions.cpp` with a focused invalid-self regression on top of the existing follow-link, armor, team, wallet, end-text, and spellbook coverage. Build, the focused `ScriptSystemsFunctions` slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 167 — Spawn compatibility quarantine (2026-04-22)

Continued Tier 1.2 inside `script_functions_spawn.c` by moving the remaining concrete-`Object` spawn, particle, poof, and attached-child compatibility pockets behind one file-local helper layer (`SpawnSelfContext`, particle/self-handle helpers, and attached-child placement helpers). Kept public seams unchanged and preserved the existing behavior: safe-position gating still only applies to `scr_SpawnCharacter()`, exact-position spawns still publish child owner/passage/kurse state, and failed attached spawns still leave `self.child` untouched without widening any role interfaces.

Extended `ScriptStateFunctions.cpp` with focused regressions for terminated-self poof failure plus the inventory-full and occupied-grip attached-spawn failure paths, while keeping the existing spawn/particle success coverage and the `test.mod` validator as the acceptance bar.

### Pass 168 — CharacterMatrix error-contract cleanup (2026-04-22)

Started Tier 1.4 by retiring `egolib_rv` from the matrix-update seam in `CharacterMatrix`: `chr_update_matrix(...)` now takes `Object&` and returns `bool`, while the internal cache-dirty check became a ref-based boolean helper instead of a legacy status-code API. Kept behavior unchanged for recursive holder updates, matrix invalidation, collision-size follow-on work, and the pointer-based query helpers.

Extended `ObjectAccessors.cpp` with focused regressions for stale-vs-current matrix publication and pointer-helper compatibility, then kept build, the focused accessor/module-spawn slice, and the `test.mod` validator as the acceptance bar.

### Pass 169 — Attachment bool-contract residue cleanup (2026-04-22)

Continued Tier 1.4 by removing the last legacy status-code residue inside `ObjectPhysics::attachToObject(...)`, keeping the public `bool` contract unchanged and preserving the existing early-false attachment guards plus success-path mutation order.

Extended `ObjectAccessors.cpp` with focused regressions that pin failed self-attach and already-held attach attempts as no-op state, then kept the attachment-heavy gameplay/script slices and the `test.mod` validator as the acceptance bar.

### Pass 170 — Import-list bool/count contract cleanup (2026-04-22)

Continued Tier 1.4 by retiring `egolib_rv` from the import-list copy/build seam in `game_export.c`: `game_copy_imports(...)` now takes `import_list_t&` and returns `bool`, while `import_list_t::from_players(...)` now returns the collected player count instead of a legacy status code. Preserved behavior: empty import lists remain successful no-ops, and copy failures still log warnings without changing the caller cleanup flow.

Added focused `ImportWorkflow.cpp` coverage for empty-list success, missing-source failure, successful character-plus-inventory copy, and active-module player-list entry construction, then kept build and the `test.mod` validator as the acceptance bar for the pass.

### Pass 171 — AI script loader bool-contract cleanup (2026-04-22)

Continued Tier 1.4 by retiring `egolib_rv` from the AI script-loading seam in `script_compile.c`: `load_ai_script_vfs(...)` and its file-local helper now return `bool` while preserving the existing behavior of trying the requested script first and then falling back to `mp_data/script.txt`.

Updated the validator's script compile check to the new bool contract without changing its `script_missing`, `script_fallback`, or `script_compile_failure` classification, and added focused `ScriptLoader.cpp` coverage for valid-primary, missing-primary fallback, invalid-primary fallback, and both-primary-and-fallback failure behavior.

### Pass 172 — Bounding-box bool/void contract cleanup (2026-04-22)

Continued Tier 1.4 by retiring `egolib_rv` from the remaining `oct_bb_t` helper family in `bbox.{h,c}`: `validate(...)` and both `downgrade(...)` overloads now return `void`, while both `cut(...)` overloads now return `bool`. Preserved the existing behavior: empty-cut inputs still fail without mutation, and downgrade/validate still only publish recomputed output state.

Added focused `BoundingBox.cpp` coverage for validate, empty/non-empty cut semantics, restricted-axis cuts, and both downgrade paths, while keeping the existing collision-volume behavior pinned through the focused `ObjectAccessors` slice.

### Pass 173 — Systems inventory compatibility closure (2026-04-22)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining target-held plus actor-pocket inventory compatibility pocket behind one file-local helper/context layer. Routed `scr_CostTargetItemID`, both target-ammo restock opcodes, and `scr_UnkurseTargetInventory` through that shared traversal while preserving the legacy behavior: target held items are visited first, actor pockets remain the compatibility fallback, and target pocket items stay excluded.

Extended `ScriptSystemsFunctions.cpp` with a focused regression that pins target-pocket items as ignored by this compatibility cluster, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 174 — Systems economics / armor seam closure (2026-04-22)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining target armor and wallet compatibility pocket behind one file-local context/helper layer. Routed `scr_GetTargetArmorPrice`, `scr_ChangeTargetArmor`, `scr_GiveMoneyToTarget`, `scr_DropMoney`, `scr_DropTargetMoney`, and `scr_TargetPayForArmor` through that layer while preserving legacy price, refund, transfer-clamp, and quiet-failure behavior.

Extended `ScriptSystemsFunctions.cpp` with a focused invalid-target regression on top of the existing armor and wallet coverage, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 175 — Systems target-role compatibility closure (2026-04-22)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining target team/state/enchant compatibility pocket behind one file-local target-context helper layer. Routed `scr_JoinTargetTeam`, `scr_TargetJoinTeam`, `scr_GiveExperienceToTarget`, `scr_UnkurseTarget`, `scr_CostTargetMana`, `scr_AddBlipAllEnemies`, `scr_GrogTarget`, `scr_DazeTarget`, `scr_KurseTarget`, `scr_SetTargetAmmo`, `scr_DisenchantTarget`, and `scr_GiveSkillToTarget` through the shared resolver while preserving legacy quiet-failure, timer, perk, and enemy-sense behavior.

Extended `ScriptSystemsFunctions.cpp` with a focused invalid-target regression for the new target-compatibility helper cluster, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 176 — Systems quest/class compatibility closure (2026-04-22)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining quest-log and self class-change compatibility pocket behind file-local helper contexts. Routed `scr_AddQuest`, `scr_BeatQuestAllPlayers`, `scr_SetQuestLevel`, `scr_AddQuestAllPlayers`, and `scr_ChangeTargetClass` through shared quest/class helpers while preserving quiet-failure for missing target quest logs, empty local-player sets, and unloaded class profiles.

Extended `ScriptSystemsFunctions.cpp` with focused regressions for empty local-player quest updates and invalid-self class-change failure, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 177 — GUI object observation seam (2026-04-22)

Started Tier 3.1 by removing strong `shared_ptr<Object>` ownership from the player-status UI path. `PlayingState`, `CharacterStatus`, `CharacterWindow`, `LevelUpWindow`, `InventorySlot`, and the status-driven `game_loop.c` helpers now store or pass `ObjectRef` plus session lookups instead of holding gameplay entities alive through presentation code.

Added a narrow non-owning `GameSessionContext::tryObject(...)` seam, kept terminated or missing observed objects as normal no-op/destroy conditions for UI widgets, and extended `ScriptSystemsFunctions.cpp` with coverage for status-monitor publication and stale status-character window suppression. Build and `test.mod` validator remained the acceptance bar.

### Pass 178 — Player observation seam (2026-04-23)

Continued Tier 3.1 by removing `Player::getObject()` as a `shared_ptr<Object>` exposure point. `Player` now publishes object identity through `getObjectRef()` and bounded `tryObject()` lookup helpers, while runtime callers in session aggregation, camera/player setup, HUD/minimap, targeting, weather, module passage music, export/import list generation, and gameplay input/cheat helpers were migrated off the old shared-object API.

Kept missing or terminated player objects as normal no-op branches, retained the existing pre-module startup/test behavior for local-player aggregation, and updated focused startup/status regressions plus the existing export/script coverage to lock the new seam. Build and `test.mod` validator remain the acceptance bar.

### Pass 179 — Systems self/module compatibility split (2026-04-23)

Returned to Tier 1.2 inside `script_functions_systems.c` by splitting the remaining broad self/module compatibility helper into narrower file-local contexts: self-role mutation, self-profile policy, and UI/module side effects. Routed the self damage-type/equip/team/money/armor writes, spellbook/follow-link/deprecation policy, minimap/status/end-text adapters, and water/fog/tile/export/pit/module helpers through those smaller contexts without widening any public role seams or changing legacy success/failure behavior.

Extended `ScriptSystemsFunctions.cpp` with focused invalid-self coverage for the widened self-role cluster on top of the existing follow-link, end-text, UI, and module-environment regressions, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 180 — Systems target-state retaliation residue cleanup (2026-04-23)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining bottom-of-file target-state and retaliation residue behind small file-local helpers. Routed `scr_GiveManaFlowToTarget`, `scr_GiveManaReturnToTarget`, `scr_DispelTargetEnchantID`, and `scr_TargetDamageSelf` through shared alive-target or retaliation helpers without widening any public role seams or changing the existing missing-target and quiet-no-op contracts.

Extended `ScriptSystemsFunctions.cpp` with focused missing-target and terminated-target regressions for the new helper cluster, then kept the focused systems slice, build, and the `test.mod` validator as the acceptance bar.

### Pass 181 — Systems passage compatibility closure (2026-04-23)

Continued Tier 1.2 inside `script_functions_systems.c` by moving the remaining bounded passage mutator/query pocket behind one file-local compatibility context and helper layer. Routed `scr_OpenPassage`, `scr_ClosePassage`, `scr_IfPassageOpen`, `scr_FlashPassage`, and `scr_AddShopPassage` through the new passage helpers without widening any public role seams or changing the existing success/failure quirks for invalid passage IDs.

Kept the focused `ScriptSystemsFunctions.cpp` passage regression as the behavior lock, then used build and the `test.mod` validator as the acceptance bar for the pass.

### Pass 182 — Target selector role closure (2026-04-23)

Continued Tier 1.2 inside `script_functions_target.c` by splitting the remaining self-side selector/query pocket behind a file-local self-selector context. Routed attacker/bump/leader/caller/holder/last-item selectors, team-comparison predicates, self-facing queries, and proximity/weapon target search helpers through the resolved self-role context without widening any public role seams or changing quiet-failure behavior for invalid self refs.

Extended `ScriptTargetFunctions.cpp` with focused invalid-self coverage for the widened selector cluster, then kept the focused target-function test slice, build, and the `test.mod` validator as the acceptance bar for the pass.

### Pass 183 — Action self/profile compatibility closure (2026-04-23)

Continued Tier 1.2 inside `script_functions_action.c` by moving the remaining self/profile message, sound, billboard, charge-display, and visual mutation residue behind one file-local self-compatibility context. Routed the lingering `pchr` / `ppro` access pockets through that shared helper layer, normalized the action-start checks to the landed bool contract on `IAnimationControl`, and preserved the existing success/failure behavior without widening any public role seams.

Extended `ScriptActionFunctions.cpp` with focused regressions for direct self-message dispatch, usage-known publication, and volume-adjusted sound routing on top of the existing action/audio/billboard/charge coverage, then kept build, the focused action test slice, and the `test.mod` validator as the acceptance bar.

### Pass 184 — Spawn self/profile compatibility closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_spawn.c` by replacing the remaining direct self/profile compatibility pockets with a strengthened file-local `SpawnSelfContext` plus a narrow target-identification helper. Routed self lifecycle, cleanup, stealth, identify-target, and poof-config helpers through the resolved context without widening any public role seams or changing the existing success/failure behavior.

Extended `ScriptStateFunctions.cpp` with focused coverage for usage-known publication during `scr_IdentifyTarget()` and the self-profile-driven `scr_SpawnPoofSpeedSpacingDamage()` path, then kept build, the focused `ScriptStateFunctionsFixture` slice, and the `test.mod` validator as the acceptance bar.

### Pass 185 — State self/profile compatibility closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_state.c` by replacing the remaining direct self/profile query pocket with a file-local `SelfStateContext`. Routed `scr_Else()`, `scr_IfUsageIsKnown()`, and `scr_IfModuleHasIDSZ()` through that resolved context without widening any public role seams or changing the existing success/failure behavior.

Extended `ScriptStateFunctions.cpp` with focused coverage for profile-script indent comparison in `scr_Else()` and self-profile usage-known detection, then kept build, the focused `ScriptStateFunctionsFixture` slice, and the `test.mod` validator as the acceptance bar.

### Pass 186 — Target compatibility context closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_target.c` by replacing the remaining direct target-side interface probes with one file-local `TargetCompatibilityContext`. Routed the repeated inventory, damage, scriptable, physical, mount, weapon, and quest target pockets through that shared resolver without widening any public role seams or changing quiet-failure behavior.

Extended `ScriptTargetFunctions.cpp` with focused invalid-target coverage for the shared target context across hand selectors, order/state/content queries, facing checks, and quest lookup, then kept build, the focused target-function slice, and the `test.mod` validator as the acceptance bar.

### Pass 187 — Systems self-role residue closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_systems.c` by widening the existing file-local `SelfRoleContext` to cover the remaining self-side team, leader, ammo, and enchant pockets. Routed `scr_JoinTargetTeam`, `scr_BecomeLeader`, `scr_IfLeaderIsAlive`, `scr_IncreaseAmmo`, `scr_CostAmmo`, and `scr_SetEnchantBoostValues` through shared self-role helpers without widening any public role seams or changing legacy success/failure behavior.

Extended `ScriptSystemsFunctions.cpp` with focused invalid-self coverage for the new helper cluster, then kept build, the focused systems slice, and the `test.mod` validator as the acceptance bar.

### Pass 188 — Action self/profile compatibility closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_action.c` by strengthening the file-local `SelfActionContext` so the remaining self/profile message, sound, billboard, charge-display, and visual-identity helpers resolve through one shared compatibility context instead of repeating direct `pchr` / `ppro` access patterns in opcode bodies. Kept audio, camera, billboard, and UI work on the already-landed `EngineContext` service seams and preserved existing success/failure behavior without widening any public role seams.

Extended `ScriptActionFunctions.cpp` with focused non-player charge-display failure coverage, then kept build, the focused action test slice, and the `test.mod` validator as the acceptance bar.

### Pass 189 — Script-entry self-resolution closure (2026-04-24)

Continued Tier 1.2 by removing `SCRIPT_FUNCTION_BEGIN()` / `SCRIPT_REQUIRE_TARGET()` dependence from `script_functions_action.c`, `script_functions_target.c`, `script_functions_state.c`, `script_functions_spawn.c`, and `script_functions_systems.c`. Added shared non-owning `ResolvedSelfContext` resolution in `script_functions_internal.h`, then routed the converted files through their existing file-local compatibility contexts or explicit resolved-self helpers instead of macro-owned `pchr` / `ppro` setup.

Kept the legacy invalid-self / missing-profile quiet-failure contract unchanged, pruned the last dead `pchr` / `ppro` residue from the converted files, and kept `egolib-library` plus `egoboo-content-validator` building clean with `test.mod` validating at 0 warnings / 0 errors. Focused `egolib-tests-executable` coverage remained blocked by the existing unresolved-link cluster in this workspace.

### Pass 190 — Movement script self-resolution closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_movement.c` by removing its `SCRIPT_FUNCTION_BEGIN()` / macro-owned `pchr` dependence and routing self movement, physical, frame, and pathfinding helpers through explicit resolved-self context. Narrowed `FindPath(...)` away from a raw `Object*` parameter to physical-role data plus the existing stopped-by mask, without changing waypoint, A* throttle, or quiet-failure behavior.

Extended `ScriptMovementFunctions.cpp` with focused pathfinding coverage for valid resolved-self waypoint publication and invalid-self no-mutation failure. Build, the focused movement test slice, and the `test.mod` validator remained the acceptance bar for the pass.

### Pass 191 — Bitwise script macro retirement (2026-04-24)

Finished the remaining script-entry macro cleanup inside `script_functions_bitwise.c` by converting all 12 bitwise alert/state opcodes to direct return flow. Removed the now-dead `SCRIPT_FUNCTION_BEGIN()` and `SCRIPT_REQUIRE_TARGET()` definitions while keeping the still-used `SCRIPT_FUNCTION_END()` helper for the other split script-function files.

Added `ScriptBitwiseFunctions.cpp` coverage for alert bit/mask helpers, state bit/mask helpers, and invalid bit-index exceptions. Build, the focused bitwise test slice, and the `test.mod` validator remained the acceptance bar.

### Pass 192 — Movement script direct-return cleanup (2026-04-24)

Continued the script return-flow cleanup inside `script_functions_movement.c` by replacing the legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the movement opcode file. Preserved the explicit self-resolution contract from Pass 190 and kept waypoint, pathfinding, teleport, frame, latch, reload, shadow, and velocity side-effect ordering unchanged.

Kept `SCRIPT_FUNCTION_END()` available for the remaining split script-function files. Acceptance for this pass is the build, the focused `ScriptMovementFunctionsFixture` slice, and the `test.mod` validator.

### Pass 193 — Action script direct-return cleanup (2026-04-24)

Continued the script return-flow cleanup inside `script_functions_action.c` by replacing the legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the action opcode file. Preserved explicit self-resolution, audio/camera/billboard/UI service seams, and the existing invalid-self, invalid-target, screenshot, billboard, message-distance, speech no-op, and charge-display success/failure behavior.

Kept `SCRIPT_FUNCTION_END()` available for the remaining target/state/spawn/systems files. Build, the focused `ScriptActionFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar.

### Pass 194 — Target script direct-return cleanup (2026-04-24)

Continued the script return-flow cleanup inside `script_functions_target.c` by replacing the legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the target opcode file. Preserved explicit self-resolution, target publication, passage-occupant lookup, quest lookup, and quiet-failure behavior.

Kept `SCRIPT_FUNCTION_END()` available for the remaining state/spawn/systems files. Build, the focused `ScriptTargetFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar.

### Pass 195 — State script direct-return cleanup (2026-04-24)

Continued the script return-flow cleanup inside `script_functions_state.c` by replacing the legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the state opcode file. Preserved explicit self-resolution, alert/state predicates, held-item hand selection, platform predicates, debug-message side effects, and quiet-failure behavior.

Kept `SCRIPT_FUNCTION_END()` available for the remaining spawn/systems files. Build, the focused `ScriptStateFunctionsFixture` slice, and the `test.mod` validator remained the acceptance bar.

### Pass 196 — Spawn script direct-return cleanup (2026-04-24)

Continued the script return-flow cleanup inside `script_functions_spawn.c` by replacing the legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the spawn opcode file. Preserved explicit self-resolution, poof/player-immunity branches, particle success gates, attached-character placement/termination behavior, child publication, morph, invictus, and stealth return semantics.

Kept `SCRIPT_FUNCTION_END()` available for the remaining systems file. Build, focused `ScriptStateFunctions` / `ModuleSpawnRealization` coverage, and the `test.mod` validator remained the acceptance bar.

### Pass 197 — Systems script direct-return cleanup (2026-04-24)

Finished the split script return-flow cleanup inside `script_functions_systems.c` by replacing the remaining legacy `returncode` / `SCRIPT_FUNCTION_END()` pattern with direct returns across the systems opcode file. Removed the now-dead shared return macros from `script_functions_internal.h` while preserving explicit self-resolution, target/passage/quest/enchant/wallet semantics, and quiet-failure behavior.

Made the `AddIDSZ` systems fixture clear its generated `test.mod` menu override before and after the test so stale ignored VFS state cannot leak across focused runs. Build, the full focused `ScriptSystemsFunctionsFixture` slice, residue search, and the `test.mod` validator remained the acceptance bar.

### Pass 198 — Target selector self-context residue closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_target.c` by replacing `SelfTargetSelectorContext`'s retained concrete `Object*` with self identity plus role pointers for script, target-info, inventory, physical, and appearance access. Resolved the concrete self only at the legacy `chr_find_target(...)` and `FindWeapon(...)` call boundaries, and moved `scr_SetTargetToRider()` onto the inventory role without widening public interfaces or changing quiet-failure behavior.

Kept the existing target-search and self-selector regressions as the behavior lock, then used build, the focused `ScriptTargetFunctionsFixture.*TargetSearch*:*SelfSelector*` slice, and the `test.mod` validator as the acceptance bar.

### Pass 199 — Target-search ObjectRef boundary closure (2026-04-24)

Continued Tier 1.2 by moving the last `script_functions_target.c` concrete self recovery out of the file-local target-search helpers. `findTargetForSelf(...)` and `findWeaponForSelf(...)` now pass self identity as `ObjectRef`, while `chr_find_target(...)` and `FindWeapon(...)` resolve the concrete object at their legacy implementation boundary and return `ObjectRef::Invalid` for missing or terminated sources.

Preserved target-search no-match behavior, invalid-self quiet failure, and weapon/mount role-query behavior without widening public role interfaces or touching spawn/systems ownership-bearing flows. Build, the focused target-search/self-selector/weapon-query slice, and the `test.mod` validator remained the acceptance bar.

### Pass 200 — ChangeTile module-effects ObjectRef boundary closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_systems.c` by replacing `ModuleEffectsContext`'s retained concrete self object with self identity. `scr_ChangeTile()` now passes only the resolved self `ObjectRef` into the module-effects helper layer, and `setActorTileType(...)` resolves the concrete object at the final legacy tile-index boundary without widening any public role or module interfaces.

Extended `ScriptSystemsFunctions.cpp` with focused invalid-self coverage that pins `scr_ChangeTile()` as a quiet false/no-mutation path when self cannot be resolved. Kept build, the focused `ChangeTile` plus adjacent module-environment systems slices, and the `test.mod` validator as the acceptance bar.

### Pass 201 — Systems presentation ObjectRef boundary closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_systems.c` by removing `PresentationEffectsContext`'s retained concrete self object. Presentation helpers now carry only self identity plus the installed playing-state/minimap adapters, and `scr_AddEndMessage()` resolves the concrete object only at the final legacy `AddEndMessage(...)` boundary without widening public role or UI interfaces.

Extended `ScriptSystemsFunctions.cpp` with focused terminated-self coverage that pins end-message publication as a quiet false/no-mutation path when self is no longer live. Kept build, the focused end-text/presentation systems slice, and the `test.mod` validator as the acceptance bar.

### Pass 202 — Systems inventory ObjectRef boundary closure (2026-04-24)

Continued Tier 1.2 inside `script_functions_systems.c` by removing the remaining concrete self recovery from the systems inventory compatibility pocket. `InventoryCompatibilityContext` now resolves both actor and target inventories from `ObjectRef` identities via role lookups, so `scr_CostTargetItemID`, both target-ammo restock opcodes, and `scr_UnkurseTargetInventory` no longer need an `Object&` actor adapter while preserving target-held-first, actor-pocket fallback, and target-pocket exclusion behavior.

Extended `ScriptSystemsFunctions.cpp` with focused invalid-self coverage for the inventory cluster, then kept build, the focused inventory systems slice, and the `test.mod` validator as the acceptance bar.

---

## Theme 10 — Object role-interface caller migration (2026-06-05)

### Pass 203 — Helper retypes onto `IInventoryHolder` / `IPhysical` (2026-06-05)

Continued Tier 1.2 by retyping file-local / anonymous-namespace helpers from the concrete `Object` to the single role interface each actually uses, narrowing caller coupling with no behavior change. The duplicated `heldItem(...)` helper in `Entities/Object_combat.cpp`, `Entities/Enchant.cpp`, `game/Graphics/ObjectGraphics.cpp`, and `game/Physics/particle_collision.c` — plus the `character` parameter of `usedHeldItemForBlock(...)` — now take `const IInventoryHolder&`. `computeReflectionAlpha`/`makeTintRenderState` (`ObjectGraphics.cpp`) and `publishSpawnWaypoint` (`Script/script.c`) now take `const IPhysical&`; `leftHandRiderRef` (`Script/script.c`) takes `const IInventoryHolder&` and drops its concrete-`Object` adapter hop. Every call site passes a concrete `Object`, so each retype upcasts implicitly — compiler-verified with zero call-site churn.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, and `ctest` steady at the two pre-existing `ScriptLoaderFixture` default-script fallback failures (#526/#527) with no new failures.

### Pass 204 — Remove `isLocalPlayer` duplicate and `detatchFromHolder` typo alias (2026-06-06)

Shrank the `Object` public surface by deleting two redundant members. The dead `isLocalPlayer()` getter (zero library callers; a literal duplicate of `isPlayer()`, both returning `islocalplayer`) is removed, and the single test assertion that used it now reads through the canonical `isPlayer()`. The misspelled `detatchFromHolder(...)` alias — a pass-through to the `ILifecycleControl` override `detachFromHolder(...)` — is removed and its 14 call sites (13 library + 1 test) renamed to the correct spelling; a stale "detatch" doc-comment typo was fixed in passing.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 205 — `override` sweep on `Object` role-interface implementations (2026-06-06)

Marked the 28 `Object` methods that implement one of the seventeen role-interface pure-virtuals but lacked the `override` specifier — the exact set reported by GCC `-Wsuggest-override` (extracted via a one-off `-fsyntax-only` compile against the generated compile-commands database). Covers render (`isPhongMapped`, `hasReflection`, `isDontCullBackfaces`, `getAlpha`/`getLight`/`getSheen`, `getUOffset`/`getVOffset`, `getModelDescriptor`, `isHidden`, `isInsideInventory`), target/state (`isItem`, `isFlying`, `isMount`, `isPlatform`, `isOnWaterTile`, `isPlayer`, `getHolderRef`, `getGender`, `isHurt`, `hasNotFullMana`), and inventory/lifecycle (`isTerminated`, `getHeldObject`, `setHeldObject`, `getInventoryMaxItems`, `getFirstFreeInventorySlot`, `dropKeys`, `dropAllItems`). No behavior change; `override` documents the interface contract and turns any future signature drift into a compile error. Each is a genuine override (the build confirms). `AudioSystem.hpp` carries five further `-Wsuggest-override` hits left for a separate focused pass.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 206 — Add `getPosition()` to `IPhysical`; migrate `phys_expand_chr_bb` (2026-06-06)

Widened `IPhysical` with `getPosition()` — the position-vector accessor previously reachable only on the concrete `Object`/`Collidable` and flagged by the role-interface map as the single highest-leverage blocker across the physics/collision helpers. `Object` supplies the override by forwarding to `Ego::Physics::Collidable::getPosition()`, mirroring its existing explicit `getPosX/Y/Z` overrides (needed because `Collidable` is a sibling base of `IPhysical`, not derived from it). `Object` is the sole `IPhysical` implementer, so the new pure-virtual has no other blast radius. With the accessor in place, `phys_expand_chr_bb(...)` — which touches only `getMaxCollisionVolume`/`getPosition`/`getVelocity` — is retyped from `Object*` to `const IPhysical*` (declaration in `physics.h` gains a `class IPhysical;` forward declaration); its single caller in `CollisionSystem.cpp` passes a concrete `Object*` and upcasts implicitly.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 207 — Narrow `INGAME_PCHR` to `IInventoryHolder`; drop dead `findItem(Object*)` (2026-06-06)

Two `IInventoryHolder`-scoped cleanups. `INGAME_PCHR(const Object*)` — whose body is only a null check plus `isTerminated()` — is retyped to `const IInventoryHolder*` (`ObjectHandler.hpp` gains a `class IInventoryHolder;` forward declaration); its single caller in `Shop.cpp` passes a concrete `Object*` and upcasts implicitly. The unused `Inventory::findItem(Object*, ...)` overload — a concrete-`Object` adapter with zero callers anywhere in the tree (live call sites use the `IInventoryHolder&` and `ObjectRef` overloads) — is deleted outright, removing dead code and one more `Object`-typed entry point.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 208 — Narrow `chr_invalidate_child_instances` to `IInventoryHolder` (2026-06-06)

`chr_invalidate_child_instances(Object&)` in `ObjectGraphics.cpp` reaches its parameter only through `heldItem(...)` (now `IInventoryHolder&`), invalidating the matrix cache on the returned held items; the parameter itself needs nothing beyond the inventory role. Retyped to `const IInventoryHolder&`; its single caller passes the `Object&` member `_object` and upcasts. Completes the `IInventoryHolder` caller migration in this TU.

Acceptance: build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 209 — Complete the `override` sweep (AudioSystem, RuntimeStatistics) (2026-06-06)

Closed the remaining `-Wsuggest-override` gaps in egolib. A full `-Wsuggest-override` build confirmed these were the only ones left after Pass 205's `Object.hpp` sweep: the five `IAudioSystem` implementations in `AudioSystem.hpp` (`update`, `stopObjectLoopingSounds`, `playSound`, `playSoundLooped`, `setMaxHearingDistance`) and `Ego::Script::RuntimeStatistics::append` in `script.c`. egolib is now `-Wsuggest-override`-clean.

Acceptance: build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 210 — Widen `IPhysical`/`ITargetInfo`/`IScriptable` and migrate the unlocked callers (2026-06-06)

Acted on the role-interface map's interface-backlog: added the legacy `Object` accessors that were blocking otherwise-clean single-role migrations, each paired with its now-unblocked caller(s).

- `IPhysical` gains `getAxisAlignedBox2D()` and `getPhysicsWeight()` (`Object` forwards to `_objectPhysics` / the `phys.weight` field). This unlocks `Passage::objectIsInPassage` and `Passage::checkPassageMusic` → `const IPhysical&`, and narrows `get_prt_mass(..., Object*, ...)` → `const IPhysical*` (replacing the `pchr->phys.weight` public-field read with `getPhysicsWeight()`).
- `ITargetInfo` and `IScriptable` gain `getObjRef()` (already triplicated on `IInventoryHolder`/`IRenderable`/`IDamageable`; `Object`'s single override satisfies all five). This unlocks `resolveHolderOrSelfRef(...)` → `const ITargetInfo&` (dropping the `targetInfo()` adapter hop) and `publishAttachedChildState(...)` → `IScriptable&`.
- `makeClassChangeCompatibilityContext(Object&)` → `IMorphControl&`, dropping its `static_cast<IMorphControl*>`.

Every call site passes a concrete `Object` and upcasts implicitly. (A `getXPForLevel`/`ICharacterState` addition was prototyped then reverted: its only candidate consumer `draw_character_xp_bar` also reads `getMoney`/`getName`, so it stays genuinely multi-role.)

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, full validator steady at 42 modules / 245 pre-existing errors, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

## Theme 11 — T1.3 singleton → EngineContext service seams (2026-06-06)

### Pass 211 — Publish `FontManager` through `EngineContext` (2026-06-06)

First T1.3 service-interface seam of this session. Extracted `Ego::IFontManager` (single method `loadFont`), made `FontManager` implement it, and added the standard `EngineContext` install/clear/try/accessor surface (`fontManager()`) — installed in `App.cpp` right after `FontManager::initialize()` and torn down in `clearEngine()`. Migrated all five `FontManager::get().loadFont(...)` call sites (4 in `game/GUI/UIManager.cpp`, 1 in `Console/Console.cpp`) to `EngineContext::get().fontManager().loadFont(...)`, decoupling the GUI and Console from the concrete `FontManager` singleton (Console now includes `Font.hpp` + `EngineContext.hpp` instead of `FontManager.hpp`). `FontManager::get()` remains only as the App-bootstrap seam.

Acceptance: full egolib build clean (0 errors), `test.mod` validator `warnings=0 errors=0` (fonts are not exercised by validator/tests), `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 212 — Finish `InputSystem` caller migration onto `EngineContext` (2026-06-06)

`InputSystem`'s EngineContext seam already existed (`IInputSystem` + install at `GameEngine.cpp:314`); this pass migrated the remaining cross-subsystem direct `Ego::Input::InputSystem::get()` callers that use only interface methods. Eleven `isKeyDown`/`getModifierKeys` sites in `graphic_hud.c` (6), `graphic_mad.c` (2), `graphic_prt.c`, `graphic_scene.c`, and `Console.cpp` now go through `EngineContext::get().inputSystem()`; `graphic_hud.c` gains the `EngineContext.hpp` include. Deferred: `Camera.cpp` and `InputDevice.cpp` still reach the non-interface `joysticks` data member, and the GameEngine `.mouse/.keyboard` accesses are dead `#if 0` — those need `joysticks` exposed on `IInputSystem` (or removed) first.

Acceptance: build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 213 — Publish `GraphicsSystem` through `EngineContext` (with headless test mock) (2026-06-06)

Seamed the highest-reach tractable graphics singleton. Extracted `Ego::IGraphicsSystem` exposing `getWindow()` (named to avoid the public `window` data-member clash), made `GraphicsSystem` implement it, and added the standard `EngineContext` install/clear/try/accessor surface — installed in `App.cpp` after `GraphicsSystem::initialize()` and torn down in `clearEngine()`. Migrated ~33 `GraphicsSystem::get().window->...` sites across 18 TUs (UIManager, GameEngine, graphic.c, Console, CameraSystem, Camera, ForegroundRenderPass, and eleven GameStates) to `EngineContext::get().graphicsSystem().getWindow()->...`; the lone `App.cpp` self-bootstrap stays on the local handle.

Render/window code is reached transitively by headless test fixtures (`CameraTracking`, `ScriptActionFunctions`, `ScriptSystemsFunctions`) that cannot create a real SDL window. Added a shared `egolib/tests/egolib/tests/TestGraphicsSystem.hpp` `MockGraphicsSystem` — a properly-constructed `IGraphicsSystem` returning each fixture's existing `StubGraphicsWindow` — and installed it into `EngineContext` in those fixtures' setup. (The fixtures' raw-allocated fake `GraphicsSystem`, via `::operator new(sizeof(...))` with no constructor, has an uninitialized vtable and cannot serve the now-virtual `getWindow()`.) This is the pattern that unblocks the remaining big graphics seams (Renderer, GFX, TextureManager).

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, full validator steady at 42 modules / 245 pre-existing errors, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527) — no SEGFAULTs.

### Pass 214 — Publish `TextureManager` through `EngineContext` (2026-06-06)

Seamed `TextureManager`, the next graphics service after the Pass 213 mock pattern landed. Extracted `Ego::ITextureManager` (the four methods callers use: `getTexture`, `updateDeferredLoading`, `reupload`, `release_all`), made `TextureManager` implement them, added the standard `EngineContext` install/clear/try/accessor surface — installed in `App.cpp` after `TextureManager::initialize()` and cleared in `clearEngine()`. Migrated the 13 game-layer `TextureManager::get()` sites (`CharacterStatus`×3, `graphic_hud`×2, `graphic.c`×2, `UIManager`, `ModuleSelector`, `InventorySlot`, `graphic_mad.c`, `MapEditorSelectModuleState`, `GameEngine`) to `EngineContext::get().textureManager()`. Deliberately left `Renderer/DeferredTexture.cpp` and the root-level `font_bmp.c` on the subsystem-local `TextureManager::get()` to avoid a lower-layer → `game/Core/EngineContext` layer inversion. No test mock was needed — the headless test paths don't reach the texture-loading code (it lives in draw methods).

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 215 — Route `GFX` billboard callers to the existing `EngineContext` billboard seam (2026-06-06)

Cleanup ahead of the `GFX` seam: 23 `GFX::get().getBillboardSystem().{makeBillboard,reset}(...)` sites reached `GFX` only to obtain the billboard system, which already has its own `EngineContext::billboardSystem()` seam. Retyped them to `EngineContext::get().billboardSystem()...` across `Entities/Object_combat.cpp`, `game/Physics/particle_collision.c`, `game/game_combat.c`, `game/graphic.c`, and `game/Core/GameEngine.cpp`. The lone `render_all` caller in `graphic_scene.c` stays on `GFX::get().getBillboardSystem()` because `render_all(::Camera&)` is not on `IBillboardSystem` (and the test `StubBillboardSystem` mock would need it). This drops the GFX-billboard reaches ~24→1, shrinking the GFX caller set before the GFX seam.

Acceptance: build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527) — `billboardSystem` is installed in the script/combat fixtures, so the migrated `makeBillboard` calls work headless.

### Pass 216 — Decouple clock callers from the `Core::System` singleton via the `Time` abstraction (2026-06-06)

T1.3 follow-on. The 7 `Core::System::get().getSystemService().getTicks()` service-locator calls in `Graphics/Font.cpp` (4, font-cache timing) and `game/GUI/MessageLog.cpp` (3, message expiry) now use the existing `::Time::now<::Time::Unit::Ticks>()` abstraction — behaviorally identical (`Time::now<Ticks>()` is a thin wrapper over the same call). Direct `Core::System::get()` reaches drop 8→1 (only `Time/Time.cpp`, the foundational wrapper, remains).

Chosen over an `EngineContext` seam: `Core::System` is initialized in the executable's `Main.cpp` (it outlives the engine, so it doesn't fit `clearEngine`), `SystemService::getTicks()` is non-virtual (an interface would crash the test fixtures' raw-allocated fake `SystemService`, which has no vtable), and the call is reached transitively via messages across many fixtures. Routing through the `Time` clock abstraction achieves the decoupling with zero behavior change and no lifecycle/mock complexity — and a clock belongs behind a time abstraction, not the engine context. (`::Time::` qualification is required because callers sit in `namespace Ego`, where bare `Time::` would resolve to the unrelated `Ego::Time`.)

Acceptance: build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Pass 217 — Publish `TextureAtlasManager` through `EngineContext` (2026-06-06)

Seamed the mesh tile-texture atlas, the next graphics service after `TextureManager` (Pass 214). Extracted `Ego::Graphics::ITextureAtlasManager` (the four methods callers use: `getSmall(int)`, `getBig(int)`, `reupload()`, `loadTileSet()`), made `TextureAtlasManager` implement it (4 `override`s), and added the standard `EngineContext` install/clear/try/accessor surface (`textureAtlasManager()`). The interface lives in `game/Graphics/` next to the concrete class and mirrors the `Ego::Graphics::IBillboardSystem` precedent (namespace `Ego::Graphics`, not engine-`Graphics/` like `ITextureManager`). Migrated all 5 game-layer `TextureAtlasManager::get()` call sites — `graphic.c`×3 (`gfx_system_reload_all_textures` reupload, `TileRenderer::get_texture` getSmall/getBig) and the two `loadTileSet()` sites in `GameStates/LoadingState.cpp` and `GameStates/MapEditorState.cpp` — to `EngineContext::get().textureAtlasManager()`. Dropped the now-redundant concrete `TextureAtlasManager.hpp` include from both GameStates TUs (they reach the atlas only through the interface now).

Lifecycle: unlike the App-bootstrap services (installed in `App.cpp`), `TextureAtlasManager` is `initialize()`d/`uninitialize()`d in `graphic.c`'s `GameAppImpl` ctor/dtor (the GFX backing object). Install/clear are therefore paired there — install right after `initialize()`, clear right before `uninitialize()` — keeping the EngineContext pointer's validity an exact mirror of the singleton's own lifecycle (no reliance on `clearEngine()` ordering). `TextureAtlasManager::get()` remains only as the `GameAppImpl` bootstrap seam.

No test mock was needed: a grep of `egolib/tests` confirms no fixture installs or reaches the atlas, and no headless test executes `gfx_system_reload_all_textures`/`TileRenderer::get_texture`/`LoadingState`/`MapEditorState`, so the throw-on-missing accessor is never hit (same load/draw-path-only situation as `TextureManager`).

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527).

### Fix — restore the billboard-system install argument in `GameEngine::initialize` (2026-06-06)

Standalone bug fix surfaced while continuing the T1.3 graphics seams. Pass 215's sed-based billboard caller migration (`GFX::get().getBillboardSystem()` → `EngineContext::get().billboardSystem()`) also rewrote the install **source** argument in `GameEngine::initialize()`, turning `installBillboardSystem(GFX::get().getBillboardSystem())` into the self-referential `installBillboardSystem(EngineContext::get().billboardSystem())`. That line is the only production install of the billboard system, so `EngineContext::get().billboardSystem()` threw `"no active billboard system"` at engine-init time and every migrated billboard call site would throw in the running game. The build/validator/ctest loop never caught it because no test invokes `GameEngine::initialize()`. Restored the install to source the billboard from `GFX::get().getBillboardSystem()`; a headless smoke-run now confirms the engine reaches the main menu cleanly (no init-time throw).

### Pass 218 — Publish `GFX` through `EngineContext` (sub-pass A: timers / update / dynalist / md2) (2026-06-06)

First slice of the `GFX` graphics god-singleton seam. Extracted `IGFX` (global namespace, `egolib/game/Graphics/IGFX.hpp`) covering the per-frame instance-update surface: `updateObjectInstancesTimer()`, `updateParticleInstancesTimer()`, `update_object_instances(Camera&)`, `update_particle_instances(Camera&)`, `getDynalist()`, `getMd2ModelRenderer()`. `GFX` (`struct GFX : public GameApp<GFX>`) now also inherits `IGFX`; the two formerly-public timer **data members** (`update_object_instances_timer`/`update_particle_instances_timer`) are made private and exposed through the new accessor methods (an encapsulation win), and `getDynalist()`/`getMd2ModelRenderer()` — owned by the `GameApp<GFX>` template base — get `override` forwarders on `GFX`. Added the standard `EngineContext` install/clear/try/accessor surface (`gfx()`), installed at `GameEngine.cpp` right after `GFX::initialize()` (next to the camera/billboard installs) and cleared both in `GameEngine::uninitialize()` and in `EngineContext::clearEngine()`. Migrated the 10 sub-pass-A callers — `graphic_mad.c`×3 (`getMd2ModelRenderer`), `graphic_scene.c`×5 (the two timers, the two `update_*_instances`, `getDynalist`), `graphic.c`×2 (timer `reinit()`) — to `EngineContext::get().gfx()`. The GFX render-pass accessors are deferred to sub-pass B; `GFX::get()` remains the bootstrap seam.

No test mock needed: `GFX` is never `initialize()`d by any headless fixture and no test reaches a `GFX::get()` path (the EngineContext fixtures construct `GameEngine` objects but never call `GameEngine::initialize()`). Member-init order is preserved (timers still declared before being initialized ahead of the render passes), verified by an adversarial multi-lens review (MI correctness, behavior preservation, completeness/lifecycle) that found zero real defects.

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527), and a headless smoke-run confirms the game survives `GameEngine::initialize()` and runs the main menu with no `"no active GFX"` throw. (Runtime world-render paths that call the migrated methods are not exercised by tests or the menu smoke-run; the migrations are mechanically equivalent — `EngineContext::get().gfx()` returns the installed `GFX` singleton via virtual dispatch.)

### Pass 219 — Publish `GFX` through `EngineContext` (sub-pass B: render-pass accessors) (2026-06-06)

Second slice of the `GFX` seam, completing the migratable `GFX::get()` surface. Widened `IGFX` with the 11 render-pass accessors callers actually use — `getNonOpaqueEntities`, `getOpaqueEntities`, `getReflective0`, `getReflective1`, `getNonReflective`, `getEntityShadows`, `getWater`, `getEntityReflections`, `getForeground`, `getBackground`, `getHeightmap` (each returning `Ego::Graphics::RenderPass&` const; `RenderPass` is forward-declared `struct` in `IGFX.hpp` to match its definition) — and added `override` to those 11 methods on `GFX`. `getMotionBlur()` is deliberately excluded from the interface (no caller uses it and `motionBlur` is not constructed in the `GFX` ctor), so it keeps no `override`. Migrated the 21 sub-pass-B callers to `EngineContext::get().gfx()`: 11 `.run(camera, tileList, entityList)` render-pass invocations in `graphic_scene.c` and 10 render-pass `.clock.reinit()` calls in `graphic.c`.

After this pass the only remaining `GFX::get()` references are the two install sites in `GameEngine.cpp` (`installGFX(GFX::get())`, `installBillboardSystem(GFX::get().getBillboardSystem())`) and the lone `GFX::get().getBillboardSystem().render_all(*camera)` in `graphic_scene.c` (kept because `render_all(::Camera&)` is not on `IBillboardSystem`). `GFX::get()` otherwise survives only as the bootstrap seam.

Verified by an adversarial review (override signatures, behavior preservation, completeness 21/21, `getMotionBlur` exclusion, tag consistency, layering) that found zero defects.

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527), headless smoke-run reaches the main menu cleanly. (World-render paths remain test-unreached; migrations are mechanically equivalent virtual-dispatch substitutions.)

## Theme 12 — Front 1 `Object` role-interface decoupling (resumed) (2026-06-06)

### Pass 220 — Extract `IProfiled` (`getProfile()`) role interface (2026-06-06)

Introduced the 19th `Object` role interface, `IProfiled` (`egolib/Entities/IProfiled.hpp`), a single-method seam exposing `const std::shared_ptr<ObjectProfile>& getProfile() const` (forward-declares `ObjectProfile`, includes only `<memory>` — no heavy profile header). `Object` now inherits `IProfiled` (its existing `getProfile()` gets `override`, satisfying the pure-virtual with zero implementation change). Narrowed the one cleanly-eligible caller: the file-local free function `publishSpawnOverrides(ai_state_t&, const Object&)` in `Script/script.c` — which calls only `object.getProfile()` (→`getStateOverride`/`getContentOverride`) — to `const IProfiled&`, mirroring its sibling `publishSpawnWaypoint(ai_state_t&, const IPhysical&)` already narrowed in the same TU. The call site (`publishSpawnOverrides(self, *pchr)`) upcasts the concrete `Object` implicitly (zero churn).

**Scope finding (important for planning):** a categorization workflow over all ~346 `getProfile()` sites found the "huge fan-out" is raw site count, not clean-narrowable count. The breakdown: ~137 are `Particle::getProfile()`/`Enchant::getProfile()` (different types), ~78 are internal to `Object`'s own methods, and the rest are locals/loop-vars (not params), `shared_ptr<Object>` params (the T1.2 reference-retype doesn't apply), concrete-`Object`-escape, or **multi-role-blocked** (a single param can't be multiple interface types). Exactly **one** external caller uses `getProfile()` and nothing else, hence the one-caller batch. The next two candidates — `draw_character_xp_bar` (getProfile + `ICharacterState`) and `AddEndMessage` (getProfile + `getObjRef`) — would require co-locating `getProfile()` onto those existing interfaces (interface pollution with the `ObjectProfile` dependency), deliberately deferred pending an explicit decision. **`Object` role-decoupling via single-param narrowing has essentially reached its ceiling; the remaining `getProfile()` coupling is intrinsic to multi-role functions.**

Acceptance: full egolib + tests build clean (0 errors), `test.mod` validator `warnings=0 errors=0`, `ctest` steady at the two pre-existing `ScriptLoaderFixture` fallback failures (#526/#527), headless smoke-run reaches the main menu cleanly. `publishSpawnOverrides` runs on object spawn, which `ctest`'s module-spawn / ScriptSystems suites exercise.

## Theme 13 — Tier 2 build and cross-platform cleanup (2026-06-06)

### T2.6/T2.1 — Quarantine legacy READMEs, remove proprietary build artifacts (2026-06-06)

Moved the four deprecated platform READMEs (`README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX`) to `doc/legacy/`. Removed unreferenced proprietary artifacts `distribute.ps1` and `egoboo.gta.runsettings` from the superproject. Commit `63530491d`.

### T2.1 — Drop MSVC-only CMake branches and platform.h warning-pragma island (2026-06-06)

Removed the MSVC-only CPACK block from root `CMakeLists.txt`, the `VS_DEBUGGER_WORKING_DIRECTORY` from `egoboo/CMakeLists.txt` (kept the surrounding `if(WIN32) egoboo_stage_windows_runtime_libraries()` which the mingw cross build needs), and the `#if defined(_MSC_VER)` warning-pragma island from `platform.h`. Commit `ede1ed976`.

### T2.4 — Delete orphaned SDL2-2.0.3 and physfs-2.1.1 from external submodule (2026-06-06)

Deleted `external/SDL2-2.0.3` and `external/physfs-2.1.1` (1450 files, ~428k lines) from the `external` submodule. Linux uses system pkg-config SDL2; Windows cross uses `external/mingw/`; the real PhysFS is `idlib-game-engine/library/physfs-3.0.0`. External submodule bumped to SHA `481b913`; superproject pointer-bump commit `cb836a2f5`.

### T2.3 — Vendor googletest 1.16.0 for offline builds (2026-06-06)

Updated vendored `external/googletest` to v1.16.0 in the external submodule. The default build path (`idlib-with-fetch-googletest=OFF`) now builds and passes tests offline with no network fetch. The `-Didlib-with-fetch-googletest=ON` workaround is obsolete. External submodule bumped to SHA `4a97d80`; superproject pointer-bump commit `12bd9463e`.

### T3.3 (uber-header teardown) — Pass 221: Entities role-interface cluster self-contained (2026-06-06)

Began the reframed T3.3 uber-header teardown (`game/egoboo.h`'s `#include egolib/egolib.h` propagates 54 subsystems into ~55 game headers transitively). A keep-going probe build (egolib.h neutralized) measured the true footprint: 185/286 egolib TUs fail, rooted in 37 non-self-contained headers + ~30 sources (full data + symbol→header dictionary in `72-uber-header-teardown.md`). Added an `EGOBOO_NO_UBER_INCLUDE` guard to `egoboo.h` (egolib.h still included by default → tree stays green) plus precise includes for egoboo.h's own thin body. Pass 1 made all 19 `Entities/I*.hpp` role interfaces + `Logic/ObjectSlot.hpp` + `game/Graphics/Vertex.hpp` self-contained and removed their `egoboo.h` include, verified per-header by standalone `-fsyntax-only` under the cut. Build clean, `test.mod` warnings=0 errors=0, ctest 736/738 (only pre-existing #526/#527). Branch `refactor/uber-header-teardown`. Commit `e0531056e`.

### T3.3 (uber-header teardown) — Pass 222: graphics/logic/script leaf headers self-contained (2026-06-06)

Made 11 more headers self-contained: `game/lighting.h`, `game/mesh.h` (the 2023-error keystone), `game/graphic.h`, `game/graphic_fan.h`, `game/graphic_mad.h`, `game/graphic_prt.h`, `Logic/Team.hpp`, `game/Shop.hpp`, `game/link.h`, `game/script_compile.h`, `game/script_implementation.h`. Dropped `egoboo.h` from 9 (added precise Math/bbox/GL-vertex/map-file/Mesh-info/Ref/IDSZ includes, plus `class Object;`/`class ObjectProfile;` forward decls where only pointer/shared_ptr used); kept a thin `egoboo.h` in `graphic_mad.h`/`graphic_prt.h` which use `gfx_rv`. The full keep-going build surfaced one secondary leech-break — `FileFormats/SpawnFile/SpawnFileReaderImpl.cpp` was getting `Ego::trim_ws` and `Info<float>::Grid::Size()` transitively via `Team.hpp`→egoboo.h — fixed with direct `Core/StringUtilities.hpp` + `FileFormats/map_file.h` includes. Process note: per-header `-fsyntax-only` selfcheck is necessary but not sufficient (misses sources that leech through a narrowed header); a keep-going build is the net for those. Build clean, `test.mod` warnings=0 errors=0, ctest 736/738 (only #526/#527).

### T3.3 (uber-header teardown) — Pass 223: camera/module/physics/inventory + leaf interface headers (2026-06-06)

Narrowed 15 more headers off `egoboo.h`: `game/Graphics/{Camera,CameraSystem,EntityList,TileList,IBillboardSystem,ICameraSystem,Md2ModelRenderer}.hpp`, `game/{game.h,game_internal.h,Inventory.hpp,physics.h,script_functions.h}`, `game/Module/{Passage,Module}.hpp`, `game/GameStates/LoadPlayerElement.hpp`. Includes added: `frustum.h` (Ego::Graphics::Frustum), `IDSZ.hpp` (IDSZ2), `integrations/color.hpp` (Colour4f), `_math.h`, `typedef.h`, `<cstddef>`/`<cstdint>`. Kept thin `egoboo.h` only in `TileList.hpp` (gfx_rv). Notable cascade win: fixing the leaf headers in Passes 221-222 dropped `Object.hpp` from 376 probe-errors to 1 (its remaining issue is an intentional `#error` direct-include guard, not coupling). Keep-going build caught one leech-break — `DefaultMd2ModelRenderer.hpp` lost `<vector>` + `idlib::vertex_descriptor` via the narrowed `Md2ModelRenderer.hpp` — fixed with `integrations/video.hpp` + `<vector>`. Build clean, `test.mod` warnings=0 errors=0, ctest 736/738 (only #526/#527).

### T3.3 (uber-header teardown) — Pass 224: the last 10 consumer headers (2026-06-06)

Self-contained the final 10 `egoboo.h`-consuming headers: `Entities/{Object,ObjectHandler,Particle,ParticleHandler}.hpp` (all 4 have `#error` direct-include guards → verified via `Entities/_Include.hpp`), `game/Graphics/{ParticleGraphics,Billboard,BillboardSystem}.hpp`, `game/GUI/{Material,UIManager}.hpp`, `game/CharacterMatrix.h`. Includes added per actual use: `Profiles/_Include.hpp` (prt_ori_t, ObjectProfile), `integrations/{color,math,video}.hpp` (Colour/Point/Rect/idlib-vertex), `Time/Time.hpp`, `_math.h`, `bbox.h`, `Logic/ObjectSlot.hpp`, `Math/Standard.hpp`, `typedef.h`, plus `class Object;`/`namespace Ego { class Texture; }` forward decls. `ParticleGraphics.hpp` keeps a thin `egoboo.h` (gfx_rv). Two source leech-breaks fixed: `Graphics/ObjectGraphics.hpp` (+`Profiles/_Include.hpp` for full `ObjectProfile`) and `GUI/Material.cpp` (+`Renderer/Renderer.hpp`). **All header consumers are now self-contained** — only 5 thin-`gfx_rv` keeps (`graphic_mad.h`, `graphic_prt.h`, `IGFX.hpp`, `ParticleGraphics.hpp`, `TileList.hpp`) still pull `egoboo.h`, so the egoboo.h→egolib.h link is ready to cut. Build clean, `test.mod` warnings=0 errors=0, ctest 736/738 (only #526/#527).

### T3.3 (uber-header teardown) — Pass 225: CUT the egoboo.h→egolib.h link (payoff) (2026-06-06)

**Removed `#include "egolib/egolib.h"` (and the migration guard) from `game/egoboo.h`** — it is now a thin game header (gfx_rv alias + gameplay constants + HUD timer globals + config_synch, needing only `typedef.h`/`egoboo_setup.h`/`<cstdint>`). Its ~10 remaining includers no longer transitively pull the 54-subsystem uber-header. A keep-going full build surfaced the source/header sites that had been leeching egolib types through the egoboo.h chain; all fixed with precise includes (no code changed): (a) 16 source TUs (`egoboo_setup.c` → `vfs.h`/`file_common.h`; 4 RenderPasses TUs → `Clock.hpp`/`Renderer.hpp`; 5 GameStates → `Time/Time.hpp`/`Graphics/Font.hpp`/`GraphicsWindow`/`Display`/`DisplayMode`; 5 GUI → `Font.hpp`/`Renderer.hpp`/`<cstddef>`); (b) 3 headers that weren't egoboo.h includers themselves but leeched transitively — `Graphics/RenderPass.hpp` (+`Clock.hpp`), `GUI/Button.hpp` + `GUI/Label.hpp` (+`Graphics/Font.hpp`, needed complete `Ego::Font` for the `LaidTextRenderer` member); (c) the executable `Main.cpp` (+`Core/System.hpp`). The 16-TU batch was fixed by a 4-agent parallel workflow (each verifying with `g++ -fsyntax-only`); the 3 headers + Main.cpp + final integration were caught by successive keep-going builds. Build clean across all targets (`egoboo`, content-validator, tests), `test.mod` warnings=0 errors=0, ctest 736/738 (only #526/#527). `egolib.h` itself still has 22 direct includers — deleting it is a separate, optional stretch goal.

### T3.3 (uber-header teardown) — Pass 226: DELETE `egolib.h` (uber-header eliminated) (2026-06-07)

**Completed the stretch goal: physically deleted `egolib/egolib.h`** (the 54-subsystem all-in-one aggregate), the last uber-header in the tree. After Pass 225 cut the `egoboo.h→egolib.h` link, `egolib.h` still had **18 direct includers** (12 headers + 6 sources). Method mirrored the egoboo.h teardown: a temporary `#ifndef EGOLIB_NO_UBER_INCLUDE` guard around `egolib.h`'s body let each includer be probed/narrowed hermetically (`/tmp/selfcheck.sh <hdr> -DEGOLIB_NO_UBER_INCLUDE` → `errors=0`) while the normal build stayed green.

- **Phase A — narrow the 18 includers** with precise includes (no code changed). Leaf headers got their real homes (Module/{Fog,Water,Weather,AnimatedTiles} → `FileFormats/wawalite_file.h`+`<cstdint>`; Physics/{particle_collision,ObjectPhysics,CollisionSystem} → `typedef.h`/`_math.h`/`Logic/ObjectSlot.hpp`+`class Object;` fwd-decls; Logic/QuestLog → `IDSZ.hpp`; GUI/Layout → `_math.h`+`<vector>`; renderer_3d → `_math.h`/`Math/_Include.hpp`/`bbox.h`/`integrations/color.hpp`/`<cstddef>`). The shared-dependency headers were fixed at the correct (lowest) level: `game/game.h` += `wawalite_file.h`+`strutil.h`+`struct script_state_t;` fwd-decl; `Module/module_spawn.h` += `SpawnFile/spawn_file.h`. Discovered a **pure-leech header** `Module/damagetile_instance.h` that had *zero* includes (relied entirely on egolib.h) — gave it 7 precise includes.
- **Phase B — the cut**: removed all 18 includes; a keep-going build surfaced the deep transitive-leech tail — **139 errors across 30 TUs** (files that pulled egolib types *through* a narrowed header without ever including egolib.h themselves). All fixed with precise includes, no code logic touched: 1 header (`GameStates/LoadPlayerElement.hpp` → `Renderer/DeferredTexture.hpp`, which cascade-fixed its `.cpp`), **28 source TUs via a 28-agent parallel workflow** (each agent probed with `srccheck`, mapped symbols via a shared dictionary, self-verified `errors=0`), plus `tools/egoboo-content-validator.cpp` (→ `spawn_file.h`). Recurring homes: `Renderer/Renderer.hpp`, `Graphics/GraphicsWindow.hpp`, `Graphics/GraphicsSystemNew.hpp`, `Extensions/ogl_extensions.h` (GL prims + `GL_DEBUG`), `font_bmp.h` (legacy bitmap-font globals), `map_functions.h` (`twist_to_normal`/`XX/YY/ZZ`), `Profiles/_Include.hpp` (Module/ObjectProfile complete types), `AI/LineOfSight.hpp`, `Time/Time.hpp`, `Core/StringUtilities.hpp` (`Ego::isspace` etc.).
- **Delete**: removed `egolib.h` + its `egolib/library/CMakeLists.txt` source-list entry.

**Verified** (all green): reconfigure + build all 4 targets (egolib-library, egoboo, content-validator, tests) = 0 errors; `test.mod` warnings=0 errors=0; ctest 736/738 (only pre-existing #526/#527); menu smoke-run exit 124, clean boot (OpenGL/Image/Font/Audio/atlas), error-scan empty. **The uber-header pattern is now fully gone from the live codebase** (both `egoboo.h`'s aggregate link and `egolib.h` itself eliminated). NOTE: disconnected/unbuildable **cartman** (4 files) + **utilities/migrator** (1 file) retain dangling `#include "egolib/egolib.h"` — left as-is (no `CMakeLists.txt`, not in the build graph, already bit-rotted; tracked under T3.5). They will need include fixes when/if those tools are rewired. *(Superseded by the T3.5 cartman-integration entries below: cartman is now in the CMake graph behind `option(EGOBOO_BUILD_CARTMAN OFF)` with **zero** `egolib.h` includes — those 4 were removed in Phase 1. The last dangling `#include "egolib/egolib.h"` in the tree, in `utilities/migrator/src/Tool.hpp`, was replaced with precise standard-library includes on 2026-06-08 — the deleted uber-header is now referenced by no source file anywhere.)*

---

## Tier 3 — T3.5 cartman build integration

### T3.5 Phase 1 — gated CMake seam + mechanical rename sweep (2026-06-07)

Wired the disconnected `cartman/` map editor (~9,254 LOC, 35 files, last touched 2017, no build files) into CMake behind a new gated `option(EGOBOO_BUILD_CARTMAN OFF)` (`cartman/CMakeLists.txt` with an early `return()` when OFF; links only `egolib-library`) + `add_subdirectory(cartman)` in root — standard build untouched. Mechanical rename sweep across `cartman/src` (`id::`→`idlib::`, `Ego::Math::Colour*`→`Ego::Colour*`, `Ego::{fill,set_pixel,blit}`→`idlib::…`, bare-math `using` block) and removed the 4 dangling `egolib/egolib.h` includes left by Pass 226 (curated egolib core surface now in `cartman_config.h`). Real `EGOBOO_BUILD_CARTMAN=ON` error trajectory **719→60**. Scouting + plan in `73-cartman-build-integration-scouting.md`. Commit `36fd71c21`.

### T3.5 Phase 2 — port the 60 API-drift errors; cartman compiles AND links (2026-06-07)

**cartman is now a working build target after ~8.5 years of bit-rot: `EGOBOO_BUILD_CARTMAN=ON` → 0 compile + 0 link errors, 89 MB executable.** The 60 genuine egolib-API-drift errors were ported via 5 categories, each mapped to the current API by reading egolib's own runtime usage (UIManager/Console/Camera/GameEngine/graphic): (1) `Ego::App<GFX>` — not a rename, just a missing `#include "egolib/App.hpp"` (the 24-error header cascade + incomplete-GFX); (2) `GraphicsWindow::getSize()/getDrawableSize()` → `size()/drawable_size()` (~13 sites); (3) `Ego::Math::Transform::{ortho,scaling,lookAt}` → idlib free functions `orthographic_projection_matrix`/`scaling_matrix`/`look_at_matrix` (7 sites, +`idlib/math.hpp`); (4) `Matrix4f4f::identity()` → `idlib::identity<Matrix4f4f>()` (5 sites); (5) `Ego::Core::System`/`Console` + `Ego::FontManager` — missing includes only (Core/System.hpp, Console/Console.hpp, FontManager.hpp). Then the **link phase** surfaced exactly 4 ODR collisions with `egolib-library` (proven complete by an upfront `nm` symbol-diff): `SDL_main`→`main` (egolib `#undef`s SDL's macro; mirrors `egoboo/.../Main.cpp`); cartman's editor `struct GFX` namespaced to **`Cartman::GFX`** (was clashing with egolib's game `GFX` singleton); and cartman's dead duplicate `config_download`/`config_upload` deleted. **Verified all green:** cartman build 0 errors + binary linked; reconfigure OFF → default build exit 0; `test.mod` warnings=0 errors=0; ctest 736/738 (only #526/#527). Diff: 6 files, +50/-37, all `cartman/src/cartman/`. **Flagged (not from this port):** a pre-existing no-arg/bad-args core-dump (`atexit(main_end)` runs VFS cleanup before `System::initialize()`). Full record in `73-cartman-build-integration-scouting.md` "Phase 2 — EXECUTED".

### T3.5 Phase 3 — runtime verified: cartman RUNS and renders (2026-06-07)

**Cleared the former top risk ("compile-green ≠ works").** Launched the cartman GUI against `test.mod` on a real display (`DISPLAY=:0 SDL_VIDEODRIVER=x11 EGOBOO_DATA_DIR=$PWD/data … cartman "$PWD/data" test`) and captured a mid-run screenshot. Clean boot log (filesystem init, SDL subsystems, **OpenGL 4.6 context**, ImageManager, FontManager), ran the full duration with **zero error/exception/crash lines**, and the screenshot shows the editor **fully rendered**: the four viewport windows (Vertex wireframe + vertex markers; Tile/FX textured tiles incl. red floor + arrow border; Side wireframe) plus both HUD text blocks (tile-info + minimap; brush/lighting stats + the S/H/I/B/A/D/R/O key legend). This visually exercises every ported path — `App<GFX>` startup, `FontManager::loadFont`, all four matrix builders, `GraphicsWindow::size()/drawable_size()`, module load, Console init. `EGOBOO_BUILD_CARTMAN` left OFF by default (the default-flip is now unblocked but deferred — keeps default builds fast; only `test.mod`/Linux-Mesa exercised; pre-existing no-arg crash still open). Recipe + observations in `73-cartman-build-integration-scouting.md` "Phase 3 — RUNTIME VERIFIED".

### In-game `MapEditorState` — fix garbled HUD overlap; document it as an incomplete stub (2026-06-07)

While verifying cartman, the user reported the **separate in-game Map Editor** (main menu → "Map Editor", `egolib/.../GameStates/MapEditorState.cpp`) was garbled and non-functional. Diagnosis: it is an **incomplete stub** distinct from cartman — it has a free-fly camera + minimap + three mode buttons ("Objects"/"Passages"/"Mesh"), but the buttons' `onClick` only sets an `_editMode` flag; **no object/tile/passage editing tools were ever implemented** (only PASSAGES mode draws an overlay). The buttons render correctly (at x=0/125/250 — `setSize` does feed `getWidth()`) and are clickable (via `Container::drawAll`/`notifyMouseButtonPressed`); the *visual* garbling was `MapEditorState::drawContainer` calling `draw_hud()` — the in-game **player** HUD (FPS/help/debug/timer/game-status, all anchored at y=0) — which drew on top of the editor buttons. **Fix:** removed that `draw_hud()` call (1 line + explanatory comment); the editor now renders the world + mode buttons + minimap without the overlay. Build green (egolib-library 0 errors), egoboo boot smoke-run clean (exit 124, no errors). The in-game editor remains a stub for actual editing — the full editor is cartman (`run-cartman.sh`). Not a regression from the cartman work (different files; `MapEditorState` only had refactoring touches recently).

### T1.3 CameraSystem EngineContext DI seam — Pass 1: widen interface + migrate PlayingState (2026-06-07)

A scouting workflow over the remaining heavy-refactoring fronts found the CameraSystem seam **mid-flight, not "deferred"**: the seam already existed and was wired (`ICameraSystem`, `EngineContext::installCameraSystem/cameraSystem()`, `GameEngine.cpp:335/507` install/clear lifecycle), but **28 `CameraSystem::get()` sites** still bypassed it — 18 in `.cpp` (migratable), 10 in deferred `.c`. The blocker: `ICameraSystem` exposed only 3 methods (`updateAll`/`setNumberOfCameras`/`getCameraList`) while consumers use `getMainCamera()`×10, `getCameraOptions()`×6, `getCameraList()`×4, `getCamera()`×4, `renderAll()`×2. **Pass 1:** widened `ICameraSystem` with the 4 missing consumer-used methods (`getMainCamera`/`getCamera`/`getCameraOptions`/`renderAll`) — declaration-only, `CameraSystem` already implemented them (now marked `override`); the header stays light (forward-declares `CameraOptions`/`Ego::Graphics::TileList`/`EntityList`, adds `egolib/typedef.h` for `ObjectRef` + `<functional>`). Updated both `StubCameraSystem` test mocks (`EngineContext.cpp` gains `Camera.hpp`; `ScriptActionFunctions.cpp`) and migrated `PlayingState.cpp`'s 3 sites to `EngineContext::get().cameraSystem()`, swapping its `CameraSystem.hpp` include to the lighter `Camera.hpp` (decoupling it from the concrete class). Pure same-instance virtual-dispatch swap — no behavior change. Green: build 0 errors, validator `test.mod` warnings=0 errors=0, ctest 736/738 (#526/#527 only), smoke-run exit 124 clean (OpenGL + Image + Font init, clean shutdown). **`Audio/AudioSystem.cpp`'s 4 sites deliberately left on `CameraSystem::get()`** — it is a lower-layer subsystem; routing it through `game/Core/EngineContext.hpp` would be the layer inversion the program already avoided for TextureManager/`font_bmp.c`. With the interface fully widened, Passes 2–3 (remaining game-layer consumers: MapEditorState, Object_appearance, Player, GameEngine) are pure call-site swaps.

### T1.3 CameraSystem EngineContext DI seam — Pass 2: migrate remaining game-layer consumers (2026-06-07)

Migrated the remaining clean game-layer `.cpp` consumers off `CameraSystem::get()` onto `EngineContext::get().cameraSystem()` (pure call-site swaps — interface already widened in Pass 1, no interface/stub changes): **MapEditorState.cpp** (6 sites; include swapped `CameraSystem.hpp` → `Camera.hpp`), **Player.cpp** (1 site, `getCamera`; include swapped to `Camera.hpp`), **GameEngine.cpp:336** (the post-install `getCameraOptions().turnMode` sync — the install at `:335` and `CameraSystem::initialize()` at `:334` stay on the concrete singleton, so its `CameraSystem.hpp` include remains). **`Object_appearance.cpp` deliberately NOT migrated** (reverted): its 2 sites sit behind a `CameraSystem::is_initialized()` existence-guard, and routing that guard through `EngineContext::get().tryCameraSystem()` is **not behavior-preserving** — the singleton's "constructed" semantics and the seam's "installed" semantics diverge in the test bootstrap (the `ScriptActionFunctions` fixture installs a `StubCameraSystem` whose `getMainCamera()` is `nullptr`, so the seam guard passes where the `is_initialized()` guard short-circuited, turning a skip into a `nullptr->getTileList()` deref). Caught by ctest: 5 `ScriptActionFunctionsFixture` tests SEGFAULTed; reverting `Object_appearance.cpp` restored baseline. So `Object_appearance.cpp`'s 2 sites join the documented kept-on-`::get()` set with `Audio/AudioSystem.cpp` (×4, layering) and `GameEngine.cpp:335` (install). After Pass 2 the only `CameraSystem::get()` sites left are: the install (1), AudioSystem (4), `Object_appearance` (2), and 10 in deferred `.c` (`egoboo.c`, `game_wawalite.c`, `script_variables.c`, `graphic_hud.c`). Green: build 0 errors, validator `test.mod` warnings=0 errors=0, ctest 736/738 (#526/#527 only), smoke-run exit 124 clean. **LESSON: never migrate a `Foo::is_initialized()` existence-guard to an `EngineContext` seam accessor — they answer different questions in the headless test bootstrap.**

### T3.4 Physics characterization tests — pin the untested collision-math free functions (2026-06-07)

First T3.4 hardening pass: a new `egolib/tests/egolib/tests/PhysicsCollisionNormal.cpp` (sibling of `PhysicsIntersection.cpp`, auto-picked by the `GLOB_RECURSE CONFIGURE_DEPENDS` in `egolib/tests/CMakeLists.txt` — **zero CMake edits, zero production-code edits**, pure-math, no fixture/bootstrap, runs <1ms). Pins 12 cases across three previously-0-coverage public free functions in `physics.h`, chosen because they are pure and headlessly reachable (the genuinely-uncovered render/GUI surfaces need `GameEngine::initialize()` + a GL context the test bootstrap deliberately omits). Coverage: **`phys_expand_oct_bb`** (4 exact-value cases — zero-velocity passthrough, +X/−X swept-bound growth, and the `[tmin,tmax]` semantics where a non-zero `tmin` translates BOTH endpoints so the t=0 origin is excluded); **`phys_estimate_collision_normal` / `phys_estimate_pressure_normal`** (4 structural-characterization cases — partial overlap yields a unit normal dominated by the shallow axis with the correct sign; the `exponent != 1` cylinder-warp branch (`phys_warp_normal`) still yields a valid unit normal + positive depth; fully-separated boxes return false; a contained box delegates collision→pressure and the two calls return identical nrm/depth); **`apos_t`** displacement accumulator (4 exact cases pinning the non-obvious "evaluate = maxs+mins extrema, NOT the running sum" semantics, the per-component sign split, and the out-of-bounds-index throw). Float assertions use exact `EXPECT_FLOAT_EQ` only where the math is integer-clean (expand, apos) and `EXPECT_NEAR`/sign/axis-dominance for the normal estimators (`exponent=1.0` → no warp, plus one `exponent=2.0` warp-path case) to avoid over-pinning. Green: build 0 errors (file compiled + linked via re-glob), 12/12 new tests pass, validator `test.mod` warnings=0 errors=0, full ctest 748/750 — the only failures are the 2 pre-existing `ScriptLoaderFixture` cases (renumbered as the new file inserts alphabetically before them). Easy fast-follow if wanted: the `phys_data_t` accumulator wrappers (`sum_acoll`/`sum_avel`/`sum_aplat`) are equally pure and header-exposed.

### VFS cstdio-backend elimination — Pass 1: elide the dead `VFS_FILE_TYPE_CSTDIO` branches (2026-06-07)

First pass of the `vfs.c` dead-backend cleanup (the largest non-Object TU at 2,456 lines; chosen by a fresh scouting workflow as the highest-value heavy front after Object/singleton/uber-header work was exhausted). `vsf_file` was a tagged union over `VFS_FILE_TYPE_CSTDIO` (a libc `FILE *`) vs `VFS_FILE_TYPE_PHYSFS` (a `PHYSFS_File *`), but **`VFS_FILE_TYPE_CSTDIO` is never assigned anywhere in the source tree** (grep across egolib/idlib/idlib-game-engine/tools/cartman for `= VFS_FILE_TYPE_CSTDIO` returns zero — the only `type` writes are the three `= VFS_FILE_TYPE_PHYSFS` at the open sites), so the entire cstdio backend was dead code. Deleted the 33 `if (VFS_FILE_TYPE_CSTDIO == ...) { ... }` dead branches across ~22 functions (32 followed by `else if (PHYSFS)` → turned into a plain `if (PHYSFS)`; the lone irregular `vfs_printf` `if(CSTDIO){...} else {...}` had its `else` keyword dropped, leaving the PHYSFS call as an unconditional bare block) plus the `VFS_FILE_TYPE_CSTDIO` enum value and the union's `FILE *c` member. **Zero caller churn** — `vsf_file` is fully opaque (`vfs.h:187` forward-decl only; its internals are referenced nowhere outside `vfs.c`), so the public `vfs_FILE *` API is byte-identical. Mechanically applied via a brace-aware Python script (asserted 33 sites / 32 conversions / 1 printf-else), then audited by hand (incl. the two nested-brace blocks in `vfs_getc`/`_vfs_translate_error` and the `context->type` SDL_RWops field, which is unrelated and untouched). Behavior-preserving: every live `vfs_FILE` has `type == PHYSFS`, so each surviving `if (PHYSFS)` guard is always-true and the deleted branches were unreachable. **224 lines removed (2,456 → 2,232).** Green: build 0 errors, validator `test.mod` warnings=0 errors=0, full validator 42 modules errors=245 (pre-existing content baseline — include/dead-code edits can't move it), ctest 748/750 (only the 2 pre-existing `ScriptLoaderFixture` Missing/Invalid-PrimaryScript-fallback cases). Pass 2 (collapse the now-single-valued `type` tag + union + the PHYSFS guards + the corrupted-`else` arms) and Pass 3 (dedup the ~30 near-identical `vfs_read/write_*N` integer bodies into a templated PHYSFS helper, fixing the latent `sizeof(int8_t)`-where-`uint8_t`-intended bug at the old `vfs_read_Uint8`) follow.

### VFS cstdio-backend elimination — Pass 2: collapse the single-valued type tag and the union (2026-06-07)

With the cstdio backend gone (Pass 1), `vsf_file::type` is only ever `VFS_FILE_TYPE_PHYSFS` (the three open sites set it; nothing else writes it), so every surviving `if (VFS_FILE_TYPE_PHYSFS == X->type)` guard is always-true and `vfs_close`'s `else { "invalid vfs file descriptor" }` arm is unreachable. Collapsed `vsf_file` from `{ BIT_FIELD flags; vfs_file_type type; vfs_fileptr_t ptr; }` (a discriminated union) to a plain `{ BIT_FIELD flags; PHYSFS_File *p; }`: deleted the `vfs_file_type` enum (`UNKNOWN`/`PHYSFS`), the `vfs_fileptr_t` union (its `void *u` member was referenced nowhere), the `type` field, the 3 `type = VFS_FILE_TYPE_PHYSFS;` writes, the 32 always-true guard lines (left as `{ body }` bare scope blocks — bodies unchanged, and several declare locals like `cTmp`/`convert` that need the scope anyway), and `vfs_close`'s unreachable corrupted-`else`. Renamed the 18 `X->ptr.p`/`X.ptr.p` accessors to `X->p`/`X.p`. The SDL_RWops `context->type`/`rwops->type` fields are a different `type` (SDL's) and were left untouched. Mechanically applied via a brace-aware Python script (asserted 32 guards / 1 else / 3 assignments removed, and post-conditions `no VFS_FILE_TYPE` + `no ptr.p` remain). Behavior-preserving: the guards were always taken and the else never reached. **55 more lines removed (2,232 → 2,178; 278 total across Passes 1–2).** Green: build 0 errors, validator `test.mod` warnings=0 errors=0, full validator errors=245 (baseline), ctest 748/750 (only the 2 pre-existing `ScriptLoaderFixture` cases). Pass 3 (dedup the `vfs_read/write_*N` integer-helper boilerplate + the latent `sizeof(int8_t)`-in-`vfs_read_Uint8` fix, and tidy the bare blocks these passes left) follows.

### VFS cstdio-backend elimination — Pass 3: dedup the fixed-width read/write helper boilerplate (2026-06-07)

The 18 fixed-width binary helpers (`vfs_read_{Sint,Uint}{8,16,32,64}` + `vfs_read_float` + the 9 `vfs_write<T>` specializations) were all the same ~20-line shape post-Pass-2: declare `int retval; bool error;`, `retval = 0;`, a leftover `{ }` bare block doing `retval = PHYSFS_xxx(...); error = <cond>; if(error) flags|=ERR else flags&=~ERR;`, then `if(error) _vfs_translate_error(&file); return retval;`. Extracted the identical tail into one file-local `static int vfs_finish_io(vfs_FILE& file, bool ok, int retval)` (set/clear the error flag, translate on failure, return the raw PhysFS result unchanged), shrinking each helper to its single distinctive PHYSFS call (e.g. `int retval = PHYSFS_readSLE16(file.p, val); return vfs_finish_io(file, 0 != retval, retval);`). Kept each PHYSFS call **explicit per width** (no template/trait dispatch) so the per-type mapping stays visible and reviewable, and preserved the 64-bit pointer casts and the float `union { float; uint32_t; }` reinterpret paths verbatim. Behavior is byte-identical (`ok == !error`; same flag updates, same translate-on-failure, same `retval`). **Also fixed the latent `sizeof(int8_t)` in `vfs_read_Uint8` → `sizeof(uint8_t)`** (harmless — sizes are equal — but wrong on its face). Generated mechanically from a per-type table (no hand-transcription) and replaced the exact `vfs_read_Sint8`…`vfs_write<float>` span. **256 more lines removed (2,178 → 1,922; 534 total across Passes 1–3 — `vfs.c` is now down from 2,456 to 1,922, ~22%).** Green: build 0 errors, validator `test.mod` warnings=0 errors=0, full validator 42 modules errors=245 (baseline — the validator reads every module's `spawn.txt`/`data.txt`/MD2 geometry through these exact readers), ctest 748/750 (only the 2 pre-existing `ScriptLoaderFixture` cases), and a headless smoke-run (menu boot through OpenGL/font-atlas/image loading, all of which read assets via these helpers) exited clean. This completes the `vfs.c` cstdio-elimination front.

### T3.4 Characterization tests — batch 2: pin four more pure headless surfaces (2026-06-07)

Second T3.4 hardening batch — **four new auto-globbed test files, ~50 cases, zero production-code edits, zero CMake edits**, all pure (no fixture/VFS/GL/Object), surfaces chosen by a scouting workflow as the cheapest remaining 0-coverage targets. Each surface was deep-read for its **verbatim implementation** (a 4-agent analysis workflow extracted the exact bodies; expected values were then hand-derived from those bodies, not assumed). (1) **`BoundingBoxOps.cpp`** (suite `OctBB*`, ~18 cases) pins the `oct_bb_t` ops in `bbox.c` that `BoundingBox.cpp` left uncovered: both `contains` overloads (incl. the diamond-cut rejection where a point inside the X/Y AABB but outside the XY axis is rejected, and the empty-box short-circuits), the `join` family (vector/static-`src1,src2,dst`/restricted-index incl. the out-of-range `std::runtime_error`), `intersection` (overlap + disjoint-empty + both-empty-default), `interpolate` (midpoint lerp + exact `flip==0/1` endpoint return + empty-operand cases), `self_grow` (symmetric `abs()` growth), and the `to_points`→`points_to_oct_bb` round-trip (an axis-aligned box emits exactly 16 corner points that reconstruct the original 5-axis bounds; traced by hand against `bbox.c:30`). (2) **`MapTwist.cpp`** (`MapCalcTwist`/`MapTwistToNormal`/`MapTwistRoundTrip`, ~11 cases) pins `cartman_calc_twist` (the asymmetric `[-7,8]` clamp, `+7` bias, `(y<<4)+x` nibble-pack, flat=`0x77`) and `twist_to_normal` (flat→exactly `(0,0,1)`; the X/Y sign asymmetry — raising the low nibble tilts `+X` but raising the high nibble tilts `−Y`; the normal is always unit-length and upward). (3) **`ParticleRecoil.cpp`** (`GetRecoilFactors`, 10 cases) pins `get_recoil_factors`'s load-bearing branch order: the cross-assigned finite split `recoil_a=wtb/(wta+wtb)`, the equal/both-zero→½½ (equal-branch precedes the zero branch), the counterintuitive "a massless object takes ALL its own recoil", the infinite-mass normalization (`>=2^32`→negative-sentinel) and the "negative weight behaves like infinite", and the safe-`nullptr` out-params. (4) **`LogicDamageAttribute.cpp`** (`LogicDamage`/`LogicAttribute`, ~10 cases) pins the header-only enum maps in `Logic/{Damage,Attribute}.hpp`: `DamageType_isPhysical` (only SLASH/CRUSH/POKE), `DamageType_getColour` (per-type singleton mapping, the three physical types aliasing the one `white()` object via address-identity, and the `DAMAGE_DIRECT`/`COUNT`→`std::runtime_error` throw), `toString` (incl. the surprising `MAX_MANA`→"Mana"/`ACCELERATION`→"Speed" and the `idlib::unhandled_switch_case_error` default), `resistFromDamageType`/`modifierFromDamageType` (full 8-type maps + `NR_OF_ATTRIBUTES` sentinel), and `isOverrideSetAttribute` (the `*_MODIFIER`-true vs `*_RESIST`-false contrast). Assertions follow the established discipline: `EXPECT_FLOAT_EQ`/`EXPECT_EQ` only where integer-clean, `EXPECT_NEAR`/sign/unit-length structural checks for float-derived normals, `EXPECT_THROW` for documented throws. Green: build 0 errors, **50/50 new tests pass**, full ctest **798/800** (only the 2 pre-existing `ScriptLoaderFixture` cases, now renumbered #588/#589 as the new files insert alphabetically before them), validator unaffected (test-only). Tests-to-code coverage continues its upward trend toward protecting behavior before the next restructuring wave.

### egolib include-level decoupling (logging seam) — Pass 1: retarget 6 logging-only leaf TUs off the EngineContext hub (2026-06-08)

First pass of a new structural front (roadmap T3.7), chosen by a fresh `scout-next-heavy-front` workflow as the one remaining candidate that is simultaneously heavy, fully headless-verifiable, low-risk, and not stale. The dominant directional-include violation in egolib is the app-layer service hub `game/Core/EngineContext.hpp`: **51 non-game leaf TUs reach UP into it**, a large share for nothing but `EngineContext::get().logTarget()`. The lower-layer-safe `Log::activeTarget()` seam (`egolib/Log/_Include.hpp`) already exists and resolves through the **same** installed `activeLogTarget` pointer (both route through `EngineContext::tryLogTarget()` → `activeLogTarget`), only adding a default-target fallback in the uninstalled bootstrap edge — so the swap is behavior-identical in every exercised path and architecturally correct (leaf code should log through the Log seam, not the app hub). Retargeted the 6 leaf TUs whose *only* EngineContext use was logging — `FileFormats/{map_file,map_tile_dictionary,wawalite_file}.c`, `Extensions/ogl_extensions.c`, `InputControl/ControlSettingsFile.cpp`, `Logic/PerkHandler.cpp` — by swapping `EngineContext::get().logTarget()` → `Log::activeTarget()` and dropping/replacing the `EngineContext.hpp` include (5 already had the Log seam header; PerkHandler.cpp had its include swapped). **Cuts 6 leaf→game upward includes (51 → 45).** No GL/render path touched. Green: build 0 errors, validator `test.mod` warnings=0 errors=0, ctest 798/800 (only the 2 pre-existing `ScriptLoaderFixture` cases).

### egolib include-level decoupling (logging seam) — Pass 2: retarget 11 more logging-only leaf TUs (2026-06-08)

Same behavior-preserving swap applied to the 11 remaining non-game leaf TUs whose only EngineContext use was `logTarget()`: `AI/AStar.cpp`, `egoboo_setup.c`, `file_common.c`, `font_bmp.c`, `Graphics/{FontManager,MD2Model,ModelDescriptor}.cpp`, `map_functions.c`, `Renderer/OpenGL/{Renderer,Utilities}.cpp`, `typedef.c` (3 already had the Log seam header → include dropped; 8 had it replaced). **Cuts 11 more leaf→game upward includes (45 → 34).** **`vfs.c` deliberately excluded:** its `tryLogTarget()` at `vfs.c:161` is an intentional bootstrap-edge guard that chooses engine-logging vs raw `stderr` based on whether the *engine* target is installed; `Log::tryActiveTarget()`'s default-target fallback would change that decision, so vfs.c was deferred to the ownership-move pass (which adds the exact-semantics `Log::tryInstalledTarget()`). Green: build 0 errors, validator `test.mod` 0/0, ctest 798/800.

### egolib include-level decoupling (logging seam) — Pass 3: move active-log-target ownership into the Log subsystem (2026-06-08)

The keystone. `Log/_Include.cpp` reached UP into `game/Core/EngineContext.hpp` (its *only* upward dependency) to resolve the engine-installed override inside `tryActiveTarget()` — the residual inversion that kept the whole logging seam from making Log link-cleavable. Relocated the override pointer (formerly the anonymous-namespace `activeLogTarget` in `EngineContext.cpp`) into `Log/_Include.cpp` as `g_activeTarget`, and added Log-owned entry points `Log::installActiveTarget` / `clearActiveTarget` / `tryInstalledTarget`. `Log::tryActiveTarget()` now resolves the override locally, so the `EngineContext.hpp` include is dropped from `Log/_Include.cpp` — **the Log subsystem no longer reaches into `game/` anywhere and is now a clean downward leaf** (idlib + own headers only; verified by `grep -r game/ egolib/.../Log/` → empty). `EngineContext`'s `installLogTarget`/`clearLogTarget`/`tryLogTarget` became thin **downward** delegators to the Log subsystem (`EngineContext.cpp` only — the `EngineContext.hpp` declarations are unchanged, so all ~120 `EngineContext::get().logTarget()` callers keep compiling untouched). Every semantic is preserved exactly: `installLogTarget` still throws on double-install, `logTarget()` still throws when uninstalled, and `tryLogTarget()` still returns the raw nullptr-if-uninstalled that `vfs.c`'s guard depends on (`tryInstalledTarget()` returns the override without the default fallback). 3 files touched (`Log/_Include.{hpp,cpp}`, `EngineContext.cpp`). Green: build 0 errors, validator `test.mod` 0/0, ctest 798/800, **smoke-run** boots OpenGL/image/font/audio + font atlas and shuts down clean ("See you next time") with an empty error scan — confirming both the relocated leaf seam (`egoboo_setup.c`, `FontManager.cpp` log through `Log::activeTarget()`) and the EngineContext delegators (`ImageManager`, `Font.cpp`, `GameEngine`) resolve logging correctly at runtime against the relocated ownership. **Front result: 18 leaf→hub upward includes cut (51 → 33, −35%) and the Log subsystem made link-cleavable** — Passes 1–2 cut 17 logging-only leaf TUs (51 → 34), and Pass 3 cut the 18th, `Log/_Include.cpp`'s own upward include (34 → 33). Remaining 33 leaf includers are genuine *service*-hub users (config/audio/particle/profile/image), blocked on the deeper service-DI work — a separate, harder front, not a logging cut.

### T3.4 characterization — combat damage resolution on a live Object (2026-06-08)

The **first LIVE-fixture** T3.4 batch (prior batches were pure-math, no fixture). New auto-globbed `egolib/tests/egolib/tests/CombatDamageResolution.cpp` (**zero production-code edits, zero CMake edits**), 11 cases pinning the core combat damage-scaling surface in `Entities/Object_combat.cpp` on a spawned `follower.obj` via the proven `ContentRuntimeBootstrap` + `ObjectHandler::insert` path (mirrors `ObjectAccessors.cpp`'s fixture — installs AudioSystem [`EGOBOO_DISABLE_AUDIO=1`] + ParticleHandler; **no GL context / `GameEngine::initialize`**). The driving inputs (the per-damage-type `*_RESIST` + `*_MODIFIER` attributes and `DEFENCE`) are fully controlled via `setBaseAttribute` *through the same `Ego::Attribute::resistFromDamageType`/`modifierFromDamageType` maps the implementation uses*, and `Object::getAttribute` returns those base values unchanged for an object with no temp (enchant) attributes (none of resist/modifier/DEFENCE hit `getAttribute`'s special-case switch), so no profile default leaks in. Expected values are **hand-derived from the verbatim impls** (and the no-perk assumption is made explicit/self-validating via `ASSERT_FALSE(hasPerk(STALWART))`, the only perk that touches SLASH/POKE resistance). Coverage: **`getRawDamageResistance`** — out-of-range (`DAMAGE_DIRECT`/`DAMAGE_COUNT`)→0; positive passthrough without armor; `+DEFENCE/14` bonus with armor (5+14/14=6); the `DAMAGEINVERT` modifier bit gating that bonus off; negative weakness unreduced without armor; weakness softened by `1−DEFENCE/512` with armor (−4·0.5=−2). **`getDamageReduction`** — out-of-range→0; the `DAMAGEINVICTUS` modifier bit short-circuiting to full `1.0` immunity *before* resistance is computed; the positive diminishing curve `(r·0.06)/(1+r·0.06)` (r=10→0.375); the negative-resistance amplification `1−0.94^r` (r=−2→≈−0.1317, a negative reduction = extra damage). **`isInvictusDirection`** — the `isInvincible()` short-circuit is direction-independent (true for every facing); the profile-frame-dependent directional arc is intentionally left for a future targeted pin. Green: build 0 errors, **11/11 new pass** (verified directly — expected values matched actual behavior first try), full ctest **809/811** (only the 2 perennial `ScriptLoaderFixture` cases, now #599/#600 as the new file sorts alphabetically before them). Validator unaffected (test-only). Natural next batch: `Object::damage(...)` integration (life/mana effects, alerts) and the `do_chr_prt_collision` pipeline, which need richer multi-object/attacker setup.

### egolib include-level decoupling (service-hub seams) — Passes 1–11 (2026-06-08)

The continuation of the T3.7 logging slice onto the *service* hub. After the logging slice the dominant directional violation was that **33 non-game leaf TUs still reached UP into the app-layer service hub `game/Core/EngineContext.hpp`** — not for logging this time, but for `profileSystem`/`config`/`particleHandler`/`imageManager`/`audioSystem`. This front cut that to **8** (−25, −76%) over eleven verified passes on branch `refactor/egolib-service-hub-decoupling` (not yet merged). The maintainer chose the **free-function `active*()` accessor** seam style over a direct `Singleton::get()` swap, which kept the front uniform with the `Log::activeTarget()` precedent **and** *reduced* the tracked `::get()` count (replacing each `EngineContext::get().X()` with a `::get()`-free `activeX()`): total egolib `::get()` fell **895 → 794** (−101), so the include-decoupling and T1.3 goals aligned rather than traded off.

**Two seam patterns, chosen per service:**
- **Sugar over an existing lower-layer singleton** — for `profileSystem` (`activeProfileSystem()` → `ProfileSystem::get()`, in `IProfileSystem.hpp`/`ProfileSystem.cpp`) and `imageManager` (`Ego::activeImageManager()` → `ImageManager::get()`). The singleton already lives below the hub and is exactly what the hub re-publishes, so no ownership move is needed; behaviour is identical on every exercised path. (One name collision: `EngineContext.cpp`'s anon-ns `activeProfileSystem` pointer was renamed `g_activeProfileSystem` to free the accessor name — the `g_` convention from the Log keystone.)
- **Ownership-move keystone (Log-keystone style)** — for `config` (`Ego::{install,clear,try,}activeConfig` in `egoboo_setup.{h,c}`, owning `g_activeConfig`), `particleHandler` (in `IParticleHandler.hpp`/`ParticleHandler.cpp`), and `audioSystem` (in `IAudioSystem.hpp`/`AudioSystem.cpp`). The installed pointer was moved out of `EngineContext.cpp`'s anonymous namespace into the service's own subsystem, and `EngineContext`'s install/clear/try methods became thin downward delegators (the `.hpp` is unchanged, so all existing callers keep compiling and `config()`/`particleHandler()`/`audioSystem()` keep their exact throwing contract). **The keystone is mandatory for particle and audio** because their tests install a recording stub *through* `EngineContext` and assert that the subsystem code routes through the installed stub — a `ParticleHandler::get()`/`AudioSystem::get()` swap would bypass the stub. `config` has no stub hazard but used the keystone anyway to stay `::get()`-free; its `activeConfig()` falls back to `egoboo_config_t::get()` when uninstalled (the accepted logging-slice bootstrap-edge divergence), while `activeParticleHandler()`/`activeAudioSystem()` throw when uninstalled (no fallback) to preserve the exact app-hub semantic.

**Pass sequence** (each green: build 0 errors · validator `test.mod` 0/0 + full `errors=245` · ctest **809/811**): P1 `ObjectHandler.cpp`+`script.c` (profile+log, 33→31); P2 delete the genuinely-dead include in `ObjectProfile_internal.h`; P3 `imageManager` cuts `fileutil.c`+`DefaultTexture.cpp` (→28); **P4 config keystone** (smoke-run verified); P5 four config-only leaves `Utilities`/`RendererInfo`/`DeferredTexture`/`ModuleProfile` (→24); **P6 particle keystone** (8/8 `ModuleUpdate` installed-handler sentinels); P7–P8 Entities particle+profile swaps, dropping `Object_appearance`+`Particle_combat`/`spawn`/`update` (→20); P9 the graphics/image cluster `Font`/`GraphicsSystem`/`GraphicsContext`/`GraphicsWindow`/`TextureManager`/`ImageManager.cpp`/`ParticleHandler.cpp` (config+image+log, each EngineContext include replaced with the precise seam header it was getting transitively); P10 `vfs.c` (log + the stderr-guard `tryLogTarget()` → `Log::tryInstalledTarget()`, raw-nullptr semantics preserved) (→12); **P11 audio keystone** + audio leaf swaps, dropping `Object_interaction`/`Object_lifecycle`/`Particle_core`/`Enchant` (27/27 audio-stub sentinels `ScriptActionFunctions`/`ConfigMutations`/`ContentParsers`/`GameplayAlert`) (→**8**). Config, particle, and audio keystones plus the Pass-9 render path were each menu-smoke-run verified (exit 124, OpenGL 4.6 boot + image/font/atlas render + clean audio init, no throws).

**The remaining 8 are genuinely blocked or out of scope** (do not treat as easy follow-ons): `App.cpp` and `Core/System.cpp` are the **bootstrap installers** that publish services *into* the hub (the legitimate downward direction — exclude, not violations); `Object_attributes`/`Object_combat` are blocked on `billboardSystem` (not an `idlib::singleton`, owned in `game/` — no lower-layer seam without relocating ownership) plus `perkHandler`; `ObjectProfile_export`/`ObjectProfile_load`/`Object_attributes` are blocked on `perkHandler` (a stub-hazard service → needs the ownership-move keystone, low ROI: a perk keystone fully drops only `ObjectProfile_export`); `Console.cpp` needs `fontManager`/`graphicsSystem`/`inputSystem` keystones (graphics/input carry stub hazards); and **`Object_internal.h` is not a clean leaf** — it includes ~12 `game/` headers (`GameEngine.hpp`, `PlayingState.hpp`, `game.h`, `CameraSystem.hpp`, `Billboard.hpp`, …), of which `EngineContext.hpp` (for `tryActivePlayingState()`, which returns an app-layer `PlayingState`) is just one, so cutting it alone is cosmetic; the Object_* implementation cluster is genuinely game-coupled and belongs to a separate, much larger Entities↔game decoupling front. With the service-hub seams now in place, the lib-split blocker shifts from the EngineContext fan-in to that Entities↔game coupling and the residual perk/billboard/graphics seams.

### Entities ↔ game include-decoupling (propagating headers) — Passes 1–6 (2026-06-08)

The Entities↔game coupling the service-hub front named as the next lib-split blocker. The structural harm lives in **propagating headers** (a header's `game/` include flows into every consumer); the Entities layer has four — `Common.hpp`, `IRenderable.hpp`, `Object.hpp`, `Particle.hpp`. A scouting workflow (5 parallel per-edge probes → ranked plan) classified every `game/` include by usage in the including header (by-value member / base / pointer / unused / relocatable). **Branch `refactor/entities-game-decoupling`, 6 verified passes; not yet merged.** Result: propagating-header `game/` edges **15 → 7 (−53%)**, with `Common.hpp` and `IRenderable.hpp` now **fully game-free** and the 7 survivors all genuine by-value compositions / base classes (the deferred hard core). Full mechanics, the three relocation patterns, and the deferred boundary in [74-entities-game-decoupling.md](74-entities-game-decoupling.md).

**The load-bearing gotcha — dead-in-header ≠ safe-to-remove.** An include can be unused by the header itself yet be a **transitive conduit** that downstream TUs free-ride on; removing it compiles the header but breaks the free-riders. Method: after each removal, run a **keep-going build** (`cmake --build build -j4 -- -k`) to enumerate the *complete* free-rider set in one pass, then add the precise direct include each free-rider actually uses (IWYU). Every free-rider in this front was a game-layer TU or a test — a clean `game→game`/`game→egolib-core` include, no new upward coupling (~11 TUs fixed across the passes).

**Pass sequence** (each green: build 0 · validator `test.mod` 0/0 · ctest **809/811** — the 2 perennial `ScriptLoaderFixture` cases #599/#600 — · render-path passes smoke-run exit 124): **P1** drop 4 dead game/ includes (`Common.hpp`→`mesh.h`, `Object.hpp`→`BillboardSystem.hpp`, both `*_internal.h`→`CharacterMatrix.h`) + IWYU 5 free-riders (`GameEngine.cpp`/`graphic_scene.c` +BillboardSystem.hpp; `PlayingState`/`MiniMap`/`LevelUpWindow` +`Time/Time.hpp`); **P2** relocate `ONESECOND` (50 UPS) from the `game/egoboo.h` monolith down to `egolib/egolib_config.h` (transparent — egoboo.h reaches it via `typedef.h`); **P3** drop `Object.hpp`→`graphic_mad.h` + `Particle.hpp`→`graphic_prt.h` (each pulled the `egoboo.h` monolith into every consumer) + IWYU the 2 render-pass free-riders (`EntityReflections`/`EntityShadowsRenderPass`); **P4** `git mv` the mislocated `game/Graphics/Vertex.hpp` primitive down to `egolib/Graphics/Vertex.hpp` + forward-declare `GLvertex` in `IRenderable.hpp` → **IRenderable.hpp game-free**, no free-riders; **P5** split `game/physics.h` — extract the pure-math primitives (`orientation_t`/`apos_t`/`phys_data_t`/`PLATTOLERANCE`/`PLATFORM_STICKINESS`/`PHYS_PLATFORM`) to a new lower-layer `egolib/PhysicsData.h`, keep the game-aware `phys_expand_*` function decls in `physics.h`; repoint `Common.hpp`+`Object.hpp` → **Common.hpp game-free** + IWYU 4 free-riders (`CollisionSystem.cpp`/`ObjectPhysics.cpp`/`ObjectProfile_export.cpp`/`tests/ScriptMovementFunctions.cpp`); **P6** drop `Object.hpp`'s redundant direct `Module.hpp` (still flows via the `Collidable.hpp` base) so its remaining 4 game/ includes are exactly the by-value compositions.

**Final state.** `Common.hpp` 2→0, `IRenderable.hpp` 1→0 (both game-free), `Object.hpp` 8→4, `Particle.hpp` 4→3. The **7 remaining edges are all genuine compositions** — `Object`: `Inventory`(member)/`Collidable`(base)/`ObjectPhysics`(member)/`ObjectGraphics`(member); `Particle`: `ParticleGraphics`(member)/`Collidable`(base)/`ParticlePhysics`(member). **Deferred / out of scope:** the by-value composition core needs relocating game service classes a layer down or inverting ownership (flag-day scale); the `Collidable` base (→`Module.hpp`) is reducible only via an `ICollidable` extraction gated behind T3.4 collision characterization tests; the internal headers + impl `.cpp`s are non-propagating and belong to the deeper Entities↔game `.cpp` front.

### Entities ↔ game — Collidable base-class `Module.hpp` conduit-cut (2026-06-08)

The propagating-header front (P6 above) left the `Collidable` base class as the **sole header conduit** still dragging `game/Module/Module.hpp` UP into *both* `Object.hpp` and `Particle.hpp` (they publicly derive from `Ego::Physics::Collidable`, and `Collidable.hpp` included `Module.hpp`). Doc 74 had deferred this as "reducible **only** via an `ICollidable` extraction gated behind T3.4 tests" — a re-scout found that **partially stale**: the `Collidable` *interface* never names `GameModule` (it uses only `Index1D` + `mesh_wall_data_t` + idlib vectors, all from `game/mesh.h`, which does **not** include `Module.hpp`); the real `GameModule` use (`isInside`/`getMeshPointer`/`getTileIndex`) lives entirely in `Collidable.cpp`. So the edge was cut **without** any `ICollidable` extraction: swap `Collidable.hpp`'s `#include Module.hpp` → `#include "egolib/game/mesh.h"`, and add the direct `#include Module.hpp` to `Collidable.cpp` (which previously got the full def transitively through its own header; `GameSessionContext.hpp` only forward-declares `GameModule`).

**Free-rider fan-out (the doc-74 keep-going IWYU method, at scale).** Because the conduit ran through `Object.hpp`/`Particle.hpp` — included nearly tree-wide — the swap exposed **32 free-rider TUs** that had been leeching not just `GameModule` but the heavier transitive load `Module.hpp` carried (`fileutil.h`/`ReadContext`/`vfs_*`, `Ego::Renderer`, `AudioSystem`). A `-- -k 0` build enumerated the complete set in one pass; each was fixed with the precise include it actually uses (all clean `game→game` / `game→egolib-core` — no new upward coupling): **Module.hpp** added to `Entities/Object_internal.h` (one edit fixes all 7 `Object_*.cpp`) + 11 standalone TUs (`ParticleHandler.cpp`, `ObjectGraphics.cpp`, `ObjectPhysics.cpp`, `CollisionSystem.cpp`, `ParticlePhysics.cpp`, `Player.cpp`, `Inventory.cpp`, `Passage.cpp`, `CharacterMatrix.c`, `script_variables.c`, `tests/ObjectAccessors.cpp`); **`Renderer/Renderer.hpp`** added to 8 graphics TUs (the 4 entity render passes + `CameraSystem.cpp` + `BillboardSystem.cpp` + `graphic_prt.c` + `graphic_mad.c`); **`fileutil.h`** added to 5 (`ProfileSystem.cpp`, `Audio/AudioSystem.cpp`, `ObjectProfile_export.cpp`, `ObjectProfile_load.cpp`, `link.c`); **`Audio/AudioSystem.hpp`** to `GameEngine.cpp`. 28 files +1 include each, plus `Collidable.{hpp,cpp}`.

**Measured result** (g++ `-H` syntax-only closure on each header, before/after): `Object.hpp` game/ transitive closure **14 → 7 (−50%)**, `Particle.hpp` **13 → 6 (−54%)** — the *entire* `game/Module/` subtree dropped from both (`Module.hpp` + `AnimatedTiles.hpp` + `damagetile_instance.h` + `Fog.hpp` + `Water.hpp` + `Weather.hpp` + `module_spawn.h`), and therefore from the wide consumer set that pulls these two headers. Green: build 0 errors, validator `test.mod` 0/0, ctest **815/817** (the only 2 failures are the perennial `ScriptLoaderFixture` cases, now #605/#606). **Residual:** `Collidable.hpp` still depends on `game/mesh.h` (a `game/` header), so the base is *not* fully game-leaf — a true `ICollidable` pure-interface extraction (relocating the position/tile accessors + the `mesh.h` dependency a layer down) is the next step, now unblocked by the T3.4 combat/collision net below + the existing position/wall-math fixtures.

### T3.4 characterization — combat damage *integration* on a live Object (2026-06-08)

The integration follow-on to the pure-math `CombatDamageResolution.cpp` batch, and the **behavioral net that gates the deeper `ICollidable` / collision-pipeline surgery**. New auto-globbed `egolib/tests/egolib/tests/CombatDamageIntegration.cpp` (**zero production edits beyond the IWYU above, zero CMake edits**), 6 cases pinning the full integrated `Object::damage(...)` side-effect chain in `Entities/Object_combat.cpp`. Unlike the pure-math batch (which spawned into a *local* `ObjectHandler`), these spawn the follower **through the active test module** (`ShopInteractions`-style `beginActiveTestModule` + `module.spawnObject`) — **required** because `Object::kill(...)` iterates `activeModule().getObjectHandler()`. Bring-up installs AudioSystem (`EGOBOO_DISABLE_AUDIO=1`) + ParticleHandler + a recording `StubBillboardSystem`; **no GL context / `GameEngine::initialize`**. Determinism: `rand==0` makes `Random::next(base,base)==base`, and the reduction inputs are zeroed via `setBaseAttribute` + `ignoreArmour`, so `actual_damage == base_damage` exactly; expected life deltas are `FP8_TO_FLOAT(base)`, hand-derived. follower `Blud=False` → no blud particle; not a player → no difficulty scaling.

**Coverage:** non-lethal SLASH hit (life −= 2.0 exactly, `isAlive`, `damage_timer == DAMAGETIME`, `ALERTIF_ATTACKED` after clearing the gate, `careful_timer` re-armed to `CAREFULTIME`); lethal hit → `Object::kill` (`!isAlive`, `_currentLife == -1.0`, `_hasBeenKilled`); dead-victim guard (second hit returns 0, no state change); invictus guard (`invictus && !ignoreInvictus` → 0/unchanged, then lands when `ignoreInvictus`); zero-damage guard.

**Two behavioral findings surfaced (turned into pinned assertions, not silent fudges):** (1) a freshly-spawned object starts with `careful_timer == CAREFULTIME (50)`, which gates `updateLastAttacker` — so its **first** incoming hit does *not* raise `ALERTIF_ATTACKED` (the friendly-fire suppressor) even though life loss + hurt timer still apply. (2) `kill()` sets `ALERTIF_KILLED` (`:547`) but then runs the victim's AI script one last time (`scr_run_chr_script` at `:581`), which **consumes** the alert — so `ALERTIF_KILLED` is not observable on the victim post-`kill`; the persistent witness (set at `:579`, before the final tick) is `_hasBeenKilled`. **Deferred:** the `DAMAGEMANA`/`DAMAGECHARGE`/`DAMAGEINVERT` modifier branches (the mana-shield two-step at `Object_combat.cpp:115-133` + the `heal()` route) interact with `setMana()` clamping in a non-obvious, arguably buggy way — they deserve a focused pass with the mana accessors characterized first. Green: build 0, **6/6 new pass**, full ctest 815/817.

### Entities ↔ game — Collidable header made fully game-include-free (`ICollidable` decoupling) (2026-06-08)

The follow-on the conduit-cut named as "now unblocked," gated by the `CombatDamageIntegration.cpp` net. The conduit-cut had left `Collidable.hpp` with one residual `game/` include — `game/mesh.h` — the **last** game dependency on the base class both `Object` and `Particle` publicly derive from. Cut it so `Collidable.hpp` is **fully game-include-free (0 `game/` include edges)**. The header needs only mesh *primitives*: `Index1D` (a by-value `_tile` member → full type, from the game-free `egolib/Mesh/Info.hpp`), the idlib vector aliases (`egolib/integrations/math.hpp`), and `BIT_FIELD` (`typedef.h`). The one game-mesh type in the interface, `mesh_wall_data_t` (it embeds a `const ego_mesh_t*` + has `ego_mesh_t`-based ctors, so it can't itself relocate down), is used only **by reference** in the two `hit_wall` overloads → **forward-declared** (the doc-74 P4 `GLvertex` pattern); its definition is supplied to the deriving impls (`Object_appearance.cpp`/`Particle_core.cpp`) and to `Collidable.cpp`, which pull `mesh.h` transitively via `Module.hpp`.

**Free-rider fan-out:** only 3 TUs (mesh.h is far lighter than Module.hpp) — `BillboardSystem.cpp`, `ParticleGraphics.cpp`, `ObjectProfile_core.cpp` — had been leeching mesh symbols (`Info<float>::Grid::Size()`, `MAPFX_IMPASS`, `ego_mesh_t`) through the `Object/Particle.hpp → Collidable.hpp → mesh.h` path; each given a direct `egolib/game/mesh.h` (clean game→game IWYU).

**Measured result** (g++ `-H` closure): `Object.hpp` game/ closure **7 → 5**, `Particle.hpp` **6 → 4** — both shed `game/mesh.h` *and* `game/lighting.h` (mesh.h pulled lighting.h), since `mesh.h` was reachable from these two headers **only** via the `Collidable` base (verified: none of the by-value member headers `Inventory`/`ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics`/`ParticlePhysics` pull it). Across the two passes of this front, `Object.hpp` is down **14 → 5** and `Particle.hpp` **13 → 4**. The 5 remaining `Object.hpp` game/ edges are `Collidable.hpp` (base, now game-include-free), the three by-value member headers (`Inventory`/`ObjectPhysics`/`ObjectGraphics`), and `CharacterMatrix.h`. Green: build 0 errors, validator `test.mod` 0/0, ctest **815/817**.

**Residual / next step.** `Collidable.hpp` is game-include-free, but `Collidable` still physically lives in `game/Physics/` and `Collidable.cpp`'s `setPosition`/`setSpawnPosition` call `activeModule()` (GameModule) for the bounds-check + tile lookup — addressed by the next entry.

### Entities ↔ game — Collidable decoupled from GameModule + relocated to a lower layer (2026-06-08)

The capstone of the Collidable front: removed the last *runtime* coupling (`Collidable.cpp`'s `activeModule()` calls) and physically relocated the now-fully-game-free `Collidable` into the lower-layer `egolib/Physics/`, alongside a new collision-world seam.

**The seam.** `Collidable::setPosition`/`setSpawnPosition` validated positions via `GameSessionContext::get().activeModule()` — `GameModule::isInside(x,y)` (a mesh-bounds check: `_mesh->_tmem._edge_*`) and `getMeshPointer()->getTileIndex(point)` (a tile lookup). Both are pure mesh queries over lower-layer types (`bool`/`float`/`Index1D`/`Vector2f`). Extracted a lower-layer abstract interface `egolib/Physics/ICollisionWorld.{hpp,cpp}` (`Ego::Physics::ICollisionWorld`; 2 methods `isInside` + `getTileIndex`) with installed-pointer free-function accessors (`installCollisionWorld`/`clearCollisionWorld`/`activeCollisionWorld`/`tryActiveCollisionWorld`) — the established `active*()` seam (Log / service-hub keystone style; `g_activeCollisionWorld` ownership lives in the lower-layer `.cpp`). `GameModule` now `: public Ego::Physics::ICollisionWorld` (its existing `isInside` becomes the `override`; a thin `getTileIndex` delegating to `_mesh->getTileIndex` was added). `GameSessionContext::beginModule` installs the active GameModule as the collision world — right after construction, **before** `spawnAllObjects()` (which positions objects through `Collidable::setPosition`) — and `quitModule` clears it; this exactly tracks the `_activeModule` lifetime (verified the only set/reset points). `activeCollisionWorld()` throws `std::logic_error` when uninstalled, mirroring `activeModule()`. `Collidable.cpp` now references **no game symbol** (`GameModule`/`activeModule`/`getMeshPointer` all gone).

**Relocation.** With the runtime coupling gone, `git mv`'d `Collidable.{hpp,cpp}` from `game/Physics/` → `egolib/Physics/` (co-located with `ICollisionWorld`, the seed of a future `egolib-physics` sub-library); updated the 3 includers (`Object.hpp`/`Particle.hpp`/`Collidable.cpp`) + the CMake source list. `Collidable.cpp` now has **0 `game/` include edges**; `Collidable.hpp` is a pure lower-layer header.

**Result.** `Collidable` is a fully lower-layer component. `Object.hpp` game/ closure **5 → 4**, `Particle.hpp` **4 → 3** (the base header left the game/ count). **Across the entire Collidable front (Module.hpp conduit-cut → ICollidable header → activeModule decouple + relocate): `Object.hpp` 14 → 4, `Particle.hpp` 13 → 3.** The 4 + 3 remaining edges are exactly the by-value composition members (`Inventory`/`ObjectPhysics`/`ObjectGraphics`; `ParticleGraphics`/`ParticlePhysics`) + `CharacterMatrix.h` (Object) / `egoboo.h` (Particle) — the deferred hard core. Green: build 0 errors, validator `test.mod` 0/0, ctest **815/817**, and a **menu smoke-run** boots clean (SDL video / OpenGL context / font atlas) and shuts down clean ("exiting Egoboo 2.9.0. See you next time") with no crash/assert/throw. The seam's runtime path (install → spawn → `setPosition` → clear) is covered by the headless `ModuleSpawnRealization`/`ShopInteractions`/`CombatDamageIntegration`/`ObjectAccessors` fixtures.

**Remaining toward an actual `egolib-physics` sub-library:** `Collidable`'s sibling physics TUs (`CollisionSystem`/`ObjectPhysics`/`ParticlePhysics`/`particle_collision`) still live in `game/Physics/` and remain game-coupled; `Collidable` + `ICollisionWorld` + `PhysicsData.h` are the first lower-layer physics nucleus. Splitting `egolib-library` into actual link targets is still gated on the broader Entities↔game `.cpp` coupling.

---

## Entities↔game `.cpp` / internal-header decoupling slice (2026-06-09)

Branch `refactor/entities-game-cpp-decoupling`. The "broader Entities↔game `.cpp` coupling" front the propagating-header + Collidable work kept naming as *next*. Target: the **shared internal infrastructure headers** `Object_internal.h` (included by 7 `Object_*.cpp`) and `Particle_internal.h` (included by 4 `Particle_*.cpp`) — semi-propagating headers where a `game/` include flows into every sibling impl TU. Full recipe + per-pass table in [74-entities-game-decoupling.md](74-entities-game-decoupling.md) ("The `.cpp` / internal-header slice").

**Core technique — header-used vs. conduit-only includes.** A shared header's `game/` includes split into *header-used* (referenced by its own inline helpers — must stay) and *conduit-only* (present only so consumer `.cpp`s free-ride — the reduction target: push down to the precise consumer via IWYU, enumerated with a keep-going build `-- -k 0`). Watch the within-TU cascade: a first compile error in a TU can suppress a later one in the same TU (hit on `Object_appearance.cpp`: `CameraSystem` error masked a `TileList` incomplete-type error until the first was fixed).

- **Pass A** — routed the four active `GameEngine::GAME_TARGET_UPS` (=50) `Entities/` sites onto the existing low-layer `ONESECOND` seam (`egolib_config.h`; value-identical, semantically exact for UPS scaling, and already the established Entities UPS constant — `Object_update.cpp` used *both* names for 50). Dropped `GameEngine.hpp` from `Enchant.cpp` (its only `GameEngine` use). `GameEngine` is now referenced nowhere in `Entities/`, turning its `Object_internal.h`/`Particle_internal.h` edge into a *dead* conduit (enabled E/H).
- **Pass E** — confined the **minimap reveal chain** out of `Object_internal.h`: moved `tryActivePlayingState()` (sole consumer `Object_update.cpp`) into that TU; dropped `EngineContext.hpp` (only that helper used it in the header), `GameEngine.hpp` (dead post-A), `PlayingState.hpp`, `MiniMap.hpp`. **12 → 8** game/ includes; 0 free-riders (the two billboard `.cpp`s already self-included `EngineContext`).
- **Pass F** — pushed the conduit-only includes down: `CameraSystem`/`TileList` → `Object_appearance`; `Billboard` → attributes/combat; `Player` → combat; plus the hidden-transitive `Physics/PhysicalConstants.hpp` (`CHR_INFINITE_WEIGHT`/`CHR_MAX_WEIGHT`) to appearance/attributes/update. `script_implementation.h` + header-side `TileList.hpp` were dead/redundant. **8 → 3**.
- **Pass G** — pushed the **`game.h` gravity-well** (272-line game-core free-function header) down to the five consumers that call its functions + `Shop.hpp` (`Shop::drop`, which had free-ridden through `game.h`'s transitive tail) to interaction; `Object.cpp`/`Object_update.cpp` use neither and are freed. **3 → 2** — now exactly `GameSessionContext.hpp` + `Module.hpp`, the inline helpers' genuine deps.
- **Pass H** — same treatment for `Particle_internal.h`: dropped `GameEngine.hpp` (dead), `game.h` (→ spawn/update/combat), `PhysicalConstants.hpp` (`g_environment` → spawn); `Particle_core.cpp` needed none. **5 → 2** (same minimal core).

**Result.** `Object_internal.h` **12 → 2**, `Particle_internal.h` **5 → 2** game/ includes — both shared infrastructure headers now pull from `game/` only the two headers their own helpers use; all thirteen conduit-only edges are gone from the propagating surface, with consumers carrying precise non-propagating impl includes. Byte-identical compiled code. Every pass green: build `-- -k 0` 0-errors, validator `test.mod` 0/0, ctest -j1 **815/817** (the 2 perennial `ScriptLoaderFixture` cases), menu smoke-run clean boot + graceful shutdown.

**Pass C / Pass D (analysis).** The consumers' *remaining* `game/` includes are genuine service/ownership coupling (`activeModule().getObjectHandler()`/`spawnObject()` lifetime; `EngineContext` billboard/perk/config/log), not include hygiene. Pass D scoped the `IBillboardSystem` DIP seam: `game/Graphics/IBillboardSystem.hpp` already exists and has *only* lower-layer includes (relocatable to `egolib/Graphics/`), and `EngineContext` already publishes through it — so the proven Log/ICollisionWorld ownership-move keystone (add `activeBillboardSystem()`, relocate the installed pointer, leave `EngineContext` a delegator) applies. Keystone mandatory (stub-hazard: `ScriptActionFunctions`/`CombatDamageIntegration` install `StubBillboardSystem` via `EngineContext`). Belongs to the **EngineContext fan-in front**, not this slice — but with `config`/`logTarget` already seamed (`activeConfig()`/`Log::activeTarget()`), **`Object_combat.cpp` is one billboard-keystone + one `config()`→`activeConfig()` migration away from being `EngineContext.hpp`-free**; `Object_attributes.cpp` additionally needs the `perkHandler` keystone.

**Next:** the billboard/perk EngineContext-fan-in keystones above; and the deferred hard core — the by-value `ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics` composition members + `activeModule()` object-lifetime/spawn coupling (ownership-inversion scale) — toward an `egolib`-shaped sub-library split.

---

## Billboard + perk EngineContext-fan-in keystones (2026-06-09)

Branch `refactor/billboard-enginecontext-seam`. The "next" the entities-game `.cpp` slice's Pass D analysis named: continue cutting the EngineContext service-hub fan-in by giving its remaining stub-hazard services the proven Log / `ICollisionWorld` / audio **ownership-move keystone** (a lower-layer `active*()` free-function accessor owning the installed pointer; `EngineContext`'s methods become thin delegators), then migrating the upward leaf callers onto the accessors. Five verified passes; build 0 / validator `test.mod` 0/0 / ctest -j1 **815/817** / menu smoke clean every applicable pass.

- **Pass 1 — billboard seam.** `git mv` `game/Graphics/IBillboardSystem.hpp` → `egolib/Graphics/IBillboardSystem.hpp` (the interface names only lower-layer types, so it relocates with no game/ includes); add `Ego::Graphics::installActiveBillboardSystem/clearActiveBillboardSystem/tryActiveBillboardSystem/activeBillboardSystem`, owned in a new `egolib/Graphics/IBillboardSystem.cpp`. `EngineContext`'s billboard methods delegate; its file-static pointer is removed. Keystone is **mandatory** (not a `Singleton::get()` swap): `ScriptActionFunctions` (DrawBillboard…) and `CombatDamageIntegration` install a `StubBillboardSystem` *through* `EngineContext` and assert routing — delegation preserves that single-pointer seam.
- **Pass 2 — `Object_combat.cpp` fully off `EngineContext.hpp`.** 5× `billboardSystem()` → `Ego::Graphics::activeBillboardSystem()`; its `config()` helper → `Ego::activeConfig()` (the service-hub slice's existing `egoboo_setup.h` accessor). Drop `EngineContext.hpp`. **A core Entities combat TU no longer reaches the game/ service hub.**
- **Pass 3 — perk keystone.** `IPerkHandler.hpp` already lives at `egolib/Logic/` (lower layer), so no relocation — just add `Ego::Perks::installActivePerkHandler/.../activePerkHandler`, owned in a new `egolib/Logic/IPerkHandler.cpp`. `EngineContext` delegates (file-static removed). Mandatory ownership-move (the `EngineContext` perk tests install handlers via `EngineContext` and assert the double-install throw + routing).
- **Pass 4 — `Object_attributes.cpp` fully off `EngineContext.hpp`.** `tryBillboardSystem()` → `tryActiveBillboardSystem()` (Pass 1), `perkHandler()` → `Ego::Perks::activePerkHandler()` (Pass 3), 3× `logTarget()` → `Log::activeTarget()` (logging slice). **Both Entities billboard-consuming TUs are now EngineContext-free** — the Entities layer no longer reaches the hub for billboards/perks/config/logging.
- **Pass 5 — `ObjectProfile_export.cpp` + `ObjectProfile_load.cpp` off `EngineContext.hpp`.** The perk keystone unblocks the two Profiles-layer leaves the service-hub slice had flagged as perk-blocked. Both build a local services struct from `EngineContext`; repoint onto the lower-layer accessors (export: `activePerkHandler`; load: `Log::activeTarget` / `activePerkHandler` / `activeProfileSystem` / `Ego::activeConfig` / `tryActiveAudioSystem`). The keep-going build surfaced that `ObjectProfile_load` had been free-riding on `EngineContext.hpp` for the `Ego::Perks::IPerkHandler` *type* → added a precise `egolib/Logic/IPerkHandler.hpp`.

**Result.** Non-game leaf includers of `game/Core/EngineContext.hpp`: **8 → 4** (egolib `::get()` ~760 → 752). Two new lower-layer ownership-move seams (`IBillboardSystem` relocated + accessor; `IPerkHandler` accessor) join the existing `active*()` family (`activeAudioSystem`/`activeConfig`/`activeProfileSystem`/`activeCollisionWorld`/`Log::activeTarget`). Four upward leaf callers fully freed (`Object_combat`, `Object_attributes`, `ObjectProfile_export`, `ObjectProfile_load`). The remaining 4 leaves are the floor: `App.cpp` + `Core/System.cpp` are **bootstrap installers** (publish INTO the hub — legitimate downward direction); `Console.cpp` needs font/graphics/input keystones (3 stub-hazard services for 1 TU — low ROI); `Object_update.cpp` has a genuine game-state dependency (`tryActivePlayingState` for the minimap reveal, no lower-layer analog). Behavior-identical throughout (same installed services reached via equivalent delegating accessors).

---

## Bug fix — off-GL-thread texture-delete crash on module reload (2026-06-09)

Branch `fix/gl-texture-delete-on-loading-thread`. A **pre-existing** crash (not introduced by the refactoring fronts — none of their commits touched the crash path; the background-thread teardown dates to 2026-04-21), surfaced by a maintainer playtest: opening a second module after returning to the menu segfaulted in `glDeleteTextures`.

**Root cause.** `LoadingState::loadModuleData()` runs on a background `std::thread`. There it tears down the previous module (`game_quit_module()` + `billboardSystem().reset()` + `profileSystem().reset()`), which destroys that module's GL textures via `~Texture → Texture::release() → glDeleteTextures`. That thread has **no current GL context**, so the delete is undefined behaviour and crashes the driver. Only the 2nd module load triggers it (the 1st has no prior-module textures to free). Texture *creation* was already safe on the loading thread (`DeferredTexture` defers the GL upload to the main thread); *deletion* had no equivalent.

**Fix** (mirrors the `DeferredTexture` deferral). `Ego::OpenGL::Renderer` gained `queueTextureDeletion(GLuint)` / `drainPendingTextureDeletions()`: `queueTextureDeletion` deletes immediately when `SDL_GL_GetCurrentContext() != nullptr` (the GL thread, matching the existing `TextureManager` check), else pushes the id to a mutex-guarded vector; `drainPendingTextureDeletions` (GL thread) batch-deletes the queued ids. `Texture::release()` and the four `Texture::load()` error-path deletes route through `m_renderer->queueTextureDeletion(...)`; `GameEngine::renderOneFrame()` drains the queue each frame (GL thread, after the frame's draws, before SwapWindow), and `~Renderer` drains once more so nothing leaks at exit while the context is still current. Kept deliberately minimal after an adversarial review of a broader first design caught three defects (a wrong-singleton-type compile error, a new `TextureManager::_unload` data race, and a shutdown UAF): `DefaultTexture` (only the renderer's error textures, destroyed in `~Renderer` on the main thread) keeps its direct delete, and `TextureManager` is untouched — routing `Texture::release()` already makes every `Texture` destruction thread-safe regardless of which container holds it. **Maintainer-confirmed:** module A → menu → module B no longer crashes (3 reloads, clean shutdown). build / validator `test.mod` 0/0 / ctest -j1 815/817 / menu smoke all green.

---

## egolib-physics decoupling front (2026-06-09)

Branch `refactor/egolib-physics-decoupling`. The original next-heavy-front scout's runner-up, now unblocked (the Entities↔game `.cpp` slice it was gated on landed). Goal: pay down `game/Physics/` → `game/` coupling toward an eventual `egolib-physics` link target, growing the lower-layer nucleus seeded by the Collidable + ICollisionWorld relocation. Five verified passes; build 0 / validator `test.mod` 0/0 / ctest -j1 **815/817** / menu smoke clean throughout.

- **Pass 1 — relocate `PhysicalConstants.hpp` down.** The game's physics invariants/defaults (gravity/friction constants, `Environment` + `g_environment`, `CHR_INFINITE_WEIGHT`/`CHR_MAX_WEIGHT`, `STOP_BOUNCING`, `MOUNTTOLERANCE`) are pure-data but lived in `game/` while being consumed UP from lower layers. `git mv game/Physics/PhysicalConstants.hpp → egolib/Physics/PhysicalConstants.hpp` (joining the nucleus); made it self-contained (it used `Vector3f`/`std::numeric_limits`/`uint32_t` with ZERO includes → added `integrations/math.hpp` + `<limits>` + `<cstdint>`); repointed 12 includers + CMake. **Fixed 5 upward-layering violations** (`Entities/{Object_appearance,Object_attributes,Object_update,Particle_spawn}` + `Profiles/ObjectProfile_export` no longer reach UP into `game/Physics/`).
- **Pass 2 — `CollisionSystem` + `ParticlePhysics` off EngineContext.** Both used it only for `particleHandler` → `activeParticleHandler()` (the service-hub seam). Both drop `EngineContext.hpp`.
- **Pass 3 — lower-layer `g_environment`'s definition.** It was still defined in `game/physics.c`, so an `egolib-physics` library would have an undefined reference. Moved the definition into a new `egolib/Physics/PhysicalConstants.cpp`; the PhysicalConstants module (header + storage) is now fully lower-layer.
- **Pass 4 — `particle_collision.c` off EngineContext.** Its four services all have seams now: `billboardSystem` (×13) → `Ego::Graphics::activeBillboardSystem()`, `particleHandler` (×8) → `activeParticleHandler()`, `profileSystem` (×2) → `activeProfileSystem()`, `audioSystem` (×1, via the file-local helper) → `activeAudioSystem()`. Drops `EngineContext.hpp`.
- **Pass 5 — `ObjectPhysics` off EngineContext + GameEngine.** Used GameEngine for `engine().getCurrentUpdateFrame()` (×2) — which is *literally* `return GameSessionContext::get().worldUpdateCount();` (GameEngine.cpp:641), the same counter → `worldUpdateCount()` — and `GameEngine::GAME_TARGET_UPS` (×2) → `ONESECOND` (value-identical). Drops both `EngineContext.hpp` and `GameEngine.hpp`.

**Result.** The egolib-physics nucleus is now Collidable + ICollisionWorld + **PhysicalConstants** (header + storage, fully lower-layer). **All four `game/Physics/` impl TUs are EngineContext-free**; their `game/` include total dropped **26 → 19** (CollisionSystem 4→3, ObjectPhysics 7→4, ParticlePhysics 6→4, particle_collision 9→8), and egolib `::get()` fell **752 → 725**. Behavior-identical throughout (relocations + value-identical-counter/constant swaps via equivalent accessors).

**Deferred hard core (the real link-split blocker).** The remaining `game/` edges on the physics TUs are genuine game coupling, not include hygiene: `GameSessionContext::activeModule()` and the **`GameModule` mesh queries** (`getMeshPointer()` → `get_twist`/`getElevation`/`test_fx`/…), `game.h`/`graphic.h`/`physics.h`, `Module.hpp`, `Shop`, `CharacterMatrix`, and `game/Graphics/Billboard*`. Decoupling the mesh queries means **extending the `ICollisionWorld` DIP seam** (today just `isInside`/`getTileIndex`) to cover them — and per the methodology that surgery must be **gated behind collision-pipeline characterization tests** (the pure swept-bounds/normal math is covered; `do_chr_prt_collision`/`particle_collision.c` pipeline behaviour is not). That is the next heavy thrust for this front, distinct from these bounded include/EngineContext wins.

---

## T3.4 — collision-pipeline characterization (2026-06-09)

Branch `test/collision-pipeline-characterization`. The net that gates the deferred `ICollisionWorld` mesh-seam extension above. `CollisionPipeline.cpp` (8 cases, ctest **815→823**/825; the 2 reds remain the perennial `ScriptLoaderFixture` cases) pins the two public collision entry points on live module-spawned entities, mirroring the `CombatDamageIntegration` headless fixture (live `test.mod`, `ParticleHandler` + `StubBillboardSystem` installed, audio disabled) plus `CollisionSystem::initialize()`.

- **chr-prt** (`do_chr_prt_collision`): a damage particle subtracts exactly `FP8_TO_FLOAT(base)` life (owner Invalid → no perk/crit branches; reduction primed to 0) and records the hit; the same non-eternal particle cannot re-damage the same target (hasCollided gate); an invincible target deflects (handled, no life loss, no collision recorded); a TEAM_NULL particle cannot damage a TEAM_NULL target (friend-foe gate); a spatially-separated particle early-returns (geometry gate).
- **chr-chr** (`CollisionSystem`): `detectCollision` overlap math (overlapping vs far-apart), and a plain follower is neither mounted nor platformed.

**Bring-up findings worth keeping** (the determinism levers, documented in-file): the collision volume `chr_min_cv` is computed in the per-frame physics update, so a headless test must call `Object::updateCollisionSize(true)` after spawn or the geometry gate sees a degenerate volume; the follower has no local pips, so a spawned damage particle needs `DAMFX_ARRO` cleared (else non-attached damage is skipped, `particle_collision.c:757`) and `DAMFX_NBLOC` set (else a particle at the target's exact origin degenerates `vec_to_facing(0,0)` into the invictus arc → a `spawnDefencePing` deflect that sets `damage_timer = DEFENDTIME(24)` and blocks the damage block); per-instance `damage`/`team`/`owner` fields are public and overridden directly for determinism.

---

## collisionworld-mesh-seam — extend ICollisionWorld to cover the mesh queries (2026-06-09)

Branch `refactor/collisionworld-mesh-seam`. The deferred-hard-core "next heavy thrust" the egolib-physics front named, now unblocked by the `CollisionPipeline.cpp` characterization net (`CollisionPipeline.cpp`'s own header comment was written to gate exactly this surgery). Four verified passes route the physics step's terrain queries through the lower-layer `Ego::Physics::ICollisionWorld` DIP seam instead of reaching up into `game/mesh.h` via `GameModule::getMeshPointer()`. Build 0 / validator `test.mod` 0/0 (full `errors=245` baseline unchanged) / ctest -j1 **823/825** (only the 2 perennial `ScriptLoaderFixture` reds) / menu smoke-run clean (OpenGL/font/audio/image boot, no SIGSEGV).

- **Pass 1 — relocate `MeshLookupTables` down.** The precomputed, mesh-independent twist tables (`g_meshLookupTables`: surface normals, twist facings, steep-hill slide velocity, flatness) lived in `game/mesh.h`/`mesh.c`, whose own comment said *"this should be in map, not in mesh."* Zero game-layer deps (the ctor uses only `twist_to_normal` [`egolib/map_functions`], `vec_to_facing` [`egolib/_math`], and the already-lower-layer `Ego::Physics::g_environment.gravity`). Moved struct+extern → new `egolib/Physics/MeshLookupTables.hpp`, ctor+global def → matching `.cpp` (mirroring the `PhysicalConstants` relocation). `game/mesh.h` re-includes the new header, so the four non-physics users (`RenderPasses`, `Module_spawn`, `mesh.c`, `mesh.h`) are byte-identical; symbols stay in the **global** namespace so all ~6 call sites are unchanged.
- **Pass 2 — extend `ICollisionWorld`.** Widened the seam from the 2 position-validation methods (`isInside`/`getTileIndex`) to the full terrain-query surface: `gridIsValid`, `getTwist`, `getFanTwist`, `testFX`, `getElevation` (waterwalk + plain overloads), `isWater`. All traffic only lower-layer types (`Index1D`/`Ego::Vector2f`/`uint8_t`/`uint32_t`/`float`/`bool`; `testFX` flags are a plain `uint32_t` so the seam needn't pull `BIT_FIELD`). `GameModule` implements each as a thin forwarder to its `_mesh` (and, for `isWater`, its `_water._is_water` flag read directly so the `const` override keeps `const`). Purely additive — no callers yet.
- **Passes 3–4 — migrate `ObjectPhysics.cpp` (8 sites) + `ParticlePhysics.cpp` (9 sites).** Each `activeModule().getMeshPointer()->get_twist/get_fan_twist/grid_is_valid/test_fx/getElevation(...)` and `getWater()._is_water` became a `collisionWorld().getTwist/getFanTwist/gridIsValid/testFX/getElevation/isWater(...)` call (a small file-local `collisionWorld()` helper returning `Ego::Physics::activeCollisionWorld()`, mirroring each file's existing `activeModule()` helper). Also dropped ParticlePhysics's now-single-use `auto mesh = ...getMeshPointer()` local, and made each file's `g_meshLookupTables` dependency explicit by including the now-lower-layer `egolib/Physics/MeshLookupTables.hpp` directly rather than via the `mesh.h` conduit.

**Behavior-identical.** The installed collision world IS the active `GameModule` (`GameSessionContext` does `installCollisionWorld(_activeModule.get())` over the exact `_activeModule` lifetime), so every seam call forwards to the same mesh/water access during the physics step; `activeCollisionWorld()` and `activeModule()` never diverge while physics runs (both throw `std::logic_error` when uninstalled). `testFX` widens the flags param to `uint32_t`, which is a no-op identity (`BIT_FIELD` *is* `typedef uint32_t`); `isWater()` reads `_water._is_water` directly, identical to the old `getWater()._is_water` (`getWater()` returns `_water` by reference). Verified by a 4-dimension adversarial review workflow (forwarding-equivalence / completeness / relocation-fidelity / layering-purity), all **clean at high confidence**.

**Two notes carried forward** (both pre-existing, neither a regression): (a) Pass 1 preserves the pre-existing cross-TU static-init dependency — the `g_meshLookupTables` ctor reads `Ego::Physics::g_environment.gravity`, which lives in `PhysicalConstants.cpp` — relevant only if/when these become separate link units (it was already a cross-TU read from `mesh.c`). (b) three *dead* `game.c` helpers (`get_mesh_max_vertex_1/2`, `get_chr_level`; zero callers tree-wide) still do raw object-vs-mesh terrain math and conceptually belong to the physics layer — delete or route them when the egolib-physics extraction continues.

**Result.** The lower-layer physics nucleus grew to **Collidable + ICollisionWorld + PhysicalConstants + MeshLookupTables**, and `ICollisionWorld` is now a complete terrain-query surface rather than just position validation. **Neither `ObjectPhysics` nor `ParticlePhysics` calls a mesh method directly any more** — all terrain queries flow through the seam. The two sibling physics TUs (`CollisionSystem.cpp`, `particle_collision.c`) make no terrain queries at all (only `getObjectHandler`/`getTeamList` entity-world usage). This is **API/data decoupling, not include reduction**: both files still need `Module.hpp` for `getObjectHandler()`/`getTeamList()`, so their `game/` include count is unchanged — that drop is gated on the deferred entity-world strand (seaming `ObjectHandler` / object-lifetime / spawn = ownership-inversion scale).

---

## game.h directional-decoupling — thin-header extraction (2026-06-09)

Branch `refactor/game-h-decoupling`. Picked by a four-angle scout workflow over the next-front candidates: `game.h` is the **single heaviest directional include violation in egolib** — a 272-line game-layer grab-bag whose include block drags a heavy conduit (`EngineContext.hpp` / `mesh.h` / `Inventory.hpp` / `Shop.hpp` / `AnimatedTiles`/`Water`/`Weather`/`Fog`) into **11 lower-layer consumers**, each using only a tiny slice. Unlike the physics-TU decoupling (whose files already live in `game/Physics/`, an intra-layer move), the dominant `game.h` consumers are **genuinely-lower-layer `Entities/` TUs reaching UP** — a true directional violation. Three verified passes via the proven thin-header / IWYU technique. Build 0 / full ctest -j1 **823/825** (only the 2 perennial `ScriptLoaderFixture` reds) / validator `test.mod` 0/0 / menu smoke-run clean.

- **Pass 1 — drop the stale include.** `Script/script.c` included `game.h` but referenced none of its symbols → removed (1 edge). *(The scout also flagged `Profiles/ProfileSystem.cpp` as stale, but a keep-going build proved otherwise — it genuinely uses `game.h`'s `MAX_IMPORT_PER_PLAYER`/`MAX_IMPORT_OBJECTS` **macros** (`game.h:65-66`, which the scout's per-function-symbol grep missed) plus `EngineContext` — so it was correctly **kept**. A reminder to verify scout claims against the macro surface, not just declared functions.)*
- **Pass 2 (keystone) — extract the thin header.** Created game-free `egolib/game/CharacterParticleOps.h` declaring the char/particle-op entry points the Entities layer calls back into (`chr_stoppedby_tests`/`chr_pressure_tests` externs, `number_of`/`disaffirm`/`reaffirm_attached_particles`, `chr_do_latch_attack`, `character_swipe`, `prt_find_target`, `chr_get_lowest_attachment`, `DisplayMsg_printf`/`print`) — every signature uses only lower-layer types (`ObjectRef`/`TEAM_REF`/`PIP_REF`, `slot_t`, `Facing`, `Ego::Vector3f`, `Object` fwd-decl), so it has **zero `game/` includes**. Moved those decls out of `game.h` (which re-includes the thin header, so game-layer callers + the `game_{loop,combat,targeting}.c` definition TUs are unaffected), and routed the **8 `Entities/` TUs** (`Object_appearance`/`attributes`/`combat`/`interaction`/`lifecycle.cpp`, `Particle_combat`/`spawn`/`update.cpp`) onto it. The keep-going build found **zero free-riders**. Proven via the ninja deps DB: `Object_combat.cpp`'s transitive set no longer contains `game.h`, `EngineContext.hpp`, or `Shop.hpp` (`Inventory.hpp`/`mesh.h` persist via `Object.hpp`'s own inventory/physics edges — separate strands).
- **Pass 3 — relocate `Zeitgeist` down.** `AudioSystem.cpp` pulled `game.h` solely for `Zeitgeist::CheckTime` (seasonal-theme selection). `namespace Zeitgeist` is self-contained (an `enum class Time` + `CheckTime`, body using only `Ego::Time::LocalTime` + idlib) → relocated to new lower-layer `egolib/Zeitgeist.{hpp,cpp}` (body moved verbatim from `game.c`; `game.h` re-includes the header). `AudioSystem.cpp` swapped onto `egolib/Zeitgeist.hpp`; the keep-going build surfaced its genuine `EngineContext` use (free-riding on the conduit), fixed with a precise `EngineContext.hpp` include. AudioSystem now sheds the whole `game.h` conduit (transitive `game.h` count 0), keeping only the one `EngineContext.hpp` it uses.
- **Pass 4 — route `particle_collision.c` (review follow-up).** A four-dimension adversarial review of this front caught a missed routing: the physics TU `particle_collision.c` still included `game.h` but used only `chr_get_lowest_attachment` + `reaffirm_attached_particles` — both already in the thin header (the Pass-3 "genuinely needs game.h" claim was wrong). Swapped it onto `CharacterParticleOps.h` (deps DB confirms `game.h` count 0), shedding its `game.h` conduit too. Also fixed a minor review NOTE (an over-precise `GCC_PRINTF_FUNC` include comment).

**Result.** `game.h`'s includers **outside `egolib/game/` dropped to exactly one** — `ProfileSystem.cpp` (genuine `MAX_IMPORT` macros) — from the original eleven; the heavy `game.h` conduit (`EngineContext`/`mesh`/`Inventory`/`Shop`/Module-state) no longer reaches the 8 `Entities/` TUs, `AudioSystem`, or `particle_collision.c`. Two new lower-layer artifacts: the game-free `CharacterParticleOps.h` declaration seam and the fully-lower-layer `Zeitgeist` utility. The four review dimensions came back **clean / high-confidence** (behavior preservation, layering purity, hidden-breakage) except the one `particle_collision.c` warning, now fixed in Pass 4. **Deferred (out of scope, unchanged):** the `Entities/` TUs still pull `GameSessionContext.hpp` + `Module.hpp` via `Object_internal.h`/`Particle_internal.h` (the entity-world ownership-inversion strand); the `Billboard::Flags` enum edge in `Object_attributes`/`Object_combat` (a future thin-enum-header pass); the `Inventory.hpp`/`mesh.h` edges that arrive via `Object.hpp` rather than `game.h`; and a future option to relocate the `MAX_IMPORT_*` macros to a thin header to free `ProfileSystem.cpp` (the last consumer).

---

## IObjectWorld entity-world seam — decouple the physics TUs from GameModule (2026-06-09)

Branch `refactor/iobjectworld-seam`. Picked by an 8-way scout-next-heavy-front workflow as the genuinely heavy, non-stale, proven-pattern continuation toward an `egolib-physics` link target. The `game/Physics/` TUs were the heaviest remaining Entities↔game *runtime* coupling: they reach entity-world state through `GameSessionContext::activeModule().getObjectHandler()` (object iteration/lookup/existence/spatial query) and `.getTeamList()`. Five verified passes mirror the proven `Ego::Physics::ICollisionWorld` ownership-move keystone (which already seamed the terrain/mesh queries). Build 0 / full ctest -j1 **823/825** (only the 2 perennial `ScriptLoaderFixture` reds) / validator `test.mod` 0/0 / menu smoke-run clean (exit 124, no crash markers) at **every** pass.

> **Scout correction recorded by this front:** the documented "quick CMake carve of `egolib-physics`" is **not** a clean incremental win. The lower-layer nucleus (`Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`) is *include*-clean against `game/` but **not link-symbol closed**: `MeshLookupTables.cpp` calls `twist_to_normal` (in `map_functions.c`, which pulls `Log/`/`FileFormats/map_file`/`Mesh/Info`) and needs egolib `Math/Random.cpp`/`Standard.cpp` symbols. A naive `add_library(egolib-physics-library)` would be **circular**. The real link split is gated on this front (upper edges) **plus** a measured dependency-closed *foundation* carve (lower edges) — the recommended next front. Also confirmed stale: **T1.4 `egolib_rv` is effectively done** (3 occurrences — the `typedef.h` enum + the `gfx_rv` alias).

- **Pass 0 (keystone, additive).** New lower-layer `Ego::Entities::IObjectWorld` (`egolib/Entities/IObjectWorld.{hpp,cpp}`): a 2-method interface (`getObjectHandler()` → `ObjectHandler&`, `getTeamList()` → `std::vector<Team>&` — both already lower-layer types, so they're returned by reference and the interface drags in **no `game/` header**, forward-declaring `ObjectHandler`/`Team`) plus free-function `installObjectWorld`/`clearObjectWorld`/`tryActiveObjectWorld`/`activeObjectWorld` over a file-static pointer, identical in shape to `ICollisionWorld`. `GameModule` now also `public`-inherits `IObjectWorld` (its existing `getObjectHandler()`/`getTeamList()` became `override`s — the same multiple-interface pattern it already used for `ICollisionWorld`), and `GameSessionContext::{beginModule,quitModule}` install/clear it alongside the collision world (same `_activeModule` pointer, same lifetime). Purely additive (no caller migrated) → green before any migration.
- **Pass 1 — `ParticlePhysics.cpp` (cleanest).** Replaced the file-local `activeModule()` helper with `objectWorld()` → `activeObjectWorld()`; swapped the 6 `getObjectHandler()` + 1 `getTeamList()` call-site prefixes. This TU used `GameSessionContext` *only* for `activeModule()` (no `worldUpdateCount`), so it dropped **both** `game/Module/Module.hpp` **and** `game/Core/GameSessionContext.hpp`. Keep-going build exposed two free-riders reaching the mesh-fx constants `TWIST_FLAT`/`MAPFX_SLIPPY` via `Module.hpp → mesh.h`; fixed with the precise lower-layer `egolib/FileFormats/map_fx.hpp` (IWYU). **game/ includes 4 → 2** (only its own header + `CharacterMatrix.h` — entity-world coupling gone).
- **Pass 2 — `ObjectPhysics.cpp`.** Same swap, 7 `getObjectHandler()` sites. Dropped `Module.hpp`; kept `GameSessionContext.hpp` for the still-unseamed `worldUpdateCount()` strand. Two free-riders fixed: `Info<float>::Grid::Size` (`egolib/FileFormats/map_file.h`) and `MAPFX_SLIPPY` (`map_fx.hpp`). **game/ includes 4 → 3.**
- **Pass 3 — `CollisionSystem.cpp`.** Same swap, 8 `getObjectHandler()` sites (incl. the local `ObjectHandler& handler =` binding + two `iterator()` range-loops). Dropped `Module.hpp`; kept `GameSessionContext.hpp` (`worldUpdateCount`). Added `map_file.h` (`Info<float>::Grid::Size`). **game/ includes 3 → 2.**
- **Pass 4 — `particle_collision.c` (heaviest).** Same swap, all **26** `getObjectHandler()` sites (`exists`/`get`/`operator[]`/`addCollision`); the `activeParticleHandler().iterator()` loop is a *particle* handler, left untouched. Dropped `Module.hpp`; kept `GameSessionContext.hpp` (`worldUpdateCount`). No free-riders (the TU already includes `graphic.h`/`physics.h` for the mesh constants). **game/ includes 8 → 7** (the residual billboard/graphic/physics/`CharacterParticleOps`/`GameSessionContext` edges are out of scope for this front).

**Result.** `game/Module/Module.hpp` is **gone from all four physics TUs**, and **zero** `activeModule().getObjectHandler()/getTeamList()` sites remain in `game/Physics/` — the entity-world container access now routes through the lower-layer `IObjectWorld` seam. `ParticlePhysics.cpp` is fully free of both `Module.hpp` and `GameSessionContext.hpp`. The change is a **pure indirection swap** (the installed object world *is* the active `GameModule`, the same pointer `activeModule()` returns), gated by the existing `CollisionPipeline.cpp` (8 live-spawn cases driving `do_chr_prt_collision`'s internal `getObjectHandler()` path + `CollisionSystem` detect/mount/platform) and `CombatDamageIntegration.cpp` nets — no new test needed. New lower-layer artifact: the game-free `Ego::Entities::IObjectWorld` interface (beside `ObjectHandler`), the entity-world sibling of `ICollisionWorld`. **Deferred (out of scope):** the residual `GameSessionContext.hpp` edge on 3 TUs for `worldUpdateCount()` (a future `activeWorldUpdateCount()` free-function seam closes it, exactly as the collisionworld-mesh-seam pass left this strand); the `game/physics.h` free-function edge (`phys_expand_*`), `graphic.h`, and the billboard edges; and the actual `egolib-physics` `add_library` link split, now gated on a dependency-closed foundation carve (see the scout-correction note above).

---

## egolib-foundation carve — the first real link-split (branch `refactor/egolib-physics-nucleus-carve`, 2026-06-09)

The continuation the IObjectWorld entry named: *"the actual `egolib-physics` `add_library` link split, gated on a dependency-closed foundation carve."* Executed in eight verified passes. **Headline: the historically monolithic `egolib-library` STATIC archive is now split into `egolib-foundation-library` (77 TUs) + `egolib-library` (215 TUs), one-way dependent and verified acyclic (0 cycle edges by nm proof).** This is the first genuine link-level modularization of egolib; idlib's eleven sub-libraries are the target pattern.

**Decoupling passes (1–4)** — narrowed the physics TUs' `game/` coupling and proved the carve's shape:

- **Pass 1** — `activeWorldUpdateCount()` seam (added to `egolib/Entities/IObjectWorld.{hpp,cpp}`): the 3 physics TUs read the world-update tick through an installed `const uint32_t*` aliasing `GameSessionContext::_worldUpdateCount` (installed/cleared in `beginModule`/`quitModule` beside the object/collision worlds) instead of `GameSessionContext::get()`; `GameSessionContext.hpp` is gone from all three. *A dedicated free-function seam, NOT a `worldUpdateCount()` method on `IObjectWorld`: a `GameModule` override of that name shadows the `module_detail::worldUpdateCount()` free helper inside every GameModule member, which broke the `worldUpdateCount()++` incrementer in `Module_update.cpp` — caught at build, corrected mid-pass.*
- **Pass 2** — `git mv`'d `game/physics.h`+`physics.c` (the `oct_bb_t` collision-geometry free functions `phys_expand_*`/`phys_estimate_*`/`phys_intersect_*`) into `egolib/Physics/`; stripped the empirically-dead `game.h`/`mesh.h` includes from `physics.c` (kept `Entities/_Include.hpp` — it deref's `IPhysical`/`Particle`). `physics.c` and `CollisionSystem.cpp` reach **zero** `game/` includes. *Scout corrections recorded: the plan called `Entities/_Include.hpp` dead (it is not) and claimed `particle_collision.c` reaches zero game/ includes (it does not).*
- **Pass 3** — `particle_collision.c` off `game/graphic.h` → direct `egolib/Audio/IAudioSystem.hpp` (it used only `GSND_*`/`activeAudioSystem()`).
- **Pass 4** — `map_file.c` gratuitous `fileutil.h` drop + **the nm symbol-closure proof that reshaped the carve**. The proof showed a standalone `egolib-physics-nucleus` archive is **circular**: its foundation symbols (`twist_to_normal`/`vec_to_facing`/`oct_bb`/`Log`) and `physics.c`'s Entities symbols (`Particle`/`IPhysical`) all live in the monolith, while the monolith depends back on the nucleus's `Collidable` base. The real first *acyclic* split is a dependency-closed **foundation** library — exactly the gate this front named.

**Foundation-closure verification (methodology worth reusing).** An nm-based fixpoint over the 294 egolib `.o`: partition into candidate-foundation vs rest, build symbol→TU defining maps, then iteratively drop any candidate TU that needs a symbol defined *only* in the rest, until stable. The remaining set is dependency-closed (every external resolves within it or in idlib/std). A first run gave **47 TUs**; the irreducible root blockers were just two code seams:

**Seam passes (5a–5b):**

- **Pass 5a** — `Time::now<Ticks>()` calls `SDL_GetTicks()` directly (exactly what `SystemService::getTicks()` does) instead of routing through `Ego::Core::System` (the game-coupled bootstrap installer that reaches into `game/Core/EngineContext`). Removes the `Time → Core/System → game` link dep.
- **Pass 5b** — relocated `ego_texture_exists_vfs()` (the lone `activeImageManager()` call) from `fileutil` to its natural home in `Image/ImageManager.{hpp,cpp}` (kept global-scope to preserve its 5 call sites). This was the one root entanglement that, via fileutil's `ReadContext`, had cascade-removed the 6 FileFormats parsers + TreasureTables + SpawnName. With the Script DDL/PDL lexer pulled in (it is itself clean), the foundation grew **47 → 77 TUs**.

**Carve pass (5c):** `list(REMOVE_ITEM SOURCE_FILES ${EGOLIB_FOUNDATION_SOURCES})` carves the foundation out of the existing accumulation, then two `add_library()` calls; `egolib-library` links `egolib-foundation-library` (+ idlib) one-way. The OS file backend (`file_linux.c`/`file_win.c`) is placed in the foundation **conditionally** (else its `fs_*` callers form a per-OS foundation→library cycle); C-as-C++ re-asserted on the foundation's 23 C sources. Consumers (egoboo, egolib-tests, content-validator) need **zero** changes — they link `egolib-library`, which carries the foundation transitively via INTERFACE. **Verified: in-place AND from-scratch clean builds both green (foundation 77 + library 215 objects, egoboo built); nm proof 0 foundation→library cycle edges; ctest -j1 823/825; validator `test.mod` 0/0; menu smoke clean** at every pass.

**Foundation set:** whole subsystems Math/Log/Mesh/VFS/Time/FileFormats/Platform + the Physics nucleus (`Collidable`/`ICollisionWorld`/`MeshLookupTables`/`PhysicalConstants`) + the **Script DDL/PDL lexer** (everything in `Script/` except the EgoScript VM `script.c`) + `Logic/TreasureTables` + toplevel math/IO (`_math`/`bbox`/`geometry`/`frustum`/`map_functions`/`typedef`/`strutil`/`vfs`/`Zeitgeist`/...). Genuinely-higher TUs correctly stay in `egolib-library`: Image impl (→`Graphics/PixelFormat`), `font_bmp` (→`Renderer/Texture`), `physics.c` (→Entities), `script.c` (the VM).

**Next toward fuller modularization:** Image could join the foundation once `Graphics/PixelFormat` (`pixel_descriptor`) is relocated down; an `egolib-physics` sub-library could split out of the foundation (the nucleus is a clean sub-DAG within it); the residual `Core/System` bootstrap edge (resolved by the 2026-06-10 seam-swap below) and the `physics.c`/Entities ownership-inversion remain the larger lower-layer fronts.

---

## egolib-physics middle-layer carve — the second link-split (branch `refactor/egolib-physics-middle-carve`, 2026-06-09)

The continuation the foundation carve named: *"an `egolib-physics` sub-library could split out of the foundation (the nucleus is a clean sub-DAG within it)."* Picked by a scout+adversarial-nm workflow (4 parallel feasibility scouts → 3 nm skeptics → synthesis): **both physics skeptics returned `refuted:false`** and the synthesis lead reproduced the full closure on the live `.a`/`.o` artifacts. **Headline: `egolib-foundation-library` (77 TUs) is re-split into `egolib-foundation-base` (73 TUs) + `egolib-physics` (4 TUs), giving the acyclic three-layer chain `egolib-foundation-base ← egolib-physics ← egolib-library`.** This lands the **`egolib-physics` link target** named across ~6 prior decoupling fronts (collisionworld-mesh-seam, IObjectWorld, the physics-nucleus carve, …). A **pure CMake partition — ZERO source edits.**

**The carve (single CMake change in `egolib/library/CMakeLists.txt`):** split the `EGOLIB_FOUNDATION_BASE_SOURCES` list (renamed from `EGOLIB_FOUNDATION_SOURCES`) from a new `EGOLIB_PHYSICS_SOURCES` (the 4 nucleus TUs `Physics/{Collidable,ICollisionWorld,MeshLookupTables,PhysicalConstants}.cpp`); `list(REMOVE_ITEM SOURCE_FILES …)` both out of the upper accumulation; three `add_library()` calls wired `egolib-library → egolib-physics → egolib-foundation-base` (+ idlib). The WIN32 `Shlwapi`/`shlwapi` block was replicated onto all three targets. The target rename `egolib-foundation-library → egolib-foundation-base` is safe: a tree grep found that name referenced **only** within this one CMakeLists (no external consumer). Consumers (egoboo, egolib-tests, content-validator) need **zero** changes — all three sub-libs carry `INTERFACE` include dir `src/` and the link chain is transitive.

**nm acyclicity proof (Python set-intersection on mangled symbols, freshly-built artifacts):**
- **DOWNWARD** — the 4 physics `.o` reference only foundation-base symbols (`vec_to_facing` in `_math.c`, `twist_to_normal` in `map_functions.c`) plus 2 intra-nucleus symbols (`Ego::Physics::activeCollisionWorld`, `g_environment`); **0** refs into the upper `egolib-library`.
- **BACK-EDGE** — **0** of the 73 base TUs reference any physics-defined symbol.
- **BASE closure** — base references **0** physics-only OR library-only symbols (it remains the true bottom).
- **POSITIVE CONTROL** — the harness correctly detected the two known downward edges (`vec_to_facing`/`twist_to_normal` both `IN-BASE`), proving the 0-results are real and not a false-empty closure (the project's #1 documented failure mode; cf. the zsh word-split trap, sidestepped here by doing the set math in Python, not a shell loop).
- `game/physics.c` is correctly **NOT** in the nucleus — it needs the upper Entities symbol `Ego::Particle::isTerminated`, so it stays in `egolib-library`.

**Verification (every gate green):** clean from-scratch build (foundation-base 73 + physics 4 + library 215 objects, egoboo/validator/tests linked); `ar t` membership counts exact (73/4/215); nm acyclic as above; validator `test.mod` 0/0 (`errors=245` baseline elsewhere); ctest -j1 **823/825** (only the 2 known `ScriptLoaderFixture` PrimaryScript-fallback failures #613/#614); menu smoke-run clean (exit 124, full OpenGL 4.6 + SDL_image/ttf/mixer boot, graceful shutdown, no crash markers). Committed as one carve commit.

**Next (nm-pre-verified by the same scout, queued as their own bounded fronts):** **(1)** absorb **InputControl** (3 TUs `InputControl/{InputDevice,InputSystem,ControlSettingsFile}.cpp`) into `egolib-foundation-base` — 0 library blockers, only SDL_* + existing-foundation refs; **(2)** absorb **Image + `Graphics/PixelFormat.cpp`** (7 TUs together — Image alone has 6 `pixel_descriptor` blockers, so they must move as a unit) and add SDL2_image to the foundation `target_link_libraries` for hygiene. Deferred larger fronts at that point: the `game/mesh.c` `ego_mesh_t` chokepoint (blocks AI), the `Core/System` bootstrap edge (resolved by the 2026-06-10 seam-swap below), the `physics.c`/Entities ownership-inversion.

---

## egolib-foundation-base growth absorptions (2026-06-09)

The two nm-pre-verified foundation-growth fronts the physics middle-carve queued. Each is a **pure CMake source-list move** (the named TUs relocate from `egolib-library` into `egolib-foundation-base`; identical object code, just a different archive), verified safe by the same `nm` set-intersection method (a TU is safe to move down iff it references **0** library-only symbols, excluding symbols defined within the moving group itself). Each landed on its own branch with the full gate (build / `ar t` membership / nm acyclic with a live positive control / validator `test.mod` 0/0 / ctest -j1 823/825 / menu smoke exit-124).

- **InputControl absorb** (branch `refactor/egolib-inputcontrol-foundation-absorb`). Moved `InputControl/{ControlSettingsFile,InputDevice,InputSystem}.cpp`. nm pre-check: 61 undefined symbols, **0** library-only blockers (all resolve in base / idlib / std / SDL); the sole cross-edge `game → InputControl` (e.g. `Ego::Input::InputDevice::DeviceList`) becomes a clean `library → foundation` edge. Membership **base 73 → 76, library 215 → 212** (physics 4 unchanged). Acyclic re-proven on fresh artifacts (base needs 0 physics/library symbols; positive control: base references 9 `SDL_*` symbols, so the 0-result is real).
- **Image + PixelFormat absorb** (branch `refactor/egolib-image-foundation-absorb`). Moved the 6 Image TUs (`Image/{Image,ImageLoader,ImageLoader_SDL,ImageLoader_SDL_image,ImageManager}.cpp` + `Image/SDL_Image_Extensions.c`) **together with** `Graphics/PixelFormat.cpp`. The two **must** move as a unit: Image alone has **6** library-only blockers — `Ego::pixel_descriptor::get_{red,green,blue,alpha,color_depth}()` + the `get<(pixel_format)3>()` specialization — all defined in `PixelFormat.cpp`; with `PixelFormat.cpp` in the group the blocker count is **0** (verified both ways). `PixelFormat.cpp` itself is a clean idlib-only leaf (drags nothing down). The SDL2_image link dep needs **no** CMake change: the moved TUs reference `IMG_Init`/`IMG_Load_RW`/`IMG_Quit`/`IMG_SavePNG_RW`, which already resolve at final link through `idlib-game-engine-library` (PUBLIC-linked by `egolib-foundation-base`) — the scout's "add SDL2_image to the foundation link deps" hygiene step is already satisfied transitively, and `SDL2_IMAGE_LIBRARIES` is not even in egolib's CMake scope. Membership **base 76 → 83, library 212 → 205** (physics 4 unchanged). Acyclic re-proven on fresh artifacts (base needs 0 physics/library symbols; positive controls: base now references the 4 `IMG_*` symbols, and `pixel_descriptor::get_red` is now defined intra-base — the former 6 blockers resolve inside the base).

---

## egolib frontier absorption — the third link-split: egolib-renderer (2026-06-09/10)

Branch `refactor/egolib-frontier-absorb`, seven passes. Picked by a 5-scout scout-next-heavy-front workflow
(fresh nm fixpoint over all 205 library TUs + one deep scout per deferred front + a generalist sweep →
ranking synthesis → **six adversarial skeptics, every one `refuted:false` at high confidence**, each with an
independently-written from-scratch nm parser reproducing the scout's numbers exactly on live artifacts —
the strongest cross-verification this thread has had). Headline: **the 60-TU nm-clean frontier of
`egolib-library` moved down, plus the physics pair behind a 1-symbol seam — `egolib-library` 205 → 143 TUs
(−30%), and a fourth archive `egolib-renderer` exists. The link layout is now an acyclic DAG:**

```
egolib-foundation-base (114)  ◄──  egolib-physics (6)    ◄──  egolib-library (143)
egolib-foundation-base (114)  ◄──  egolib-renderer (29)  ◄──  egolib-library (143)
```

**Foundation absorptions (P1–P4, 31 TUs, pure CMake source-list moves, 0 cumulative blockers at every step):**
- **P1 (14):** `Profiles/{EnchantProfile,EnchantProfileWriter,LocalParticleProfileRef,ParticleProfile,ParticleProfileWriter,RandomName}.cpp`, the seam interfaces `Entities/IObjectWorld.cpp` + `Graphics/IBillboardSystem.cpp` + `Logic/IPerkHandler.cpp`, `Logic/MissileTreatment.cpp`, `game/GUI/InputListener.cpp`, `AI/WaypointList.c`, `game/lighting.c`, `game/script_functions_bitwise.c` (83→97). Subsumes the mesh-AI front's "Pass 1" and pre-resolves `mesh.c`'s `lighting_cache_t::init` blocker downward.
- **P2 (7):** the MD2 model cluster `Graphics/{MD2Model,ModelDescriptor,VertexFormat,IndexFormat}.cpp`, `game/Graphics/{Md2ModelRenderer,DefaultMd2ModelRenderer}.cpp`, `game/GUI/Material.cpp` (97→104).
- **P3 (6):** the texture/font cluster `Renderer/{Texture,DeferredTexture}.cpp`, `Graphics/{TextureManager,Font,FontManager}.cpp`, `font_bmp.c` (104→110). Co-moving `Renderer/Texture.cpp` clears `font_bmp.c`'s two `Ego::Texture::getSource{Width,Height}` blockers — **the long-documented `font_bmp` "genuinely-higher" exception is retired.**
- **P4 (4):** `Profiles/{ObjectProfile_core,ModuleProfile}.cpp`, `Logic/{Perk,PerkHandler}.cpp` (110→114).

**The egolib-renderer carve (P5–P6, 29 TUs — the third link-split).** The frontier's display/GL cluster is
plumbing, not foundation, so it became its own middle layer rather than bloating the base: a new STATIC
archive `egolib-renderer`, **sibling of `egolib-physics`** — both depend one-way on `egolib-foundation-base`,
with **zero symbol edges between renderer and physics in either direction** (nm-verified; fold-into-base was
the documented fallback, with identical nm math). P5 moved the 10-TU SDL display/window cluster
(`Graphics/{Display,DisplayMode,GraphicsContext,GraphicsWindow,Viewport}.cpp`, `Graphics/SDL/{Display,DisplayMode,GraphicsContext,Utilities}.cpp`, `game/GUI/DrawingContext.cpp`); P6 the 19-TU OpenGL backend
(`Renderer/{Renderer,RendererInfo}.cpp`, the 10 `Renderer/OpenGL/*` TUs, `Graphics/{GraphicsSystem,GraphicsSystemNew}.cpp`, `Graphics/SDL/{GraphicsSystemNew,GraphicsWindow}.cpp`, `Extensions/ogl_extensions.c`, `game/Graphics/RenderPass.cpp`, `game/renderer_3d.c`) — the 8-TU mutually-recursive renderer SCC and the
`{ogl_extensions, OpenGL/Utilities}` SCC moved atomically within the pass. Consumers needed **zero** changes
(`egolib-library` PUBLIC-links the new target; INTERFACE include dirs identical). The duplicate-basename trap
(9 basenames × 2 members in the old monolith — `Texture.cpp.o`, `GraphicsWindow.cpp.o`, `Utilities.cpp.o`, …)
was sidestepped in gating by doing nm math on **archive-level aggregate** symbol sets, which need no member
attribution.

**The physics pair (P7, the one source edit).** `Particle::isTerminated()` was inlined into
`Entities/Particle.hpp` (out-of-line definition deleted from `Particle_core.cpp`; body `return _isTerminated;`
— semantics identical, in-class definition implicitly inline). That was the pair's **only** residual blocker
(nm-verified exactly `_ZNK3Ego8Particle12isTerminatedEv`), so `Physics/physics.c` + `Entities/Common.cpp`
joined `EGOLIB_PHYSICS_SOURCES` (4→6) — **the documented "physics.c needs Ego::Particle::isTerminated"
blocker is retired; the `oct_bb_t` collision-geometry free functions now live in the archive named for them.**

**Verification (every pass):** build 0 errors → `ar t` membership exact (97/104/110/114 × 4/6 × 10/29 ×
191→143) → nm acyclicity across all archives with **live positive controls** (physics→base edges incl.
`vec_to_facing`+`twist_to_normal`, 2→6 syms after P7; base undefined `SDL_*` 31→32; renderer→base 11→36 syms)
→ validator `test.mod` 0/0 → ctest -j1 **823/825** (only `ScriptLoaderFixture` #613/#614) → menu smoke-run
exit 124 clean. A 4-dimension adversarial review (CMake/layering audit, behavior-preservation hunt,
from-scratch rebuild, Windows cross-build check) gated the merge.

**Terminal finding — move-only absorption is EXHAUSTED.** The fixpoint proved the remaining `egolib-library`
(143 TUs) is one large mutually-recursive game-core SCC plus 4 blocked satellites (`Core/System.cpp` —
residual 6, needs only the `EngineContext`→`Log::activeTarget()`/`installActiveConfig` seam swap, the named
quick follow-on; `OptionsButton`, `Panel`, `DebugParticlesScreen`). **Every future link-split gain now
requires seam-cutting, not list-moving.** Next fronts as ranked by the scout cycle: the mesh-AI chain
(`mesh.c` residual is now 4 blockers — the `EngineContext::get/logTarget` pattern + `GameSessionContext::get`/
water — after `lighting.c` landed in base); the `Core/System.cpp` seam-swap (+1 satellite); the questlog
chain via `Log::activeTarget()`. The physics/Entities ownership-inversion remainder (by-value
`ObjectPhysics`/`ObjectGraphics`/`ParticleGraphics` members) re-measured at **123 blockers — correctly
flag-day, do not attempt incrementally.** A valuable side-find: a **4th racing test fixture** shares the
writable `.egoboo-runtime/user` path with no env override (`Platform/file_linux.c:190-200`) — fixing fixture
isolation would make `ctest -j20` trustworthy and every future gate cycle ~70 s faster.

## Core/System bootstrap seam-swap (2026-06-10)

The named quick follow-on from the frontier-absorption pass is done. `Core/System.cpp` now publishes logging
and configuration through the lower-layer ownership seams directly (`Log::installActiveTarget` /
`Log::clearActiveTarget`, `Ego::installActiveConfig` / `Ego::clearActiveConfig`, `Ego::activeConfig`) instead
of including `game/Core/EngineContext.hpp`; behavior is unchanged because `EngineContext` already delegates
to those same seams.

With that upward include removed, `Core/System.cpp` moved into `EGOLIB_FOUNDATION_BASE_SOURCES`. Current
archive layout: **`egolib-foundation-base` 115 / `egolib-physics` 6 / `egolib-renderer` 29 /
`egolib-library` 142**. Verified: build 0, exact `ar t` membership counts, aggregate nm acyclicity
(base→physics/renderer/library 0; physics↔renderer 0; middle→library 0; live positive controls fired),
validator `test.mod` 0/0, ctest -j1 **823/825** (only `ScriptLoaderFixture` #613/#614), and menu smoke
exit-124 clean.

## mesh-AI terrain seam (2026-06-10)

The ranked mesh-AI follow-on from the frontier absorption is done. Added the lower-layer
`Ego::Mesh::ITerrainQuery` interface for the terrain operations AI actually needs:
`getTileIndex(Index2D)`, `testFX`, `tileHasBits`, and `isFanOff`. `GameModule` implements that interface by
forwarding to its existing mesh surface, so production behavior remains the active module's terrain data.

`AI/AStar.{hpp,cpp}` and `AI/LineOfSight.{hpp,cpp}` now depend on `ITerrainQuery` instead of
`std::shared_ptr<const ego_mesh_t>` / `game/mesh.h`. Runtime call sites in script implementation, targeting,
and Object attribute/update code pass the active `GameModule` as the terrain view. With the game mesh include
removed, `AStar.cpp` and `LineOfSight.cpp` moved into `EGOLIB_FOUNDATION_BASE_SOURCES`, shrinking the upper
library again. Current archive layout: **`egolib-foundation-base` 117 / `egolib-physics` 6 /
`egolib-renderer` 29 / `egolib-library` 140**.

Added focused lower-layer coverage in `egolib/tests/egolib/tests/AITerrainQueries.cpp`: clear LOS, blocked
LOS with collision data, A* blocked endpoints, A* path around a blocked tile, and A* fan-off exclusion. This
keeps the AI terrain contract pinned without needing a live `GameModule` fixture.

**Verification:** build 0; `AITerrainQueries` 5/5; ctest -j1 **828/830** (only the known
`ScriptLoaderFixture` #618/#619 PrimaryScript fallback failures); validator `test.mod` 0/0; exact archive
membership 117/6/29/140; aggregate nm acyclicity (base→physics/renderer/library 0; physics↔renderer 0;
middle→library 0; live positive controls fired); menu smoke-run exit 124 with clean startup/shutdown. Deferred:
broader `mesh.c` relocation, remaining `getMeshPointer()` cleanup, Particle/Object mesh-collision callers, and
Entities ownership inversion.

---

## QuestLog foundation absorb (2026-06-10)

Seam-cut the `QuestLog` and `PlayerQuestLog` TUs into `egolib-foundation-base`. `QuestLog.cpp` had a single
upward edge — `EngineContext::get().logTarget()` in `exportToFile()` — replaced with the proven
`Log::activeTarget()` seam (include swapped from `game/Core/EngineContext.hpp` to `Log/_Include.hpp`). All
other dependencies (`fileutil.h`, `ReadContext`, `IDSZ2`, `vfs_*`, `Log::*`) were already in the base.
`PlayerQuestLog.cpp` had no blockers (its only dependency is `QuestLog::loadFromFile`, which co-moves).

CMake: removed `QuestLog.cpp`, `QuestLog.hpp`, `PlayerQuestLog.cpp`, `PlayerQuestLog.hpp` from
`EGOLIB_GAME_LOGIC_SOURCES` and added the two `.cpp` files to `EGOLIB_FOUNDATION_BASE_SOURCES`. Current
archive layout: **`egolib-foundation-base` 119 / `egolib-physics` 6 / `egolib-renderer` 29 /
`egolib-library` 138**.

**Verification:** build 0; ctest -j1 **828/830** (only the known `ScriptLoaderFixture` #618/#619); validator
`test.mod` 0/0 (`errors=245` baseline); exact archive membership 119/6/29/138; aggregate nm acyclicity with
mangled symbols (all 7 back-edges = 0; positive controls: `vec_to_facing`/`twist_to_normal` in physics→base,
37 SDL_* undefs in base); menu smoke-run exit 124 with clean startup/shutdown.

---

## Files touched most by this pass log

The following translation units or headers were modified by five or more of the passes above. Consult git history if you need the exact sequence of changes:

- `egolib/library/src/egolib/game/Core/GameSessionContext.{hpp,cpp}`
- `egolib/library/src/egolib/game/Core/GameEngine.{hpp,cpp}`
- `egolib/library/src/egolib/game/Module/Module.cpp` and siblings
- `egolib/library/src/egolib/Entities/Object.{hpp,cpp}` and `Object_*.cpp`
- `egolib/library/src/egolib/game/Graphics/ObjectGraphics.{hpp,cpp}`
- `egolib/library/src/egolib/game/game.c` and siblings
- `egolib/library/src/egolib/game/game_loop.c`
- `egolib/library/src/egolib/game/script_functions_*.c`
- `egolib/tests/egolib/tests/ObjectAccessors.cpp`
- `egolib/tests/egolib/tests/ModulePlayerStartup.cpp`

---

## Theme — Broader getMeshPointer() cleanup (2026-06-10)

### getMeshPointer() migration — dead code + seam reroutes + ICollisionWorld widening

Migrated 17 `getMeshPointer()` call sites off direct `ego_mesh_t` access and onto DIP seam interfaces, reducing impl-file sites from 32 to 15.

**Pass 0 — Dead code removal:** removed the `#if defined(_DEBUG) && defined(DEBUG_WAYPOINTS)` block in `script_implementation.c` (lines 169–213). `DEBUG_WAYPOINTS` is explicitly `#undef`'d in `egolib_config.h` and the `ego_mesh_hit_wall` free function no longer exists — the block would not compile if enabled.

**Pass 1 — Trivial reroutes (4 sites):** routed through existing `ICollisionWorld` methods using the established file-local `collisionWorld()` helper pattern:
- `Object_appearance.cpp` `isOnWaterTile()`: `test_fx` → `testFX`
- `Particle_core.cpp` `isOverWater()`: `test_fx` → `testFX`
- `graphic.c` tile-validity gate: `grid_is_valid` → `gridIsValid`
- `Module_spawn.cpp` `tiltCharactersToTerrain`: `get_twist` → `getTwist`

**Pass 2 — ICollisionWorld widening + dimension consumer migration (11 sites):** added 4 new pure-virtual methods to `ICollisionWorld` (`getEdgeX`, `getEdgeY`, `getTileCountX`, `getTileCountY`) with `GameModule` implementations delegating to the mesh. Migrated all edge/tile-count consumers:
- `Particle_spawn.cpp` (2 sites): position clamping via `getEdgeX/Y`
- `Weather.cpp` (2 sites): boundary checks via `getEdgeX/Y`
- `MiniMap.cpp` (2 sites): coordinate scaling via `getEdgeX/Y`
- `graphic_scene.c` (4 sites): tile list iteration via `getTileCountX/Y`
- `MapEditorState.cpp` (3 sites): camera centering + quad-tree bounds via `getTileCountX/Y` + `getElevation`

**Pass 3 — Passage constructor reroute (1 site):** `Passage.cpp` ctor switched from `module.getMeshPointer()->getTileIndex(...)` to `module.getTileIndex(...)` (method already public via `ITerrainQuery` override).

**Intentionally deferred (15 remaining sites):**
- Wall collision (`hit_wall`/`test_wall`, 6 sites): `mesh_wall_data_t` holds raw `ego_mesh_t*` — can't move to lower-layer `ICollisionWorld` without dragging full mesh type down.
- Mutating tile operations (8 sites in `script_implementation.c` and `Passage.cpp`): pure game-layer mutations (`add_fx`, `clear_fx`, `set_texture`, lighting writes). No layering benefit.
- Infrastructure (1 site): `GameSessionContext::mesh()` forwarder — last to go.

All passes green: build / ctest -j20 **830/830** / validator `test.mod` 0/0 / menu smoke clean exit.

### IGraphicsSystem widening — route GraphicsSystemNew::get() through EngineContext

Widened `IGraphicsSystem` with 3 methods (`setCursorVisibility`, `update`, `getDisplays`) and implemented them in `GraphicsSystem` by delegating to `GraphicsSystemNew::get()`. Migrated all 6 game-layer `GraphicsSystemNew::get()` callers to `EngineContext::get().graphicsSystem()`:
- `graphic_hud.c` (2 sites): `setCursorVisibility`
- `GameEngine.cpp` (1 site): `update`
- `PlayingState.cpp` (1 site): `setCursorVisibility`
- `VideoOptionsScreen.cpp` (1 site): `getDisplays`
- `MapEditorState.cpp` (1 site): `setCursorVisibility`

All 5 migrated files dropped their `GraphicsSystemNew.hpp` include. `GraphicsSystemNew::get()` is now confined to `GraphicsSystem.cpp` (the single delegation/bootstrap file, 6 sites: `createContext`, `createWindow`, `CreateWindowAndContext`, plus the 3 new delegators). Updated `MockGraphicsSystem` in `TestGraphicsSystem.hpp` with no-op implementations for the new interface methods.

Gates green: build / ctest -j20 **830/830** / validator `test.mod` 0/0 / menu smoke clean exit.

---

## egolib-gui carve — the fifth link-split (2026-06-10/11, branch `refactor/egolib-gui-layer`)

The first cohesive (non-grab-bag) middle layer: extracted the **generic GUI widget toolkit** as a new `egolib-gui` STATIC archive — a sibling that sits ABOVE `egolib-renderer` and `egolib-foundation-base`, below `egolib-library`. Layout went **base 143/physics 6/renderer 29/library 132 → base 141 ◄ {physics 6, renderer 28 ◄ gui 22} ◄ library 116**. Three verified passes; along the way it also **restored the acyclic DAG**, which fresh nm analysis found was *already broken* in the live tree (recent absorption commits had not nm-verified and had left base/renderer→library back-edges via `EngineContext::renderer()`).

**Pass 1 — UIManager decoupling (no archive change).** UIManager is the toolkit keystone (every widget reaches it transitively via `Container`). Decoupled it from the app-layer `EngineContext` and the game `DisplayMsg` system so it could move to a lower layer:
- New **`Ego::activeGraphicsSystem()` ownership-move seam** (`Graphics/IGraphicsSystem.cpp`, foundation-base) — mandatory because the graphics tests install `MockGraphicsSystem` through `EngineContext` and assert routing; `EngineContext` graphics methods became thin delegators (storage moved out of `EngineContext.cpp`).
- New **`Ego::GUI::postScreenMessage` sink seam** (`game/GUI/ScreenMessage.{hpp,cpp}`) for UIManager's screenshot status lines (the acyclic way for a lower layer to reach the game's on-screen message log); `GameEngine` installs the sink (wired to `DisplayMsg_print`) at init, clears at teardown.
- Routed UIManager's 8 `EngineContext::get().*()` calls onto lower-layer concretes/seams (`TextureManager::get`/`FontManager::get` (base), `Ego::activeRenderer`, `Ego::activeGraphicsSystem`, `activeImageManager`, `Log::activeTarget`) and dropped the `game/Core/EngineContext.hpp` + `game/game.h` includes (replacing the latter's `game/graphic.h` transitive with a precise `Extensions/ogl_extensions.h`).

**Pass 2a — `activeRenderer()` base seam (acyclicity fix).** Fresh nm showed base→library / renderer→library back-edges via `EngineContext::renderer()` in `Font.cpp`/`TextureManager.cpp` (base) and `ogl_extensions.c`/`OpenGL/Texture.cpp`/`renderer_3d.c` (renderer). Added the `Ego::activeRenderer()` ownership-move seam (`Renderer/ActiveRenderer.cpp`, foundation-base; `EngineContext` renderer methods delegate) and routed all 6 TUs onto it. Result: **renderer→library back-edges 0**.

**Pass 2b — the gui carve.** Created `egolib-gui` and moved 22 toolkit TUs into it (Component, Container, Layout, Material, InputListener, DrawingContext + the leaf widgets + UIManager + ScreenMessage + Component_bridge). Seams/relocations to keep it library-symbol-free:
- New **`Ego::GUI::activeUIManager()` seam** (in `UIManager.cpp`) for `Component::uiManager()` (used by every widget); `GameEngine` installs `*_uiManager` at creation, clears at teardown. `EngineContext::uiManager()`/`tryUIManager()` keep their GameEngine-derived path (the script-function test fixtures manipulate `engine._uiManager` directly), so both resolve the same instance — the two fixtures were updated to also publish the fake through the seam.
- Moved the unused-by-widgets `engine()` accessor **down** from `Component` (gui) to `GameState` (library), since it returns the game-layer `GameEngine` (~15 GameStates use it via `GameState : Container : Component`).
- Routed the 5 remaining widget `EngineContext` calls onto seams (`Button`/`InternalWindow` → `::activeAudioSystem()`; `Button`/`ProgressBar`/`Material` → `Ego::activeRenderer()`; `MessageLog` → `Ego::activeConfig()`).

Verified acyclic by per-archive nm closure (mangled symbols, positive controls): **egolib-gui, egolib-renderer, egolib-physics all 0 forbidden back-edges**; nothing below gui references a gui symbol. Gates green at each pass: build, exact `ar t` counts, ctest -j20 **830/830** (incl. the `MockGraphicsSystem` + fake-UIManager script-function fixtures, confirming the graphics/uiManager seams preserve stub routing), validator `test.mod` 0/0. (Menu smoke is not a usable gate in this sandbox — fonts fail to load via SDL_ttf on pristine `master` too, a pre-existing environment issue, not a regression.)

**Discovered, deferred → DONE next (see below):** one pre-existing back-edge remained — `game/mesh_geometry.c` (base) reads `g_meshLookupTables` (defined in `egolib-physics`), from the earlier mesh_geometry absorption.

## MeshLookupTables → foundation-base — DAG fully acyclic (2026-06-11, branch `refactor/mesh-lookup-tables-to-base`)

Closed the last back-edge the gui-carve pass left. Investigation showed `MeshLookupTables`'s *only* physics dependency was `twist_vel` (the gravity-derived steep-hill slide table, computed from `g_environment.gravity` in `PhysicalConstants`) — and `twist_vel` is **dead**: written in the constructor, read nowhere in the active tree. The other three tables (`twist_nrm`/`twist_facing_x/y`/`twist_flat`) are pure geometry, derived from base-layer `twist_to_normal`/`vec_to_facing`.

So instead of a data-split: dropped the dead `twist_vel` field + its computation (and the `PhysicalConstants` include), then relocated `MeshLookupTables.{hpp,cpp}` from `EGOLIB_PHYSICS_SOURCES` to `EGOLIB_FOUNDATION_BASE_SOURCES` (a pure CMake move once the dependency was gone). `mesh_geometry.c` (base) → `g_meshLookupTables` is now intra-base; the physics/graphics consumers (`ObjectPhysics`/`ParticlePhysics`/`Module_spawn`/`RenderPasses`) read it downward. Layout: **base 142 / physics 5 / renderer 28 / gui 22 / library 116**.

All five archives now verify **0 forbidden back-edges (ACYCLIC ✓)** — the first fully-clean state since the multi-archive split began. Behavior-identical (twist_vel was unused dead data). Gates: build, exact `ar t`, per-archive nm acyclicity with positive controls, ctest -j20 830/830, validator `test.mod` 0/0.

---

## GUI Component/Container characterization net (2026-06-11, branch master — uncommitted)

The first step of the maintainer-chosen **"de-risk → carve GameStates"** program: lay a behavioral
characterization net over the egolib-gui base classes BEFORE the next link-split (carving the
GameStates/menu screens, which derive from `Container`). A scout-next-heavy-front workflow (8 fronts,
adversarially verified) found the safe move-only modularization vein exhausted and ranked this the
highest-value sandbox-verifiable bounded front: it pins the base-class pair every widget (~30 GUI TUs)
and every `GameState` (`GameState : Container`) inherits, so the egolib-gui layer and the eventual
GameStates carve can be refactored without silently changing geometry, hit-testing, z-order, state
gating, or input propagation.

**New TU (test-only, zero production/CMake edits — auto-globbed by `GLOB_RECURSE CONFIGURE_DEPENDS`):**
`egolib/tests/egolib/tests/GuiComponentBehavior.cpp` — **44 tests** across three suites:

- **GuiComponent** (geometry/state/lifecycle): default bounds (0,0)-(32,32); `setPosition`/`setX`/`setY`
  translate keeping size; `setSize`/`setWidth`/`setHeight` keep the min corner; `setCenterPosition`
  (both axes + `onlyHorizontal`); `contains()` is **closed on both bounds** (min & max corners
  inclusive, per idlib `is_enclosing`); the `isEnabled() = _enabled && !_destroyed && _visible`
  gate (hiding also disables; disabling does not hide); `destroy()` forces enabled/visible false and
  **dominates** later `setEnabled(true)`/`setVisible(true)`; derived-position/bounds accumulation
  through a 2-level parent chain (`getDerivedBounds` translates own bounds by the **parent's** derived
  position); `bringToFront()` no-ops without a parent; the `InputListener` defaults (all `notify*`
  return false).
- **GuiContainer** (membership/z-order/input): add/remove set/clear the child parent + count;
  add/remove `nullptr` throws `idlib::argument_null_error` (note: the header doc says
  `invalid_argument_error` — **wrong**, the code throws `argument_null_error`);
  `clearComponents()`/`setComponentList()` empty the list but **do NOT** null dropped children's
  parent pointers (asymmetry vs `removeComponent`); `bringComponentToFront` and `Component::bringToFront`
  move to the back of draw order; `destroy()` auto-removes from parent; **input propagation** iterates
  reverse (last-added/top consumes first), skips `!isEnabled()` children, stops on first consumer,
  returns false (visiting all) if none consume; **mouse** events are translated by the container's
  **own** `getPosition()` and **compose per nesting level** (outer (10,20) → inner (5,5) → leaf sees
  event − (15,25) cumulatively — the seam a nesting refactor is most likely to perturb, since a
  top-level container alone cannot distinguish own- from derived-position); **keyboard/wheel** events
  propagate reverse-order/first-consumer-wins but are **NOT** position-translated; `KeyboardKeyReleased`/
  `KeyboardKeyTyped`/`MouseButtonClicked` are **not** among Container's five overrides, so they are
  **not forwarded** to children (fall through to the false default); Container does **NO hit-testing**
  (dispatches to enabled children regardless of bounds — a maintainer must not add a `contains()` gate).
- **GuiLayout** (pure geometry): `LayoutColumns`/`LayoutRows` stack/arrange + wrap on overflow; empty
  list is a no-op; **multi-wrap quirk pinned** — the column wrap resets x to a SINGLE
  `leftTop.x()+horizontalIncrement` (not cumulative per column index), so on a second wrap the third
  column overlaps the second (Layout.cpp:36; rows analogous at :81). Pinned as CURRENT behavior so a
  future "fix" toward the documented cumulative formula is a conscious, test-updating change.

**Engine-free by construction:** the tests never call `draw()`/`drawAll()` (the only paths that reach
`activeUIManager()`, which throws with no UIManager installed); construction, geometry, state, membership,
`notify*`, and layout are all engine-free. All instances are heap-allocated via `std::make_shared`
(Component derives from `enable_shared_from_this`; `destroy()`/the `shared_ptr` cast call
`shared_from_this()`). Positions are float-exact (`point_2s == Point2f`; idlib `single == float`), so the
scout's proposed "integer-truncation" pin category was dropped as false.

**Adversarial review (3 lenses — semantic accuracy / robustness / coverage gaps + synthesis): ship-as-is,
zero must-fix.** All 33 initial assertions were independently re-derived from source and confirmed; no UB,
tautology, or isolation issue. The four high-value should-fix coverage gaps it surfaced (nested own-vs-derived
translation, keyboard/wheel propagation, the not-forwarded trio, the InputListener defaults) plus cheap
high-value additions (no-hit-testing, `Component::bringToFront`, destroy-dominance, `setComponentList`
stale-parent, the layout multi-wrap quirk) were folded in (33 → 44 tests).

**Verification:** build 0; new tests 44/44; ctest -j20 **874/874** (830 prior + 44 new — note the current
baseline is 874, not the docs' stale "828/830 with 2 ScriptLoaderFixture failures"; those two pass here);
validator `test.mod` 0/0. No archive/DAG change (test-only), so no nm re-check needed. Menu smoke-run is not
a usable gate in this sandbox (SDL_ttf fonts fail on pristine master too).

**Next (Pass 2 of the program):** relocate the `GameState` base class to break the menu screens'
vtable/typeinfo back-edges into the game-core remainder, then measure the most-isolated leaf screen's
residual coupling — the first bounded seam-cut toward an eventual `egolib-gamestates` link-split (Front B,
refuted as a direct move: GameStates measured at 63 back-edges, needs seam-cutting first).

---

## Pass 2: GameState base seam-cut — activeGameEngine() ownership-move seam (2026-06-11, branch refactor/gamestates-decoupling)

Step 2 of the "de-risk → carve GameStates" program: **free the `GameState` base class of its only
game-core coupling**, making it relocatable toward an eventual `egolib-gamestates` archive. A
plan-gamestate-seam workflow (measure + design + adversarial-refute across 9 break-vectors) confirmed the
bounded scope and the install point before any edit.

**Measured starting point (nm on the live `egolib-library.a`):** `GameState.hpp` was *already* lower-layer
clean (includes only `GUI/Container.hpp` — in egolib-gui — and forward-declares `class GameEngine;`). The
**only** game-core coupling was in `GameState.cpp`: `engine()` did `return EngineContext::get().engine();`,
giving `GameState.cpp.o` exactly two game-core undefined symbols — `EngineContext::get()` and
`EngineContext::engine()`.

**The seam-cut (the proven ownership-move keystone pattern — mirrors `Ego::activeRenderer` /
`activeGraphicsSystem` / `Ego::GUI::activeUIManager`):**

- New `egolib/game/Core/ActiveGameEngine.{hpp,cpp}` — **global-namespace** (because `GameEngine` is a
  global-namespace class, unlike the `Ego::`-namespaced renderer/graphics seams) free functions
  `installActiveGameEngine(GameEngine&)` / `clearActiveGameEngine()` / `tryActiveGameEngine()` /
  `activeGameEngine()` over an anonymous-namespace `g_activeGameEngine` pointer. The header keeps to a
  `class GameEngine;` forward decl and pulls in **no** game-core header (so it is includable from a future
  lower-layer `GameState`); the `.cpp` includes only its own header + `<stdexcept>` (the accessor returns a
  reference and never dereferences `GameEngine`, so the forward decl suffices, exactly like `ActiveRenderer.cpp`).
- **Install point = `EngineContext::setEngine` / `clearEngine`** (NOT `GameEngine::initialize`/`uninitialize`).
  This is load-bearing: the script/engine **test fixtures publish the engine ONLY via `EngineContext::setEngine()`**
  (never `GameEngine::initialize()`), and `EngineContext` *owns* the `GameEngine` (`activeEngine` unique_ptr),
  so `setEngine` is the true install moment. Installed **after** both `setEngine` guards (null + double-install),
  so the throw paths never reach the install; cleared **before** `activeEngine.reset()`, so the seam never
  points at a destroyed object.
- `GameState.cpp` now `#include`s `ActiveGameEngine.hpp` instead of `EngineContext.hpp`, and both `engine()`
  overloads `return activeGameEngine();` (binding the non-const seam return to the const overload's
  `const GameEngine&` is a legal implicit const-add — identical to the prior `EngineContext::get().engine()`).
- `ActiveGameEngine.{cpp,hpp}` added to `EGOLIB_GAME_CORE_SOURCES` — they stay in **egolib-library** this
  pass, so the build/link/topology is neutral (the seam is defined and consumed entirely within the top
  archive). A new `EngineContextFixture.SetEnginePublishesActiveGameEngineSeam` test locks the
  install/clear lifecycle + reference identity (`&activeGameEngine() == context.tryEngine()`).

**Result:** `GameState.cpp.o` now has **zero** game-core (`EngineContext`) undefined symbols — its only
engine reference is `activeGameEngine()`, defined (`T`) in `ActiveGameEngine.cpp.o` in the same archive. The
`GameState` base is now relocatable; this is the keystone that unblocks the (deferred) GameStates carve.

**Verification:** build 0; nm proof (`GameState.cpp.o` shows no `EngineContext`, only `U activeGameEngine()`);
ctest -j20 **875/875** (874 + the new seam test — incl. the `ScopedPlayingStateHarness` / screenshot-hotkey
fixtures that exercise `GameState::engine()` in the real engine-using paths, confirming behavior identity);
validator `test.mod` 0/0; DAG sanity (no lower-layer archive references the seam — topology-neutral, still
acyclic). Behavior-identical (the seam resolves to the same `GameEngine`).

**Deferred (per the plan + the topological-shape caveat):** the actual `egolib-gamestates` archive carve —
even with the base freed, only 4 small leaf screens (`DebugFontRenderingState`, `InputOptionsScreen`,
`LoadPlayerElement`, `OptionsConfigActions`) become nm-clean, too small a cohort to justify a new archive +
its CMake/DAG surface, and GameStates is topologically near the *top* of the call graph (orchestrates; little
calls it) — the wrong shape for a below-remainder layer. **Next (Pass 3 candidate):** enlarge the clean
cohort by seaming the LIGHT 2–4-`EngineContext` screens (`AudioOptionsScreen`, `MainMenuState`,
`OptionsScreen`, `Select{Character,Module,Players}State`, `VideoOptionsScreen`, `MapEditorSelectModuleState`)
onto the existing `active*()` seams, then reassess whether the cohort is large enough to be worth a layer.

---

## vfs.c split — Pass A: extract the SDL_RWops adapter (2026-06-11, branch refactor/vfs-split)

First (lowest-risk) slice of splitting `vfs.c` (the largest TU, 1,920 lines) along its four
mutually-independent clusters, picked by a scout-next-heavy-front workflow as the cleanest available
file-split. The SDL_RWops adapter is a self-contained island: it wraps a `vfs_FILE` as an `SDL_RWops`
for SDL loaders, touches **no** VFS mount state, and uses `vfs_FILE` only opaquely through the public
`vfs.h` API — so it needs no VFS-internal header (the scout's proposed premature `vfs_internal.h`
scaffolding was correctly dropped per the adversarial verifier).

**Change:** moved the 10 RWops functions (6 static helpers `vfs_rwops_{size,seek,read,write,close,create}`
+ the 4 public `vfs_openRWops{,Read,Write,Append}` declared in `vfs.h`) out of `vfs.c` into a new sibling
`egolib/vfs_rwops.c`. The new TU includes `egolib/vfs.h` (for `vfs_FILE`, the public `vfs_*` API, and the
`struct SDL_RWops;` forward decl) plus **`<SDL.h>`** for the full `SDL_RWops` definition it dereferences
(`vfs.h` only forward-declares it — the verifier's key catch), `<cstdlib>` (malloc/free), `<cstdio>`
(`SEEK_*`). Registered `vfs_rwops.c` in BOTH `EGOLIB_VFS_SOURCES` (compilation) and
`EGOLIB_FOUNDATION_BASE_SOURCES` (membership), keeping it in `egolib-foundation-base`.

**Result:** `vfs.c` 1,920 → 1,786 lines (−134). Topology-neutral — `vfs_rwops.c.o` builds into
`egolib-foundation-base` and its only undefined symbols are `vfs_*` (defined in `vfs.c`, same archive) +
SDL/libstdc++/libc (lower/external); zero upward edge, DAG still acyclic.

**Verification:** build 0; nm (vfs_rwops.o defines the 4 public `vfs_openRWops*`, no upward undefined
symbols); ctest -j20 **875/875**; validator `test.mod` 0/0. Behavior-identical (pure code move).

**Next slices (deferred):** the mount-management cluster (`vfs_add_mount_point`/`remove` +
`_vfs_mount_info_*` + the shared `_vfs_mount_infos`/`_vfs_initialized`/`_vfs_atexit_registered` statics →
`vfs_mount.c`) — the only cluster touching the shared mutable mount state, so it needs a narrow
`extern`-ing seam (a private internal header) and is higher-risk; and the `SearchContext` class →
`vfs_search.c`.

---

## vfs.c split — Pass B: extract the mount-point management cluster (2026-06-11, branch refactor/vfs-split)

Second slice of the `vfs.c` split. The mount-info cluster owns the mount registry and the add/remove/query
functions; careful nm + call-graph measurement (NOT the scout's first estimate) showed the true seam:

- **Only `_vfs_mount_infos`** (the registry) and the `vfs_path_data_t` record are mount-cluster-private —
  used nowhere else — so both move to the new TU. (The other two statics the scout named,
  `_vfs_initialized` / `_vfs_atexit_registered`, are **init-cluster** state, staying in `vfs.c`.)
- The mount functions guard with `BAIL_IF_NOT_INIT()`, which reads `_vfs_initialized` (owned by `vfs.c`),
  and `vfs.c`'s path normalization (one call site) still calls `_vfs_mount_info_search`. So the narrow
  cross-TU seam is exactly: `extern _vfs_initialized` + the `BAIL_IF_NOT_INIT` macro + the
  `_vfs_mount_info_search` declaration — placed in a new **`egolib/vfs_internal.h`** (named to avoid
  colliding with the unrelated `egolib/VFS/internal.hpp`, which holds idlib path-parsing internals — the
  collision the adversarial verifier flagged).

**Change:** moved `vfs_add_mount_point`, `vfs_remove_mount_point`, `vfs_mount_info_strip_path`,
`_vfs_mount_info_search` (now non-static, header-declared), and the static helpers
`_vfs_mount_info_{matches×2,add,remove}` — plus the `s_vfs_path_data`/`vfs_path_data_t` struct and the
`_vfs_mount_infos` vector — out of `vfs.c` into a new sibling `egolib/vfs_mount.c`. `_vfs_initialized`
became a non-static definition in `vfs.c` (extern-declared in `vfs_internal.h`); the `BAIL_IF_NOT_INIT`
macro moved to `vfs_internal.h` (used 53× across `vfs.c` + the mount TU). `vfs.c` includes
`vfs_internal.h`. The cluster is non-contiguous in `vfs.c` (the `SearchContext::hasData/getData` methods sit
between the two mount blocks and stay), so the removal was done by a boundary-asserted scripted rewrite, not
a line-range truncation. Registered `vfs_mount.c` + `vfs_internal.h` in both CMake blocks, keeping them in
`egolib-foundation-base`.

**Result:** `vfs.c` 1,786 → 1,500 lines (−286); it is **no longer the largest TU** (`Object.hpp` at 1,613
now is). Topology-neutral — `vfs_mount.c.o` builds into `egolib-foundation-base`; its only undefined symbols
are `vfs_*`/PHYSFS/idlib/std/libc (no upward edge), and `vfs.c.o` resolves `_vfs_mount_info_search` within
the same archive.

**Verification:** build 0; nm (vfs_mount.o defines the mount API + the seam, no upward edges; vfs.c.o has
`_vfs_initialized` defined + `_vfs_mount_info_search` undefined-resolved-in-base); ctest -j20 **875/875**
(incl. `ModuleLoadSmoke`/`ImportWorkflow` mount paths; the content-validator itself mounts
`mp_data`/`mp_objects`, clean 0/0); validator `test.mod` 0/0. Behavior-identical (pure code move + a
narrow init-flag/macro seam).

**Next slice (deferred):** the `SearchContext` class (file enumeration; spans `vfs.c` ~978–1198 + the
`hasData`/`getData` pair) → `vfs_search.c` — also non-contiguous.

---

## egolib-gamestates carve — the SIXTH link-split, the first ABOVE egolib-library (2026-06-11)

The "de-risk → carve GameStates" program, executed to completion — and in the process **refuting the
strategic note's "Front B blocked at flag-day scale" verdict** (it had measured the FORWARD edge direction
and assumed a below-library carve). GameStates is topologically at the *top* of the call graph, so it carves
**above** `egolib-library`: the screens orchestrate the game and reach *down* into game-core, and the only
blocker is the small set of `library → screen` **reverse** edges (nm-measured on the live archives, including
the `.c` TUs and after stale-`.o` filtering: **7 staying TUs / 8 symbols** — `PlayingState::{getMiniMap,
getMessageLog,getStatusCharacterRef,addStatusMonitor,displayCharacterWindow}`, `VictoryScreen`/`MainMenuState`
ctors, `typeinfo PlayingState`).

**Pass 1 (de-risk step 3) — EngineContext seam for the menu cohort.** Migrated the 10 menu screens
(`AudioOptionsScreen`, `OptionsScreen`, `VideoOptionsScreen`, `Select{Character,Module,Players}State`,
`MapEditorSelectModuleState`, `MainMenuState`, `InGameMenuState`, `VictoryScreen`) off
`EngineContext::get().{config,graphicsSystem,profileSystem,textureManager}()` onto the lower-layer `active*()`
seams (`Ego::activeConfig`/`activeGraphicsSystem`/`activeTextureManager`, global `::activeProfileSystem`),
dropping `EngineContext.hpp`. Added the one missing seam, **`Ego::activeTextureManager()`** — a new
`egolib/Graphics/ITextureManager.cpp` (foundation-base, mirrors `IGraphicsSystem.cpp`'s ownership-move);
`EngineContext`'s texture-manager methods became thin delegators (no test stub, zero risk). nm: all 10 screens
`EngineContext`-free; cohort external reverse edges 17 → 12.

**The carve (3 reverse-edge seam-cuts, then the CMake split).**
- **`IPlayingStateController`** (`egolib/game/IPlayingStateController.hpp`, library layer) — the in-game control
  surface game-core reaches into (`getMiniMap`/`getMessageLog`/`getStatusCharacterRef`/`addStatusMonitor`/
  `displayCharacterWindow`/`endModuleInVictory`). `PlayingState` implements it. `GameEngine::getActivePlayingState()`
  + `EngineContext::{try,}activePlayingState()` + the `game_internal.h`/`script_functions_internal.h` inline
  wrappers now return `shared_ptr<IPlayingStateController>` via `dynamic_pointer_cast<IPlayingStateController>(
  getActiveGameState())`. **Key trick:** the cast's source/target typeinfos (`GameState`, `IPlayingStateController`)
  are *both* lower-layer, so the cast site references **no** concrete-`PlayingState` symbol — yet the runtime RTTI
  walks the real object, preserving the EXACT old "is the active state a PlayingState?" null/non-null semantics
  with **no** install/clear lifecycle. The 7 consumers (`Object_update`, `Player`, `game_loop.c`,
  `script_functions_{appearance,quests,commerce}.c`) route through the interface.
- **`GameEngine` main-menu-state factory** — `std::function<std::shared_ptr<GameState>()> _mainMenuStateFactory`,
  injected from `egoboo/Main.cpp::setMainMenuStateFactory` before `start()`; the two `pushGameState(make_shared<
  MainMenuState>())` sites call it (guarded with a clear `std::logic_error` if uninstalled). `GameEngine` no
  longer `#include`s/constructs `MainMenuState`.
- **`endModuleInVictory`** — `script_functions_commerce.c::pushModuleEndVictoryScreen()` routes through the
  active controller instead of constructing `VictoryScreen`; `PlayingState::endModuleInVictory()` does the push.
- **CMake:** `EGOLIB_GAMESTATES_LAYER_SOURCES` (19 `.cpp`), `REMOVE_ITEM`'d from `SOURCE_FILES`;
  `add_library(egolib-gamestates STATIC ...)` linking `egolib-library` (upward). `egoboo` + `egolib-tests-executable`
  link `egolib-gamestates`; `cartman` stays on `egolib-library`. `GameState` base stays in `egolib-library`.

**Layout: base 145 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 98 ◄ gamestates 19.** Gates at every step:
in-place **and** from-scratch clean builds; exact `ar t` (145/5/28/22/98/19); nm-acyclic (`egolib-library →
egolib-gamestates` = **0** undefined refs; positive control `gamestates → library` = 67; all four lower
archives → gamestates = 0); validator `test.mod` 0/0; ctest **875/875**. A 4-lens adversarial-review workflow
(behavior / layering / cmake / completeness, with verification of high-severity findings) returned **zero
confirmed high/critical**; its medium/low findings were addressed: the empty-`std::function` factory landmine
→ guarded with a named error; a gratuitous `cartman` link-altitude bump → reverted to `egolib-library`; the
`pushModuleEndVictoryScreen` guard + a stale `getActivePlayingState()` doc comment → documented; a dead
`VictoryScreen.hpp` upward include in `script_functions_internal.h` → removed.

**Next upward-carve candidates:** the strategic note's ScriptVM (91) / game-graphics-render (74) counts were
*forward*-direction — re-measure their *reverse* edges before declaring them blocked; they may be tractable as
further above-library layers. Otherwise the `Object` god-class multi-role decoupling (T1.2) or the deferred
Entities ownership-inversion (flag-day).

---

## egolib-scriptvm carve — the SEVENTH link-split (2026-06-11, branch `refactor/egolib-scriptvm-carve`)

The second *upward* split and the seventh egolib archive. Acts on the gamestates strategic
note's own prediction ("ScriptVM may be tractable as an upward layer; re-measure reverse edges").

### Scouting / verification (9-agent workflow, all 3 refuters `refuted:false`)

A deterministic inline nm pass measured the **reverse** edges (rest-of-`egolib-library` → cluster)
for four candidate upward-layer clusters, then a 9-agent workflow (5 deep-feasibility probes +
3 independent adversarial refuters + a synthesizer) verified the winner by reading code:

| candidate | TUs | reverse edges | verdict |
|-----------|-----|---------------|---------|
| graphics-render FULL | 27 | 45 | dead — `ObjectGraphics`/`ParticleGraphics` entity-coupled |
| graphics-render NARROW | 18 | 16 | feasible-with-seams (the next/8th carve candidate) |
| **ScriptVM** | 16 | **10** | **chosen** — dominated by `ai_state_t` relocation + 3 drivers |
| HUD widgets | 6 | 2 | trivially clean but small (a future easy carve) |

The 10 ScriptVM reverse edges (all defined in `script.c.o`): 7 `ai_state_t::*` methods (ctor/dtor/
reset/spawn/add_order/set_bumplast/set_changed, referenced by `Object_lifecycle`/`Object_update`/
`Object_attributes`) + 3 driver entries (`scr_run_chr_script(Object*)` ← `Object_combat`/`game_loop`;
`set_alerts` ← `game_loop`; `scripting_system_end` ← `GameSessionContext`). 85 forward edges
(cluster → library) are fine for an above-library layer. R1 ("could not refute after 5 attacks")
confirmed `ai_state_t`'s methods are pure data ops with zero interpreter coupling; R2 flagged the
test-harness install gap; R3 reproduced the exact 10-edge set and confirmed cluster↔gamestates = 0.

### P1 — relocate `ai_state_t` lifecycle/state down (commit `caba9fc26`)

`ai_state_t : public AI::State<ObjectRef>` is embedded by-value in `Object` (`Object.hpp:1437`) but
its 7 referenced methods are pure state ops. Moved them — plus their private spawn/bump helpers
(`isRuntimeObjectRefValid`, `tryRuntimeSpawnObject`, `publishSpawn{Identity,Overrides,Waypoint,
OrderDefaults}`, `shouldPublishBumpAlert`, each used only by the moved methods, verified by grep) —
from `Script/script.c` into a new `egolib/Entities/AiState.cpp` that **stays in egolib-library**
beside `Object` (it needs `GameSessionContext`/`Object`, which are library). Declarations stay in
`script.h`; `get_wp`/`ensure_wp` (not reverse edges) and the interpreter-coupled `set_alerts` stay
in `script.c`. nm: reverse edges **10 → 3**; `AiState.cpp.o → cluster` = **0** (no new edge).
Gate: build clean, ctest 875/875 (live-Object spawn tests exercise the relocated ctor/reset/spawn),
validator `test.mod` 0/0.

### P2 — `IScriptSystem` driver seam (commit `e71d78280`)

Routed the 3 driver entries through a lower-layer interface, mirroring `Ego::Entities::IObjectWorld`:
- `Script/IScriptSystem.hpp` — 3-method interface (`runCharacterScript(Object*)`/`setAlerts(ObjectRef)`/
  `endScriptingSystem()`; `Object` fwd-declared + `ObjectRef` from `typedef.h` → no game/ include) +
  install/clear/try/`activeScriptSystem()` accessors + `installDefaultScriptSystem()` boot entry.
- `Script/IScriptSystem.cpp` — accessor ownership (`g_activeScriptSystem`); placed in
  **egolib-foundation-base** beside `IObjectWorld.cpp`; `activeScriptSystem()` throws when uninstalled.
- `Script/ScriptSystemAdapter.cpp` — the VM-side adapter forwarding to `script.c`; in library for P2,
  joins scriptvm in P3 (where its refs to the 3 functions become intra-layer).
- The 3 library call sites (`game_loop.c` ×2, `Object_combat.cpp`, `GameSessionContext.cpp`) now call
  `Ego::Script::activeScriptSystem().<method>()`.
- **Boot install injected from ABOVE library**: `egoboo/Main.cpp` (the game) and a gtest global
  environment `ScriptSystemEnvironment.cpp` (the test executable — R2's required gap-fix, since tests
  drive `quitModule→endScriptingSystem` and `kill→runCharacterScript` and would otherwise throw).
nm: the 3 driver symbols are now referenced **only by `ScriptSystemAdapter.cpp.o`**; the real
consumers are cut. ctest 875/875 (harness install verified), validator 0/0.

### P3 — carve `egolib-scriptvm` (commit `293d626b9`)

`EGOLIB_SCRIPTVM_LAYER_SOURCES` = the 16 VM TUs (`script.c` + `script_implementation.c` +
`script_variables.c` + the 13 `script_functions_*.c`; `script_functions_bitwise.c` already in base) +
`ScriptSystemAdapter.cpp`. `REMOVE_ITEM`'d from `SOURCE_FILES`; `add_library(egolib-scriptvm STATIC)`
PUBLIC-links `egolib-library`. `egoboo` + `egolib-tests-executable` link `egolib-scriptvm` alongside
`egolib-gamestates`; `cartman` + the content-validator (no VM path — grep-confirmed they call no
`quitModule`/`kill`/`scr_*`) stay on `egolib-library`. **scriptvm and gamestates are SIBLINGS** above
library (nm: 0 edges either way).

Layout: **base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 83 ◄ {scriptvm 17, gamestates 19}.**

Full gate: in-place + from-scratch clean builds; exact `ar t` (146/5/28/22/83/17/19); nm acyclic
(0 forbidden back-edges across all 7 archives; positive controls real — scriptvm→library 85,
library→scriptvm 0, gamestates↔scriptvm 0/0); validator `test.mod` 0/0; ctest 875/875 (3×);
validator/cartman confirmed linking without scriptvm. Smoke-run n/a (no usable GL context in the
Wayland session — boot reaches the video-mode stage after all VFS/config/profile-load init; the
relocated/seamed paths are covered by the live-spawn ctest cases + the validator instead).

**Lesson reaffirmed:** measure REVERSE edges, not forward, for an above-library carve; the original
"ScriptVM 91 back-edges, blocked" verdict was the forward count. The two-tool kit: relocate-down for
mislocated definitions (`ai_state_t`, a pure win valuable on its own), interface-seam for genuine
upward calls (the 3 drivers). The test executable is a second install site whenever a seam is driven
by a runtime path the tests exercise (the IObjectWorld precedent auto-installs from library, but a
VM-above-library adapter cannot — it must be injected from above, like the main-menu factory).

---

## egolib-hud-widgets carve — the EIGHTH link-split (2026-06-11, branch `refactor/egolib-hud-widgets-carve`)

The maintainer pivoted to this after finding the graphics-narrow carve's hard core GL-gated (see that
entry). The in-game HUD widgets are a much smaller, render-driver-FREE upward carve that completed cleanly
with no GL gate. **The easiest link-split yet: a single reverse edge.**

**Measurement (inline, on the post-scriptvm tree).** HUD-widget cluster = the 6 game-coupled GUI widgets
(`CharacterStatus`/`CharacterWindow`/`InventorySlot`/`LevelUpWindow`/`MiniMap`/`ModuleSelector`) that stayed
in egolib-library when the generic egolib-gui toolkit was carved. After the scriptvm carve, `library →
HUD` reverse edges = **1**: `MiniMap::setShowPlayerPosition(bool)`, from the AI minimap-reveal in
`Object_update.cpp` (NAVIGATION perk) + `game_loop.c` (debug reveal). Topology: cluster→scriptvm = 0,
cluster→gamestates = 0, scriptvm→cluster = 2 (`script_functions_quests` reveals the minimap), gamestates→
cluster = 8 (PlayingState owns the HUD). So the cluster is a **MIDDLE upper layer: above library, BELOW both
scriptvm and gamestates** (both link it). 43 forward edges into library.

**P1 — the seam (commit `69ab6b5aa`).** Both reverse-edge call sites already reach the minimap via
`tryActivePlayingState()->getMiniMap()`, but `->setShowPlayerPosition()` link-references the concrete
`MiniMap` symbol. Added `IPlayingStateController::setMiniMapShowPlayerPosition(bool)` (implemented by
`PlayingState` as `_miniMap->setShowPlayerPosition(...)`) and routed the 2 call sites through it — the edge
becomes a lower-layer vtable dispatch (no concrete-`MiniMap` link edge). `getMiniMap()->setVisible()` stays
(setVisible is `Component`'s, in egolib-gui — not a reverse edge). nm: `library → HUD` reverse edges 1 → 0.

**P2 — the carve (commit `6c6a711e3`).** `EGOLIB_HUD_WIDGETS_LAYER_SOURCES` (the 6 widgets) REMOVE_ITEM'd
into `add_library(egolib-hud-widgets)` that PUBLIC-links egolib-library; both `egolib-scriptvm` and
`egolib-gamestates` link it (egoboo + the test executable inherit it transitively); `cartman`/the
content-validator (no in-game HUD, `library → HUD` = 0) stay on egolib-library.

Archive layout: **base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 77 ◄ hud-widgets 6 ◄
{scriptvm 17, gamestates 19}.**

Full gate green: in-place + from-scratch clean builds; exact `ar t` (146/5/28/22/77/6/17/19); nm-acyclic
(**0 forbidden back-edges across all 8 archives**; positive controls real: hud→library 43, scriptvm→hud 2,
gamestates→hud 8; invariants library→hud 0, hud→scriptvm 0, hud→gamestates 0); validator `test.mod` 0/0;
ctest **875/875**. No GL gate — the carve changes only archive membership + 1 flag-setter interface method;
the widgets' render code is byte-identical and the seam goes through the already-installed
`IPlayingStateController`. This is the first carve where measuring REVERSE edges yielded a near-trivial
blocker (1), confirming the HUD widgets are genuinely top-of-call-graph.

## egolib-game-graphics carve — the NINTH link-split (2026-06-11, branch `refactor/egolib-game-graphics-carve`)

Paired with a VM safety-net test landed first (commit `7495f2955`): `ScriptRuntimeFixture.
RunCharacterScriptExecutesCompiledOperationAndFunction` compiles a 2-instruction EgoScript from source
(`tmpargument = 12` → `SetContent`) into a spawned object and runs it through `scr_run_chr_script`;
`ai_state.content == 12` proves BOTH dispatch branches (`run_operation` + `run_function_call`) of the
`script.c` `runCharacterScript` loop executed real bytecode — the last fully-uncovered VM path (the other
runtime tests clear `_instructions`). ctest 875 → **876**.

**Measurement (archive-member nm).** Lesson re-learned the hard way: the `CMakeFiles/egolib-library.dir/`
object dir held **224 stale `.o`** from before the upper carves, polluting an object-glob measurement
(phantom `MapEditorState` edges). The real `libegolib-library.a` had **77 members** — always measure
against the live `.a`. Cluster = `Camera`/`CameraSystem`/`BillboardSystem`/`RenderPasses`/
`TextureAtlasManager` + the 11 `RenderPasses/*` (16 TUs). **15 reverse edges** library→cluster, from
exactly 2 TUs: `graphic.c` (14: the 11 RenderPass ctors via `GFX::GFX` make_unique + BillboardSystem ctor +
`render_all` via `GFX::renderBillboards` + TextureAtlasManager via `GameAppImpl`) and `GameEngine.cpp` (1:
`CameraSystem::CameraSystem` via the `idlib::singleton` `initialize`/`get` instantiation). `EntityList`/
`TileList`/`ObjectGraphics`/`ParticleGraphics` excluded (Entities/GameSession coupling) and confirmed NOT
to reference the cluster. Positive controls real (library→{hud,scriptvm,gamestates}=0); negative 318/427.

**Seam type: CONSTRUCTION, not call-dispatch.** Runtime access already routed through the `IGFX`/
`ICameraSystem`/`IBillboardSystem` interfaces (EngineContext); the only concrete references were the
*construction* of the singletons, triggered order-sensitively mid-`GameEngine::initialize()` (after the
gfx-config download, before `gfx_system_init_all_graphics`/the console rect). So moving the install to
`Main.cpp` pre-`start()` was unsafe.

### P1 — relocate-down + bootstrap-hook seam (commit `6a0a9808e`, behavior-preserving, no link change)
New `egolib/game/Graphics/GraphicsBootstrap.{hpp,cpp}` (stays in egolib-library): a `std::function`
register/run holder (`registerGraphicsBootstrap`/`runGraphicsBootstrap{Init,Teardown}`, null-safe) plus
declarations of the upper-defined `installDefaultGraphicsSystems`/`clear`. New `graphic_init.cpp`: the
`GFX::GFX`/`~GFX`/`renderBillboards` + `GameAppImpl` ctor/dtor/accessors relocated out of `graphic.c`,
and `installDefaultGraphicsSystems()` registering the construct+install / clear+destroy hooks (the
former `GameEngine` init/teardown blocks verbatim). `GameEngine.cpp` calls `runGraphicsBootstrap*` at the
identical call sites (ordering byte-identical); `graphic.c` keeps only the `RenderPass` base
(`reinitClocks` touches `RenderPass::clock` via IGFX); `egoboo/Main.cpp` registers the bootstrap before
`start()`, next to `installDefaultScriptSystem`. Still all in egolib-library here. ctest 876/876,
validator 0/0.

### P2 — split `egolib-game-graphics` (commit `e35e9edaa`, pure CMake, zero source edits)
`EGOLIB_GAME_GRAPHICS_LAYER_SOURCES` (the 16 cluster files + `graphic_init.cpp` = **17 TUs**) `REMOVE_ITEM`'d
from `SOURCE_FILES`; `add_library(egolib-game-graphics STATIC)` PUBLIC-links `egolib-library`.
`egolib-hud-widgets` re-pointed to link `egolib-game-graphics` (scriptvm/gamestates get it transitively;
also listed explicitly). `egolib-library` drops **77 → 62** (−16 cluster, +1 GraphicsBootstrap.cpp).
`GraphicsBootstrap` holder + the 4 excluded Graphics TUs stay in library; `cartman`/content-validator stay
on egolib-library only and link clean (proof the remainder is cluster-free).

Layout: **base 146 ◄ {physics 5, renderer 28 ◄ gui 22} ◄ library 62 ◄ game-graphics 17 ◄ hud-widgets 6 ◄
{scriptvm 17, gamestates 19}.**

Full gate green: in-place + from-scratch clean builds (egoboo/cartman/content-validator/tests); exact `ar t`
(library 62, game-graphics 17); nm-acyclic (**0 forbidden back-edges across all 9 archives**; headline
library→game-graphics 0, game-graphics→{hud,scriptvm,gamestates} 0, library→{hud,scriptvm,gamestates} 0;
forward control game-graphics→library 61); ctest **876/876**; validator `test.mod` 0/0. No GL gate — the
construction code is byte-identical and runs at the original call site through the hook; live-render smoke
n/a (no usable GL context in this environment). First carve to use a *construction-injection* seam
(`std::function` bootstrap hook) rather than a method-dispatch interface — the right tool when the reverse
edges are object *constructors* triggered order-sensitively from below.

---

## Theme 21 — Within-layer file-splits batch (2026-06-12)

After the nine-archive DAG was closed, the move-only absorption vein into the lower layers reached its
ceiling (the 2026-06-12 10th-archive scout — five candidates, each with an adversarial refuter — confirmed
no clean candidate remains; the residual `egolib-library` 62 TUs are intentional Entities↔Module↔
Physics↔GameSession↔Graphics game-core glue). The next-best ROI shifted to navigability splits: shrink
the over-1000-line TUs without crossing archive boundaries. An 11-agent scout-and-refute workflow ranked
five candidates; three came back as `go` and were executed and merged the same day. None changed link
topology — ctest 877/877 throughout.

### Pass 240 — `script_functions_spawn.c` 3-way split (merge `e6aaf1291`, branch
`refactor/script-functions-spawn-3way`, 2026-06-12)

The 1576-line `script_functions_spawn.c` — the largest .c TU after the prior `script_functions_systems.c`
decomposition — split into three within-`egolib-scriptvm` siblings:

- `script_functions_spawn_particle.c` (463 lines) — the 12 particle-spawn entries: `scr_SpawnParticle`,
  `scr_DisaffirmCharacter`, `scr_ReaffirmCharacter`, `scr_SpawnAttachedParticle`, `scr_SpawnExactParticle`,
  `scr_SpawnPoof`, `scr_SpawnAttachedSizedParticle`, `scr_SpawnAttachedFacedParticle`,
  `scr_SpawnAttachedHolderParticle`, `scr_SpawnExactChaseParticle`, `scr_SpawnExactParticleEndSpawn`,
  `scr_SpawnPoofSpeedSpacingDamage`.
- `script_functions_spawn_character.c` (458 lines) — the 8 character-spawn/respawn entries:
  `scr_SpawnCharacter`, `scr_RespawnCharacter`, `scr_SpawnCharacterXYZ`, `scr_SpawnExactCharacterXYZ`,
  `scr_SpawnAttachedCharacter`, `scr_EnableRespawn`, `scr_DisableRespawn`, `scr_MorphToTarget`.
- `script_functions_spawn.c` (629 lines, residual) — the 24 lifecycle/drop/cleanup/identify/state-mutation
  entries: `scr_DropWeapons`, `scr_GoPoof`, `scr_DropKeys`, `scr_DetachFromHolder`, `scr_CleanUp`,
  `scr_MakeCrushValid`, `scr_PoofTarget`, `scr_SetChildState`, `scr_DropItems`, `scr_RespawnTarget`,
  `scr_NotAnItem`, `scr_SetChildAmmo`, `scr_IdentifyTarget`, `scr_DropTargetKeys`, `scr_MakeCrushInvalid`,
  `scr_SetDamageTime`, `scr_SetTargetToChild`, `scr_SetDamageThreshold`, `scr_SetChildContent`,
  `scr_EnableInvictus`, `scr_DisableInvictus`, `scr_SetTargetSize`, `scr_EnableStealth`,
  `scr_DisableStealth`.

A new private `script_functions_spawn_internal.h` (74 lines) holds the truly shared spawn-helper
infrastructure in a `script_spawn_detail` namespace (`SpawnSelfContext` struct, `makeSpawnSelfContext`
both overloads, `gameSession()`, `isLiveSpawnObjectRef`), mirroring the existing
`script_functions_internal.h` / `script_detail` pattern (do-not-include-from-other-script_functions_*.c
TUs). Each TU keeps group-exclusive helpers in its own anonymous namespace: particle has
ownerRef/spawnLocalParticleForSelf/tryAttachParticleToResolvedSelf; character has
SpawnAttachmentTargetContext + log*/spawn/attach/publish helpers; residual has forEachLiveSpawnObjectRef +
drop/trySet*/publishCleanUp* helpers.

No public-header changes (`script_functions.h` already declares all 44 dispatch entries). 2 CMakeLists.txt
edits at the `EGOLIB_GAME_TOPLEVEL_SOURCES` (line 770) and `EGOLIB_SCRIPTVM_LAYER_SOURCES` (line 1193)
blocks. No archive boundary crossed, no nm back-edge check needed. Gates: in-place build clean, ctest -j20
**877/877**, validator `test.mod` 0/0. Refuter caught two non-load-bearing inaccuracies in the scout's
plan: the "~100-line shared preamble" was actually ~59 lines, and the predicted per-TU sizes
(700/650/580) were inverted relative to reality (residual is the largest at 629, not the smallest).

### Pass 241 — `vfs_search.c` extraction (merge `7d682043e`, branch `refactor/vfs-search-extraction`,
2026-06-12)

Completes the four-slice `vfs.c` carve (slices A `vfs_rwops.c` + B `vfs_mount.c` landed 2026-06-11 at
merge `49a2c532a`). Moved `SearchContext` (the four ctors + dtor + the two `makePredicate` factories +
`predicate` + `nextData` + `hasData` + `getData` + `enumerateFiles`) plus its only client
`vfs_copyDirectory` (51 lines) from `vfs.c` (1500 → 1276) into the new sibling `vfs_search.c` (260 lines).
`vfs_removeDirectoryAndContents` stays in `vfs.c` (zero SearchContext dep — it only calls
`vfs_resolveWriteFilename` + `fs_*`).

Two seam additions to `vfs_internal.h` (48 → 58 lines):

1. **`to_physfs_path` declaration** — the scout's refuter caught this. `SearchContext::enumerateFiles`
   calls `to_physfs_path` (defined at vfs.c line 222, no header decl). The function already has external
   linkage (no `static`), so this is purely a decl-side fix; without it the new `vfs_search.c` would not
   compile.
2. **`VFS_PATH` / `VFS_MAX_PATH` typedef** — relocated from `vfs.c`'s body so `vfs_copyDirectory`'s
   `srcPath`/`destPath` fixed-size buffers see the same 1024-byte bound from both TUs.

2 CMakeLists.txt edits (the `egolib-foundation-base` source block at line 453 + the second consumer at
line 1052). All four pieces stay in `egolib-foundation-base`; topology-neutral. Test coverage is solid for
a foundation-base file: ModuleLoadSmoke ×10 (SearchContext ctor + iteration), ImportWorkflow ×11 +
LoadPlayerElement + ModulePlayerStartup (vfs_copyDirectory against live VFS state). Gates: build clean,
ctest -j20 **877/877**, validator `test.mod` 0/0.

### Pass 242 — `particle_collision.c` 2-way split (merge `eba024d19`, branch
`refactor/particle-collision-split`, 2026-06-12)

The 1528-line `particle_collision.c` split into two within-`egolib-library` siblings (in
`game/Physics/`):

- `particle_collision_physics.c` (274 lines) — the game-state-light slice: `get_prt_mass` (75 lines,
  particle effective-mass calc), `get_recoil_factors` (38 lines, pure impulse ratio math),
  `do_prt_platform_detection` (83 lines, platform-attachment geometry) + static `attach_prt_to_platform`
  (25 lines). Three anonymous-namespace seams (`objectWorld`, `worldUpdateCount`, `physical`).
- `particle_collision_response.c` (1308 lines) — the chr-prt response chain: `do_chr_prt_collision_init`,
  `_get_details`, `_deflect`, `_damage`, `_bump`, `_handle_bump`, `_knockback` + the
  `do_chr_prt_collision` entry-point orchestrator + `spawn_bump_particles` (the bump-spawn fallout, used
  only by `_handle_bump`). The full 10-seam anonymous namespace
  (objectWorld/audioSystem/worldUpdateCount/damageable/physical/scriptable/heldItem/
  usedHeldItemForBlock/publishScoredHit/publishWeaponScoredHit). `chr_prt_collision_data_t` and its ctor
  live entirely here (no consumer outside the `do_chr_prt_collision_*` family).

The two TUs communicate only through the public `particle_collision.h` surface: response's `_knockback`
calls physics's `get_prt_mass`. The 3 shared seams (objectWorld/worldUpdateCount/physical) duplicate in
each TU's anonymous namespace; the 7 response-only seams stay in response alone (zero physics caller).

**Key refuter catch: `spawn_bump_particles`.** Original file declared this static (forward decl at L105,
definition at L1369). The scout's plan put the definition in physics.c with the forward decl in
response.c, which would not link — TU-local static cannot cross TUs. The fix: move
`spawn_bump_particles` *entirely* to response.c. Its only caller is `do_chr_prt_collision_handle_bump`,
which is also in response.c.

**Immovable-tent guards are behavior-preserved.** The L1069 guard in `do_chr_prt_collision_knockback`
(`CHR_INFINITE_WEIGHT == phys.weight || bumpdampen == 0.0`) lives unchanged in
`particle_collision_response.c`; the particle-side guard in `get_prt_mass`
(`CHR_INFINITE_WEIGHT == pprt->phys.weight`) lives unchanged in `particle_collision_physics.c`. The
recent `3d6614852` / `bc0b26b40` weight-255 fix is preserved.

No public-header changes (`particle_collision.h` still declares the same four-function surface
consumed by `CollisionSystem.cpp` + `ParticleRecoil.cpp` + `CollisionPipeline.cpp` externally). 1
CMakeLists.txt edit at the `game/Physics/` source block (line 722). Gates: in-place build clean, ctest
-j20 **877/877** (CollisionPipeline.cpp's 8 tests pin `do_chr_prt_collision` end-to-end;
ParticleRecoil.cpp's 10 tests pin `get_recoil_factors` edge cases including infinite weight;
PhysicsCollisionNormal + PhysicsIntersection cover the lower-layer geometry that
`do_chr_prt_collision_get_details` depends on), validator `test.mod` 0/0.

**Cumulative effect of the batch.** The over-1000-line .c list dropped from nine entries to **seven**:
gone are `script_functions_spawn.c` (1576, now three sub-1000 TUs) and `particle_collision.c` (1528, now
274 + 1308). The new largest .c TU is `particle_collision_response.c` (1308); `Object.hpp` (1613) is
still the single largest TU overall. The `vfs.c` carve was completed (1500 → 1276). The two parked
candidates from the scout: the **10th-archive carve** was parked (the best candidate `egolib-audio`
came out 1 TU / ~6 seam sites — the same ballpark as hud-widgets's 1 seam / 6 TUs, which is a 6× worse
payoff ratio; and the refuter found the main claimed payoff — cartman/validator stop linking SDL_mixer —
was false because `idlib-game-engine-library` links SDL_mixer unconditionally as a transitive dep of
`egolib-library`), and the **`Object.hpp` mechanical split** was parked (the refuter cut the scout's
claimed payoff from 415 → ~123 lines saved, because de-inlining single-line methods does not remove
header lines — the method declaration stays on the same line; the header is documentation-dominated
rather than code-dominated, ~979 of 1613 lines are doxygen + blanks).

### Pass 243 — `script_functions_teams_presentation.c` team split (2026-06-21)

The 735-line `script_functions_teams_presentation.c` residual from the earlier quests split had stayed as
a mixed team / minimap / end-message / follow-link / enemy-sense TU. This pass split out the team opcode
cluster into a sibling within `egolib-scriptvm`:

- `script_functions_teams.c` (301 lines) — the 11 team-management entries:
  `scr_JoinTargetTeam`, `scr_IfLeaderKilled`, `scr_BecomeLeader`, `scr_IfLeaderIsAlive`,
  `scr_JoinTeam`, `scr_TargetJoinTeam`, `scr_JoinEvilTeam`, `scr_JoinNullTeam`, `scr_JoinGoodTeam`,
  `scr_GiveExperienceToGoodTeam`, `scr_GiveExperienceToTargetTeam`.
- `script_functions_teams_presentation.c` (422 lines) — the residual presentation/link state:
  minimap reveal/blip helpers, end-message helpers, status monitor publication, deprecated
  `EnableListenSkill` logging, follow-link hook/test seam, and enemy-sense publication.

No public API changes (`script_functions.h` unchanged), no private internal header, and no static-to-extern
promotions. The new team TU duplicates only tiny file-local context helpers and uses `SelfRoleContext`
directly, so team opcodes no longer construct the presentation compatibility context or call the
`EngineContext`/GUI presentation adapters. The follow-link test hook
`scr_systems_set_follow_link_by_modname_for_test` stays in the presentation residual. The new `.c` is
registered in BOTH the master `EGOLIB_GAME_TOPLEVEL_SOURCES` and `EGOLIB_SCRIPTVM_LAYER_SOURCES` lists;
scriptvm archive membership is now 31 `.o`. Gates: build clean, focused `ScriptSystemsFunctions` 77/77,
ctest **897/897**, validator `test.mod` 0/0.

### Pass 244 — `ModelAnimationMetadata.cpp` legacy-input split (2026-06-21)

The 758-line `Graphics/ModelAnimationMetadata.cpp` split into two within-`egolib-foundation-base`
siblings:

- `ModelAnimationMetadata.cpp` (500 lines) — construction/reset, neutral metadata application
  (`initializeFromActionData`/`applyMetadata`), action-name mapping, heal-action fallback policy,
  walk/framelip initialization, public query methods, and action randomization.
- `ModelAnimationMetadata_legacy.cpp` (275 lines) — legacy MD2 frame-name recovery:
  `initializeFromLegacyFrames`, `ripActions`, `parseFrameDescriptors`, and `collectHealAliases`.

This is a pure member-function split: all cross-TU calls were already declared in
`ModelAnimationMetadata.hpp`, so there is no internal header and no static-to-extern promotion. The
function-local/static legacy frame-effects token state stays inside `parseFrameDescriptors` in the legacy
TU. The new `.cpp` is registered in BOTH the master graphics source list and
`EGOLIB_FOUNDATION_BASE_SOURCES`; foundation-base archive membership is now 160 `.o`.

Archive proof: both `ModelAnimationMetadata.cpp.o` and `ModelAnimationMetadata_legacy.cpp.o` are members
of `build/products/x64/lib/libegolib-foundation-base.a`. nm set-intersection of undefined symbols from
`ModelAnimationMetadata_legacy.cpp.o` against all eight upper egolib archives
(`physics`/`renderer`/`gui`/`library`/`game-graphics`/`hud-widgets`/`gamestates`/`scriptvm`) is empty.
Positive control against `egolib-foundation-base` finds the expected intra-base dependencies
(`ReadContext`, `vfs_read_string_lit`, `AnimatedModel::getFrames`, `Log`, and the residual
`ModelAnimationMetadata` methods).

Gates: build clean; focused `ModelAnimationMetadataTest.*` 6/6; ctest **897/897**; validator `test.mod`
0/0; full validator completed at the known legacy content baseline (42 modules, 10 warnings, 245 errors,
nonzero exit due to existing content errors).

### Pass 245 — `Script/script.c` VM-driver split (2026-06-21)

The 798-line `Script/script.c` split into two within-`egolib-scriptvm` siblings:

- `script_driver.c` (455 lines) — the character-script and alert-polling driver:
  `scripting_system_begin`/`scripting_system_end`, both `scr_run_chr_script` overloads,
  `set_alerts`, `ai_state_t::get_wp`/`ensure_wp`, and the anonymous driver helpers
  (`RuntimeActorContext`, script-error context publication, debug dump, invisible-target reset,
  waypoint alert/velocity helpers, and `runCharacterScript`).
- `script.c` (377 lines) — Runtime construction/statistics, script name arrays,
  `Ego::Script::runtimeState`, `script_error_model`/`script_error_classname` storage,
  opcode-dispatch methods (`run_function_call`/`run_operation`/`run_function`), the
  order-publication helpers (`issue_order`/`issue_special_order`), and `script_state_t` construction.

No public API changes (`script.h` unchanged), no private internal header, and no static-to-extern
promotions. The `script_error_*` storage remains in `script.c`; `script_driver.c` writes it through the
existing `script_internal.h` extern seam, and `script_operand.c` keeps reading it through the same seam.
The moved public driver entries are still resolved through `script.h`, so `ScriptSystemAdapter` and the
script runtime tests need no call-site changes. The new `.c` is registered in BOTH the master
`EGOLIB_SCRIPT_SOURCES` and `EGOLIB_SCRIPTVM_LAYER_SOURCES` lists; scriptvm archive membership is now
32 `.o`.

Gates: targeted build (`egolib-scriptvm`, `egolib-tests-executable`, `egoboo-content-validator`) clean;
focused `ScriptRuntime` 17/17; ctest **897/897**; validator `test.mod` 0/0. Archive proof:
`script_driver.c.o` is present in `libegolib-scriptvm.a`; nm set-intersections are zero for
`egolib-library -> egolib-scriptvm`, `egolib-scriptvm -> egolib-gamestates`, and
`egolib-gamestates -> egolib-scriptvm`; positive control `egolib-scriptvm -> egolib-library` finds 88
expected downward edges.

### Pass 246 — `ObjectGraphics.cpp` vertex/cache split (2026-06-21)

The 736-line `game/Graphics/ObjectGraphics.cpp` residual split into two within-`egolib-library`
siblings, leaving the existing animation sibling unchanged:

- `ObjectGraphics.cpp` (269 lines) — construction/destruction, lighting, profile setup, tint, frame
  accessors, flash, and matrix/reflection state.
- `ObjectGraphics_vertices.cpp` (502 lines) — vertex interpolation/cache/model-binding:
  `needsUpdate`, `interpolateVerticesRaw`, `updateVertices`, `updateVertexCache`,
  `updateGripVertices`, `clearCache`, `getVertex`, `setModel`, `isVertexCacheValid`,
  `getVertexCount`, and `flashVariableHeight`.
- `ObjectGraphics_animation.cpp` remains 587 lines — animation state machine unchanged.

No public API changes (`ObjectGraphics.hpp` unchanged), no private header changes, and no static-to-extern
promotions. The flip tolerance constant moved with its only consumers into the new vertex TU. The new
`.cpp` is registered only in the master `EGOLIB_GAME_GRAPHICS_SOURCES` grouping; it is intentionally NOT
in `EGOLIB_GAME_GRAPHICS_LAYER_SOURCES`, because `ObjectGraphics` stays in the game-core/library
remainder rather than the above-library scene-rendering archive.

Gates: targeted build (`egolib-library`, `egolib-tests-executable`, `egoboo-content-validator`) clean;
focused `ObjectGraphics` tests **27/27**; ctest **897/897**; validator `test.mod` 0/0. Archive proof:
`ObjectGraphics.cpp.o`, `ObjectGraphics_animation.cpp.o`, and `ObjectGraphics_vertices.cpp.o` are all
members of `libegolib-library.a`; no `ObjectGraphics*` member appears in `libegolib-game-graphics.a`.
nm set-intersections are zero for `egolib-library -> {game-graphics,hud-widgets,scriptvm,gamestates}`
and `egolib-game-graphics -> {hud-widgets,scriptvm,gamestates}`; positive control
`egolib-game-graphics -> egolib-library` finds 68 expected downward edges.

### Pass 247 — `script_compile.c` parser-helper split (2026-06-22)

The 798-line `game/script_compile.c` residual split into two within-`egolib-foundation-base` siblings,
leaving the existing lexer sibling unchanged:

- `script_compile.c` (575 lines) — parser lifetime, line loading, `parse_line_by_line`, jump parsing,
  opcode-table initialization, script file loading/fallback, and the debug-print code.
- `script_compile_parser.c` (254 lines) — token parsing, opcode emission, and syntax diagnostics:
  `parse_indention`, `parse_token`, `emit_opcode`, and `raise`.
- `script_compile_lexer.c` remains 387 lines — `line_scanner_state_t` scanning methods unchanged.

No public API changes (`script_compile.h` unchanged), no private internal header, and no static-to-extern
promotions. All moved parser helpers were already declared as `parser_state_t` methods in
`script_compile.h`, so cross-TU calls from `parse_line_by_line` resolve normally. `Opcodes` remains
defined in `script_compile.c` and is consumed through the existing extern declaration. The new `.c` is
registered only in `EGOLIB_FOUNDATION_BASE_SOURCES`.

Gates: targeted build (`egolib-foundation-base`, `egolib-tests-executable`,
`egoboo-content-validator`) clean; focused `Compilation|ScriptLoaderFixture|ScriptRuntimeFixture`
**21/21**; ctest **897/897**; validator `test.mod` 0/0. Archive proof:
`script_compile_parser.c.o` is present in `libegolib-foundation-base.a` (archive membership now 161 `.o`)
and absent from all eight upper egolib archives; nm set-intersection from `script_compile_parser.c.o`
into upper egolib archives is empty, with the base positive control finding expected intra-base
dependencies (`Opcodes`, `activeProfileSystem`, `line_scanner_state_t` scan methods, `ConstantPool`,
`CLogEntry`, and `Log`).

### Pass 248 — `script_functions_alerts.c` context split (2026-06-22)

The 643-line `game/script_functions_alerts.c` split into two within-`egolib-scriptvm` siblings:

- `script_functions_alerts.c` (460 lines) — the residual simple alert-bit predicates, hit-direction
  checks, level-up, and shop-theft checks.
- `script_functions_alerts_context.c` (187 lines) — the role/context-dependent alert predicates:
  `scr_IfSitting`, `scr_IfGrogged`, `scr_IfDazed`, `scr_IfBackstabbed`, `scr_IfHolderBlocked`, and
  `scr_IfHeldInLeftHand`, plus the local `SelfStateContext` helper cluster they already used.

No public API changes (`script_functions.h` unchanged), no private internal header, and no
static-to-extern promotions. The helper cluster moved wholesale with its only consumers, so the split is
purely TU-local. The new `.c` is registered in BOTH the master `EGOLIB_GAME_TOPLEVEL_SOURCES` and
`EGOLIB_SCRIPTVM_LAYER_SOURCES` lists; scriptvm archive membership is now 33 `.o`.

Gates: targeted build (`egolib-scriptvm`, `egolib-tests-executable`, `egoboo-content-validator`) clean;
focused `ScriptStateFunctionsFixture.*` **59/59**; ctest **897/897**; validator `test.mod` 0/0. Archive
proof: both alert objects are present in `libegolib-scriptvm.a`, no alert object appears in
`libegolib-library.a`, and the six moved `scr_*` symbols are defined by
`script_functions_alerts_context.c.o`.

### Pass 249 — `GameEngine.cpp` lifecycle/bootstrap split (2026-06-22)

The 661-line `game/Core/GameEngine.cpp` split into two within-`egolib-library` siblings:

- `GameEngine.cpp` (430 lines) — constants, construction, shutdown request, main loop, frame timing,
  update/render frames, screenshot hotkey polling, SDL event pumping, state-stack mutation, and accessors.
- `GameEngine_lifecycle.cpp` (277 lines) — lifecycle/bootstrap and teardown:
  `initialize`, `uninitialize`, `subscribe`, `unsubscribe`, and `renderPreloadText`.

No public API changes (`GameEngine.hpp` unchanged), no private internal header, and no static-to-extern
promotions. All moved methods were already declared on `GameEngine`, so cross-TU calls resolve through the
existing class surface. The new `.cpp` is registered only in `EGOLIB_GAME_CORE_SOURCES`; it is not in any
above-library carve-layer source list.

Gates: targeted build (`egolib-library`, `egolib-tests-executable`, `egoboo-content-validator`) clean;
focused engine-publication/screenshot tests **3/3**; ctest **897/897**; validator `test.mod` 0/0. Archive
proof: `GameEngine.cpp.o` and `GameEngine_lifecycle.cpp.o` are both present in `libegolib-library.a`
(archive membership now 76 `.o`), and no `GameEngine*` object appears in `libegolib-game-graphics.a`,
`libegolib-hud-widgets.a`, `libegolib-gamestates.a`, or `libegolib-scriptvm.a`. Live-archive nm
set-intersection from `egolib-library` into the four upper layers remains zero, with positive control
`egolib-game-graphics -> egolib-library` finding 68 expected downward edges.

### Pass 250 — `ObjectProfile_load.cpp` data-parser split (2026-06-22)

The 730-line `Profiles/ObjectProfile_load.cpp` split into two within-`egolib-foundation-base` siblings:

- `ObjectProfile_load.cpp` (203 lines) — texture/message loading plus the two `loadFromFile` orchestration
  overloads: service resolution, model/enchant/message/particle/sound setup, texture/default-name loading,
  data-file dispatch, and post-load lighting fixup.
- `ObjectProfile_data.cpp` (543 lines) — the `data.txt` parser:
  `ObjectProfile::loadDataFile`, the local `normalizePerkName` helper, and the local
  `vfs_get_next_object_profile_ref` helper.

No public API changes (`ObjectProfile.hpp` unchanged) and no static-to-extern promotions. The only shared
private shape is `ObjectProfile::LoadServices`, whose full definition moved from the `.cpp` body into
`ObjectProfile_internal.h` because both sibling TUs need the complete type. All member calls still resolve
through the existing `ObjectProfile` declarations, and the parser-local helpers moved with their only
consumer.

The new `.cpp` is registered only in `EGOLIB_FOUNDATION_BASE_SOURCES`, preserving the existing
foundation-base ownership of object-profile loading. Gates: targeted build (`egolib-foundation-base`,
`egolib-tests-executable`, `egoboo-content-validator`) clean; focused profile/content-path test slice
**164/164**; ctest **897/897**; validator `test.mod` 0/0; full validator at the known legacy content
baseline (**42 modules, 10 warnings, 245 errors**, nonzero exit from pre-existing content errors).
Archive proof: `ObjectProfile_core.cpp.o`, `ObjectProfile_data.cpp.o`, and `ObjectProfile_load.cpp.o` are
all present in `libegolib-foundation-base.a` (archive membership now 162 `.o`); `ObjectProfile_data.cpp.o`
is absent from all eight upper egolib archives; nm set-intersection from `ObjectProfile_data.cpp.o` into
upper archives is empty, with the base positive control finding expected intra-base dependencies
(`ReadContext`, `vfs_get_next_*`, `ObjectProfile::setupXPTable`, `ModelDescriptor::charToAction`, and
`Log`).

### Pass 251 — EngineContext owned-service seam closure (2026-06-22)

The remaining directly owned `EngineContext` service slots now route through active service registries:
input, image, font, texture-atlas, and GFX. New seam implementations back the service interfaces for
`IInputSystem`, `IFontManager`, `ITextureAtlasManager`, and `IGFX`; `IImageManager` uses the existing
`ImageManager.cpp` TU and preserves the legacy concrete-singleton fallback for non-EngineContext callers.

`EngineContext_services_owned.cpp` is now a compatibility delegation layer instead of a second set of
file-local service pointers. `EngineContext::imageManager()` intentionally keeps installed-only semantics
by checking `tryActiveImageManager()` before the fallback-capable `activeImageManager()`, and
`EngineContext::clearEngine()` now clears the texture-atlas seam along with the other owned services.

CMake placement keeps the DAG intact: `IInputSystem.cpp.o` and `IFontManager.cpp.o` are in
`libegolib-foundation-base.a`; `IGFX.cpp.o` and `ITextureAtlasManager.cpp.o` are in
`libegolib-library.a` and are intentionally absent from `libegolib-game-graphics.a`. Archive membership is
now **164 / 6 / 28 / 24 / 78 / 21 / 6 / 33 / 19** in the health-doc order.

Gates: `cmake --build build -j20` clean; focused `EngineContextFixture.*` **70/70**; ctest **912/912**;
validator `test.mod` 0/0. Archive proof: `foundation-base -> upper egolib`, `library -> upper four`, and
`game-graphics -> top three` nm set-intersections are all **0**; positive control
`game-graphics -> library` finds 68 expected downward edges.

### Pass 252 — `Object_ai.cpp` script-runtime seam split (2026-06-22)

The `Object` AI accessor/helper block moved out of `Object_update.cpp` into a new
within-`egolib-library` sibling:

- `Object_ai.cpp` (228 lines) — the private `scriptRuntimeState()` bridge plus the
  `IScriptable` AI accessors/helpers (`getAIAlertBits` through `spawnAIState`).
- `Object_update.cpp` (387 lines) — per-frame update, water/ripple handling, timers, perks,
  stealth detection, latch updates, and resize effects.

No public role-interface changes and no `ai_state_t` layout or script opcode behavior changes.
`Ego::Script::runtimeState(Object&)` now reaches the embedded AI state through the private
`Object::scriptRuntimeState()` friend seam instead of reading `object.ai` directly. The new `.cpp` is
registered only in `EGOLIB_ENTITIES_SOURCES`, so it remains in `libegolib-library.a` and is intentionally
absent from `egolib-game-graphics`, `egolib-hud-widgets`, `egolib-scriptvm`, and `egolib-gamestates`.

Gates: `cmake --build build -j20` clean; focused object/script-runtime slice **8/8**; ctest
**912/912**; validator `test.mod` 0/0. Archive membership is now
**164 / 6 / 28 / 24 / 79 / 21 / 6 / 33 / 19** in the health-doc order. Archive proof:
`Object_ai.cpp.o` appears only in `libegolib-library.a`; `library -> upper four` and
`game-graphics -> top three` nm set-intersections are **0**; positive control
`game-graphics -> library` finds 68 expected downward edges.

### Pass 253-257 — Object role caller cleanup and attribution quarantine (2026-06-22)

Continued the stable-first `Object` role program without changing public role interfaces, script opcode
APIs, `IDamageable::{damage,heal,kill}` signatures, CMake source placement, or archive membership.

The script combat/stat-gift helpers now keep their compatibility contexts role-sized: self/target state
resolution goes through existing `try*Role` helpers, while the remaining `std::shared_ptr<Object>` handle is
kept only where the current `IDamageable` attribution API still requires it. `Object_combat.cpp` now
quarantines combat attribution and reward publication behind file-local helpers: holder/mount/rider blame
resolution, kill-credit recipient resolution, killed-target publication, direct kill rewards, and the
death-alert/team-experience loop. `game_combat.c` gained local role adapters for script alert publication,
inventory/target/character-state reads, lifecycle termination, movement velocity, and ammo-known visual
publication, while preserving the legacy longbow kursed-slot gate exactly as characterized by tests.
`CharacterMatrix.c` now routes attachment-target, script-target, physical-transform, and movement-scale
observation through existing role surfaces; the public matrix APIs and concrete graphics mutation paths are
unchanged.

No source files moved between archives, so no live-archive back-edge proof was required for this pass.
Touched runtime files total 2,853 lines after the cleanup. Gates: `cmake --build build -j20` clean; focused
combat/script/matrix role fixture slice **167/167**; ctest **912/912**; validator `test.mod` 0/0.

### Pass 258 — Script spawn role-context cleanup (2026-06-22)

Continued the stable-first `Object` role program inside the spawn script helper layer. The private
`SpawnSelfContext` no longer exposes a concrete `Object&`; it now carries explicit self identity/profile
data plus the existing role surfaces needed by the spawn helpers (`IPhysical`, `IScriptable`,
`ITargetInfo`, `IInventoryHolder`, and `ILifecycleControl`) and copied log/old-position data.

`script_functions_spawn_particle.c` now uses that role context for self position, facing, team, owner,
holder/mount rider resolution, and particle profile metadata. `script_functions_spawn_character.c` now
uses the same context for self profile/team/facing/z-position, curse inheritance, inventory slot choice,
lifecycle respawn, and logging names. Concrete `Object` handles remain only where current APIs still
require them: spawned child ownership, safe-position checks, grip attachment, vertex placement, poof
spawning, and the temporary holder mutation around `scr_run_chr_script`.

No public role interfaces, script opcode APIs, CMake source placement, or archive membership changed, so no
live-archive back-edge proof was required. Gates: `cmake --build build -j20` clean; focused
`ScriptStateFunctionsFixture.*` **59/59**; ctest **912/912**; validator `test.mod` 0/0.

### Pass 259 — Script spawn service and child-context closure (2026-06-22)

Continued the stable-first spawn helper cleanup without changing public role interfaces, script opcode
APIs, CMake source placement, or archive membership. `script_functions_spawn_character.c` now resolves
spawned children into a local role-sized context (`IScriptable`, `ICharacterState`, `ILifecycleControl`,
and `IMovementControl`) and quarantines the remaining concrete child operations behind file-local helpers:
safe-position checks, termination, grip attach, temporary holder mutation, and script reruns.

`script_functions_spawn_particle.c` now uses the installed lower-layer service seams directly:
`activeParticleHandler()` for poof/local/poof-speed particle spawning and `activeProfileSystem()` for the
particle-profile lookup. Spawn-character logging now routes through `Log::activeTarget()` instead of the
EngineContext delegator. Singleton metrics dropped to **615** `::get()` lines, including **428**
`EngineContext::get()` and **133** `GameSessionContext::get()` lines.

Gates: `cmake --build build -j20` clean; focused spawn/poof/morph slice **17/17**; full
`ScriptStateFunctionsFixture.*` **59/59**; ctest **912/912**; validator `test.mod` 0/0.

### Pass 260 — Script spawn self-context resolver consolidation (2026-06-22)

Continued the stable-first spawn helper cleanup without changing public role interfaces, script opcode
APIs, CMake source placement, or archive membership. `SpawnSelfContext` now has a private
`isResolved()` validity predicate, and the three `script_functions_spawn*.c` TUs route self validation and
context construction through `resolveSpawnSelfContext(...)` instead of separately checking
`resolveSelfContext(self)` and rebuilding context from `Object`.

The concrete self `Object` lookup is now quarantined in `script_functions_spawn_internal.h` at the context
construction boundary. Spawn functions that need profile/name/old-position/role data reuse the checked
context; functions that only need the legacy "self must be live" gate call the same resolver for a uniform
failure path. Concrete `Object` handles remain only where current APIs still require them: spawned child
ownership, safe-position checks, grip attachment, vertex placement, poof spawning, and temporary holder
mutation around `scr_run_chr_script`.

Gates: `cmake --build build -j20` clean; focused spawn/script slice **21/21**; ctest **912/912**;
validator `test.mod` 0/0; full validator at the known legacy content baseline (**42 modules, 10 warnings,
245 errors**, nonzero exit from pre-existing content errors).

### Pass 261 — Object damage attribution and equipment role boundary (2026-06-22)

Introduced the 19th `Object` role interface, `IEquipmentControl`, and made `Object` implement it through
the existing `setEquipped(...)` behavior. The script appearance helpers now use that role for equipment
mutation and use the already role-owned `IDamageable::setDamageTargetType(...)` path for self damage-type
changes.

`IDamageable::{damage,heal,kill}` no longer expose `std::shared_ptr<Object>` on the role surface. They now
take `ObjectAttribution`, a small value carrying object ref, object team, damage-source team,
player/liveness snapshot data. `Object` keeps compatibility overloads for concrete callers, and
`Object::damage(...)` preserves the old split semantics: friendly-fire and easy-mode checks use the
attacker object's team/player snapshot, while blood particles and `TEAM_DAMAGE` alert behavior use the
explicit source team. Module terrain damage, particle damage, script combat, stat-gift healing, and focused
tests were migrated to the value attribution surface.

The script helper layer also gained `SelfProfileSnapshot` and shared attribution builders, letting
appearance/team-presentation code drop duplicated profile resolver structs and letting combat/stat-gift
contexts stop carrying concrete object handles solely for damage ownership. Alert and spawn helper pockets
were tightened to use existing role resolvers where only role data is needed.

No source files moved between archives; the only CMake change registers the new public header. Archive
membership remains **164 / 6 / 28 / 24 / 79 / 21 / 6 / 33 / 19** in the health-doc order. Gates:
`cmake --build build -j20` clean; ctest **912/912**; validator `test.mod` 0/0; `git diff --check` clean.
Live-archive proof after the CMake touch: all nine forbidden higher-layer `nm` set-intersections are **0**.

### Pass 262 — Script object role-context narrowing (2026-06-22)

Continued the stable-first `Object` role program inside the script helper layer without changing script
opcode APIs, source placement, CMake archive membership, or public damage/enchant behavior. The shared
script helper header now exposes live-object gates and an `IProfiled` resolver, and `SelfProfileSnapshot`
uses role surfaces for profile/base-model data with only the legacy display name still coming from the
concrete object.

Appearance and enchant helpers dropped duplicated concrete self/profile adapters. They now route spell,
spellbook, class-change, equipment, armor, money, damage-type, enchant, disenchant, and kurse gates through
role-sized contexts or the shared profile snapshot. `script_functions_appearance.c` and
`script_functions_enchant.c` no longer include or call `EngineContext` directly.

Combat helper contexts no longer carry `std::shared_ptr<Object>` only for attribution; they now store an
`ObjectAttribution` value plus the object ref. State and movement self contexts stopped carrying concrete
`Object*` handles, with `IMovementControl` widened by the existing stopped-by-mask getter so pathfinding can
read that state through the movement role. The graphics-only `SetFrame` path keeps its concrete object
lookup local to the opcode that still needs encoded animation frame mutation.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics are now **613** `::get()` lines, including **426**
`EngineContext::get()` and **133** `GameSessionContext::get()` lines. Gates: `cmake --build build -j20`
clean; focused script/object role slice **226/226**; ctest **912/912**; validator `test.mod` 0/0;
`git diff --check` clean.

### Pass 263 — Script action role-context closure (2026-06-22)

Continued the stable-first `Object` role program in the action/movement/team script helpers without
changing script opcode APIs, source placement, CMake archive membership, or spawn/enchant ownership
behavior. `SelfActionContext` no longer stores a concrete `Object*`; it now resolves profile, physical,
animation, visual, team-member, and target-info role surfaces, with `IProfiled::getProfileRef()` and
`IPhysical::getOldPosition()` covering the remaining profile-id and old-position reads.

`IAnimationControl` gained a narrow `setEncodedActionFrame(...)` wrapper so `scr_SetFrame` can publish the
legacy encoded `ACTION_DA` frame through the animation role instead of a concrete object/graphics lookup.
Team self-context construction now uses existing role resolvers for `ITargetInfo` and `ITeamMember`.
Concrete object handles remain intentionally unchanged in the legacy end-message/minimap-icon and
spawn/enchant ownership pockets.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Runtime source lines are now **128,594**, and `Object.hpp` is **994** lines;
singleton metrics remain **613** `::get()` lines, including **426** `EngineContext::get()` and **133**
`GameSessionContext::get()` lines. Gates: `cmake --build build -j20` clean; focused action/movement/team
role slice **23/23**; ctest **912/912**; validator `test.mod` 0/0; `git diff --check` clean.

### Pass 264 — Script presentation minimap controller seam (2026-06-22)

Continued the stable-first script presentation cleanup without changing script opcode APIs, source
placement, CMake archive membership, or spawn/enchant ownership behavior. `IPlayingStateController` now
owns the minimap show and icon-blip adapter surface, and `script_functions_teams_presentation.c` no longer
stores or calls `MiniMap` directly.

Script blip icons are resolved from existing role data (`IProfiled` plus `ITargetInfo`) with the same
profile/spellbook icon policy as `Object::getIcon()`, while `game_loop.c` routes the debug map reveal
through the same controller seam. Removing `MiniMap.hpp` from `script_functions_internal.h` exposed a real
free-rider in `script_functions_action_visual.c`; that translation unit now includes `game/graphic.h`
directly for HUD color constants instead of relying on the shared script header.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Gates: `cmake --build build -j20` clean; focused presentation script slice
**6/6**; ctest **912/912**; validator `test.mod` 0/0; `git diff --check` clean.

### Pass 265 — Script/Object presentation and service seam closure (2026-06-22)

Continued the stable-first `Object` role program across presentation, update, action, and spawn helper
pockets without changing script opcode APIs, source placement, CMake archive membership, or spawn/enchant
ownership behavior. `IPlayingStateController` now owns the remaining message-log publication adapter and no
longer exposes concrete `MiniMap` or `MessageLog` getters on the lower-layer interface; concrete
`PlayingState` getters remain local owner/test conveniences.

`Object_update.cpp` now uses the existing `IParticleHandler` active seam for water splash/ripple spawning,
and its cartography perk reveal routes through `IPlayingStateController::showMiniMap()`. Script action
helpers now use installed audio, camera, billboard, and UI-manager seams directly instead of the
`EngineContext` delegators. `SpawnSelfContext` now resolves its profile/physical/script/target/inventory
and lifecycle surfaces through role resolvers; the only concrete self read left in that construction path is
the legacy display name used in warning text.

A focused module-update characterization now proves object water-entry splash publication goes through the
installed particle service. Singleton metrics are now **607** `::get()` lines, including **420**
`EngineContext::get()` and **133** `GameSessionContext::get()` lines. Runtime source lines are now
**128,638**; ctest configures **913** cases. Archive membership remains
**164 / 6 / 28 / 24 / 79 / 21 / 6 / 33 / 19** in the health-doc order.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Gates: `cmake --build build -j20` clean; focused presentation/action/spawn
and water-update seam slice **43/43**; ctest **913/913**; validator `test.mod` 0/0; full validator at the
known legacy content baseline (**42 modules, 10 warnings, 245 errors**, nonzero exit from pre-existing
content errors).

### Pass 266-268 — Script helper service and role seam cleanup (2026-06-22)

Continued the stable-first script helper cleanup without changing script opcode APIs, source placement,
CMake archive membership, or spawn/particle ownership behavior. The module and presentation helper TUs now
route active playing-state discovery through the shared script helper seam, fog config reads through
`Ego::activeConfig()`, and deprecated-function logging through `Log::activeTarget()` instead of direct
`EngineContext` calls.

`IPhysical` gained a read-only tile accessor and `ITargetInfo` gained a read-only display-name accessor.
`script_functions_commerce_module.c`, `SelfProfileSnapshot`, and `SpawnSelfContext` now use those role
surfaces for actor tile and legacy display-name reads. The legacy `AddEndMessage(Object*)`, spawned child
ownership handles, and particle vertex-placement handles remain explicit concrete-object compatibility
pockets.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics are now **604** `::get()` lines, including **417**
`EngineContext::get()` and **133** `GameSessionContext::get()` lines. Runtime source lines are now
**128,656**, and `Object.hpp` is **997** lines. Gates: `cmake --build build -j20` clean; focused
role/script helper slice **219/219**; ctest **913/913**; validator `test.mod` 0/0; full validator at the
known legacy content baseline (**42 modules, 10 warnings, 245 errors**, nonzero exit from pre-existing
content errors); `git diff --check` clean.

### Pass 269 — Collision pipeline ownership boundary cleanup (2026-06-22)

Retired collision-dispatch ownership leakage from the object/object and object/particle paths without
changing source placement, archive membership, or gameplay collision policy. `CollisionSystem` now resolves
iterator handles once, passes `Object&` / `Particle&` through detection and chr-chr response, and no longer
calls `shared_from_this()` while dispatching nearby-object or nearby-particle collisions.

The chr-prt response entry point now accepts stable `ObjectRef` / `ParticleRef` ids, validates them through
the existing collision initializer, and compares attached-particle ownership by id. The former
`shared_ptr<Object>` / `shared_ptr<Particle>` API remains as a compatibility wrapper that rejects null
inputs before delegating to the ref-based path. `CollisionPipeline` coverage now pins the ref-based
chr-prt cases, invalid stale-id behavior, and compatibility wrapper null handling.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics remain **604** `::get()` lines, including **417**
`EngineContext::get()` and **133** `GameSessionContext::get()` lines. Runtime source lines are now
**128,667**, test files/lines are **50 / 23,994**, and ctest configures **916** cases. Gates:
`cmake --build build -j20` clean; focused `CollisionPipeline` slice **11/11**; ctest **916/916**;
validator `test.mod` 0/0; `git diff --check` clean.

### Pass 270 — Team leadership ref ownership cleanup (2026-06-22)

Continued the stable-first ownership-boundary cleanup from collision into team leadership and script leader
variable resolution without changing script opcode APIs, source placement, CMake archive membership, or
gameplay team policy. `Team` now stores leader and caller-for-help state as canonical `ObjectRef` values,
adds ref-first leader/caller setters and getters, and keeps the former shared-pointer `getLeader`,
`setLeader`, `getSissy`, and `callForHelp` methods as compatibility wrappers that resolve through the
active object handler.

Production leadership claim/clear, firstborn leadership, call-for-help publication, death leader-killed
alerts, and module team accessors now use ref comparisons instead of shared-pointer identity. Focused tests
were migrated to assert `ObjectRef` team state directly, including removing the direct `_sissy` fixture
reset. Script operand context no longer carries a concrete `leaderObject`; `VARLEADERX/Y/DISTANCE/TURN`
use the existing `leaderPhysical` role surface while preserving the missing-leader fallback behavior.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics are now **605** `::get()` lines, including **417**
`EngineContext::get()` and **134** `GameSessionContext::get()` lines. Runtime source lines are now
**128,707**, test files/lines are **50 / 23,993**, and ctest configures **916** cases. Gates:
`cmake --build build -j20` clean; focused team/leader slice **13/13**; ctest **916/916**; validator
`test.mod` 0/0.

### Pass 271 — Team ObjectRef seam closure (2026-06-22)

Closed the follow-up Team compatibility pocket without changing script opcode APIs, source placement,
CMake archive membership, or gameplay team policy. The unused shared-pointer Team methods
`getLeader`, `setLeader`, `getSissy`, `getSissyRef`, and `callForHelp(shared_ptr)` were removed, leaving
leader and caller-for-help state exposed through the canonical `ObjectRef` API only.

`Team.cpp` no longer includes or calls `GameSessionContext` / `GameModule`; leader/caller validation,
team lookup, XP publication, and call-for-help alert publication now reach the active object/team world
through the installed `Ego::Entities::IObjectWorld` seam. XP and help-alert broadcasts continue to use
the existing locked object iterator, preserving deferred-removal behavior.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics are now **601** `::get()` lines, including **417**
`EngineContext::get()` and **130** `GameSessionContext::get()` lines. Runtime source lines are now
**128,664**, and ctest configures **916** cases. Gates: `cmake --build build -j20` clean; focused
Team/target/runtime/alert slice **140/140**; ctest **916/916**; validator `test.mod` 0/0; diff hygiene
clean.

### Pass 272 — Inventory ObjectRef ownership cleanup (2026-06-23)

Continued the stable-first ownership-boundary cleanup from Team into inventory storage and holder role
surfaces without changing script opcode APIs, source placement, CMake archive membership, or gameplay
inventory policy. `IInventoryHolder` no longer exposes shared-pointer inventory get/set/remove methods;
inventory observation and mutation now route through `ObjectRef` APIs only.

`Inventory` now stores canonical `ObjectRef` slot values instead of `std::weak_ptr<Object>` handles. Slot
observation validates refs through the active session object handler when one exists, while keeping empty
slot reads usable for direct module-spawn realization tests that run without an active session module.
Production export, passage checks, key/pack drops, spawn/import tests, and script inventory callers were
migrated to the ref-first surface, including stale/terminated item coverage.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics are now **602** `::get()` lines, including **417**
`EngineContext::get()` and **131** `GameSessionContext::get()` lines. Runtime source lines are now
**128,666**, test files/lines are **50 / 23,989**, and ctest configures **916** cases. Gates:
`cmake --build build -j20` clean; focused inventory/object/script/import slice **271/271**; ctest
**916/916**; validator `test.mod` 0/0; `git diff --check` clean.

### Pass 273 — Particle and enchant ref API boundary cleanup (2026-06-23)

Continued the stable-first ownership-boundary cleanup into the first particle/enchant public API seam
without changing script opcode APIs, source placement, CMake archive membership, or deeper
particle/enchantment storage behavior. `IParticleHandler::spawnPoof(...)` and
`spawnDefencePing(...)` now accept `ObjectRef` values, with `ParticleHandler` resolving live object handles
internally and preserving the stale-attacker behavior that publishes `ObjectRef::Invalid`.

`IEnchantable::addEnchant(...)` now accepts owner/spawner refs. `Object::addEnchant(...)` resolves the
legacy weak-owner/spawner handles internally, while keeping `Enchantment`'s weak-pointer storage,
retargeting, overlay spawn, and active-enchant insertion behavior unchanged. Script enchant helpers no
longer carry temporary `shared_ptr<Object>` handles solely for role API calls, and the particle collision
spawn-enchant path now passes the particle owner ref directly. Focused coverage pins stale defence-ping
attacker refs and the legacy missing-spawner add-enchant behavior.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Singleton metrics remain **602** `::get()` lines, including **417**
`EngineContext::get()` and **131** `GameSessionContext::get()` lines. Runtime source lines are now
**128,677**, test files/lines are **50 / 24,016**, and ctest configures **917** cases. Gates:
`cmake --build build -j20` clean; focused particle/enchant/script/collision slice **227/227**; ctest
**917/917**; validator `test.mod` 0/0; `git diff --check` clean.

### Pass 274 — Documentation compaction refresh (2026-06-23)

Compacted the maintained docs without changing code: `README.md` now stays a short entry point, root
`README` is a pointer, Linux/Windows build docs avoid duplicated validator/platform detail, and `AGENTS.md`
plus current refactoring docs defer volatile metrics to `CODEBASE-HEALTH-STATUS.md`. Refreshed stale
current-state references for ctest count, singleton counts, archive/member counts, and large test-file
metrics. Gates: `git diff --check` clean; `ctest --test-dir build -N` still reports **917** cases.

### Pass 275 — Particle/enchant ObjectRef boundary cleanup (2026-06-23)

Continued the stable-first ownership-boundary cleanup in the particle/enchant seam without changing script
opcode APIs, source placement, CMake archive membership, or enchant storage. `Enchantment` no longer
exposes a concrete owner handle publicly; kill-on-end and missile-treatment deflection now use owner
attribution/payment helpers while the legacy weak-owner storage remains internal.

`Particle` collision history is now ref-first: `hasCollided(...)` and `addCollision(...)` take
`ObjectRef`, and the chr-prt collision response no longer resolves object handles solely to record or query
collision history. Focused coverage now pins enchant-backed missile deflection through a real loaded
missile-treatment enchant.

No files moved between archives and `egolib/library/CMakeLists.txt` was untouched, so no live-archive
back-edge proof was required. Runtime source lines are now **128,698**, test files/lines are
**50 / 24,059**, and ctest configures **918** cases. Gates: `cmake --build build -j20` clean; focused
collision/object/update slice **22/22**; ctest **918/918**; validator `test.mod` 0/0.
