# Completed Passes Log

Chronological summary of the numbered refactoring passes (10 through 92) completed between 2026-04-13 and 2026-04-19. Passes 10 through 69 each had their own per-pass document before 2026-04-18; those documents were consolidated into this log to reduce directory clutter. Later passes append directly here. Full per-pass detail (scope constraints, acceptance commands, follow-on recommendations) remains in git history.

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
