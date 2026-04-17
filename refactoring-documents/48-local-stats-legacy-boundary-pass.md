# Local-Stats Legacy Boundary Pass

This document records the `local_stats` compatibility-boundary cleanup completed on 2026-04-17 after the respawn-cooldown ownership pass.

## What changed

- audited in-repo `local_stats` and `local_stats_t` consumers after pass 47
- confirmed production usage is limited to:
  - the legacy compatibility mirror definition in `egolib/game/egoboo.c`
  - the compatibility bridge in `egolib/game/Core/GameSessionContext.cpp`
- confirmed direct test usage is limited to focused mirror assertions in:
  - `egolib/tests/egolib/tests/ModulePlayerStartup.cpp`
- moved the legacy ABI declaration for `local_stats_t` and `local_stats` out of the broad `egolib/game/egoboo.h` umbrella header into a dedicated compatibility header:
  - `egolib/game/LegacyLocalStats.hpp`
- updated the compatibility bridge, legacy definition site, and focused compatibility tests to include the dedicated header explicitly

## Why this pass now

Document 47 left one explicit follow-on question:

- should `local_stats` remain a broad exported compatibility surface, or be isolated behind a narrower legacy boundary?

The repo audit answered the first part safely:

- no active in-repo production reader still treats `local_stats` as ownership state
- no other runtime subsystem in this repo depends directly on the `local_stats_t` layout
- only the compatibility bridge and focused tests still need the ABI surface

That made the next low-risk step clear: keep the legacy ABI available, but stop advertising it through one of the most widely included runtime headers.

## Scope constraints kept

- no behavior changes
- no field removals, renames, or layout changes in `local_stats_t`
- no symbol rename for the exported `local_stats` global
- no change to `GameSessionContext` ownership of local-player status, perception, enemy-sense, or respawn-cooldown state
- no change to test intent; the focused compatibility assertions remain intact

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `egolib/game/egoboo.h` no longer declares `local_stats_t` or `local_stats`
- the legacy ABI remains available through `egolib/game/LegacyLocalStats.hpp`
- only the compatibility bridge, legacy definition site, and focused compatibility tests include the dedicated header
- the `local_stats` symbol and struct layout remain unchanged for any out-of-repo compatibility consumer already depending on them
- existing focused local-player/session compatibility tests still pass
- `test.mod` still validates with `egoboo-content-validator`

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

The next practical follow-on is to decide whether out-of-repo consumers still need the exported `local_stats` global at all.

If no such consumers exist, the next cleanup can replace the exported global with a narrower accessor-based compatibility shim and keep the focused tests pointed at that shim instead of the raw variable.
