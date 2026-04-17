# Local-Stats Export Retirement Pass

This document records the export-retirement cleanup completed on 2026-04-17 after the accessor-shim pass.

## What changed

- removed the raw `local_stats` variable declaration from `egolib/game/LegacyLocalStats.hpp`
- changed `LegacyLocalStats.hpp` to expose only the explicit compatibility accessors:
  - `legacy_local_stats()`
  - `legacy_local_stats_const()`
- replaced the exported file-scope `local_stats` definition in `egolib/game/egoboo.c` with file-local compatibility storage plus the accessor definitions
- kept the `GameSessionContext.cpp` compatibility bridge publishing through the accessor seam
- kept the focused compatibility assertions in `ModulePlayerStartup.cpp` reading the mirror through the accessor seam

## Why this pass now

Document 49 left one remaining branch:

- keep the raw exported `local_stats` variable for hypothetical external consumers
- or retire that export and keep only the explicit accessor-based compatibility surface

This pass takes the second path under the repo-level assumption that no supported out-of-repo consumer still links against the raw variable directly.

With that assumption in place, the narrowest safe cleanup was:

- preserve the legacy mirror data shape
- preserve the in-repo accessor seam
- stop advertising or exporting the raw compatibility variable name itself

## Scope constraints kept

- no behavior changes
- no field removals, renames, or layout changes in `local_stats_t`
- no change to `GameSessionContext` ownership of local-player state
- no change to compatibility mirror publication order
- no reopening of migrated gameplay write/read sites outside the compatibility bridge

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- repo code reaches the legacy mirror through `legacy_local_stats()` / `legacy_local_stats_const()`
- `LegacyLocalStats.hpp` no longer declares a raw `local_stats` variable
- the legacy mirror storage is file-local to `egoboo.c`
- `GameSessionContext.cpp` still publishes all mirrored fields through the accessor-based compatibility bridge
- focused compatibility tests still verify mirror publication, reset clearing, and respawn cooldown ticking
- the runtime and validator behavior remain unchanged

Validated command set for this pass:

```bash
cmake --build build -j4
```

```bash
ctest --test-dir build --output-on-failure -R ModulePlayerStartupFixture
```

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod
```

```bash
rg -n "\\blocal_stats\\b" egolib egoboo idlib idlib-game-engine
```

## Follow-on recommendation

The `local_stats` ownership and compatibility cleanup sequence is now complete inside this repo:

- gameplay-visible ownership lives in `GameSessionContext`
- the remaining legacy mirror is quarantined behind one accessor seam
- the raw exported variable name is no longer part of the maintained in-repo surface

The next practical refactoring target is to resume the broader global-state cleanup around the remaining `_gameEngine` and `update_wld` seams, as already called out in the runtime-context and maintainability plans.
