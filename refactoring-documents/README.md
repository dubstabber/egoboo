# Refactoring Documents

Architecture and refactoring reference for the current Egoboo workspace. Originally opened 2026-04-12. Index last rewritten 2026-04-18 as part of the documentation consolidation pass (`70-documentation-consolidation.md`); refreshed 2026-06-30 after the second documentation compaction pass.

This directory has three kinds of documents: **reference** (how the project is shaped today and how to change it safely), **roadmap** (what is outstanding), and **history** (what has already landed).

## Reference

- [`CODEBASE-HEALTH-STATUS.md`](CODEBASE-HEALTH-STATUS.md) — current-state health snapshot: source/archive counts, hotspots, tests, validator baseline, platform status. **Start here** for volatile quantitative facts.
- [`01-repository-and-build-audit.md`](01-repository-and-build-audit.md) — active vs. inactive repo areas, build graph, canonical build docs, portability behavior.
- [`02-runtime-architecture.md`](02-runtime-architecture.md) — boot path, `GameEngine`, main loop, state stack, global-state status, subsystem map, pain points.
- [`03-data-and-content-audit.md`](03-data-and-content-audit.md) — content pipeline, module and object file conventions.
- [`04-refactoring-strategy.md`](04-refactoring-strategy.md) — non-negotiable rules and phase plan. **Read before any large refactor.**
- [`05-playtesting-and-bug-hunt-plan.md`](05-playtesting-and-bug-hunt-plan.md) — risk-based playtest matrix for manual verification.
- [`06-validator-baseline.md`](06-validator-baseline.md) — validator scope and the pre-existing content failures that are *not* regressions.
- [`07-historical-docs-audit.md`](07-historical-docs-audit.md) — legacy-doc inventory; what is still usable and what is obsolete.
- [`08-spawn-format-spec.md`](08-spawn-format-spec.md) — spawn.txt format contract.
- [`09-data-format-spec.md`](09-data-format-spec.md) — data.txt format contract.

## Roadmap

- [`19-refactoring-roadmap.md`](19-refactoring-roadmap.md) — single consolidated forward plan. Supersedes the earlier `19-new-refactoring-plan.md`, `22-module-runtime-ownership-plan.md`, `25-entity-layer-decomposition-plan.md`, and `33-maintainability-improvement-plan.md`.

## History

- [`70-documentation-consolidation.md`](70-documentation-consolidation.md) — meta-record of the 2026-04-18 documentation consolidation pass that collapsed ~50 per-pass docs and four overlapping plans.
- [`71-completed-passes-log.md`](71-completed-passes-log.md) — compact history of completed refactoring fronts through Pass 293. Former long-form per-pass and completed-front notes are preserved in git history; live guidance was folded into the reference docs.

## How to add to this directory

Future refactoring passes append a compact entry to `71-completed-passes-log.md` with date, theme, what boundary moved or split, and verification. Reserve a new numbered document only for active work that introduces a new architectural boundary or requires multi-page design context.
