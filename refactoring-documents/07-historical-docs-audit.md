# Historical Docs Audit

This document captures what can still be recovered from the legacy `.odt` documents in `doc/` and how those documents should influence future refactoring work.

The goal is not to treat these manuals as the current source of truth. The goal is to preserve historical intent before large code and data migrations erase the context that explains why the repository looks the way it does.

## Source set

The following OpenDocument files were inspected:

- `doc/Egoboo Development Guide.odt`
- `doc/Egoboo Player Manual.odt`
- `doc/Egomap Tutorial.odt`
- `doc/Quickstart Guide.odt`
- `doc/egoboo manual.odt`
- `doc/peoguide.odt`

## Extraction method and trust level

- These files were read by extracting `content.xml` from the `.odt` archives rather than through a full office-format renderer.
- Text content is accessible and useful.
- Formatting, tables, images, and some structure are lossy in this extraction mode.
- These documents are valuable as historical reference, not as authoritative documentation of current runtime behavior.
- Any claim taken from these files should be cross-checked against current code, current data, and the validator baseline.

## Refactor value ranking

1. `Egoboo Development Guide.odt`
2. `Egomap Tutorial.odt`
3. `egoboo manual.odt`
4. `Quickstart Guide.odt`
5. `Egoboo Player Manual.odt`
6. `peoguide.odt`

## Document findings

### `Egoboo Development Guide.odt`

This is the single most important historical document for the refactor effort.

Relevant topics recovered from the text include:

- making objects
- object slots
- spawning
- AI scripting
- EgoScript
- module expansions
- making modules
- making passages
- EgoScript reference

Important historical facts preserved here:

- The content pipeline was built around plain text data files, `MPD` maps, `BMP` images, and `MD2` models.
- Modules were expected to use an `objects/` directory together with `spawn.txt`.
- `data.txt` was described as positional data, with the first entry containing the object slot number.
- Object slots were expected to be unique, and the guide describes lower slot ranges as reserved for player or import behavior.
- `spawn.txt` fields were documented explicitly in legacy terms such as object name, slot, position, facing, money, passage, level, status, and team.
- The game logic model depended heavily on EgoScript and passage-driven module behavior.

Refactor relevance:

- This document is the best historical explanation for why the content tree is convention-heavy and fragile.
- It gives us a starting point for formalizing `data.txt`, `spawn.txt`, passage semantics, and object-slot meaning into structured specs.
- It strongly supports the planned replacement of positional text formats and the eventual removal or containment of EgoScript.

### `Egomap Tutorial.odt`

This document is the most useful companion to the development guide when reasoning about modules and maps.

Recovered themes include:

- a tile-based map model using 64x64 tiles
- editor modes for geometry and object placement
- light editing
- random generation features
- workflow assumptions tied to the historical EgoMap editor

Refactor relevance:

- It preserves old map-authoring assumptions that still affect module layout and level data expectations.
- It helps explain why `level.mpd`, passages, and object placement conventions look editor-shaped rather than engine-shaped.
- It should be mined later when documenting current `MPD` semantics and planning map-format migration work.

### `egoboo manual.odt`

This is more player-facing, but it still contains useful system-level context.

Relevant information includes:

- gameplay and class assumptions
- module progression expectations
- save and load behavior
- death and experience-loss rules
- older platform and installation notes

Refactor relevance:

- It captures gameplay rules and terminology that may no longer be obvious from code alone.
- It can help when building playtest checklists for regressions in progression, save handling, and basic class behavior.

### `Quickstart Guide.odt`

This is a lighter onboarding document, but it still helps reconstruct expected game flow.

Relevant information includes:

- starter modules
- module unlock flow
- basic player guidance and progression expectations

Refactor relevance:

- It can inform future smoke-test scenarios and minimal playtest runs.
- It is useful for understanding what the game expected a new player to experience first, which is relevant when prioritizing test coverage.

### `Egoboo Player Manual.odt`

This is mostly a player manual rather than a development reference.

Its value is mainly:

- world and class terminology
- player-facing feature explanations
- historical gameplay assumptions

Refactor relevance:

- Limited direct architectural value.
- Useful as vocabulary support when mapping old data fields and gameplay text to current code paths.

### `peoguide.odt`

This appears to be mostly lore and world-reference material.

Refactor relevance:

- Minimal technical value.
- Keep as reference material for naming, lore, and content interpretation, but it should not influence architecture decisions.

## Cross-document themes

Several consistent historical assumptions show up across the document set:

- Content was authored around directory conventions and positional text files rather than structured schemas.
- Modules, object factories, and spawning were central to how gameplay content was assembled.
- Passage logic was treated as a first-class gameplay mechanism.
- EgoScript was deeply embedded in the content authoring model.
- Tooling and author workflows were built around editor-era assumptions rather than clean engine/content boundaries.

These themes align with the current code and data audit: the repository still carries the shape of those older assumptions even where the implementation has drifted.

## Implications for the refactor plan

The historical documents strengthen several conclusions from the active code audit:

- A structured replacement for `data.txt`, `spawn.txt`, and related text files is justified and overdue.
- Any content migration has to preserve old module and spawn semantics well enough to translate them automatically.
- Scripting replacement work needs a specification pass first; the old docs still contain meaning that is not encoded cleanly in the runtime.
- Map-format work should not begin by guessing. The old editor and authoring model need to be documented alongside the current loader behavior.

## Recommended next documentation work

1. Create a dedicated markdown spec for `spawn.txt` based on both the development guide and the current loader code.
2. Create a dedicated markdown spec for `data.txt` positional fields and object-slot semantics.
3. Document current passage behavior in code and compare it against the historical module-authoring model.
4. Build a glossary that maps historical EgoScript terminology to current runtime functions and data structures.

## Bottom line

The `.odt` files are worth preserving because they still contain design intent that the codebase does not document clearly anymore. They should guide specification work and migration planning, but they should never override observed current behavior without verification.
