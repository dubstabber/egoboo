# MD2 to glTF Runtime Preparation

This note records the first runtime-prep milestone for moving object models from
legacy `tris.md2` assets toward glTF/GLB.

## Runtime Shape

- `Ego::Graphics::AnimatedModel` is the format-neutral runtime model consumed by
  `ModelDescriptor`, `ObjectGraphics`, and the model draw path.
- `MD2Model` is now an MD2 loader adapter that populates `AnimatedModel`.
- `Ego::Graphics::ModelAnimationMetadata` owns the legacy action ranges,
  frame-effect parsing, walk-lip tables, and `copy.txt` healing behavior that
  used to live inside `ModelDescriptor`. `ModelDescriptor` keeps its public API
  as a facade over the metadata object plus the runtime `AnimatedModel`.
- `ModelAnimationMetadata` now has a format-neutral ingest seam. The legacy
  MD2 frame-name parse is just one producer: `initializeFromLegacyFrames`
  recovers an `AnimationMetadataInput` (per-action presence + frame ranges, plus
  `copy.txt`/`extras.egoboo` heal aliases) and feeds it to a shared
  `applyMetadata` step that builds the action map, walk-lip tables, and per-frame
  walk-lip write-backs. A non-MD2 loader (glTF via `extras.egoboo`) calls
  `initializeFromActionData(model, input)` directly, supplying resolved action
  ranges instead of forging 16-char MD2 frame-name strings. Per-frame effects
  (`framefx`) remain model data set on each `AnimatedModelFrame` by the loader,
  not carried in the input. The legacy path is behavior-preserving (a parity test
  in `ContentParsers.cpp` asserts `initializeFromActionData` reproduces
  `initializeFromLegacyFrames` exactly across the full action table, walk-lip
  tables, and per-frame `framelip`).
- The obsolete raw `id_md2.c` reader has been removed. `id_md2.h` now contains
  only packed MD2 file-layout structures shared by the current `MD2Model`
  adapter.
- Object model resolution prefers `tris.gltf`, then `tris.glb`, then `tris.md2`.
  Until a glTF parser is implemented, runtime loading explicitly falls back to
  `tris.md2` when a higher-priority future asset is present beside it.
- Object model candidate filenames and search-order diagnostics are centralized
  in `ObjectModelAsset`, so runtime and validator errors report the same
  format-forward candidate list instead of hardcoding `tris.md2`.
- Runtime loadability is centralized behind `ObjectModelLoader`: preferred asset
  discovery remains format-forward, while `canLoadObjectModelFormat`,
  `resolveLoadableObjectModelAsset`, and `loadObjectModelAsset` keep MD2 as the
  only loadable format until a real glTF/GLB loader lands.
- The render scratch-buffer seam is format-neutral (`ModelVertexBuffer` /
  `DefaultModelVertexBuffer`) and sizes itself from `AnimatedModel` draw
  commands.

## Future glTF Metadata

Converted assets should keep Egoboo-specific animation semantics self-contained
in glTF `extras.egoboo` metadata rather than requiring a sidecar file. The first
loader should preserve:

- metadata schema version
- original frame order and frame names
- frame effects equivalent to `ModelFrameEffects`
- action ranges equivalent to `ModelAction`
- copy/healing compatibility data currently read from `copy.txt`

`ModelAnimationMetadata` is the current action and frame-effect behavioral
reference until the glTF loader can prove equivalent action maps, walk-lip
tables, frame effects, bounding boxes, and vertex interpolation.

With the `initializeFromActionData` ingest seam in place, the metadata side of a
glTF loader narrows to: (1) define the `extras.egoboo` schema enumerated above,
(2) decode it into an `AnimationMetadataInput` (action ranges + heal aliases) and
per-frame `framefx`, and (3) call `initializeFromActionData`.

The env-map normal palette is also decoupled. `AnimatedModelVertex` no longer
carries an MD2 palette index; it carries `envU`, a precomputed environment-map U
coordinate (azimuth of the vertex normal, in turns). The MD2 loader derives it
from the legacy palette (`MD2Model` now owns the 162-normal table privately),
`makeEquallyLit` pins it to zero, and a glTF loader sets it from real per-vertex
normals. The global `indextoenvirox` table and `gfx_system_make_enviro()` are
gone; `AnimatedModel::getLegacyNormal` / `normalCount` are no longer public. The
change is behavior-preserving for MD2 (the per-vertex `envU` equals the retired
`indextoenvirox[normalIndex]` value bit-for-bit, since both apply the same
`atan2 * inv_two_pi` to the same legacy normal).

The remaining non-metadata MD2 coupling tracked for later passes is the
strip/fan-only draw-command primitive set (no indexed-triangle mode) and the
unconditional legacy `scaleModel(-3.5, 3.5, 3.5)` in the `ModelDescriptor` ctor,
which a format-conditional post-load branch should make MD2-only.
