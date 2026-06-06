---
name: doc-accuracy-issues
description: Stale paths, wrong claims, and confirmed accurate items found in build docs and AGENTS.md; audited June 2026
metadata:
  type: project
---

## Confirmed stale / wrong

### egolib/AGENTS.md hotspot list
- Lists `library/src/egolib/game/script_functions.c` — this file NO LONGER EXISTS.
- The monolith was split into seven files: script_functions_action.c, _bitwise.c, _movement.c, _spawn.c, _state.c, _systems.c, _target.c
- CLAUDE.md already reflects the correct split; egolib/AGENTS.md does not.

### AGENTS.md (root) — agent path wrong
- Line 30: "Project-scoped custom agents live in `.codex/agents/` and share shallow delegation defaults from `.codex/config.toml`."
- Actual location: `.claude/agents/` (five .md files confirmed). No `.codex/` directory exists.

### README.md — GitHub URL inconsistency
- clone URL uses `github.com/dubstabber/egoboo` (fork URL)
- build-linux.md uses `github.com/egoboo/egoboo` (upstream URL)
- License link also uses `dubstabber/egoboo`
- These are different repos; at minimum they should be consistent.

### build-windows.md — missing DLLs in external/mingw/bin
- Doc says the bundle is expected to contain SDL2_image.dll and SDL2_mixer.dll (implied by the DLL copy mechanism)
- Actual external/mingw/bin/ has: SDL2.dll, SDL2_ttf.dll, and various codec DLLs — but NO SDL2_image.dll and NO SDL2_mixer.dll
- The CMake code in idlib-game-engine/library/CMakeLists.txt uses `if (EXISTS ...)` guards for each, so it degrades silently
- This is a real gap in the bundle but the build won't hard-fail on it

### debug-output.txt — referenced but missing
- build-windows.md and README.md both reference `debug-output.txt` for a recent Windows startup failure
- The file does not exist in the repository root

## Confirmed accurate

### doc/build-linux.md
- Submodule init command is correct: `git submodule update --init data external idlib idlib-game-engine`
- CMake configure/build commands, output paths, and run instructions all match current state
- EGOBOO_DATA_DIR and SDL_VIDEODRIVER=x11 env var usage is accurate
- Fedora package list is accurate for current dependencies
- PhysFS claim ("fetched and built by CMake from the vendored idlib-game-engine setup") is substantively correct — physfs-3.0.0 is bundled in idlib-game-engine/library/ and built from source; no system physfs needed
- Validator flags (--verbose, --skip-scripts) and scope description are accurate
- Pointer to idlib-game-engine/library/CMakeLists.txt as "real dependency contract" is correct

### doc/build-windows.md
- Toolchain file path `cmake/toolchains/mingw-w64-x86_64.cmake` is correct and file exists
- EGOBOO_MINGW_DEPENDENCY_ROOT default is `external/mingw` (confirmed in idlib-game-engine/library/CMakeLists.txt)
- shlwapi and winmm are handled internally (egolib/library/CMakeLists.txt and idlib-game-engine respectively)
- Cross-build test exclusion behavior is accurate
- `--recursive` in "if the build fails" troubleshooting step is technically broader than needed (build-linux.md uses the more precise named-submodule form), but not wrong

### doc/error-handling-policy.md
- idlib::argument_null_error exists (confirmed at idlib/library/src/idlib/exception/argument_null_error.hpp)
- egolib_rv / gfx_rv are actively present (20 files still use them)
- Policy content is internally consistent and matches actual migration posture

### cmake/toolchains/mingw-w64-x86_64.cmake
- Sets C, C++, and RC compilers correctly using x86_64-w64-mingw32 prefix
- Sets CMAKE_FIND_ROOT_PATH to /usr/x86_64-w64-mingw32
- Does NOT set AR or RANLIB; CMake infers them from the prefix (correct behavior)

**Why:** Useful as a reference when writing new build instructions or editing any of these doc files.
**How to apply:** Check this before asserting doc accuracy or proposing doc edits.
