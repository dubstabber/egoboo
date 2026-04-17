# Engine Context Ownership Pass

This document records the engine-ownership encapsulation cleanup completed on 2026-04-17 after the local-stats export retirement pass.

## What changed

- extended `egolib/game/Core/EngineContext.hpp` with explicit ownership operations:
  - `setEngine(std::unique_ptr<GameEngine>)`
  - `clearEngine()`
- moved active-engine storage out of `GameEngine.hpp` / `GameEngine.cpp` and into file-local storage in `EngineContext.cpp`
- removed the raw `extern std::unique_ptr<GameEngine> _gameEngine` declaration from `GameEngine.hpp`
- updated `egoboo/src/game/Main.cpp` to install, start, and clear the active engine through `EngineContext`
- added focused `EngineContext` tests covering empty-state behavior, installation, double-install rejection, and clearing

## Why this pass now

Document 50 closed the in-repo `local_stats` cleanup and returned the refactor track to the last broad legacy-global seam:

- `_gameEngine` still had one exported declaration and one direct bootstrap owner
- `update_wld` no longer existed as a live global seam; only naming residue remained in comments and debug labels

That made engine ownership the next narrow, decision-complete follow-on:

- keep current runtime behavior
- keep `Main.cpp` as the only engine construction site
- stop advertising the raw singleton storage as public runtime surface

## Scope constraints kept

- no `GameEngine` lifecycle redesign
- no `EngineContext` read-side API removals or signature changes
- no broader dependency-injection framework
- no gameplay, UI, or validator behavior changes
- no rename sweep for legacy `update_wld` wording in comments or cached field names

## Acceptance baseline

This pass is considered healthy when all of the following remain true:

- `GameEngine.hpp` no longer exports a raw `_gameEngine` singleton
- `Main.cpp` installs and starts the engine through `EngineContext`
- `EngineContext` is the only maintained in-repo ownership seam for the active engine
- focused `EngineContext` tests verify empty-state failure, installation visibility, double-install rejection, and clearing
- the runtime and validator behavior remain unchanged

Validated command set for this pass:

```bash
cmake --build build -j4
```

```bash
ctest --test-dir build --output-on-failure -R EngineContextFixture
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
rg -n "\\b_gameEngine\\b" egolib egoboo idlib idlib-game-engine
```

## Follow-on recommendation

The raw active-engine singleton export is now retired inside this repo:

- runtime callers reach the active engine through `EngineContext`
- engine construction remains centralized in `Main.cpp`
- the maintained in-repo `_gameEngine` surface is gone

The remaining `update_wld` mentions are now terminology residue only. The next cleanup should therefore target a different active coupling seam rather than treat `update_wld` naming comments as architectural debt on the same priority tier.
