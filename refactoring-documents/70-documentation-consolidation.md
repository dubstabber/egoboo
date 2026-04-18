# Documentation Consolidation

This document records the documentation consolidation completed on 2026-04-18.

## What changed

- Merged four strategic plans (`19-new-refactoring-plan.md`, `22-module-runtime-ownership-plan.md`, `25-entity-layer-decomposition-plan.md`, `33-maintainability-improvement-plan.md`) into a single `19-refactoring-roadmap.md`. Outstanding items only; completed items moved to the history log.
- Consolidated ~50 per-pass documents (10–16, 20–21, 23–24, 26–31, 34–45, 47–69) into a single chronological `71-completed-passes-log.md` organized by seven themes. Per-pass documents deleted; full detail remains in git history.
- Stripped inline retroactive `> **Status update (2026-04-17):**` blocks from foundational docs 01–06 and rewrote affected sections to describe current state directly instead of patching past prose.
- Refreshed quantitative snapshots in docs 01 and 02 to match 2026-04-18 repository state.
- Rewrote `README.md` with a three-section index (Reference / Roadmap / History) replacing the ~60-entry "Recommended reading order" list.
- Updated `CODEBASE-HEALTH-STATUS.md` section 13 cross-references to point at the new roadmap and history files; refreshed snapshot date to 2026-04-18.

## Why this pass now

The refactoring-documents directory had grown to 65 files (~250 KB). Three problems made the folder hard to use:

- Per-pass logs dominated the directory. Each followed a similar template (what changed / why this pass now / scope constraints / acceptance baseline / follow-on recommendation). The "follow-on recommendation" sections were immediately superseded by the next pass and had become stale noise.
- Strategic plans 19, 22, 25, 33 overlapped. Plans 22 and 25 were fully executed. Plan 33 cross-referenced plan 19 and referenced already-retired docs 17, 18, 32, 46.
- Foundational docs 01–06 carried inline retroactive patch blocks that contradicted the original prose around them, plus line-count and reference-count tables that no longer matched reality.

The owner had already proved the consolidation pattern with `CODEBASE-HEALTH-STATUS.md` (which replaced 17, 18, 32, 46). Extending that pattern to the rest of the tree was the cleanest improvement.

## Scope constraints kept

- No runtime, validator, build, or test changes.
- No deletion of foundational reference docs (01–09) — only their inline status-update blocks and stale metrics.
- No change to `04-refactoring-strategy.md` — the non-negotiable rules and phase plan remain in force.
- Git history preserves every deleted per-pass doc at its original path; only the working tree was pruned.

## End-state directory

After this pass, `refactoring-documents/` contains:

- `README.md` — three-section index
- `CODEBASE-HEALTH-STATUS.md` — current-state snapshot
- `01-repository-and-build-audit.md` through `09-data-format-spec.md` — reference docs (no inline status-update blocks)
- `19-refactoring-roadmap.md` — consolidated forward plan
- `70-documentation-consolidation.md` — this document
- `71-completed-passes-log.md` — chronological history of passes 10–69

Total: 14 documents, down from 65.

## Follow-on recommendation

Future refactoring passes should continue appending entries to `71-completed-passes-log.md` under the appropriate theme heading rather than creating new per-pass documents. Keep each entry compact (2–4 lines: date, theme, what moved behind a seam or got split). Reserve a new numbered doc only for work that introduces a new architectural boundary or requires multi-page design context.
