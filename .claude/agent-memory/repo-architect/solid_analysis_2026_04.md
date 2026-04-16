---
name: SOLID Analysis 2026-04
description: SOLID principles and design patterns assessment for key egolib subsystems — findings from April 2026 audit
type: project
---

## SRP

- GameEngine (~280 lines header): manages main loop timing, state machine, screenshot requests, cursor visibility, UIManager ownership, config loading, SDL init. 5-6 distinct responsibilities. Tolerable but borderline.
- Object.hpp: 995 lines, 4 public sections, ~80 public methods. Covers physics, combat, AI state, graphics, enchants, inventory, perks, money, stealth, team, rendering. Clear SRP violation — a God Object.
  - Notable: `public: //TODO: Hack make private` block at line 975 exposes ObjectGraphics `inst` directly.
  - Large public raw-field block (lines 827–942): ai_state, orientation, bump collision volumes, timers, all exposed as public data members.
- ObjectProfile.hpp: 807 lines, similar breadth (stats, sounds, skins, perks, AI defaults, geometry). Also a God Object but for static data rather than runtime state.
- PlayingState.hpp: Clean. 77 lines, delegates rendering and status tracking. SRP largely respected.

## OCP / Game State Machine

- GameState base class (GameState.hpp): abstract `update()` and `drawContainer()` pure virtuals. 19 concrete states all extend this single interface. Textbook State pattern, OCP-compliant for adding new states.
- Script dispatch: 808 total `scr_` functions spread across 7 `.c` files (script_functions_action.c: 46, _target: 81, _state: 91, _systems: 96, etc.). Each function is a standalone `uint8_t scr_Xxx(script_state_t&, ai_state_t&)`. Dispatch is via a flat opcode-indexed function table (declared in script_functions.h, 404 entries). Adding a new opcode requires modifying the table — not OCP-compliant, but a well-understood extension mechanism.

## LSP / Entity Hierarchy

- Entity hierarchy is flat: Object, Particle, Enchantment are independent classes. No deep inheritance that could cause LSP violations.
- Object inherits: PhysicsData, Collidable (interface), enable_shared_from_this. Collidable has pure-virtual `hit_wall` / `test_wall` — Object overrides all of them. No substitute types exist; everything is a concrete Object.
- Particle and Object are entirely separate lineages — no shared game-entity base class.

## ISP

- GameEngine public interface: ~15 public methods, cohesive. `getActivePlayingState()` returns a concrete subtype (PlayingState), creating a downcast dependency from engine to state.
- Object: ~80 public methods — extreme ISP violation. Callers that only need damage(), or only need inventory access, or only need AI state, must take a full Object reference.
- ObjectHandler: appropriately narrow — iteration, existence check, add/remove/get.

## DIP

- ProfileSystem: `idlib::singleton<ProfileSystem>`. Called via `ProfileSystem::get()` — 140 direct references across 53 files. No abstraction layer; all callers depend on the concrete singleton.
- AudioSystem: same pattern, same access.
- GameSessionContext: singleton with `GameSessionContext::get()` — 101 references across 51 files. Introduced as part of `_currentModule` migration but still a global concrete.
- game.h: exposes free functions (`check_stats`, `readPlayerInput`, `let_all_characters_think`, `move_all_objects`) directly — procedural global coupling, no interface.
- Script functions: access `objectHandler()`, `ProfileSystem::get()`, `_currentModule` directly via macros (SCRIPT_FUNCTION_BEGIN/END, SCRIPT_REQUIRE_TARGET). No injection.
- PlayingState.cpp: 12 direct `_currentModule` / `GameSessionContext::get()` accesses.

## Design Patterns

- **Singleton**: at least 4 (GameEngine as global unique_ptr, ProfileSystem::singleton, AudioSystem::singleton, GameSessionContext::singleton). Pattern is consistently applied but injection is never used — callers are hard-wired.
- **State**: cleanly implemented. GameEngine holds a `std::forward_list<shared_ptr<GameState>>` stack. States pushed/popped. 19 states, all conforming. This is the healthiest pattern in the codebase.
- **Observer/Signal**: `idlib::signal` / `idlib::connection` used in GameEngine (shown/hidden/resized window events), GUI subsystem, Viewport, RendererInfo — 35 connection/signal references across 12 files. Not used for gameplay events; confined to UI and window lifecycle.
- **Factory**: No formal factory class. Object creation is done via `ObjectHandler::insertObject()` called from spawn functions. Particle creation via `Particle_spawn.cpp`. No factory interface — callers call spawn free functions directly.
- **Strategy**: Not applied. AI logic is encoded as bytecode executed by the script VM; no strategy interface for AI behaviors, rendering variants, or physics modes.
