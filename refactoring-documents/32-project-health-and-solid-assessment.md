# Project Health, Design Patterns, and SOLID Assessment

This document provides a comprehensive assessment of the Egoboo codebase as of 2026-04-16, evaluating code cleanliness, adherence to SOLID principles, design pattern usage, and C++ modernization state. It builds on the quantitative snapshot in document 17 but adds a design-quality lens and connects findings to actionable improvement areas.

## 1. Executive Summary

The codebase is in a **transitional state** — the original C dungeon crawler is being incrementally migrated to modern C++, and recent refactoring work (documents 17–31) has made real progress on file splitting, build hygiene, context wrappers, and test infrastructure. However, the core architecture still carries significant design debt:

- **SOLID adherence: 2/5** — Single Responsibility and Dependency Inversion are the weakest areas
- **Design pattern quality: 2.5/5** — Some patterns are used well (State, Iterator), others are misapplied (Singleton abuse, no Strategy/Factory)
- **Code cleanliness: 2.5/5** — Mixed C/C++ idioms, inconsistent naming, public field exposure, but headers are organized and newer code is cleaner
- **C++ modernization: 2.5/5** — Smart pointers adopted but misused (`shared_ptr` everywhere), modern features used inconsistently

The project is **functional but high-friction for changes**. The recent refactoring work is well-directed and the right investment, but the core coupling problems (globals, god classes, circular dependencies) remain the primary maintainability blockers.

---

## 2. SOLID Principles Assessment

### 2.1 Single Responsibility Principle (SRP) — Score: 1.5/5

This is the weakest SOLID dimension. Multiple critical classes carry far too many responsibilities.

**Object class** (`Entities/Object.hpp`, 996 lines):
The `Object` class is a textbook SRP violation. It is responsible for:
- Combat (damage, healing, death, resistance calculations)
- Movement and physics (position, collision, platform attachment)
- Inventory management (items, money, equipment slots)
- AI state (script state machine, boredom timers)
- Visual appearance (skin, alpha, sheen, shadow, sparkle, icon)
- Attribute system (stats, perks, enchantments, level-up)
- Social/team mechanics (team assignment, stealth, player binding)
- Input handling (latch buttons, player controls)
- Lifecycle management (spawning, respawning, termination)

The class exposes **70+ public methods** and **50+ public data fields**. Recent file splitting (D4) distributed the implementation across 6 `.cpp` files, which helps compilation but does not address the interface bloat — callers still see one monolithic type.

**GameEngine class** (`game/Core/GameEngine.hpp`, 282 lines):
Mixes main loop orchestration, SDL event handling, frame rate estimation, screenshot management, cursor state, and game state stack management. Less severe than Object but still 4-5 distinct responsibilities.

**game.h** (270 lines):
A grab-bag header exposing 40+ free functions spanning combat (`chr_do_latch_attack`), targeting (`chr_find_target`), export/save (`export_one_character`), particle management (`reaffirm_attached_particles`), mesh queries (`get_chr_level`), message display (`DisplayMsg_printf`), and environment I/O (`read_wawalite_vfs`). This is a namespace-free "everything" header.

**ObjectProfile** (`Profiles/ObjectProfile.hpp`):
Mixes data definition (the profile template), file parsing (`loadDataFile` — 493 lines), and serialization (`exportCharacterToFile` — 350 lines). The recent file split (D5) separated implementations but the class interface still conflates data ownership with I/O.

### 2.2 Open/Closed Principle (OCP) — Score: 2.5/5

**Good: Game State Machine.** The `GameState` base class (`GameStates/GameState.hpp`) provides a clean abstract interface with `update()`, `beginState()`, and `drawContainer()` as pure virtual methods. Concrete states (`PlayingState`, `MainMenuState`, `LoadingState`, etc.) extend this properly. Adding a new game state does not require modifying existing states.

**Bad: Script Dispatch.** `script_functions.c` (now split into 7 files) implements ~404 script functions as a giant procedural dispatch. Adding a new script function requires editing the dispatch table and adding code to the appropriate split file. There is no polymorphic or table-driven extensibility — it is a single massive switch.

**Bad: Damage/Attribute System.** Damage types, attributes, and perks are all handled through switch statements and enum indexing. Adding a new damage type or attribute requires touching multiple files across the combat, profile, and script layers.

**Bad: File Format Parsers.** Each content format (`spawn.txt`, `data.txt`, `wawalite.txt`, etc.) has its own bespoke parser. There is no common parser framework or grammar-driven approach. Adding a new format means writing a new parser from scratch.

### 2.3 Liskov Substitution Principle (LSP) — Score: 3/5

**Good: Entity hierarchy is shallow.** `Object` inherits from `PhysicsData` and `Collidable` — both are interface-like types. There are no deep inheritance hierarchies that would create substitution problems.

**Good: GameState hierarchy.** All game states are properly substitutable through the `GameState` base.

**Concern: Object is not specialized.** All game entities — heroes, monsters, items, scenery, platforms, spell effects — are the same `Object` class differentiated by flags (`isitem`, `platform`, `is_overlay`). This means there is no type-level distinction, but it also means no LSP violation since there is no hierarchy to violate. The trade-off is that Object must carry every field for every entity kind, leading to the SRP bloat documented above.

**Concern: `PhysicsData` inheritance.** Object inherits from `PhysicsData` publicly, exposing raw physics fields. Some of these are also wrapped by `ObjectPhysics` (composition), creating dual access paths to the same concept.

### 2.4 Interface Segregation Principle (ISP) — Score: 2/5

**Object is a fat interface.** With 70+ public methods, every consumer of `Object` — combat code, rendering, scripts, physics, GUI — sees the full interface. There are no role-specific interfaces (e.g., `Damageable`, `Renderable`, `Scriptable`, `Inventoried`).

**GameEngine is moderately fat.** It exposes timing, state management, screenshot, cursor, and UI access all through one type. GUI code that only needs `getUIManager()` also sees `shutdown()`, `setGameState()`, etc.

**ObjectHandler is well-segregated.** It provides a clean iterator pattern with RAII locking (`ObjectIterator`), separate `findObjects()` queries, and a clear insert/remove/exists API. This is one of the better-designed interfaces in the codebase.

**ProfileSystem is a service locator.** It bundles `ObjectProfileSystem`, `ParticleProfileSystem`, `EnchantProfileSystem`, and `ModuleProfileSystem` into one access point, accessed via `ProfileSystem::get()`. Callers that only need particle profiles still pull in the full system.

### 2.5 Dependency Inversion Principle (DIP) — Score: 1.5/5

This is the second weakest SOLID dimension after SRP.

**Global singletons dominate.** The codebase has 1,239 singleton `::get()` calls across 38+ headers. Key singletons include:
- `ProfileSystem::get()` — profile loading and caching
- `AudioSystem::get()` — audio playback
- `ParticleHandler::get()` — particle management
- `Log::get()` — logging
- `egoboo_config_t::get()` — configuration
- `PerkHandler::get()` — perk definitions
- `ImageManager::get()` — texture loading

No subsystem depends on abstractions or interfaces — they depend on concrete singleton instances. There is no dependency injection, no service interface layer, and no way to substitute implementations for testing.

**Context wrappers exist but are not yet used.** `EngineContext` and `GameSessionContext` were introduced (Phase C) but still delegate to the same globals internally. Callers have not been migrated. The wrappers are a step in the right direction but currently add indirection without reducing coupling.

**`_currentModule` elimination is progress.** The global `_currentModule` references were wrapped behind `GameSessionContext`, which is a real DIP improvement in intent if not yet in practice (it is itself a singleton accessed via `get()`).

---

## 3. Design Pattern Assessment

### 3.1 Patterns Used Well

| Pattern | Where | Quality |
|---------|-------|---------|
| **State** | `GameState` hierarchy | Clean polymorphic states, proper stack management |
| **Iterator** | `ObjectHandler::ObjectIterator` | RAII lock guard, safe concurrent iteration |
| **Non-copyable** | Most manager/handler classes | Consistent use of `idlib::non_copyable` mixin |
| **Composition over inheritance** | `ObjectPhysics`, `ObjectGraphics` inside Object | Physics and graphics as composed members rather than base classes |
| **Signal/Slot** | `idlib-signal`, `idlib::connection` in GameEngine | Event subscription for window events |

### 3.2 Patterns Misapplied

| Pattern | Where | Problem |
|---------|-------|---------|
| **Singleton** | 10+ subsystem singletons | Over-used as a service locator substitute. No testing seam, no substitutability. `ProfileSystem::get()` alone has hundreds of call sites |
| **God Object** | `Object` class | Carries all entity state and behavior for every entity type. Should be decomposed into components or role interfaces |
| **Anemic Domain Model** | `ObjectProfile` | Data-heavy class where behavior (parsing, export) is bolted onto the data holder instead of separated into services |

### 3.3 Patterns Missing (Would Add Value)

| Pattern | Where it would help | Benefit |
|---------|-------------------|---------|
| **Component/ECS** | Entity system | Replace the god `Object` with composed components (`CombatComponent`, `InventoryComponent`, `AIComponent`). Would fix the SRP problem and enable selective processing |
| **Factory** | Entity creation | `ObjectHandler::insert()` creates objects but doesn't encapsulate the complex initialization. `Particle::initialize()` (403 lines) is a constructor that should be a factory |
| **Strategy** | AI, damage calculation, rendering passes | AI behavior, damage formulas, and render passes are all hardcoded. Strategy pattern would make these extensible and testable |
| **Command** | Script system | Script functions are procedural dispatch. A Command pattern would make them individually testable and extensible |
| **Service Locator / DI Container** | Subsystem access | Replace raw singletons with a service registry that supports test doubles |
| **Observer** | Game events | Combat events, level-up, death, item pickup are all inline code. Observer pattern would decouple UI updates, achievements, logging from gameplay logic |
| **Builder** | Object/Profile construction | `ObjectProfile::loadDataFile` (493 lines) and `Particle::initialize` (403 lines) are crying out for builder pattern to replace monolithic initialization |

---

## 4. Code Cleanliness Assessment

### 4.1 Naming Conventions — Score: 2.5/5

The codebase has **three naming eras** coexisting:

| Era | Style | Example | Where |
|-----|-------|---------|-------|
| Legacy C | `snake_case` with prefixes | `chr_find_target`, `prt_find_target`, `ego_mesh_t` | `game.h`, C files |
| Transitional C++ | `camelCase` methods, mixed fields | `getProfile()`, `canCollide()`, `fat_goto_time` | `Object.hpp`, most `.cpp` |
| Modern C++ | `PascalCase` types, `camelCase` methods | `GameSessionContext`, `beginModule()` | Newer context wrappers |

The Object class is the worst example of mixed naming: method names are `camelCase` (`isFlying()`, `getTeam()`) but public fields use a mix of `snake_case` (`fat_goto_time`, `bore_timer`, `jump_timer`), bare names (`ammo`, `gender`, `skin`), and even prefix-style (`is_overlay`, `is_which_player`).

### 4.2 Data Encapsulation — Score: 2/5

The `Object` class exposes **50+ public data fields** directly, including:
- `ai` (full AI state machine)
- `gender`, `experience`, `ammo`, `ammomax`
- `team`, `team_base`
- `fat`, `fat_stt`, `fat_goto`, `fat_goto_time`
- `jump_timer`, `jumpnumber`, `jumpready`
- `attachedto`, `inwhich_slot`, `inwhich_inventory`
- `bump`, `bump_stt`, `bump_save`, `bump_1`, `chr_max_cv`, `chr_min_cv`
- `stoppedby`, `inwater`, `dismount_timer`

This defeats encapsulation. Any code in the project can read or modify an Object's internal state without going through methods. The `//TODO: Hack make private` comment on line 975 of `Object.hpp` acknowledges this but has been there since the original C-to-C++ migration.

### 4.3 Magic Numbers — Score: 2.5/5

Constants are partially extracted. The `Object` class defines some (`SIZETIME = 100`, `MAXMONEY = 9999`, `JUMPDELAY = 20`) but many remain scattered:
- `game.h`: `#define EXPKEEP 0.85f`, `#define TILE_REAFFIRM_AND 3`, `#define MAX_STATUS 10`
- `script_functions_*.c`: numerous raw numeric comparisons
- `ObjectProfile::loadDataFile`: field-specific magic values during parsing

### 4.4 Error Handling Consistency — Score: 2/5

Three competing strategies coexist:
- **C++ exceptions** (`throw`): 202 occurrences — used in newer C++ code
- **C return codes** (`egolib_rv`): 39 occurrences — used in C files and C-era C++ code
- **Silent failure**: many functions return `false`/`nullptr` and callers don't check

There is no documented policy on when to use which strategy.

### 4.5 Include Hygiene — Score: 3/5

`egolib.h` is a 57-include uber-header used primarily by C files. Newer C++ code tends to use specific includes. The `#pragma once` guards are universal (good). The Entity headers enforce include discipline via `GAME_ENTITIES_PRIVATE` guard (good pattern).

### 4.6 Dead/Commented-Out Code — Score: 3/5

Improved from the initial audit. Empty directories (`Lua/`, `Network/`) were removed in Phase A. 68 `TODO`/`FIXME`/`HACK` markers remain. Some `#if 0` blocks exist (e.g., `GameEngine.hpp:46-51`). The `utilities/migrator/` and `doc/ego2xml/` directories are still present but documented as stale.

---

## 5. C++ Modernization State

### 5.1 Smart Pointer Usage — Score: 2.5/5

| Pattern | Count | Assessment |
|---------|------:|------------|
| `shared_ptr` | ~910 | **Over-used.** Many objects have clear single owners but use `shared_ptr` for convenience. `ObjectHandler` stores all objects as `shared_ptr<Object>` when `unique_ptr` with raw-pointer observers would be clearer |
| `unique_ptr` | ~28 | **Under-used.** Should be the default for owned resources |
| `weak_ptr` | ~26 | Appropriately used for back-references (enchantments, last-spawned) |
| Raw `new`/`delete` | ~232 | **Legacy debt.** Concentrated in C-era code and some older C++ |

The `shared_ptr<Object>` pattern is particularly problematic: 393 occurrences across 76 files. The `Object` class even inherits from `enable_shared_from_this<Object>`, locking the entire entity system into shared-pointer semantics. This makes ownership reasoning difficult and prevents move-only semantics.

### 5.2 Modern C++ Features — Score: 2.5/5

| Feature | State |
|---------|-------|
| `enum class` | Mixed. Newer code uses `enum class` (e.g., `Zeitgeist::Time`), older code uses C-style `enum` (60 unscoped enums remain) |
| `override` | Used inconsistently. Some virtual overrides marked, others not |
| `nullptr` | Mostly adopted in C++ code, but `NULL` and `0` remain in C code |
| Range-based for | Used in newer C++ code; C files use index-based iteration |
| `auto` | Used sparingly and appropriately |
| Move semantics | Almost absent. No move constructors observed on key types |
| `constexpr` | Used for some Object constants; not widespread |
| Structured bindings | Not used |
| `std::optional` | Not used (would benefit many "return nullptr on failure" patterns) |
| `std::string_view` | Not used (would reduce string copies in parsers) |

### 5.3 Type Safety — Score: 2/5

- 211 C-style casts remain (concentrated in C files and C-era C++ code)
- Entity references use strongly-typed wrapper types (`ObjectRef`, `PIP_REF`, `ENC_REF`) — good
- `BIT_FIELD` is used as a raw integer bitfield type — should be `std::bitset` or typed flags
- Damage types, slot types, and team references use typed enums — good
- Many function parameters are bare `int`/`float`/`bool` where stronger types would document intent

---

## 6. Consolidated Health Scorecard (Updated)

| Dimension | Score | Trend | Notes |
|-----------|:-----:|:-----:|-------|
| **SRP adherence** | 1.5/5 | ↗ | Object/GameEngine are god classes; file splits help but don't fix interfaces |
| **OCP adherence** | 2.5/5 | → | State machine is good; script/damage systems are closed to extension |
| **LSP adherence** | 3/5 | → | Shallow hierarchies avoid LSP issues, but no hierarchies also means no specialization |
| **ISP adherence** | 2/5 | → | Fat interfaces on Object, GameEngine, ProfileSystem |
| **DIP adherence** | 1.5/5 | ↗ | Context wrappers introduced but not yet adopted; singletons still dominate |
| **Design patterns** | 2.5/5 | → | State/Iterator well done; Singleton abused; Factory/Strategy/Observer missing |
| **Naming consistency** | 2.5/5 | → | Three naming eras coexist |
| **Encapsulation** | 2/5 | → | 50+ public fields on Object alone |
| **Error handling** | 2/5 | → | Three competing strategies |
| **Smart pointer discipline** | 2.5/5 | → | shared_ptr over-used, unique_ptr under-used |
| **Test coverage** | 1.5/5 | ↗ | Up from 1/5 with parser golden-file tests and module smoke tests |
| **Build system** | 3.5/5 | ↑ | Explicit source lists, validator integrated |
| **Global state** | 2/5 | ↗ | Context wrappers exist but callers not migrated |
| **File size discipline** | 3/5 | ↑ | Major hotspots split; no file now exceeds 2,500 lines |
| **C++ modernization** | 2.5/5 | → | Smart pointers adopted but misused; modern features inconsistent |
| **Overall maintainability** | **2.5/5** | ↑ | Up from 2/5 thanks to build/split/test progress |

---

## 7. Key Strengths

1. **Refactoring work is well-directed.** Documents 17–31 show a disciplined, incremental approach with verification at each step.
2. **Game state machine is clean.** The `GameState` hierarchy is properly polymorphic with a stack-based lifecycle.
3. **Entity container is well-designed.** `ObjectHandler` with RAII iterator locking and quad-tree spatial queries is solid engineering.
4. **Build system is improving.** Explicit source lists, content validator, and documented build paths.
5. **Context wrappers are the right investment.** `GameSessionContext` and `EngineContext` create the seam for future DIP improvements.
6. **idlib sets a good example.** The 11-module structure of `idlib` demonstrates the target quality level.

## 8. Key Weaknesses

1. **Object god class.** 70+ public methods, 50+ public fields, 9+ responsibility domains.
2. **Singleton proliferation.** 1,239 `::get()` calls with no abstraction boundary.
3. **No dependency injection.** Every subsystem reaches directly for concrete singletons.
4. **shared_ptr overuse.** 910 uses where unique_ptr + observers would clarify ownership.
5. **Mixed C/C++ idioms.** 50/50 split by line count creates dual maintenance burden.
6. **Near-zero behavioral test coverage.** Parser tests exist, but gameplay logic, physics, rendering, and scripting are untested.
7. **Script system is monolithic.** 404 functions in procedural dispatch with no extensibility.
8. **Circular dependencies.** Entity ↔ Module is the worst, but Script → Everything is equally damaging.
