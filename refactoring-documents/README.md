# Refactoring Documents

Architecture and refactoring reference for the current Egoboo workspace.
Originally opened 2026-04-12; consolidated 2026-04-18 (65 → 14 files) and
again 2026-07-21 (14 → 12 files, strategy merged into the roadmap).

Three kinds of documents: **reference** (how the project is shaped today and
how to change it safely), **roadmap** (what is outstanding and the rules the
work runs under), and **history** (what has already landed).

## Reference

- [`CODEBASE-HEALTH-STATUS.md`](CODEBASE-HEALTH-STATUS.md) — current-state
  health snapshot: source/archive counts, hotspots, tests, validator baseline,
  platform status, and the measured **progress comparison against the
  pre-refactoring `backup-copy/` tree**. **Start here.**
- [`01-repository-and-build-audit.md`](01-repository-and-build-audit.md) —
  active vs. inactive repo areas, build graph, canonical build docs,
  portability behavior.
- [`02-runtime-architecture.md`](02-runtime-architecture.md) — boot path,
  `GameEngine`, main loop, state stack, global-state status, subsystem map,
  target boundaries.
- [`03-data-and-content-audit.md`](03-data-and-content-audit.md) — VFS/content
  pipeline, module and object conventions, model-loader status, migration
  model.
- [`05-playtesting-and-bug-hunt-plan.md`](05-playtesting-and-bug-hunt-plan.md)
  — risk-based playtest matrix for manual verification.
- [`06-validator-baseline.md`](06-validator-baseline.md) — validator scope and
  the pre-existing content failures that are *not* regressions.
- [`07-historical-docs-audit.md`](07-historical-docs-audit.md) — legacy `.odt`
  inventory; what is still usable.
- [`08-spawn-format-spec.md`](08-spawn-format-spec.md) — `spawn.txt` format
  contract.
- [`09-data-format-spec.md`](09-data-format-spec.md) — `data.txt` format
  contract.

## Roadmap

- [`19-refactoring-roadmap.md`](19-refactoring-roadmap.md) — the single
  strategy-and-roadmap document: non-negotiable rules, phase status, and
  **what is left**. Absorbs the former `04-refactoring-strategy.md`.
  **Read before any large refactor.**

## History

- [`71-completed-passes-log.md`](71-completed-passes-log.md) — compact history
  of completed refactoring fronts through Pass 312, including the
  documentation-consolidation record (formerly
  `70-documentation-consolidation.md`).

## How to add to this directory

Append a compact entry (2–5 lines: date, theme, what boundary moved or split,
verification) to `71-completed-passes-log.md`. Reserve a new numbered document
only for active work that introduces a new architectural boundary or requires
multi-page design context. Volatile numbers go in
`CODEBASE-HEALTH-STATUS.md` only; other documents link there.
