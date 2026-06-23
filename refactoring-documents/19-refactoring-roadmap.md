# Refactoring Roadmap

Snapshot date: 2026-06-23. This is the forward plan only. Completed-pass history
lives in `71-completed-passes-log.md`; current metrics live in
`CODEBASE-HEALTH-STATUS.md`.

Superseded historical plans:

- `19-new-refactoring-plan.md`
- `22-module-runtime-ownership-plan.md`
- `25-entity-layer-decomposition-plan.md`
- `33-maintainability-improvement-plan.md`

## Principles

1. No flag-day rewrites.
2. Characterization tests before risky restructuring.
3. Preserve observable behavior unless the task explicitly changes it.
4. Reduce coupling and ownership ambiguity before cosmetic cleanup.
5. Keep Windows tooling open source; do not revive Visual Studio-only paths.
6. Treat warnings and stale docs as portability and maintenance debt.

## Tier 1: Runtime Structure

### T1.1 Keep `Object` Role Work Incremental

`Object.hpp` is now below 1,000 lines, but it is still the broadest runtime
interface. The useful next work is not mechanical line trimming; it is reducing
multi-role call surfaces and keeping callers on existing role interfaces where
those interfaces express the actual dependency. Use `CODEBASE-HEALTH-STATUS.md`
for the current role-interface count.

Good candidates:

- move mixed-domain script helpers toward narrower role parameters where this is
  behavior-preserving
- avoid adding broad `Object&` parameters to new code
- add focused tests before changing multi-role combat, inventory, or script paths

### T1.2 Continue Service-Seam Cleanup

`EngineContext::get()` and `GameSessionContext::get()` are intentional seams but
still act as service locators. Prefer existing installed services and `active*()`
helpers over raw concrete singleton access.

Good candidates:

- remove remaining low-count direct singleton calls where a service seam already
  exists
- keep subsystem-local bootstrap exceptions explicit
- do not create new hidden globals for convenience

### T1.3 Keep File Splits Behavior-Preserving

The production runtime no longer has >1,000-line files. Future file splits should
be done only when they improve ownership, navigation, or archive boundaries.

Required discipline:

- verify symbol ownership after moving sources between archive layers
- keep private helper promotions minimal
- do not put private headers into carve-layer source lists where they can become
  stray compiled `.h.o` archive members

### T1.4 Enforce The Error-Handling Policy

`doc/error-handling-policy.md` is the active target. New code should not add
silent failures. Existing mixed exception/boolean/null-return behavior should be
migrated only in bounded subsystem passes with tests.

## Tier 2: Build And Platform

### T2.1 Keep The Nine-Archive DAG Acyclic

Any CMake source movement in `egolib/library/CMakeLists.txt` must preserve the
current direction:

```text
foundation-base <- {physics, renderer <- gui} <- library
library <- game-graphics <- hud-widgets <- {scriptvm, gamestates}
```

Verify with live archive `nm` checks, not object-directory globs.

### T2.2 Native Windows Open-Source Build

Still open. Add a native Windows path based on an open-source toolchain
(for example MSYS2/UCRT64) once the current cross-build assumptions are stable.
Do not add Visual Studio-only requirements to the maintained path.

### T2.3 Wine Runtime Stabilization

The Linux-hosted Windows cross-build is useful, but Wine execution is still a
compatibility path. Continue diagnosing the mipmap/font/audio issues behind
`run-egoboo-windows.sh` compatibility defaults until the Windows artifact is a
credible runtime verification target.

### T2.4 Retire Legacy CI/Project Artifacts

Remaining legacy Visual Studio/AppVeyor artifacts should be removed or clearly
quarantined when they stop serving an active compatibility purpose.

## Tier 3: Deeper Design Work

### T3.1 `shared_ptr<Object>` Discipline

`ObjectHandler` is the practical owner, but many APIs still traffic in
`shared_ptr<Object>`. Treat this as incremental refcount and ownership clarity
work, not as a rewrite. Prefer non-owning references or `ObjectRef` where the
lifetime is already guaranteed by the handler.

### T3.2 Script Runtime Shape

Function dispatch already uses a registry/X-macro table, so a "switch to
registry" rewrite is obsolete. The remaining value is in tests around dispatch
coverage and in narrowing helper dependencies as role surfaces improve.

### T3.3 Rendering And GUI Characterization

GUI base-class tests exist, but rendering correctness and concrete game-state
transition behavior remain thin. Add focused characterization before changing
render passes, camera behavior, HUD widgets, or state-stack transitions.

### T3.4 Content Pipeline Separation

Object profile parsing, model loading, script compilation, and validator startup
still need runtime services. Continue separating pure data parsing from runtime
service access where the tests and validator can prove behavior.

## Recently Completed Fronts To Treat As Closed

- Runtime globals retired through `EngineContext` and `GameSessionContext`.
- `egolib/egolib.h` uber-header deleted.
- `cartman` is wired into CMake behind `EGOBOO_BUILD_CARTMAN`.
- `vfs.c`, script dispatch files, collision response, `GameEngine`, and object
  profile loading have been split below the old monolithic sizes.
- glTF/GLB object model loading has landed behind the current static-mesh subset.
- The full validator baseline remains 42 modules, 25 warnings, 230 known content
  errors.
