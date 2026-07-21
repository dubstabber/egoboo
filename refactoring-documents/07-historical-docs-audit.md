# Historical Docs Audit

Inventory of the legacy `.odt` manuals in `doc/`. They preserve design intent
the code no longer documents, so they should guide specification and migration
work — but they are historical reference, never authoritative for current
behavior. Text was extracted from `content.xml` (formatting/tables/images are
lossy); cross-check any claim against current code and the validator baseline.

## Ranked inventory

| Document | Refactor value | What it preserves |
| --- | --- | --- |
| `Egoboo Development Guide.odt` | **Highest** | Object making, slots, spawning, EgoScript reference, module/passage authoring. Best historical explanation of why the content tree is convention-heavy: positional `data.txt` (slot first), documented legacy `spawn.txt` fields, passage-driven module logic. Starting point for the format specs (08/09). |
| `Egomap Tutorial.odt` | High | 64×64 tile map model, editor modes, light editing, random generation. Explains why `level.mpd` and object placement look editor-shaped; mine it before any map-format migration. |
| `egoboo manual.odt` | Medium | Gameplay/class assumptions, module progression, save/load, death and XP-loss rules. Useful for playtest checklists on progression and save handling. |
| `Quickstart Guide.odt` | Medium-low | Starter modules, unlock flow, expected first-player experience. Informs smoke-test scenarios. |
| `Egoboo Player Manual.odt` | Low | Player-facing terminology and feature explanations; vocabulary support only. |
| `peoguide.odt` | Minimal | Lore/world reference; keep for naming and content interpretation only. |

## Cross-document themes

Consistent historical assumptions that still shape the repository: content
authored around directory conventions and positional text files; modules,
object factories, and spawning as the assembly model; passages as a
first-class mechanism; EgoScript deeply embedded in authoring; tooling built
on editor-era assumptions rather than engine/content boundaries.

## Implications

- A structured replacement for `data.txt`/`spawn.txt` is justified and must
  translate old semantics automatically (now specced in 08/09).
- Scripting replacement needs a specification pass first — meaning still lives
  in these docs that the runtime does not encode cleanly.
- Map-format work should document the old authoring model alongside current
  loader behavior before changing anything.
- Remaining useful documentation work: current passage-behavior notes compared
  against the historical authoring model, and a glossary mapping EgoScript
  terminology to current runtime functions.
