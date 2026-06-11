---
name: doc-accuracy-issues
description: Open + resolved doc-accuracy issues in build docs / AGENTS.md; last audited 2026-06-11
metadata:
  type: project
---

## Open issues

### build-windows.md — missing DLLs in external/mingw/bin (STILL OPEN, verified 2026-06-11)
- The DLL-copy mechanism implies the bundle contains SDL2_image.dll and SDL2_mixer.dll, but
  `external/mingw/bin/` has SDL2.dll, SDL2_ttf.dll, and codec DLLs — **no SDL2_image.dll, no SDL2_mixer.dll**.
- `idlib-game-engine/library/CMakeLists.txt` uses `if (EXISTS ...)` guards per DLL, so it degrades silently
  (build does not hard-fail, but the cross-built Windows binary may be missing image/audio at runtime).

## Resolved (2026-06-11 — re-verified against the live tree, all fixed)

- **egolib/AGENTS.md hotspot list** — no longer references the deleted `script_functions.c`/`_systems.c`; the
  file now points to root `AGENTS.md` + `CODEBASE-HEALTH-STATUS.md` §3 instead of keeping a drift-prone copy.
- **root AGENTS.md agent path** — now correctly `.claude/agents/` (was the wrong `.codex/agents/`).
- **README.md vs build-linux.md GitHub URL** — both now use `github.com/dubstabber/egoboo` (consistent).
- **debug-output.txt** — the file now exists and is no longer referenced as a dangling startup-failure artifact.

## Confirmed accurate (reference — re-check before asserting doc accuracy)

- **doc/build-linux.md** — submodule-init command, CMake configure/build commands, output paths, run
  instructions, `EGOBOO_DATA_DIR`/`SDL_VIDEODRIVER=x11`, Fedora package list, the PhysFS-bundled claim
  (physfs-3.0.0 built from source in idlib-game-engine), and the validator flags all match current state.
- **doc/build-windows.md** — toolchain path `cmake/toolchains/mingw-w64-x86_64.cmake` exists;
  `EGOBOO_MINGW_DEPENDENCY_ROOT` default `external/mingw`; shlwapi/winmm handled internally; cross-build test
  exclusion accurate.
- **doc/error-handling-policy.md** — `idlib::argument_null_error` exists; policy is internally consistent.
  (Note: the `egolib_rv`/`gfx_rv` retirement has progressed since — see roadmap T1.4; treat the policy as the
  target, not a live-count source.)
- **cmake/toolchains/mingw-w64-x86_64.cmake** — sets C/C++/RC compilers via the x86_64-w64-mingw32 prefix,
  `CMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32`, infers AR/RANLIB from the prefix (correct).

**How to apply:** Check this before asserting doc accuracy or proposing build-doc edits. Volatile counts
(archive TUs, file sizes, `::get()`, test totals) are NOT tracked here — they live in
`refactoring-documents/CODEBASE-HEALTH-STATUS.md`; ground-truth them against the live tree.
