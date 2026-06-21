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
  glTF/GLB assets are now loadable. A preferred glTF/GLB asset is authoritative:
  parse/load failure reports that asset instead of silently falling back to a
  lower-priority `tris.md2`.
- Object model candidate filenames and search-order diagnostics are centralized
  in `ObjectModelAsset`, so runtime and validator errors report the same
  format-forward candidate list instead of hardcoding `tris.md2`.
- Runtime loadability is centralized behind `ObjectModelLoader`: preferred asset
  discovery remains format-forward, `canLoadObjectModelFormat` returns true for
  MD2/glTF/GLB, and `loadObjectModel` returns both the `AnimatedModel` and any
  format-native `AnimationMetadataInput`.
- `GltfModel` uses vendored `cgltf` v1.15 (`external/cgltf`, MIT,
  single-header C99) through VFS-backed file callbacks, so object assets keep
  loading through Egoboo mount points for both `.gltf` external buffers and
  `.glb` binary chunks.
- `GltfModel.cpp` and `GltfModel_metadata.cpp` are registered in
  `egolib-foundation-base`; their symbol closure stays at the foundation
  model/log/VFS/metadata layer plus C/C++ runtime symbols.
- The render scratch-buffer seam is format-neutral (`ModelVertexBuffer` /
  `DefaultModelVertexBuffer`) and sizes itself from `AnimatedModel` draw
  commands.
- `AnimatedModel` draw commands now support triangle strips, triangle fans, and
  indexed triangle streams. MD2 keeps its legacy strip/fan command order through
  an explicit prepend path; the glTF/GLB loader appends native triangle commands
  without forging MD2 GL-command strips.

## glTF/GLB Loader v1

The first loader intentionally accepts a narrow static-mesh subset:

- triangle primitives only, indexed or unindexed
- `POSITION` as `VEC3/FLOAT`
- optional `NORMAL` as `VEC3/FLOAT` (missing normals become zero normals)
- optional `TEXCOORD_0` as `VEC2/FLOAT` (missing UVs become zero UVs)
- optional scalar indices as unsigned 8/16/32-bit values
- one glTF mesh per Egoboo animation frame; every frame must resolve to the same
  vertex count

The loader rejects required extensions, skins, morph targets, Draco primitives,
meshopt-compressed buffer views, sparse accessors, non-triangle primitives,
missing meshes, and non-identity transforms on mesh nodes. This keeps the first
runtime path deterministic and equivalent to the existing frame-table model
instead of trying to map arbitrary glTF scene semantics.

Converted assets keep Egoboo-specific animation semantics self-contained in
top-level glTF `extras.egoboo` metadata:

```json
{
  "extras": {
    "egoboo": {
      "version": 1,
      "frames": [
        { "name": "DA0", "mesh": 0, "framefx": 256 }
      ],
      "actions": {
        "DA": [0, 0]
      },
      "healAliases": [
        ["DA", "WA"]
      ]
    }
  }
}
```

`frames[].name` and `frames[].framefx` populate `AnimatedModelFrame` directly.
`actions` and `healAliases` decode into `AnimationMetadataInput`, then
`ModelDescriptor` calls `initializeFromActionData` for glTF/GLB. If a valid
glTF/GLB has no top-level `extras` or no `extras.egoboo`, the loader installs a
single-frame fallback named `DA` with `ACTION_DA` covering frame 0. That fallback
is only for minimal/static smoke assets; converted production assets should carry
explicit metadata.

`ModelAnimationMetadata` remains the action and frame-effect behavioral
reference: the loader must preserve action maps, walk-lip tables, frame effects,
bounding boxes, vertex interpolation compatibility, and copy/healing behavior.

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

The `ModelDescriptor` post-load step is format-conditional: the ctor switches on
the resolved `ObjectModelFormat`, and the MD2-era
`scaleModel(-3.5, 3.5, 3.5)` + `initializeFromLegacyFrames` run only on the
`Md2` arm. The `Gltf`/`Glb` arm uses the loader-provided metadata directly via
`initializeFromActionData` and does not apply the MD2 scale normalization.

The former non-metadata MD2 coupling in the strip/fan-only draw-command
primitive set is cleared. Remaining work is content conversion plus broadening
the glTF subset only where real converted assets require it.
