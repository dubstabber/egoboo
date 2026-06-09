---
name: cmake-sublibrary-pattern
description: idlib reference pattern for sub-library splitting and what a first egolib sub-library carve (egolib-physics) would require
metadata:
  type: project
---

## idlib Sub-Library Pattern (the reference to mirror)

Each idlib sub-library lives in `idlib/<name>/library/CMakeLists.txt` and follows this minimal template:

```cmake
add_library(idlib-<name>-library STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(idlib-<name>-library PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(idlib-<name>-library INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(idlib-<name>-library <upstream-deps>)
```

Key properties:
- Both PRIVATE and INTERFACE `target_include_directories` — upstream headers are available to consumers.
- `file(GLOB_RECURSE ...)` for source discovery (idlib uses globs; egolib uses explicit lists — egolib should stay explicit).
- Each sub-library names only its direct upstreams in `target_link_libraries`; transitive deps flow through INTERFACE.
- No install rules in sub-library CMakeLists in idlib; install rules only on final products (executables).

## egolib Current Monolith

`egolib/library/CMakeLists.txt` builds ONE static library `egolib-library` from ~32 per-subsystem `set(EGOLIB_*_SOURCES)` blocks accumulated into `SOURCE_FILES`. Consumers (egoboo, egolib-tests, validator) all link only `egolib-library`.

- Line 855: `add_library(egolib-library STATIC ${SOURCE_FILES})`
- Line 856: `target_include_directories(egolib-library INTERFACE "${PROJECT_SOURCE_DIR}/src/")`
- Line 857: `target_link_libraries(egolib-library idlib-game-engine-library idlib-document-library ...)`

## What a First Sub-Library Carve Requires

Candidate: `egolib-physics` from `EGOLIB_TOPLEVEL_SOURCES` (Physics/ nucleus: 4 files) and `EGOLIB_GAME_PHYSICS_SOURCES` (4 game TUs).

**Why it is NOT a clean carve today:**
- `EGOLIB_TOPLEVEL_SOURCES` is a mixed bag — it also contains `App.cpp`, `Clock.cpp`, `Debug.cpp`, `egoboo_setup.c`, `fileutil.c`, `font_bmp.c`, etc., which are not physics. Only the 4 `Physics/` files belong in a physics sub-library.
- The game physics TUs (`CollisionSystem.cpp`, `ObjectPhysics.cpp`, etc.) include `egolib/Entities/_Include.hpp`, `GameSessionContext.hpp`, `Shop.hpp`, `CharacterMatrix.h` — heavy game-layer dependencies that would make `egolib-physics` depend on nearly the whole library.
- `MeshLookupTables.cpp` includes `map_functions.h`, tying it to the file-format layer.

**Minimal viable split: `egolib-physics-nucleus` (4 TUs only)**

Files that could move: `Collidable.cpp/.hpp`, `ICollisionWorld.cpp/.hpp`, `MeshLookupTables.cpp/.hpp`, `PhysicalConstants.cpp/.hpp`

These pull in: `egolib/Mesh/Info.hpp`, `egolib/integrations/math.hpp`, `egolib/typedef.h`, `egolib/_math.h`, `egolib/map_functions.h`, `egolib/Debug.hpp`. All are in the current monolith — they'd need to remain in `egolib-library` (the residual), making `egolib-physics-nucleus` a dependency OF `egolib-library`, not a peer.

## Gotchas

1. **C/HEADER_FILES loop**: Lines 819-835 iterate `SOURCE_FILES` to set `LANGUAGE CXX` and `HEADER_FILE_ONLY`. If you extract a sub-library into a separate `CMakeLists.txt`, you must replicate this loop there — or use `set_source_files_properties()` for any `.c/.h` files in the sub-library.

2. **Platform-specific appending** (lines 838-852): `file_linux.c`, `file_win.c`, and Apple `.mm` glob are appended to `SOURCE_FILES` after the subsystem blocks. If file_linux.c or file_win.c need to go into a foundation sub-library, the `if(UNIX)/if(APPLE)/if(WIN32)` blocks must be replicated.

3. **Apple `.mm` glob**: `file(GLOB_RECURSE MM_FILES ${PROJECT_SOURCE_DIR}/src/*.mm)` scans the entire src tree. If a sub-library gets its own `CMakeLists.txt` in a subdirectory, this glob would no longer catch those `.mm` files — needs explicit enumeration or a scoped glob.

4. **No install rules** on `egolib-library` currently. A sub-library would similarly need no install rule — only the final products (egoboo, validator) install.

5. **INTERFACE include path**: `egolib-library` uses `INTERFACE "${PROJECT_SOURCE_DIR}/src/"`. A sub-library sharing the same `src/` root should use the same path. Since all headers live under the single `egolib/library/src/` tree, this is straightforward.

6. **Circular deps**: `egolib-physics-nucleus` would depend on other egolib headers (Mesh, Math, typedef) that remain in the monolith. Those headers cannot themselves depend on physics headers without creating a cycle. Today they do not — the cycle risk is one-directional and safe.

7. **egolib-library must list the sub-library in target_link_libraries**: Replace the raw source file inclusion with `target_link_libraries(egolib-library egolib-physics-nucleus ...)`. INTERFACE include dirs on the sub-library will propagate the headers to all consumers of `egolib-library` automatically.

8. **Tests and consumers**: `egolib-tests-executable` links only `egolib-library`. Because `egolib-library` will re-export the sub-library via INTERFACE/PUBLIC linkage, no change to `egolib/tests/CMakeLists.txt`, `egoboo/CMakeLists.txt`, or `tools/CMakeLists.txt` is needed — they keep linking `egolib-library` and get the sub-library transitively.

**Why:** Recorded after full CMake analysis session to avoid re-reading all CMakeLists for future sub-library splitting work.
**How to apply:** When scoping any future egolib sub-library split, start here to understand the pattern, constraints, and gotchas before reading CMake files again.
