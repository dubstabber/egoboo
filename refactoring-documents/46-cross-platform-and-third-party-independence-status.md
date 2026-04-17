# Cross-Platform And Third-Party Independence Status

Snapshot date: 2026-04-17.

## 1. Purpose

This document captures a point-in-time status of two related long-term goals for the Egoboo workspace:

1. **Open-source-only build path on every supported platform.** The maintained project direction is to treat Visual Studio-specific workflows as legacy and eventually remove them, keeping Windows tooling fully open source (mingw-w64, MSYS2, CMake, Ninja/Make).
2. **Third-party independence and self-containment.** Reproducible, offline-capable builds on Linux, Linux-hosted Windows cross, and native Windows-open-source, with consistent dependency resolution rather than a per-platform divergent story.

Scope deliberately excluded:

- Runtime gameplay bugs, content-validation regressions, and subsystem coupling (already covered by prior refactoring-documents passes).
- Prescriptive implementation of fixes. This is an inventory and gap assessment; each gap in §8 is intentionally small enough to become its own numbered pass.

Evidence sources and related prior audits: `README.md`, `refactoring-documents/README.md`, `refactoring-documents/01-repository-and-build-audit.md`, `refactoring-documents/07-historical-docs-audit.md`, `doc/build-linux.md`, `doc/build-windows.md`.

## 2. Project direction recap

From `README.md:7-15`:

- Move the remaining mixed C/C++ runtime toward C++.
- Make native Windows compilation a first-class target rather than treating Wine as the practical fallback.
- Make Linux-hosted Windows cross-compilation a first-class target too.
- Keep Linux, native Windows, and Linux-hosted Windows builds structurally similar.
- Keep the Windows build and tooling path fully open source — Visual Studio-specific workflows are not part of the maintained direction.
- Reduce portability debt, old dependency friction, and routine C++ warning noise.
- Improve the current buggy and incomplete runtime toward a stable, modular, maintainable baseline.

From `refactoring-documents/README.md:149-150`:

- "Make the Windows target explicit: native compilation should be supported in the future, the toolchain should remain fully open source, and Visual Studio-specific build guidance should be retired from the maintained path."
- "Make Linux-native, native-Windows, and Linux-hosted Windows builds converge toward one coherent cross-platform workflow."

## 3. Build-system status per platform

| Target                           | Configure                                                | Build              | Runtime       | Fully open-source toolchain?                      | Documented           |
| -------------------------------- | -------------------------------------------------------- | ------------------ | ------------- | ------------------------------------------------- | -------------------- |
| Linux native (x86_64)            | CMake + pkg-config (Fedora `SDL2-devel` family)          | gcc/g++            | Stable        | Yes                                               | `doc/build-linux.md` |
| Linux-hosted Windows cross (x64) | CMake + `cmake/toolchains/mingw-w64-x86_64.cmake`        | `x86_64-w64-mingw32-gcc/g++` | Unstable under Wine (font atlas, audio) | Yes                                               | `doc/build-windows.md` |
| Native Windows (open-source)     | *(no toolchain file, no doc)*                            | *(undocumented)*   | *(untested)*  | Would be yes (mingw-w64 / MSYS2)                  | **Missing**          |
| Native Windows (MSVC)            | Legacy `.sln` / Visual Studio 2017 via `appveyor-windows.yml` | MSVC           | *(untested in this workspace)* | **No — proprietary**                              | `README.VisualStudio` (marked deprecated) |
| macOS                            | Unwired Xcode project at `osx/Egoboo.xcodeproj`          | *(unwired)*        | *(unknown)*   | No (Xcode is proprietary)                         | `README.OSX` (marked deprecated) |

Key observations:

- Only one toolchain file exists: `cmake/toolchains/mingw-w64-x86_64.cmake`. No `i686-w64-mingw32`, no macOS, no MSYS2-native variant.
- Runtime instability on the cross-built Windows binary is tracked by `debug-output.txt` and documented in `doc/build-windows.md`. `run-egoboo-windows.sh` sets `EGOBOO_DISABLE_MIPMAPS=1` and `EGOBOO_DISABLE_AUDIO=1` as the workaround.
- The MSVC path is recognized by CMake (`IDLIB_CXX_COMPILER_ID_MSVC` checks), but the maintained `README.md` explicitly states Visual Studio-specific workflows are not part of the direction.

## 4. Proprietary-toolchain artifact inventory

Files still present in the tree that depend on or describe Visual Studio / Xcode / proprietary tooling. These are retirement candidates.

| Path                                               | Category                   | Notes                                                                                                |
| -------------------------------------------------- | -------------------------- | ---------------------------------------------------------------------------------------------------- |
| `appveyor-windows.yml`                             | CI (Windows)               | Generates `Visual Studio 15 2017` solution; downloads CMake 3.30 zip; runs `ctest`                   |
| `egoboo.gta.runsettings`                           | VS test runner             | Google Test Adapter configuration for Visual Studio test runner                                      |
| `distribute.ps1`                                   | Windows packaging          | PowerShell; portable to shell / CMake `install` / CPack generators                                    |
| `external/distribute.ps1`                          | Windows packaging          | PowerShell DLL staging for Windows redistributables                                                  |
| `external/install-vsix-appveyor.ps1`               | VS extension installer     | Installs Google Test Adapter VSIX into VS 2017 Community on AppVeyor                                 |
| `external/external.sln`                            | VS solution                | Legacy VS solution under `external/`                                                                 |
| `external/SDL2-2.0.3/VisualC/` (+ nested SDL libs) | Vendored VS projects       | `.vcxproj` trees shipped with vendored SDL2 family                                                   |
| `external/appveyor.yml`                            | CI (external)              | Secondary AppVeyor config inside `external/`                                                         |
| `README.VisualStudio`                              | Legacy doc                 | Marked deprecated; redirects to `README.md`                                                          |
| `README.Windows`                                   | Legacy doc                 | Marked deprecated; references VS 2013 / Code::Blocks                                                 |
| `README.MinGW`                                     | Legacy doc                 | Describes Makefile-based MinGW flow (no longer used)                                                 |
| `README.OSX`                                       | Legacy doc                 | Describes `osx/Egoboo.xcodeproj` (not integrated into CMake)                                         |
| `osx/Egoboo.xcodeproj`                             | Legacy IDE project         | Apple Xcode project; unwired from CMake build graph                                                  |
| `CMakeLists.txt:51-69`                             | MSVC-only CMake branch     | CPack installer settings gated on `IDLIB_CXX_COMPILER_ID_MSVC`                                       |
| `egoboo/CMakeLists.txt:41-46`                      | MSVC-only CMake branch     | `VS_DEBUGGER_WORKING_DIRECTORY` property set only for MSVC                                           |
| `egolib/library/src/egolib/platform.h:125-136`     | MSVC-only pragma island    | Seven `#pragma warning(disable: …)` entries guarded by `#if defined(_MSC_VER)`                      |

Non-goals here: macOS support is out of the immediate direction; the `osx/` artifacts are flagged only because they still exist and depend on Xcode.

## 5. Third-party dependency inventory

### 5a. Vendored trees under `external/`

These source trees are physically checked out via the `external` submodule but **are not all consumed by the current build**.

| Tree                          | Version    | Consumed by build?                                | Notes                                              |
| ----------------------------- | ---------- | ------------------------------------------------- | -------------------------------------------------- |
| `external/SDL2-2.0.3`          | 2.0.3 (2014) | Not on Linux (system); MinGW uses `external/mingw/` bundle | Stale vs. upstream SDL2 releases                   |
| `external/SDL2_image-2.0.0`    | 2.0.0      | Not on Linux; via `external/mingw/` on Windows    | Bundles jpeg-9, libpng-1.6.2, libwebp-0.3.0, tiff-4.0.3, zlib-1.2.8 |
| `external/SDL2_mixer-2.0.0`    | 2.0.0      | Not on Linux; via `external/mingw/` on Windows    | Bundles libogg-1.3.1, libvorbis-1.3.3, flac-1.2.1, libmikmod-3.1.12, libmodplug-0.8.8.4, smpeg2-2.0.0 |
| `external/SDL2_net-2.0.0`      | 2.0.0      | Not linked                                        | Networking unused in current runtime               |
| `external/SDL2_ttf-2.0.12`     | 2.0.12     | Not on Linux; via `external/mingw/` on Windows    |                                                    |
| `external/physfs-2.1.1`        | 2.1.1      | **Not consumed** — superseded by `idlib-game-engine/library/physfs-3.0.0` | Dead weight                                        |
| `external/lua-5.2.3`           | 5.2.3      | Partial — some Lua integration remains; see `egolib/library/src/egolib/game/Lua/` (flagged abandoned in `01-repository-and-build-audit.md`) |                                                    |
| `external/googletest`          | *(unspecified)* | **Not consumed by default** — `idlib` fetches a different gtest at configure time | See §5b                                            |
| `external/mingw/`              | Prebuilt   | Yes (Windows cross)                               | Pre-built headers + import libraries for mingw-w64 cross-build |

### 5b. Actually-consumed dependencies at build time

| Library                        | Linux native                          | Windows cross (MinGW)                              | Native Windows (MSVC) |
| ------------------------------ | ------------------------------------- | -------------------------------------------------- | --------------------- |
| SDL2, SDL2_image, SDL2_mixer, SDL2_ttf | System, via `pkg-config`          | `external/mingw/` bundle via `EGOBOO_MINGW_DEPENDENCY_ROOT` in `idlib-game-engine/library/CMakeLists.txt` | Legacy `.sln` path; not maintained |
| OpenGL                         | `find_package(OpenGL)` (system)       | `find_package(OpenGL)` (via mingw-w64 headers)     | —                     |
| GLEW (headers only)            | Vendored at `idlib-game-engine/library/glew/src/GL/` | Same                                               | —                     |
| PhysFS                         | `idlib-game-engine/library/physfs-3.0.0/` (vendored) | Same                                               | —                     |
| googletest                     | `idlib/CMakeLists.txt:43-53` fetches `https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip` via `FetchContent` when `idlib-with-fetch-googletest` is `ON` (default) | Same                                               | —                     |
| Threads                        | `find_package(Threads)`               | `find_package(Threads)`                            | —                     |

Key divergences:

- **Vendored SDL2 tree is orphaned on Linux.** Linux consumes system SDL2 via `pkg-config`; the in-tree `external/SDL2-*` trees never participate.
- **Dual-track dependency resolution.** Linux uses `pkg-config` / `find_package`, Windows cross uses a prebuilt `external/mingw/` bundle. These paths diverge on upgrade and maintenance.
- **Network access required at configure time** for default `idlib` builds (googletest fetch). This breaks offline builds and is fragile for reproducible cross-platform packaging.
- **PhysFS version split.** Two versions physically present (2.1.1 in `external/`, 3.0.0 under `idlib-game-engine/library/`); only 3.0.0 is used.

## 6. Platform-dependent code hotspots

Platform divergence in code is **small and well-isolated** today — platform debt is concentrated in build-system and artifact state rather than source code.

| Concern                        | Location                                                                                                    | State                                           |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------- | ----------------------------------------------- |
| Platform detection             | `idlib/library/src/idlib/platform/platform.hpp`                                                             | Rigorous compile-time detection, single target enforced |
| File/path abstraction          | `egolib/library/src/egolib/Platform/file_linux.c`, `file_win.c`, `file_mac.mm`, `NSFileManager+DirectoryLocations.{h,m}` | Cleanly split by platform                       |
| Direct Windows API usage       | `egolib/library/src/egolib/Platform/file_win.c`, `egolib/library/src/egolib/Log/ConsoleColor.cpp`           | Only two TUs; `ConsoleColor.cpp` is `#ifdef _WIN32`-guarded |
| MSVC-only pragmas              | `egolib/library/src/egolib/platform.h:125-136`                                                              | Seven `#pragma warning(disable: …)` entries; removable once MSVC path is retired |
| Wine compatibility gates       | `run-egoboo-windows.sh`, `egolib/library/src/egolib/Audio/AudioSystem.cpp`, `egolib/library/src/egolib/Renderer/OpenGL/Texture.cpp` | Env-var driven (`EGOBOO_DISABLE_AUDIO`, `EGOBOO_DISABLE_MIPMAPS`) |
| Path length constants          | `PATH_MAX` in `file_linux.c`, `MAX_PATH` in `file_win.c`                                                    | Legacy fixed buffers; no `std::filesystem` adoption |
| Inline assembly / endianness   | —                                                                                                           | None found                                      |
| `#pragma comment(lib, …)`      | Only inside vendored SDL2_mixer / libmikmod                                                                 | Not in own source                               |
| Legacy C file footprint        | 89 `.c` files remain across active trees (vs. 470 `.cpp`). Largest TU: `egolib/library/src/egolib/game/script_functions.c` (8153 lines) | C→C++ migration in progress (CLAUDE.md)         |

Removing Visual Studio as a supported target is **low-risk from a source-code standpoint**. The remaining MSVC-specific code is one pragma island plus two small CMake branches.

## 7. Cross-platform readiness matrix

| Target                          | Builds today? | Runs correctly? | Fully open-source toolchain? | Documented? |
| ------------------------------- | ------------- | --------------- | ---------------------------- | ----------- |
| Linux native x86_64             | Yes           | Yes             | Yes                          | Yes         |
| Linux→Windows cross (mingw-w64, x64) | Yes      | Unstable        | Yes                          | Yes         |
| Native Windows (MSYS2/mingw-w64) | Unknown      | Unknown         | Would be yes                 | **No**      |
| Native Windows (MSVC)           | Legacy only   | Unknown         | **No**                       | Deprecated  |
| macOS                           | No (Xcode proj unwired) | —    | No (Xcode)                   | Deprecated  |

## 8. Gaps toward the stated goals

Ordered roughly by value-per-risk. Each item is scoped small enough to become its own numbered pass.

1. **Windows CI still uses MSVC.** `appveyor-windows.yml` generates a Visual Studio 2017 solution. The documented, maintained Windows build path (`doc/build-windows.md`) is mingw-w64 cross from Linux. CI should match documentation.
2. **No native-Windows open-source build path.** `doc/build-windows.md` covers Linux-hosted cross only. A companion `doc/build-windows-native.md` describing MSYS2 / UCRT64 with a matching `cmake/toolchains/` file would make "native Windows compilation as a first-class target" (README goal) concretely reachable.
3. **Proprietary-toolchain artifacts still checked in.** `external/install-vsix-appveyor.ps1`, `external/external.sln`, SDL2 `VisualC/` `.vcxproj` trees, `egoboo.gta.runsettings` — each implies a Visual Studio workflow that is no longer the maintained direction.
4. **Vendored SDL2 is stale and orphaned on Linux.** `external/SDL2-2.0.3` (2014) is never consumed on Linux; Linux builds use system `SDL2-devel`. Decide the SDL2 vendoring story: either upgrade the vendored tree and actually use it on every platform, or delete the vendored sources and make system packages (Linux) + MinGW bundle (Windows) the declared contract.
5. **`FetchContent` requires network at configure.** `idlib/CMakeLists.txt:43-53` fetches googletest 1.16.0 from `github.com` at configure time when `idlib-with-fetch-googletest=ON` (default). Offline/air-gapped builds fail. Either default to `OFF` and consume the already-vendored `external/googletest`, or convert to a git submodule.
6. **PhysFS version duplication.** `external/physfs-2.1.1` is unused; `idlib-game-engine/library/physfs-3.0.0` is the real one. The 2.1.1 tree is dead weight and confuses the dependency picture.
7. **Wine runtime instability blocks cross-build validation.** Font-atlas init failure and audio loading crash (see `debug-output.txt`). Until this is resolved, the mingw-w64 cross path cannot serve as a verification substitute for native Windows.
8. **Only one toolchain file.** `cmake/toolchains/mingw-w64-x86_64.cmake` is the sole entry. An `i686-w64-mingw32.cmake` (32-bit cross) and a native MSYS2/UCRT64 toolchain file would fill the remaining open-source Windows targets.
9. **MSVC-only CMake branches remain in the root build.** `CMakeLists.txt:51-69` (CPack installer gated on MSVC) and `egoboo/CMakeLists.txt:41-46` (VS debugger working directory). Removable once §1–§3 land.
10. **Legacy READMEs still ship.** `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` continue to describe workflows that `README.md` explicitly disowns. Candidate for relocation into an `archive/` subdirectory or deletion.
11. **macOS is formally out-of-direction but physically present.** If macOS is not a near-term target, the `osx/` Xcode project and `NSFileManager+DirectoryLocations` files could be quarantined alongside the legacy READMEs; if it *is* a target, it needs its own open-source path (CMake + Clang/`homebrew` documentation).

## 9. Recommended next passes

Each of these can stand alone as a numbered pass; no specific assignment is implied here.

1. **Retire Windows CI's MSVC path.** Replace `appveyor-windows.yml` generator with mingw-w64 cross build (matches `doc/build-windows.md`); drop `external/install-vsix-appveyor.ps1` and `egoboo.gta.runsettings`.
2. **Add native-Windows open-source build doc and toolchain.** `doc/build-windows-native.md` + `cmake/toolchains/mingw-w64-msys2.cmake` (or UCRT64 equivalent).
3. **Collapse dependency-resolution divergence.** Pick one SDL2 story across platforms; either upgrade vendored source and use everywhere, or delete vendored source and rely on system packages + MinGW bundle only.
4. **Eliminate configure-time network fetch.** Default `idlib-with-fetch-googletest=OFF`; use the already-present `external/googletest`.
5. **Remove orphaned vendored deps.** Delete `external/physfs-2.1.1`; keep `idlib-game-engine/library/physfs-3.0.0` as the sole source.
6. **Diagnose Wine blockers** (font atlas init, audio crash in `debug-output.txt`) so the cross-built Windows binary is actually runnable.
7. **Drop MSVC-only CMake branches** in `CMakeLists.txt:51-69` and `egoboo/CMakeLists.txt:41-46` once §1/§2 land. `platform.h`'s MSVC pragma island can remain until a compiler-agnostic warning policy replaces it.
8. **Quarantine legacy docs.** Move `README.VisualStudio`, `README.Windows`, `README.MinGW`, `README.OSX` into `doc/legacy/` (or delete) so `README.md` remains the single source of truth.
9. **Decide macOS policy.** Either quarantine `osx/` and `NSFileManager+DirectoryLocations.*` alongside the legacy docs, or commit to a CMake + Clang macOS build path.

## 10. References

- `README.md` — project direction (2026-04-17 wording).
- `refactoring-documents/01-repository-and-build-audit.md` — original repository/build baseline.
- `refactoring-documents/07-historical-docs-audit.md` — treatment of legacy READMEs.
- `doc/build-linux.md` — canonical Linux build flow.
- `doc/build-windows.md` — Linux-hosted Windows cross-compile flow.
- `idlib/library/src/idlib/platform/platform.hpp` — canonical platform detection.
- `debug-output.txt` — recent Wine-run Windows startup failure.
