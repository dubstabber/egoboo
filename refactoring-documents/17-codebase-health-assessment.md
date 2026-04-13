# Codebase Health Assessment

This document captures a quantitative and qualitative health snapshot of the Egoboo codebase as inspected on 2026-04-13. It complements the earlier architecture audit (documents 01–16) with fresh metrics, pattern analysis, and a consolidated quality scorecard.

## 1. Size and Composition

### Source file counts (egoboo + egolib + cartman)

| Category | Count |
| --- | ---: |
| C implementation files (`.c`) | 56 |
| C++ implementation files (`.cpp`) | 220 |
| C headers (`.h`) | 64 |
| C++ headers (`.hpp`) | 266 |
| **Total source files** | **606** |

### Lines of code

| Area | Approx. Lines |
| --- | ---: |
| `egolib/library/src/egolib/game/` (top-level C files) | 25,259 |
| `egolib/library/src/egolib/game/` (all subdirs) | 52,864 |
| `egolib/library/src/egolib/` (non-game subsystems) | ~28,000 |
| `egoboo/src/` (thin executable) | 90 |
| `cartman/src/` (editor, not in main build) | ~6,000 |
| C code total (egolib + egoboo + cartman) | 41,312 |
| C++ code total (egolib + egoboo + cartman) | 42,385 |
| **Grand total (active runtime source)** | **~83,700** |

### C vs C++ split

The codebase is almost exactly 50/50 C and C++ by line count. This is a significant maintenance concern: two idioms, two resource management models, and two error handling strategies coexist in the same library.

## 2. Hotspot Files

Files over 1,000 lines are disproportionate sources of complexity and bugs.

| File | Lines | Role |
| --- | ---: | --- |
| `game/script_functions.c` | 8,183 | Script function dispatch (largest in project) |
| `Entities/Object.cpp` | 3,201 | Core entity runtime |
| `game/game.c` | 2,456 | Game loop and session lifecycle |
| `vfs.c` | 2,445 | Virtual file system |
| `game/graphic.c` | 2,257 | Rendering pipeline |
| `game/Physics/particle_collision.c` | 1,479 | Particle collision |
| `Profiles/ObjectProfile.cpp` | 1,468 | Object profile loading |
| `Entities/Particle.cpp` | 1,447 | Particle entity |
| `game/mesh.c` | 1,369 | Mesh management |
| `game/Graphics/ObjectGraphics.cpp` | 1,343 | Object rendering |
| `fileutil.c` | 1,339 | File utilities |
| `game/Module/Module.cpp` | 1,225 | Module loading and lifecycle |
| `game/script_compile.c` | 1,147 | Script compiler |
| `game/Physics/ObjectPhysics.cpp` | 1,085 | Object physics |
| `Script/script.c` | 1,064 | Script runtime |

These 15 files alone account for roughly 30,000 lines — about 36% of all active code.

## 3. Function Complexity

### Oversized functions (>100 lines)

At least 25 functions exceed 100 lines. The worst offenders:

| Function | File | Lines |
| --- | --- | ---: |
| `map_generate_fan_type_data` | `map_functions.c` | 506 |
| `ObjectProfile::loadDataFile` | `ObjectProfile.cpp` | 493 |
| `Particle::initialize` | `Particle.cpp` | 403 |
| `ObjectProfile::exportCharacterToFile` | `ObjectProfile.cpp` | 350 |
| `do_chr_chr_collision` | `CollisionSystem.cpp` | 300 |
| `character_swipe` | `game.c` | 300 |
| `Object::update` | `Object.cpp` | 283 |
| `Object::damage` | `Object.cpp` | 268 |

Functions of this size are essentially untestable, unreadable, and high-risk for regression.

### Deeply nested code

`script_functions.c` alone has **762 lines** at 4+ indentation levels, indicating extremely high cyclomatic complexity.

### Switch statement density

**123 switch statements** in `egolib` implementation files, concentrated in script dispatch and game logic.

## 4. Global State and Coupling

### Global mutable state

| Metric | Count |
| --- | ---: |
| `_currentModule` references | 126 |
| `_gameEngine` references | 176 |
| `update_wld` references | 65 |
| Singleton/`::get()`/`::instance()` uses | 1,239 |
| `extern` declarations in headers | 79 |
| Free functions in core game headers | 60 |

The pervasive use of `_currentModule` and `_gameEngine` as global singletons means virtually every subsystem has an implicit dependency on the full game runtime. This makes isolated testing, headless validation, and modular replacement extremely difficult.

### Coupling through includes

| File | Include count | Role |
| --- | ---: | --- |
| `egolib.h` (uber-header) | 57 | Umbrella include |
| `game/graphic.c` | 33 | Rendering |
| `game/game.c` | 25 | Game loop |
| `game/script_functions.c` | 23 | Script dispatch |

`egolib.h` acts as an uber-header that pulls in nearly everything, defeating modular compilation and increasing build times.

## 5. Code Quality Patterns

### Error handling

| Pattern | Count |
| --- | ---: |
| `try/catch` blocks | 60 |
| `throw` statements | 202 |
| `egolib_rv` return codes | 39 |

The codebase uses **three competing error-handling strategies**: C++ exceptions, C-style return codes (`egolib_rv`), and silent failure. There is no consistent error propagation model.

### Memory management

| Pattern | Count |
| --- | ---: |
| `shared_ptr` usage | 910 |
| `unique_ptr` usage | 28 |
| `weak_ptr` usage | 26 |
| Raw `new`/`delete` | 232 |
| Explicit destructors | 38 |

The extremely high `shared_ptr` count (910) compared to `unique_ptr` (28) suggests **over-sharing of ownership**. Many of these are likely used where unique ownership would be clearer and safer. The 232 raw `new`/`delete` calls are potential leak sites.

### Deprecated patterns

| Pattern | Count | Concern |
| --- | ---: | --- |
| `goto` statements | 25 | Error-handling in C code |
| C-style casts | 211 | Type safety |
| Magic numbers in `script_functions.c` | 34+ | Readability |
| `TODO`/`FIXME`/`HACK` markers | 68 | Acknowledged technical debt |

### Header guards

All 308 headers use `#pragma once` consistently. This is a minor positive — no header guard naming conflicts.

## 6. Test Coverage

### Current test inventory

| Test area | Files | Lines |
| --- | --- | ---: |
| `egolib/tests/` | 5 files | ~400 |
| `idlib/tests/` | 11 files | ~400 |
| `idlib-game-engine/tests/` | 1 file | ~200 |
| **Total** | **17 files** | **~1,000** |

### Test-to-code ratio

~1,000 test lines for ~83,700 source lines = **1.2% test coverage by line count**.

### What is NOT tested

- Module loading and unloading
- Object profile parsing and validation
- Script compilation and execution
- Save/import/export round-trips
- Physics and collision behavior
- Rendering correctness
- GUI state transitions
- VFS mount semantics
- Content format edge cases

The test suite provides almost no behavioral protection. It is effectively a compilation check for utility code only.

## 7. Documentation Health

### Inline documentation

- License headers are present in most files (good).
- Doxygen-style comments exist but are inconsistent across old C and new C++ code.
- Many critical behaviors in `script_functions.c`, `game.c`, and `Module.cpp` have minimal or no inline documentation.

### External documentation

- Build docs are contradictory (see document 01).
- Content format docs are fragmented across eras (see document 07).
- The `refactoring-documents/` folder (16 documents so far) is the most current and accurate documentation.

## 8. Build System Health

### Strengths

- CMake-based build works on Linux and Windows
- Clean separation of `idlib`, `idlib-game-engine`, `egolib`, and `egoboo` targets
- Content validator tool integrated into the build

### Weaknesses

- **Recursive file globbing**: `egolib` uses `GLOB_RECURSE` to collect all sources. This hides subsystem boundaries and can miss new files or include stale ones.
- **No internal module targets**: `egolib` is one monolithic static library with no internal modularity expressed in the build system.
- **Cartman not in build**: The editor code exists but is disconnected from the main build graph.
- **Stale CI**: `.travis.yml` and `appveyor-*.yml` configs exist but are likely outdated.

## 9. Abandoned/Dead Code

| Area | State |
| --- | --- |
| `game/Lua/` | Empty directory (0 lines), abandoned Lua integration attempt |
| `utilities/migrator/` | Several empty `run()` methods, stale `README.md` |
| `doc/ego2xml/` | 2015-era XML migration proposals, not integrated |
| `Network/` | 0 lines — placeholder only |
| Legacy READMEs (`README.Linux`, `README.MinGW`, etc.) | Stale, contradict current build |

## 10. Quality Scorecard

| Dimension | Score (1–5) | Notes |
| --- | :---: | --- |
| **Modularity** | 2/5 | Single monolithic library, no internal boundaries |
| **Test coverage** | 1/5 | Near-zero behavioral test protection |
| **Global state discipline** | 1/5 | Pervasive globals, 1,239 singleton accesses |
| **Error handling consistency** | 2/5 | Three competing strategies |
| **Function size discipline** | 2/5 | 25+ functions over 100 lines, one at 506 |
| **File size discipline** | 2/5 | 15 files over 1,000 lines |
| **Memory management** | 3/5 | Heavy shared_ptr use, but 232 raw new/delete |
| **Build system** | 3/5 | Works, but hides structure |
| **Documentation** | 2/5 | Fragmented, but refactoring docs improving |
| **Dead code hygiene** | 2/5 | Multiple abandoned experiments in tree |
| **Language consistency** | 2/5 | 50/50 C/C++ split with mixed idioms |
| **Namespace discipline** | 3/5 | Ego:: namespace used, but not enforced everywhere |
| **Overall maintainability** | **2/5** | Functional but high-friction for changes |
