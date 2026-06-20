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
