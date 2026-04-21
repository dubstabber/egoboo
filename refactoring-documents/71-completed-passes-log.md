# Completed Passes Log

Chronological summary of the numbered refactoring passes (10 through 150) completed between 2026-04-13 and 2026-04-21. Passes 10 through 69 each had their own per-pass document before 2026-04-18; those documents were consolidated into this log to reduce directory clutter. Later passes append directly here. Full per-pass detail (scope constraints, acceptance commands, follow-on recommendations) remains in git history.

For the current-state snapshot, read `CODEBASE-HEALTH-STATUS.md`. For the forward plan that builds on these passes, read `19-refactoring-roadmap.md`.

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

Checkpoint plan from `22-module-runtime-ownership-plan.md` executed across passes 23, 26, 27, 28. All checkpoints landed.

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
