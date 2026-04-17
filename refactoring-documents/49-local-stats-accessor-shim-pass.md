# Local-Stats Accessor Shim Pass

This document records the accessor-shim cleanup completed on 2026-04-17 after the legacy-boundary pass.

## What changed

- added explicit compatibility accessors for the exported `local_stats` global in `egolib/game/LegacyLocalStats.hpp`:
  - `legacy_local_stats()`
  - `legacy_local_stats_const()`
- updated the `GameSessionContext.cpp` compatibility bridge to publish through that shim instead of naming the raw global directly
- updated the focused compatibility assertions in `ModulePlayerStartup.cpp` to read through the shim accessor instead of the raw global name

## Why this pass now

Document 48 left a safe follow-on available without making assumptions about out-of-repo consumers:

- keep the exported `local_stats` ABI intact
- reduce in-repo direct references to the raw variable further
- prefer one explicit compatibility surface for legacy mirror access

That made an inline accessor shim the narrowest practical next step.

## Scope constraints kept

- no behavior changes
- no symbol rename or removal for `local_stats`
- no field removals, renames, or layout changes in `local_stats_t`
- no change to `GameSessionContext` ownership of local-player state
- no change to compatibility mirror publication order

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- the exported `local_stats` symbol still exists
- `legacy_local_stats()` and `legacy_local_stats_const()` expose the compatibility surface for in-repo callers
- `GameSessionContext.cpp` no longer writes the raw `local_stats` variable directly
- focused compatibility tests assert through the shim accessor rather than the raw variable name
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

## Follow-on recommendation

The remaining decision is still external-compatibility driven:

- if out-of-repo consumers require the exported `local_stats` variable, keep this shim as the stable in-repo quarantine boundary
- if no such consumers exist, the next cleanup can stop exporting the raw global and keep only an accessor-based compatibility surface
