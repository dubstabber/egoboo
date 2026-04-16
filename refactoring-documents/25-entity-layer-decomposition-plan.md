# Entity Layer Decomposition Plan

This document defines the next refactoring pass: splitting the three largest remaining files in the entity/profile layer into focused translation units.

## Implementation Status

- Phase 1 (`Object.cpp`) is complete in the current workspace.
- Phase 2 (`ObjectProfile.cpp`) completed on 2026-04-16 with:
  - `ObjectProfile_internal.h` (35 lines)
  - `ObjectProfile_core.cpp` (440 lines)
  - `ObjectProfile_load.cpp` (704 lines)
  - `ObjectProfile_export.cpp` (375 lines)
- Verification after the Phase 2 split:
  - `cmake --build build -j4`
  - `ctest --test-dir build --output-on-failure -j4`
  - `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg ./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod`
- Observed deviation from the draft plan:
  - `vfs_get_next_object_profile_ref(...)` stayed as a file-local helper in `ObjectProfile_load.cpp` instead of moving into the export unit because its only caller is the `data.txt` parser.
- Phase 3 (`Particle.cpp`) completed on 2026-04-16 with:
  - `Particle_internal.h` (54 lines)
  - `Particle_core.cpp` (404 lines)
  - `Particle_update.cpp` (320 lines)
  - `Particle_combat.cpp` (181 lines)
  - `Particle_spawn.cpp` (595 lines)
- Verification after the Phase 3 split:
  - `cmake --build build -j4`
  - `ctest --test-dir build --output-on-failure -j4`
  - `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg ./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod`
- Observed deviation from the draft plan:
  - `prt_environment_t` constructor/reset and `Particle::Particle()` moved into `Particle_core.cpp` instead of the internal header so the split keeps those non-inline definitions in a single translation unit.

## Context

The global state reduction work is complete (`_currentModule` eliminated, `_gameEngine` confined to its wrapper). The project had already established the split pattern in `script_functions.c` and `game.c`, and later applied the same approach to the remaining `graphic.c` lighting slice. The three largest remaining files for this plan were all in the entity/profile layer:

| File | Lines | Status |
|------|------:|--------|
| `Object.cpp` | 3,201 | Largest file in the codebase. God class with 20 responsibility categories, 7 methods over 100 lines |
| `ObjectProfile.cpp` | 1,468 | Three massive methods (loadDataFile 494, exportCharacterToFile 351, constructor 180) |
| `Particle.cpp` | 1,447 | One monster method (initialize 404 lines), heavy module coupling |

This plan splits all three into focused translation units, following the established `{module}_internal.h` + split-files pattern. No behavior changes. Pure mechanical code redistribution.

## Established Split Pattern

All prior splits follow the same convention:

1. **Internal header** (`{module}_internal.h`): shared includes, a named `namespace` with inline accessor functions for session/engine context, `using namespace` directive.
2. **Split `.cpp` files**: include only the internal header, plus any extra includes specific to their responsibility.
3. **CMakeLists.txt**: explicit per-file listing in the appropriate source group.
4. **Verification**: build, test, and validator after each file.

Reference implementations:
- `egolib/library/src/egolib/game/script_functions_internal.h` — namespace `script_detail`
- `egolib/library/src/egolib/game/game_internal.h` — namespace `game_detail`
- `egolib/library/src/egolib/game/graphic_internal.h` — namespace `gfx_internal`

---

## Phase 1: Split Object.cpp (3,201 lines -> 6 files + internal header)

### Step 1.0: Create `Object_internal.h`

**File:** `egolib/library/src/egolib/Entities/Object_internal.h`

Extract from Object.cpp lines 1-89:
- `#define GAME_ENTITIES_PRIVATE 1`
- All current includes (Object.hpp, Profiles, game.h, context headers, etc.)
- A `namespace object_detail` with the anonymous-namespace accessor functions currently at lines 47-78:
  - `gameSession()` -> `GameSessionContext::get()`
  - `tryActiveModule()` -> `gameSession().tryActiveModule()`
  - `activeModule()` -> `gameSession().activeModule()`
  - `worldUpdateCount()` -> `gameSession().worldUpdateCount()`
  - `characterStatClock()` -> `gameSession().characterStatClock()`
  - `tryActivePlayingState()` -> `EngineContext::get().tryActivePlayingState()`
- `using namespace object_detail;`
- The `constexpr` out-of-class definitions (DROPZVEL, DISMOUNTZVEL) and INVALID_OBJECT static

Build and verify. This step changes nothing — the original Object.cpp just switches to including the new header instead of having these definitions inline.

### Step 1.1: `Object_lifecycle.cpp` (~520 lines)

**Responsibility:** Construction, destruction, lifecycle transitions, respawn, termination, trivial getters.

Methods to move (current line ranges in Object.cpp):
- `Object()` constructor: lines 96-234
- `~Object()` destructor: lines 236-265
- `requestTerminate()`: lines 1147-1151
- `removeFromGame()`: lines 1713-1752
- `respawn()`: lines 1885-1968
- `resetBoredTimer()`: lines 3042-3046
- `resetInputCommands()`: lines 2315-2319
- `toSharedPointer()`: lines 2290-2293
- `setName()`: lines 2305-2308
- `getProfile()`: lines 2310-2313
- `getInventory()`: lines 2158-2161
- `canCollide()`: lines 2989-3012
- `getAttachedPlatform()`: lines 3014-3017

### Step 1.2: `Object_combat.cpp` (~660 lines)

**Responsibility:** Damage, healing, death, resistance, experience, level-ups.

Methods to move:
- `damage()`: lines 385-653 (270 lines)
- `updateLastAttacker()`: lines 655-695
- `heal()`: lines 697-717
- `isAttacking()`: lines 719-722
- `kill()`: lines 1427-1574 (149 lines)
- `costMana()`: lines 1836-1883
- `giveExperience()`: lines 1591-1632
- `checkLevelUp()`: lines 1380-1425
- `getRawDamageResistance()`: lines 1970-2035
- `getDamageReduction()`: lines 2037-2058
- `isInvictusDirection()`: lines 2501-2553

Extra includes: `Graphics/Billboard.hpp`, `Entities/ParticleHandler.hpp`.

### Step 1.3: `Object_update.cpp` (~340 lines)

**Responsibility:** Per-frame update loop. Isolates the heaviest engine coupling (minimap, perk checks, water interaction, stealth detection scan).

Methods to move:
- `update()`: lines 756-1039 (285 lines)
- `updateResize()`: lines 1041-1090

Extra includes: `GameEngine.hpp`, `PlayingState.hpp`, `GUI/MiniMap.hpp`.

### Step 1.4: `Object_interaction.cpp` (~700 lines)

**Responsibility:** Mount/holder mechanics, item dropping, inventory operations, input processing.

Methods to move:
- `detatchFromHolder()`: lines 1161-1300 (141 lines)
- `canMount()`: lines 346-383
- `getLeftHandItem()`: lines 1302-1305
- `getRightHandItem()`: lines 1307-1310
- `getHolder()`: lines 2703-2706
- `isBeingHeld()`: lines 1681-1695
- `isInsideInventory()`: lines 1697-1711
- `dropMoney()`: lines 2839-2879
- `dropKeys()`: lines 2881-2928
- `dropAllItems()`: lines 2930-2987
- `giveMoney()`: lines 3032-3035
- `getMoney()`: lines 3037-3039
- `getPrice()`: lines 1634-1679
- `updateLatchButtons()`: lines 3058-3201 (144 lines, self-contained)
- `setLatchButton()`: lines 3053-3056
- `isWieldingItemIDSZ()`: lines 2675-2695
- `resetAlpha()`: lines 1576-1589

Extra includes: `game/Shop.hpp`.

### Step 1.5: `Object_attributes.cpp` (~540 lines)

**Responsibility:** Attributes, stats, perks, enchantments, stealth, team/skills, polymorph.

Methods to move:
- `getAttribute()`: lines 2072-2142
- `getBaseAttribute()`: lines 2060-2064
- `setBaseAttribute()`: lines 2066-2070
- `increaseBaseAttribute()`: lines 2144-2156
- `hasPerk()`: lines 2163-2170
- `getValidPerks()`: lines 2172-2198
- `addPerk()`: lines 2200-2204
- `getLife()`: lines 2206-2209
- `getMana()`: lines 2211-2214
- `setLife()`: lines 2300-2303
- `setMana()`: lines 2295-2298
- `isFlying()`: lines 2279-2282
- `addEnchant()`: lines 2216-2239
- `removeEnchantsWithIDSZ()`: lines 2241-2254
- `getActiveEnchants()`: lines 2256-2259
- `disenchant()`: lines 2261-2272
- `getTempAttributes()`: lines 2274-2277
- `getLastEnchantmentSpawned()`: lines 2284-2287
- `isStealthed()`: lines 2555-2558
- `deactivateStealth()`: lines 2560-2572
- `activateStealth()`: lines 2574-2651
- `setTeam()`: lines 2708-2771
- `hasSkillIDSZ()`: lines 2773-2837
- `getTeam()`: lines 91-94
- `polymorphObject()`: lines 2321-2499 (180 lines)

Extra includes: `Entities/Enchant.hpp`, `game/script_implementation.h`.

### Step 1.6: `Object_appearance.cpp` (~350 lines)

**Responsibility:** Skin, appearance, visibility, physics wrappers, collision sizing, identity queries.

Methods to move:
- `setSkin()`: lines 267-301
- `setAlpha()`: lines 319-328
- `setLight()`: lines 330-340
- `setSheen()`: lines 341-344
- `setFat()`: lines 1344-1348
- `setBumpHeight()`: lines 1350-1354
- `setBumpWidth()`: lines 1356-1368
- `recalculateCollisionSize()`: lines 1370-1378
- `hit_wall()` (both overloads): lines 1754-1800
- `test_wall()`: lines 1802-1834
- `isOnWaterTile()`: lines 304-307
- `isSubmerged()`: lines 309-312
- `movePosition()`: lines 314-317
- `teleport()`: lines 724-754
- `getName()`: lines 1092-1145
- `isFacingLocation()`: lines 1154-1159
- `canSeeObject()`: lines 1312-1342
- `isHidden()`: lines 2697-2701
- `isScenery()`: lines 2653-2673
- `getSkinTexture()`: lines 3048-3051
- `getIcon()`: lines 3019-3030

Extra includes: `game/Graphics/CameraSystem.hpp`, `game/Graphics/TileList.hpp`.

### Step 1.7: Update CMakeLists.txt

Replace the `Object.cpp` entry in `EGOLIB_ENTITIES_SOURCES` (line 70 of `egolib/library/CMakeLists.txt`) with:

```
src/egolib/Entities/Object_internal.h
src/egolib/Entities/Object_lifecycle.cpp
src/egolib/Entities/Object_combat.cpp
src/egolib/Entities/Object_update.cpp
src/egolib/Entities/Object_interaction.cpp
src/egolib/Entities/Object_attributes.cpp
src/egolib/Entities/Object_appearance.cpp
```

---

## Phase 2: Split ObjectProfile.cpp (1,468 lines -> 3 files + internal header)

### Step 2.0: Create `ObjectProfile_internal.h`

**File:** `egolib/library/src/egolib/Profiles/ObjectProfile_internal.h`

Extract from ObjectProfile.cpp lines 25-34:
- `#define EGOLIB_PROFILES_PRIVATE 1`
- All current includes (ObjectProfile.hpp, GameEngine.hpp, Entities/_Include.hpp, ModelDescriptor, AudioSystem, template.h, Random)
- `static const SkinInfo INVALID_SKIN` constant

No namespace'd accessors needed — ObjectProfile has minimal session coupling (only 4 ProfileSystem references).

### Step 2.1: `ObjectProfile_load.cpp` (~520 lines)

**Responsibility:** Parsing and loading object data from files.

Methods to move:
- `loadDataFile()`: lines 433-926 (494 lines)
- `loadFromFile()`: lines 974-1067 + overload at lines 1068-1072 (~97 lines)
- `loadTextures()`: lines 238-275
- `loadAllMessages()`: lines 310-327

### Step 2.2: `ObjectProfile_export.cpp` (~380 lines)

**Responsibility:** Serializing character/profile data to files.

Methods to move:
- `exportCharacterToFile()`: lines 1083-1433 (351 lines)
- `vfs_get_next_object_profile_ref()`: lines 422-431 (free function)

### Step 2.3: `ObjectProfile_core.cpp` (~570 lines)

**Responsibility:** Everything else — constructor, destructor, getters, utility methods.

Methods to move:
- Constructor: lines 36-215 (180 lines)
- Destructor: lines 217-227
- All accessor/utility methods: `getXPNeededForLevel()`, `addMessage()`, `getMessage()`, `generateRandomName()`, `getSoundID()`, `getIDSZ()`, `getSkin()`, `getIcon()`, `getParticleProfile()`, `getSkinOverride()`, `setupXPTable()`, `getSkinInfo()`, `isValidSkin()`, `getExperienceRate()`, `hasTypeIDSZ()`, `hasIDSZ()`, `isSlotValid()`, `getRandomSkinID()`, `getAttributeGain()`, `getAttributeBase()`, `canLearnPerk()`, `beginsWithPerk()`

### Step 2.4: Update CMakeLists.txt

Replace `ObjectProfile.cpp` in `EGOLIB_PROFILES_SOURCES` (line 305 of `egolib/library/CMakeLists.txt`) with:

```
src/egolib/Profiles/ObjectProfile_internal.h
src/egolib/Profiles/ObjectProfile_load.cpp
src/egolib/Profiles/ObjectProfile_export.cpp
src/egolib/Profiles/ObjectProfile_core.cpp
```

---

## Phase 3: Split Particle.cpp (1,447 lines -> 4 files + internal header)

### Step 3.0: Create `Particle_internal.h`

**File:** `egolib/library/src/egolib/Entities/Particle_internal.h`

Extract from Particle.cpp lines 24-80:
- `#define GAME_ENTITIES_PRIVATE 1`
- All current includes (Particle.hpp, GameEngine, GameSessionContext, Module, _Include, game.h, PhysicalConstants, CharacterMatrix)
- `namespace particle_detail` with accessor functions (gameSession, activeModule, worldUpdateCount)
- `using namespace particle_detail;`
- `prt_environment_t` constructor and `reset()` implementations (lines 53-80, in `namespace Ego`)

### Step 3.1: `Particle_spawn.cpp` (~500 lines)

**Responsibility:** Particle creation and attachment.

Methods to move:
- `initialize()`: lines 803-1206 (404 lines)
- `reset()`: lines 99-175
- `attach()`: lines 1208-1228
- `placeAtVertex()`: lines 1230-1291

### Step 3.2: `Particle_update.cpp` (~400 lines)

**Responsibility:** Per-frame update loop and sub-updates.

Methods to move:
- `update()`: lines 335-399
- `updateWater()`: lines 401-475
- `updateAnimation()`: lines 477-549
- `updateDynamicLighting()`: lines 551-571
- `updateContinuousSpawning()`: lines 573-627

### Step 3.3: `Particle_combat.cpp` (~200 lines)

**Responsibility:** Particle damage application and destruction.

Methods to move:
- `updateAttachedDamage()`: lines 629-730 (102 lines)
- `destroy()`: lines 732-780

### Step 3.4: `Particle_core.cpp` (~350 lines)

**Responsibility:** Everything else — physics wrappers, queries, getters, targeting, sound.

Methods to move:
- `hit_wall()` (both overloads), `test_wall()`
- `getProfile()`, `getScale()`, `setSize()`
- `requestTerminate()`, `setElevation()`
- `isHidden()`, `hasValidTarget()`, `isTerminated()`, `getProfileID()`
- `getTarget()`, `setTarget()`, `getOwner()`, `isOverWater()`
- `playSound()`, `isAttached()`, `getAttachedObject()`
- `hasCollided()`, `addCollision()`, `setHoming()`, `isEternal()`, `canCollide()`

### Step 3.5: Update CMakeLists.txt

Replace `Particle.cpp` in `EGOLIB_ENTITIES_SOURCES` (line 74 of `egolib/library/CMakeLists.txt`) with:

```
src/egolib/Entities/Particle_internal.h
src/egolib/Entities/Particle_spawn.cpp
src/egolib/Entities/Particle_update.cpp
src/egolib/Entities/Particle_combat.cpp
src/egolib/Entities/Particle_core.cpp
```

---

## Design Decisions

1. **Methods stay intact.** No decomposition of large methods (update 285 lines, damage 270 lines, kill 149 lines, initialize 404 lines, polymorphObject 180 lines) during this pass. Method decomposition is a separate behavior-preserving refactoring step.

2. **One file at a time.** Create the internal header first, then move one responsibility group per step, building and testing after each.

3. **No header changes.** Object.hpp (with its 39 public member variables) stays untouched. Encapsulation is a future pass.

4. **No `_Include.hpp` changes.** The umbrella header is deferred. Split files use `#define GAME_ENTITIES_PRIVATE 1` and include the class header directly (the same pattern the original `.cpp` files already use).

5. **C++ split files, not C.** Unlike the prior game.c and script_functions.c splits (which were `.c` files compiled as C++), Object.cpp, ObjectProfile.cpp, and Particle.cpp are genuine `.cpp` files. The split files will also be `.cpp`.

## Key Files

| File | Role |
|------|------|
| `egolib/library/src/egolib/Entities/Object.cpp` | Phase 1 target (3,201 lines) |
| `egolib/library/src/egolib/Entities/Object.hpp` | Class declaration (995 lines, read-only for this pass) |
| `egolib/library/src/egolib/Profiles/ObjectProfile.cpp` | Phase 2 target (1,468 lines) |
| `egolib/library/src/egolib/Profiles/ObjectProfile.hpp` | Class declaration (read-only for this pass) |
| `egolib/library/src/egolib/Entities/Particle.cpp` | Phase 3 target (1,447 lines) |
| `egolib/library/src/egolib/Entities/Particle.hpp` | Class declaration (read-only for this pass) |
| `egolib/library/CMakeLists.txt` | Source listings (EGOLIB_ENTITIES_SOURCES lines 64-78, EGOLIB_PROFILES_SOURCES lines 292-312) |
| `egolib/library/src/egolib/game/game_internal.h` | Reference pattern for internal headers |
| `egolib/library/src/egolib/game/script_functions_internal.h` | Reference pattern for namespace'd accessors |

## Verification (after each split file)

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

All 253+ tests must pass. Validator baseline must not regress.

## After Completion

Update `refactoring-documents/19-new-refactoring-plan.md` progress log:
- D3 (Split graphic.c) -> mark complete (partially done in prior pass, HUD and scene split out)
- D4 (Split Object.cpp) -> mark complete
- D5 (Split ObjectProfile.cpp) -> mark complete
- Add new entry for Particle.cpp split

Update this document with actual line counts and any deviations from the plan.

## Future Follow-Up (Out of Scope)

These items are natural follow-ups but are **not** part of this plan:

1. **Method decomposition:** Break down the 7 methods over 100 lines into smaller sub-methods
2. **Object.hpp encapsulation:** Convert the 39 public member variables to private with accessors
3. **`_Include.hpp` thinning:** Replace umbrella includes with targeted includes in the 53 consumer files
4. **Entity-Module circular dependency:** Break Object.hpp -> Module/Module.hpp include chain
