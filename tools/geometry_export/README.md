# FineCraft geometry export, Phase 3

## Format decision

The prototype uses a **hybrid JSON plus binary format**. The repository already
uses Nlohmann JSON in the Android C++ target (`app/src/main/cpp/CMakeLists.txt`),
and its catalogue is JSON, while the current chunk renderer submits a packed
float stream through `glDrawArrays`. A small custom JSON parser for every vertex
would inflate asset size and parse time. GLB would provide a standard geometry
container, but the repository has no glTF dependency or mesh loader; collision
boxes, source state IDs and catalogue provenance would still need a project
convention and sidecar. The hybrid format keeps inspectable state/collision
metadata in JSON and uses compact little-endian float32 vertices plus uint32
indices for GPU-friendly geometry. The proof of concept does not add a glTF
dependency or alter the renderer.

| Concern | glTF/GLB | JSON-only | Chosen hybrid |
|---|---|---|---|
| Existing C++ compatibility | No loader in repository | Nlohmann JSON is already fetched | Reuses JSON dependency and adds an isolated loader |
| Android runtime cost | Efficient binary, but loader dependency needed | Largest parse and memory cost for arrays | Binary mesh data, small metadata parse |
| Collision and state metadata | Extension or sidecar needed | Straightforward | Straightforward JSON alongside binary |
| Debugging | Standard tools, custom data less visible | Easiest | Metadata is readable; geometry has a small documented header |
| Extensibility | Broad ecosystem | Simple but bulky | Schema can grow without changing the vertex layout |
| Implementation for this repository | New dependency and integration | Easy initial script | One exporter and a focused parser; no renderer rewrite |

## Asset layout

`app/src/main/assets/geometry/`

- `prototypes/*.json`: readable block/state/provenance and buffer descriptors.
- `prototypes/*.geom`: binary geometry for the matching JSON asset.
- `schemas/prototype.schema.json`: metadata schema version 1.
- `reports/export_report.json`: output sizes, checksums, counts and source checks.
- `reports/blender_roundtrip.json`: temporary-scene import comparison.

Binary version 1 begins with 12 bytes: ASCII `FCGB`, little-endian uint16
version, uint16 header size, then uint32 component count. Each JSON component
references an interleaved stream of position.xyz and normal.xyz float32 values
(24 bytes per vertex), followed by uint32 triangle indices. Offsets are absolute
from the beginning of the binary file. Positions are prototype-local Minecraft
coordinates: +X east, +Y up, +Z south. No laboratory translation is serialized.
No UVs or textures are emitted. Existing neutral material names and diffuse
colors are retained per component when present.

Collision bounds remain separate JSON records, grouped by state ID and source
shape ID. They are reconstructed from connected collision-mesh islands rather
than inferred from rendered bounds, then compared to `FC_Catalogue_Payload`.

## Run the exporter

From Blender with the Phase 2 file open:

```python
import sys
sys.path.insert(0, r"<repo>/tools/geometry_export")
import export_blender
report = export_blender.export_scene(
    "FineCraft_GeometryLab",
    r"<repo>/app/src/main/assets/geometry/prototypes",
    ["minecraft:stone", "minecraft:oak_stairs", "minecraft:torch", "minecraft:oak_door"],
)
```

Or run background Blender with `--python export_blender.py -- --output-dir ...`
and optionally `--blocks id,id`. Omitting `--blocks` exports every logical
prototype identified by `fc_role`, block ID and prototype metadata.

The implementation uses evaluated mesh copies for triangulation. It does not
modify the Phase 2 scene or mesh datablocks. Multipart anchor selection prefers
the lower half or foot state and otherwise uses the lowest catalogue cell.

## C++ integration boundary

There is no reusable vertex/index mesh type or asset loading interface in the
current renderer. `World.cpp` currently creates ten-float vertices for chunk
faces and `Renderer.cpp` uploads them with `glDrawArrays`. The new
`GeometryAssetLoader` is a CPU-only stream parser that returns GLM-independent
positions, normals, indices and collision boxes. Android callers can pass
`AAsset`-backed streams; selecting meshes, texture atlases and calling OpenGL
remain future engine integration. The parser is compiled into the existing
native library but is not called by gameplay or the renderer.

## Validation

Run Python checks after exporting:

```powershell
python -m unittest discover -s tools/geometry_export/tests -v
```

Run the Blender round trip in the same connected Blender process (or a fresh
process with the same Phase 2 `.blend` loaded):

```python
import sys
sys.path.insert(0, r"<repo>/tools/geometry_export")
import roundtrip_blender
report = roundtrip_blender.run_roundtrip(
    r"<repo>/app/src/main/assets/geometry/prototypes",
    r"<repo>/app/src/main/assets/geometry/reports/blender_roundtrip.json",
)
```

The documented tolerance is `1e-6` block units for positions and bounds, and
`1e-5` for unit normals. The round-trip importer creates a separate
`FineCraft_GeometryExport_RoundTrip` scene in memory and never changes or saves
the original scene.
