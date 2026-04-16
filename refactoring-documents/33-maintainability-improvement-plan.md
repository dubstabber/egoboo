# Maintainability Improvement Plan

This document defines a prioritized, actionable plan to improve the Egoboo codebase maintainability based on the design-quality assessment in document 32. It complements the existing refactoring plan (document 19) by focusing specifically on design principles, pattern adoption, and structural quality rather than mechanical file splitting or build hygiene.

## Relationship to Existing Work

Document 19 defines Phases A through G covering build hygiene, test infrastructure, global state reduction, file splitting, error handling, namespace cleanup, and content validation. This plan does not replace those phases — it adds a design-quality overlay that should be woven into the existing work as each phase progresses.

**The rule remains: no flag-day rewrites.** Every improvement here should be incremental, verifiable, and non-breaking.

---

## Tier 1: High Impact, Achievable Now (Weeks 1-4)

These items have the highest maintainability payoff relative to risk and can start immediately. They align with or extend the already-started Phase C and Phase D work.

### 1.1 Migrate Callers to Context Wrappers

**Problem:** `EngineContext` and `GameSessionContext` exist but callers still use `_gameEngine` and `_currentModule` directly. The DIP improvement is architecturally present but not realized.

**Action:**
1. Audit all direct `_gameEngine->` accesses (176 sites). Replace with `EngineContext::get().engine()` or the more specific accessors (`uiManager()`, `renderedFrameCount()`).
2. Audit all remaining `_currentModule` accesses. Replace with `GameSessionContext::get()` calls.
3. After migration, make `_gameEngine` and `_currentModule` file-local to their definition sites (remove `extern` from headers).
4. Add a CI-level grep check: no new direct global accesses outside the context wrapper implementations.

**Why now:** The wrappers already exist and compile. Migration is mechanical. The payoff is that all new code uses a single seam that can later become an injected interface.

**SOLID dimension:** DIP (directly), ISP (context accessors are narrower than the full GameEngine interface).

**Risk:** Low. Each call site replacement is a one-line change. Can be done file-by-file.

### 1.2 Encapsulate Object's Public Fields

**Problem:** `Object` exposes 50+ public data fields, defeating encapsulation and making it impossible to reason about invariants or add validation.

**Action — Phase 1 (non-breaking):**
1. For each public field category, add accessor methods that delegate to the field.
2. Start with the most-accessed field groups:
   - **Combat:** `damage_threshold`, `damagetarget_damagetype`, `reaffirm_damagetype` → `getDamageThreshold()`, `getDamageTargetType()`, `getReaffirmDamageType()` + setters
   - **Team:** `team`, `team_base` → `getTeam()` already exists; add `getBaseTeam()`, use `setTeam()` consistently
   - **Equipment:** `holdingwhich`, `equipment` → `getHeldObject(slot)`, `getEquipment(slot)`, `setHeldObject(slot, ref)`
   - **Jump:** `jump_timer`, `jumpnumber`, `jumpready` → `getJumpTimer()`, `canJump()`, etc.
   - **Fat/Size:** `fat`, `fat_stt`, `fat_goto`, `fat_goto_time` → `getFat()`, `getTargetFat()`, `getResizeTimeRemaining()`

**Action — Phase 2 (after all callers migrated):**
3. Move the fields to `private` section.
4. Add validation in setters where invariants exist (e.g., money clamped to `MAXMONEY`).

**Why now:** The field names and types don't change. This is purely adding accessor methods and migrating callers, which is mechanical work that can proceed field-group by field-group.

**SOLID dimension:** SRP (fields become managed state with invariants), ISP (future: expose only relevant accessors via role interfaces).

**Risk:** Low-Medium. Many call sites to update, but each is mechanical. Do one field group per commit.

### 1.3 Convert Plain Enums to enum class

**Problem:** 36 plain `enum` types in headers pollute the global namespace and allow implicit integer conversion.

**Action:**
1. Convert the 23 header files containing plain `enum` to `enum class`.
2. Priority targets (most widely used):
   - `e_order` in `game.h` → `enum class Order`
   - `e_targeting_bits` in `game.h` → `enum class TargetingBits` (needs bitwise operator overloads)
   - `turn_mode_t` in `Object.hpp` → `enum class TurnMode`
   - `LatchButton` in `Object.hpp` → `enum class LatchButton`
   - Plain enums in `ParticleProfile.hpp` (5 enums) and `ObjectProfile.hpp` (4 enums)
3. Add the necessary `operator|`, `operator&` overloads for flag-type enums.

**Why now:** Each conversion is a self-contained change with compiler-enforced completeness. If a switch case is missed, the compiler will catch it.

**SOLID dimension:** Type safety, which supports LSP (no accidental implicit conversions) and OCP (compiler-enforced exhaustive handling).

**Risk:** Low. Compiler errors guide the migration.

### 1.4 Fix const Correctness Gaps

**Problem:** Some methods that are logically const are not declared as such (e.g., `Object::isAnyLatchButtonPressed()`).

**Action:**
1. Audit all non-const methods on `Object`, `ObjectProfile`, `GameEngine`, `ObjectHandler` that do not modify state.
2. Add `const` qualifier where appropriate.
3. Propagate `const` to parameters: functions in `game.h` that take `Object*` but don't modify should take `const Object*`.

**Why now:** Purely additive. Adding `const` never breaks callers (it can only fail to compile if something actually modifies state through the pointer).

**Risk:** Very low.

---

## Tier 2: High Impact, Requires Design Work (Weeks 4-10)

These items require more careful design but deliver the highest structural quality improvements.

### 2.1 Introduce Role Interfaces for Object (ISP Fix)

**Problem:** Every consumer of `Object` sees 80+ methods. The script system, combat code, rendering, physics, and GUI all couple to the same monolithic type.

**Action:**
1. Define role interfaces that describe how each subsystem interacts with an entity:

```
IDamageable        — damage(), heal(), isAlive(), getDamageReduction(), isInvincible()
IInventoryHolder   — getInventory(), getLeftHandItem(), getRightHandItem(), dropKeys(), dropAllItems(), getMoney(), giveMoney()
IScriptable        — ai (AI state), getProfile(), getObjRef(), isAlive(), isPlayer()
IRenderable        — inst (graphics), getSkinTexture(), getIcon(), setSkin()
IPhysical          — getObjectPhysics(), canCollide(), hit_wall(), test_wall(), movePosition(), teleport()
IIdentifiable      — getName(), setName(), getObjRef(), getProfileID(), isNameKnown()
```

2. Have `Object` implement all of them (initially a no-behavior-change step).
3. Gradually migrate consumers to accept the role interface rather than `Object*`/`shared_ptr<Object>`:
   - Combat code takes `IDamageable&`
   - Script functions take `IScriptable&`
   - Rendering takes `IRenderable&`
   - Physics takes `IPhysical&`

**Why this matters:** This is the single highest-value ISP improvement. It decouples subsystems from the god class without requiring an ECS rewrite, and it creates testable interfaces that can be mocked.

**SOLID dimension:** ISP (directly), DIP (subsystems depend on abstractions), SRP (clarifies which responsibilities each subsystem uses).

**Risk:** Medium. The interface extraction is safe, but migrating 393 `shared_ptr<Object>` call sites takes time. Can proceed interface-by-interface.

### 2.2 Extract Entity Creation into Factory

**Problem:** Object creation is scattered across `ObjectHandler::insert()`, `GameModule::spawnAllObjects()`, and spawn realization helpers with no factory abstraction. `Particle::initialize()` is a 403-line pseudo-constructor.

**Action:**
1. Create `EntityFactory` class with methods:
   - `createObject(ObjectProfileRef, SpawnEntry, GameSessionContext&)` → `shared_ptr<Object>`
   - `createParticle(ParticleProfileRef, SpawnContext)` → `shared_ptr<Particle>`
2. Move the initialization logic from `Particle::initialize()` into the factory.
3. Move the spawn-realization logic from `Module_spawn_realization.cpp` into factory methods.
4. `ObjectHandler::insert()` becomes a pure container operation that takes an already-constructed Object.

**Why this matters:** Factory pattern separates creation complexity from container management. Makes spawn logic testable without loading a full module. Enables future spawn customization (modding, procedural generation).

**SOLID dimension:** SRP (ObjectHandler is just a container), OCP (new entity types don't modify the container), DIP (callers depend on factory interface, not spawn internals).

**Risk:** Medium. The spawn logic has subtle ordering dependencies (parent tracking, player binding) documented in documents 30-31. Must preserve those semantics.

### 2.3 Introduce Service Interface Layer (Replace Singleton Direct Access)

**Problem:** 1,239 `::get()` singleton calls with no abstraction. No way to substitute implementations for testing.

**Action:**
1. Define abstract interfaces for the most-used services:

```
IProfileRepository    — getObjectProfile(), getParticleProfile(), isLoaded()
IAudioService         — playSound(), playMusic(), stopMusic()
IImageService         — loadTexture(), getTexture()
IPerkService          — getPerk(), hasPerk()
```

2. Have the existing concrete singletons implement these interfaces.
3. Create a `ServiceRegistry` that holds interface pointers, initialized during bootstrap:
   ```cpp
   class ServiceRegistry {
       static IProfileRepository& profiles();
       static IAudioService& audio();
       static IImageService& images();
       // ...
   };
   ```
4. Migrate callers from `ProfileSystem::get()` to `ServiceRegistry::profiles()`.
5. For tests, provide a `TestServiceRegistry` that wires in mock implementations.

**Why this matters:** This is the DIP keystone. Once services are accessed through interfaces, subsystems become testable in isolation and substitutable.

**SOLID dimension:** DIP (directly), OCP (new service implementations don't modify consumers), ISP (each interface exposes only relevant operations).

**Risk:** Medium-High. 140+ `ProfileSystem::get()` call sites. Proceed service-by-service, starting with the least-coupled one (`AudioSystem` — 43 call sites).

### 2.4 Normalize Error Handling Strategy

**Problem:** Three competing strategies (exceptions, `egolib_rv` return codes, silent failure) make error propagation unpredictable.

**Action:**
1. Document the error handling policy:
   - **C++ code:** Use exceptions for unexpected failures. Use `std::optional` or explicit error returns for expected "not found" cases.
   - **C code:** Continue using return codes until migrated to C++.
   - **Content loading:** Use structured error reporting (the validator pattern) rather than silent fallbacks.
2. Migrate the 39 `egolib_rv` return sites in C++ code to exceptions.
3. Audit the 60 `try/catch` blocks. Many catch-and-ignore; convert to catch-and-log at minimum.
4. Replace `nullptr`-return patterns where `std::optional` better expresses intent.

**SOLID dimension:** Supports all principles by making failure paths explicit and predictable.

**Risk:** Medium. Exception propagation through C code boundaries requires care.

---

## Tier 3: Strategic, Longer-Term (Weeks 10-20)

These are larger structural changes that depend on Tier 1 and 2 foundations.

### 3.1 Object Component Decomposition (Toward ECS-Lite)

**Problem:** The `Object` god class (996 lines header, 3,200+ lines implementation) carries 9+ responsibility domains.

**Action — not a full ECS rewrite, but a component extraction:**
1. Extract `CombatComponent` from Object:
   - Holds: damage_threshold, reaffirm_damagetype, damagetarget_damagetype, _hasBeenKilled, damage_timer
   - Methods: damage(), heal(), kill(), getDamageReduction(), giveExperience()
   - Object gets `CombatComponent& combat()` accessor

2. Extract `InventoryComponent`:
   - Holds: _inventory, _money, holdingwhich, equipment
   - Methods: getInventory(), getLeftHandItem(), getMoney(), giveMoney(), dropKeys(), dropAllItems()
   - Object gets `InventoryComponent& inventory()` accessor (note: `_inventory` already exists as `Inventory` type)

3. Extract `StealthComponent`:
   - Holds: _stealth, _stealthTimer, _observationTimer
   - Methods: isStealthed(), activateStealth(), deactivateStealth()

4. Extract `AttributeComponent`:
   - Holds: _baseAttribute, _tempAttribute, _currentLife, _currentMana, _perks, _levelUpSeed
   - Methods: getAttribute(), setBaseAttribute(), hasPerk(), addPerk(), getLife(), setLife()

**Sequence:** Start with `StealthComponent` (smallest, most self-contained). Then `CombatComponent`. Then `InventoryComponent`. Each extraction follows the same pattern as the file splits (create component, add accessor to Object, migrate callers).

**Why this matters:** Reduces Object from a 996-line header to a ~300-line coordinator that delegates to components. Each component is independently testable.

**SOLID dimension:** SRP (each component has one reason to change), ISP (callers access only the relevant component), OCP (new behaviors can be added as new components).

**Risk:** High. Deep entanglement between combat, attributes, and AI state. Requires careful dependency analysis per component. The role interfaces from Tier 2.1 provide the roadmap for which methods belong in which component.

### 3.2 Script System Extensibility

**Problem:** 404 script functions in procedural dispatch. Adding or modifying a script function requires editing the dispatch table and implementation files.

**Action:**
1. Define `ScriptFunction` as a callable interface:
   ```cpp
   using ScriptFunction = uint8_t(*)(script_state_t&, ai_state_t&);
   ```
   (This already exists implicitly via the `scr_*` function signature pattern.)

2. Build a `ScriptFunctionRegistry` that maps opcode → function pointer at initialization time rather than through a compile-time array.

3. Keep all existing `scr_*` functions unchanged — they register themselves into the registry.

4. Future extensibility: allow content mods to register script functions at runtime.

**Why this matters:** Converts the script system from closed-to-extension to open-to-extension without rewriting any script function implementations.

**SOLID dimension:** OCP (directly).

**Risk:** Medium. The existing dispatch table works and is well-understood. The registry is additive.

### 3.3 RAII for External Resources

**Problem:** `AudioSystem` holds raw `Mix_Music*`/`Mix_Chunk*` pointers. VFS lifecycle is not RAII-guarded. FileFormat C parsers use raw `FILE*`.

**Action:**
1. Create RAII wrappers:
   - `SdlMusicHandle` (wraps `Mix_Music*` with `Mix_FreeMusic` destructor)
   - `SdlChunkHandle` (wraps `Mix_Chunk*` with `Mix_FreeChunk` destructor)
   - `VfsGuard` (calls `PHYSFS_deinit` on destruction)
2. Replace raw pointers in `AudioSystem` with these handles.
3. For C parsers: defer to C++ migration timeline, but document the FILE* leak risk.

**SOLID dimension:** Resource safety, which supports SRP (cleanup logic moves from manual destructor code to type system).

**Risk:** Low for audio wrappers. Medium for VFS guard (C code boundary).

### 3.4 Eliminate egolib.h Uber-Header

**Problem:** `egolib.h` pulls in 57 headers (Audio, Core, Logic, Graphics, Renderer, VFS, Math, AI, Time, FileFormats, Console). Only 3 C files still use it directly, but its existence invites future misuse.

**Action:**
1. Replace each `#include "egolib/egolib.h"` with specific includes.
2. Delete `egolib.h` or reduce it to a documentation-only file that lists the subsystems.

**Why now in Tier 3:** Only 3 files use it. Low effort, but low impact since the migration discipline is already holding.

**Risk:** Very low.

---

## Tier 4: Aspirational, Deferred (Month 3+)

These are the right long-term direction but should not be started until Tiers 1-3 create the necessary foundations.

### 4.1 Full Service Injection (Replace ServiceRegistry with Constructor Injection)

Replace the static `ServiceRegistry` from Tier 2.3 with proper constructor injection. Subsystems receive their dependencies at construction time rather than pulling from a global registry.

**Depends on:** Tier 2.1 (role interfaces), Tier 2.3 (service interfaces).

### 4.2 Test-Driven Behavioral Coverage

With interfaces, factories, and components in place, write behavioral tests for:
- Combat: damage calculation, resistance, death handling
- Inventory: equip, drop, money, keys
- Script functions: individual function unit tests with mock entities
- AI: state transitions, target finding

**Depends on:** Tier 2.1 (mockable interfaces), Tier 2.2 (factory for test entities), Tier 2.3 (service mocks).

### 4.3 Observer Pattern for Game Events

Replace inline UI/sound/log updates in combat and entity code with an event bus:
- `EntityDamaged`, `EntityKilled`, `ItemPickedUp`, `LevelUp`, `EnchantApplied`
- GUI, audio, logging, and achievement systems subscribe to relevant events.

**Depends on:** Tier 3.1 (component extraction clarifies which events to emit).

### 4.4 Content Pipeline Modernization

With the runtime properly decoupled:
- Structured content IR for module/object/particle/enchant definitions
- Schema-validated content files
- Content migration tooling
- Modding API surface

**Depends on:** Tier 2.2 (factory), Tier 2.3 (service interfaces), Tier 3.2 (script extensibility).

---

## Dependency Map

```
Tier 1 (Foundation)
├── 1.1 Context wrapper migration ──┐
├── 1.2 Object field encapsulation ─┤
├── 1.3 enum class conversion       │
└── 1.4 const correctness           │
                                     ▼
Tier 2 (Design Quality)             │
├── 2.1 Role interfaces ◄───────────┤ (needs 1.2 for clean field access)
├── 2.2 Entity factory               │
├── 2.3 Service interface layer ◄────┘ (needs 1.1 for context wrapper pattern)
└── 2.4 Error handling normalization
                                     │
                                     ▼
Tier 3 (Structural)                  │
├── 3.1 Object component decomposition ◄── (needs 2.1 for role boundaries)
├── 3.2 Script extensibility           │
├── 3.3 RAII for external resources    │
└── 3.4 Eliminate uber-header          │
                                       │
                                       ▼
Tier 4 (Aspirational)
├── 4.1 Full constructor injection ◄── (needs 2.3 + 3.1)
├── 4.2 Behavioral test coverage   ◄── (needs 2.1 + 2.2 + 2.3)
├── 4.3 Observer event bus         ◄── (needs 3.1)
└── 4.4 Content pipeline           ◄── (needs 2.2 + 2.3 + 3.2)
```

---

## Priority Matrix

| Item | Impact | Risk | Effort | Order |
|------|--------|------|--------|------:|
| 1.1 Context wrapper migration | High | Low | Small | **1st** |
| 1.3 enum class conversion | Medium | Low | Small | **2nd** |
| 1.4 const correctness | Low | Very Low | Small | **3rd** |
| 1.2 Object field encapsulation | High | Low-Med | Medium | **4th** |
| 2.1 Role interfaces for Object | Highest | Medium | Medium | **5th** |
| 2.3 Service interface layer | High | Med-High | Large | **6th** |
| 2.2 Entity factory | Medium | Medium | Medium | **7th** |
| 2.4 Error handling normalization | Medium | Medium | Medium | **8th** |
| 3.1 Object component decomposition | Highest | High | Large | **9th** |
| 3.2 Script extensibility | Medium | Medium | Medium | **10th** |
| 3.3 RAII for external resources | Medium | Low | Small | **anytime** |
| 3.4 Eliminate uber-header | Low | Very Low | Small | **anytime** |

---

## Success Criteria

### Tier 1 complete when:
- Zero direct `_gameEngine` / `_currentModule` references outside context wrapper implementations
- Object has zero public data fields (all behind accessors)
- Zero plain `enum` types in C++ headers
- All const-correct methods are marked `const`

### Tier 2 complete when:
- At least 4 role interfaces defined and adopted by their primary consumer subsystem
- Entity creation goes through a factory; `ObjectHandler` is container-only
- Top 3 singletons (ProfileSystem, AudioSystem, ImageManager) accessed through abstract interfaces
- Error handling policy documented; `egolib_rv` eliminated from C++ code

### Tier 3 complete when:
- Object class header is under 400 lines (vs 996 today)
- At least 3 components extracted (combat, inventory, stealth)
- Script functions registered at init time, not compile-time dispatch
- No raw resource pointers in AudioSystem

### Overall success when:
- A new contributor can understand what a subsystem does by reading its interface, not its implementation
- Unit tests can exercise combat, inventory, or script logic without loading a full module
- Adding a new damage type, attribute, or script function requires touching at most 2 files
- Object creation is a factory call, not a scattered multi-step ritual

---

## Relationship to Existing Refactoring Phases

| Existing Phase | This Plan's Extensions |
|----------------|----------------------|
| Phase A (Build Hygiene) | ✅ Complete. No additions needed. |
| Phase B (Test Infrastructure) | Tier 4.2 extends this with behavioral tests enabled by Tier 2 interfaces. |
| Phase C (Global State Reduction) | **Tier 1.1 completes this phase.** Context wrappers exist; migration is the missing step. |
| Phase D (File Splitting) | ✅ Mostly complete. Tier 3.1 (component extraction) is the next evolution. |
| Phase E (Error Handling) | Tier 2.4 aligns with this. |
| Phase F (Namespace/Header) | Tier 1.3 (enum class) and Tier 3.4 (uber-header) align with this. |
| Phase G (Content Validation) | ✅ Baseline complete. Tier 4.4 extends this. |
