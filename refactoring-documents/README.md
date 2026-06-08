# Refactoring Documents

Architecture and refactoring reference for the current Egoboo workspace. Originally opened 2026-04-12. Index last rewritten 2026-04-18 as part of the documentation consolidation pass (`70-documentation-consolidation.md`).

This directory has three kinds of documents: **reference** (how the project is shaped today and how to change it safely), **roadmap** (what is outstanding), and **history** (what has already landed).

## Reference

- [`CODEBASE-HEALTH-STATUS.md`](CODEBASE-HEALTH-STATUS.md) — current-state health snapshot: sizes, hotspots, SOLID scores, modularization, cross-platform, third-party. **Start here** for the quantitative "where are we now" picture.
- [`01-repository-and-build-audit.md`](01-repository-and-build-audit.md) — active vs. inactive repo areas, build graph, Fedora portability behavior, hotspot file sizes.
- [`02-runtime-architecture.md`](02-runtime-architecture.md) — boot path, `GameEngine`, main loop, state stack, global-state status, subsystem map, pain points.
- [`03-data-and-content-audit.md`](03-data-and-content-audit.md) — content pipeline, module and object file conventions.
- [`04-refactoring-strategy.md`](04-refactoring-strategy.md) — non-negotiable rules and phase plan. **Read before any large refactor.**
- [`05-playtesting-and-bug-hunt-plan.md`](05-playtesting-and-bug-hunt-plan.md) — risk-based playtest matrix for manual verification.
- [`06-validator-baseline.md`](06-validator-baseline.md) — validator scope and the pre-existing content failures that are *not* regressions.
- [`07-historical-docs-audit.md`](07-historical-docs-audit.md) — legacy-doc inventory; what is still usable and what is obsolete.
- [`08-spawn-format-spec.md`](08-spawn-format-spec.md) — spawn.txt format contract.
- [`09-data-format-spec.md`](09-data-format-spec.md) — data.txt format contract.

## Roadmap

- [`19-refactoring-roadmap.md`](19-refactoring-roadmap.md) — single consolidated forward plan. Supersedes the earlier `19-new-refactoring-plan.md`, `22-module-runtime-ownership-plan.md`, `25-entity-layer-decomposition-plan.md`, and `33-maintainability-improvement-plan.md`. Three tiers: in-flight work, build and cross-platform, deeper structural work.

## History

- [`70-documentation-consolidation.md`](70-documentation-consolidation.md) — meta-record of the 2026-04-18 documentation consolidation pass that collapsed ~50 per-pass docs and four overlapping plans.
- [`71-completed-passes-log.md`](71-completed-passes-log.md) — chronological log of the numbered refactors from 2026-04-13 through 2026-06-08, grouped by theme (spawn/validator, runtime context wrappers, module runtime ownership, player startup, local-player session ownership, local-stats retirement, `Object`/`ObjectGraphics` encapsulation, `Object` role extraction, the uber-header teardown, the vfs cstdio elimination, cartman build integration, the T3.4 characterization batches, and the T3.7 logging-seam decoupling). The former per-pass documents (10–16, 20–21, 23–24, 26–31, 34–45, 47–69) are preserved in git history at their original paths; later passes append directly to this log.
- [`72-uber-header-teardown.md`](72-uber-header-teardown.md) — the uber-header teardown plan, per-pass work-list, and the reusable `EGOBOO_NO_UBER_INCLUDE` selfcheck technique + symbol→home dictionary. **Front COMPLETE** (`egolib.h` deleted, 2026-06-07); retained as a technique reference.
- [`73-cartman-build-integration-scouting.md`](73-cartman-build-integration-scouting.md) — the cartman build-integration record: the port (compile-drift fixes, `nm` ODR-collision trick), the `run-cartman.sh` launch recipe, and open items (default-flip, no-arg atexit crash). **Front COMPLETE through Phase 3** (cartman builds/links/runs, gated off, 2026-06-07).

## How to add to this directory

Per the follow-on recommendation in `70-documentation-consolidation.md`: future refactoring passes append a compact entry (2–4 lines: date, theme, what moved behind a seam or got split) to `71-completed-passes-log.md` under the appropriate theme heading. Reserve a new numbered document only for work that introduces a new architectural boundary or requires multi-page design context.
