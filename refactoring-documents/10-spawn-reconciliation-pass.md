# Spawn Reconciliation Pass

This document records the next recommended refactoring pass after the initial validator baseline and format-spec work.

## Baseline confirmed on 2026-04-13

Fresh validator run:

```bash
HOME=/tmp/egoboo-home \
XDG_DATA_HOME=/tmp/egoboo-xdg \
./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --json
```

Confirmed totals:

| Metric | Value |
| --- | ---: |
| Modules validated | 42 |
| Passing modules | 9 |
| Failing modules | 33 |
| Validator warnings | 25 |
| Validator errors | 278 |
| `missing_spawn_object` errors | 277 |

The current structural problem is still dominated by spawn-reference integrity, not parser breakage and not broad runtime architecture churn.

## Scope of this pass

- Keep runtime behavior unchanged.
- Keep legacy format parsing unchanged.
- Extend the validator so it can emit a deterministic object-reconciliation report for every module.
- Make spawn-name normalization reusable from shared code instead of keeping the logic embedded only in `module_spawn.c`.

## Immediate triage targets

Start with the worst failing modules from the confirmed baseline:

- `archmage.mod`
- `abyss2.mod`
- `palwater.mod`
- `zippy.mod`
- `heist.mod`

## Repeated unresolved names worth prioritizing

The confirmed report still shows recurring unresolved names that look like typos, placeholders, or stale aliases:

- `unknown.obj`
- `shutter.obj`
- `object.obj`
- `.obj`
- `dark glower.obj`
- `guard.obj`
- `tbunny.obj`

## Expected tooling output from this pass

The validator should emit enough information to support manual content repair without guessing:

- the raw load token from `spawn.txt`
- the normalized object name actually checked by the runtime contract
- the resolved `mp_objects/...` virtual path
- a per-module inventory of reachable object directories based on the current VFS mount order
- canonical-key candidate names for likely alias matches such as `g'nome.obj` versus `gnome.obj`

## Follow-up workflow

After this pass lands:

1. Refresh the full validator JSON report.
2. Use the reconciliation rows to build a manual alias and typo triage list.
3. Fix the highest-signal content references in small batches.
4. Re-run the validator after each batch and keep the JSON report as the baseline source of truth.

This pass is intentionally a tooling pass first. It should make the current content debt explicit before any larger runtime-context extraction or data-format migration.
