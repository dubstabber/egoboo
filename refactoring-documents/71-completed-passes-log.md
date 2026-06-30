# Completed Passes Log

Compact historical record for refactoring passes that have already landed.
Detailed per-pass narratives are recoverable from git history; this file keeps
only outcomes, durable constraints, reusable techniques, and current follow-up
signals.

Last compacted: 2026-06-30. Current metrics live in
`CODEBASE-HEALTH-STATUS.md`; do not duplicate volatile counts here.

## Maintenance Rule

Future refactoring passes should append a compact entry here: date, theme,
files or subsystem touched, behavior preserved or intentionally changed, and the
verification gate. Reserve a new numbered design document only for an active
multi-page architectural boundary.

## April Baseline And Validator Work

- Passes 10 and 24 reconciled `spawn.txt` behavior and established the
  validator as the structural content-health baseline. The live validator
  baseline is maintained in `06-validator-baseline.md`.
- Passes 11-23 replaced direct runtime-global reach with `EngineContext`,
  `GameSessionContext`, and module/session accessors. The former `_gameEngine`
  and `_currentModule` globals are retired from active runtime code.
- Passes 22 and 26-31 moved module runtime ownership into `GameModule` and split
  loading, spawn planning, and spawn realization into narrower units.
- Passes 34-51 moved player startup, local-player bookkeeping, perception,
  respawn cooldowns, and legacy local stats behind session/module surfaces.

## Object And Script Surface Cleanup

- Passes 52-69 encapsulated broad `Object` and `ObjectGraphics` fields,
  timers, flags, attachments, collision volumes, graphics state, and animation
  publication.
- Passes 72-112 introduced and widened role interfaces such as inventory,
  renderable, scriptable, damageable, physical, profile, character state,
  target info, and team-member surfaces. The main lesson: migrate callers only
  where the role captures the real dependency; avoid broad `Object&` in new
  code.
- Passes 113-202 pushed script helper callers toward role/context surfaces,
  retired many shared-pointer compatibility pockets, and replaced direct result
  mutation with explicit helper paths. Script opcode behavior stayed unchanged.

## Build, Service, And Header Fronts

- Passes 203-210 resumed role-interface cleanup, removed duplicate or typo
  compatibility APIs, and completed `override` coverage on role
  implementations.
- Passes 211-219 published input, graphics, font, texture, GFX, billboard,
  texture-atlas, and timing access through installed services instead of
  reaching directly for concrete singletons.
- Passes 220-226 eliminated the `egolib/egolib.h` uber-header. The reusable
  technique was to guard the aggregate include, then compile headers under the
  cut until each one was self-contained:

```cpp
#ifndef EGOBOO_NO_UBER_INCLUDE
#include "egolib/egolib.h"
#endif
```

- The June platform cleanup quarantined legacy platform READMEs under
  `doc/legacy/`, removed proprietary/Visual-Studio-only build hooks from the
  maintained path, and kept Linux plus Linux-hosted MinGW cross-builds as the
  active open-source build story.

## Cartman Integration

- On 2026-06-07, `cartman/` was wired into CMake behind
  `EGOBOO_BUILD_CARTMAN=OFF`, ported through API drift, and runtime-verified
  against `test.mod` on Linux with all four editor viewports rendered.
- `run-cartman.sh` builds the gated target on demand and validates arguments to
  avoid the legacy no-argument shutdown crash.
- Current status: useful but still not part of the default build. Open items are
  deciding whether to flip the default after more module coverage and fixing the
  pre-existing no-arg/bad-arg `atexit`/VFS cleanup crash in the editor itself.

## Include Decoupling And Characterization

- The service-hub front moved active log/config/audio/profile/image/particle
  access toward lower-layer `active*()` style accessors while preserving the
  existing `EngineContext` delegating API for callers and tests.
- The Entities/game include front removed conduit-only `game/` includes from
  propagating headers, moved pure primitives down, and used keep-going builds to
  identify free-riders that needed direct includes. Durable rule: classify
  header self-use separately from transitive include free-riding.
- The Collidable and collision-world work moved terrain/entity world queries
  behind lower-layer interfaces, then relocated collision pieces toward the
  physics archive without changing gameplay behavior.
- Characterization nets were added around pure physics helpers, combat damage,
  collision pipeline behavior, GUI component/container behavior, script runtime
  dispatch, and gameplay helper surfaces before risky dependency cuts.

## Archive Carves

The monolithic `egolib-library` was split into nine static archives while
preserving an acyclic intended direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

Key completed fronts:

- `egolib-foundation-base`: math, file formats, VFS, model loading,
  low-level services, and dependency-closed content/runtime helpers.
- `egolib-physics`: collision nucleus and physics primitives.
- `egolib-renderer` and `egolib-gui`: SDL/OpenGL rendering base and generic GUI
  toolkit.
- `egolib-gamestates`, `egolib-scriptvm`, `egolib-hud-widgets`, and
  `egolib-game-graphics`: upper archives carved out with injection hooks or
  interface seams so lower archives do not name upper concrete types.

When moving sources between archives, measure live `.a` archives with `nm` and
`ar`; do not trust stale `CMakeFiles/*.dir` object directories.

## File-Split And Loader Fronts

- Passes 240-250 split large runtime files such as script spawn helpers, VFS
  search/mount/RWops helpers, particle collision, script compiler helpers,
  `GameEngine`, and object profile loading.
- The `vfs.c` cleanup removed the dead cstdio backend, collapsed the single live
  PhysFS representation, and deduplicated fixed-width read/write helpers without
  changing the public `vfs_FILE*` API.
- The MD2-to-glTF preparation created a format-neutral `AnimatedModel` path,
  `ObjectModelAsset` search-order helpers, `ObjectModelLoader`, and
  `ModelAnimationMetadata`. Current loader behavior is documented in
  `03-data-and-content-audit.md`.

## ObjectRef Ownership Arc

- Passes 271-279 moved team, inventory, particle, enchantment, module spawn,
  particle attachment, shop, and combat attribution surfaces away from public
  `std::shared_ptr<Object>` APIs and toward `ObjectRef` or explicit attribution
  values. The validator baseline briefly fluctuated during audit work, then was
  rechecked back to the maintained `42 modules / 10 warnings / 245 errors`.
- Passes 280-292 completed the ref-first cleanup across `GameModule` spawning,
  player binding, `ObjectHandler` query/enumeration APIs, passage/team/module
  loops, gameplay targeting, inventory, message/export helpers, shop/particle
  helpers, script spawn contexts, and enchantment owner/target/overlay identity.
- Current state after Pass 292: public object enumeration is ref-first, the
  legacy `ObjectHandler::operator[](ObjectRef)` and public handle iterator are
  retired, and remaining shared-handle lookups are intentional ownership or
  weak-storage paths such as player bootstrap and billboard attachment.

## Documentation Passes

- Pass 274 on 2026-06-23 shortened the top-level docs, redirected volatile
  numbers to `CODEBASE-HEALTH-STATUS.md`, and refreshed the then-current test
  count and validator references.
- Pass 293 on 2026-06-30 compressed this history log, removed superseded
  standalone historical-front notes, folded live glTF and cartman guidance into
  canonical docs, and rechecked current archive, test, singleton, and validator
  metrics.
- Pass 294 on 2026-06-30 widened the lower-layer `IObjectWorld` seam with live
  object lookup helpers, redirected `GameSessionContext::tryObject()` through
  that seam, and moved a bounded batch of team/player/graphics/HUD callers off
  direct session object lookup. Ownership and lifetime behavior stayed with
  `GameModule`/`ObjectHandler`; coverage was added in `ObjectHandlerQueries`.
  Verified with the Linux build, focused object/session ctest filter, full
  `ctest`, and the `test.mod` validator smoke.
