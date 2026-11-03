# FineCraft Minecraft Java 26.2 Block Catalogue

This is a data-only foundation for future FineCraft block work. It does not change the renderer, world generation, save format, JNI, gameplay registry, textures, or atlas.

## Source and reproducibility

The base registry source is [Complexity-ML/minecraft-data-26.2](https://github.com/Complexity-ML/minecraft-data-26.2), Java Edition data at `data/pc/26.2`, pinned to commit `2a6a5fd9ebb0d964a73312d11beef596b7ec029b`. It contains **1,196** blocks. The four source JSON files are copied unchanged into `tools/catalogue/source/`; SHA-256 digests and byte lengths are in `source_manifest.json`.

Additional authority for this QA pass is Mojang's Java 26.2 server report output (`blocks.json`, `registries.json`) and official client blockstate/model assets. The server jar SHA-1 is `823e2250d24b3ddac457a60c92a6a941943fcd6a`; the client jar SHA-1 is `2dc72797acbc1b63fc16a11c4ac393605f453754`. The compact model index is reproducibly extracted by `tools/catalogue/extract_mojang_client_geometry.py` from the official client jar. Hashes and provenance are recorded in `source_manifest.json`. No client textures are copied.

This distribution states that it is MIT licensed and derived from PrismarineJS minecraft-data, retaining upstream history and contributor attribution. Complexity-ML states it is not affiliated with Mojang or Microsoft. The dataset notes some data may be extracted from wiki.vg and minecraft.gamepedia.com; see the source repository README for those upstream notices. Minecraft is a Mojang/Microsoft property.

The official [Minecraft Java Edition 26.2 release notes](https://www.minecraft.net/en-us/article/minecraft-java-edition-26-2) were used to describe Potent Sulfur and Sulfur Spike mechanics and verify the Cinnabar/Sulfur block families. Snapshot-specific Sulfur Spike behavior was cross-checked against the [official 26.2 Snapshot 3 notes](https://feedback.minecraft.net/hc/en-us/articles/45491765809037-Minecraft-Java-Edition-26-2-Snapshot-3). No Bedrock or modded registry data is used.

Regenerate with Python 3 and no third-party dependencies:

```powershell
python tools/catalogue/extract_mojang_client_geometry.py <official-26.2-client.jar>
python tools/catalogue/generate_catalogue.py
python tools/catalogue/validate_catalogue.py
python -m unittest discover -s tools/catalogue -p 'test_*.py'
```

Use `--source-dir` and `--output-dir` to work with another source snapshot or staging directory. Do not change the source commit without reviewing the corresponding manifest and output changes.

## Files

- `blocks_master.json`: one definition per source block type; state properties remain attached to the type and are not expanded into duplicate definitions.
- `blocks_by_category.json`: identifier references grouped by physical classification and behavior tag.
- `collision_shapes.json`: source shape boxes and per-state shape mappings. Coordinates preserve the upstream floating point values and are ordered `x_min,y_min,z_min,x_max,y_max,z_max` in block-local units. Shapes may extend beyond a unit cell.
- `block_behaviours.json`: reusable behavior tags and separate gravity, support, lighting, waterlogging, interaction, physics, multipart, block entity, and known 26.2 mechanic data.
- `block_item_relationships.json`: exact-name item matches and unresolved candidates. Drop candidates are never treated as verified placement relationships. The official item reports contain a `block_state` component for 12 item definitions; all 12 already have exact-name block entries and none resolve the 154 missing exact-name cases.
- `catalogue_statistics.json`: registry and property counts and coverage measures.
- `catalogue_audit.json`: generation-time omissions, warnings, and unresolved field inventories.
- `catalogue_validation_report.json`: validator results, warnings, and unresolved inventories.
- `source_manifest.json`: pinned commit and hashes for copied source inputs.

## Schema and classification rules

Schema version 2 separates dimensions. `classification.primary_geometry` contains exactly one value: `full_cube`, `partial_geometry`, `multipart_structure`, `non_solid`, or `fluid`. Geometry is derived from official client blockstate model element bounds, with the multi-part block structures and liquids assigned their dedicated classes. `technical`, `gravity_affected`, light emission, support, and other behaviors are not geometry classes. Behavior tags live independently in `block_behaviours.json`. For example, sand is `full_cube` plus `gravity_affected`; glowstone is `full_cube` plus `light_emitter`; torch is `partial_geometry`, has empty collision, emits light, and has a support-dependent tag.

Model element bounds support this coarse geometry classification; they do not alone prove pixel output, render layer, selection shape, or runtime neighbor behavior. A full cube means a referenced model variant includes a full 16×16×16 model element; partial geometry means model elements do not satisfy that test. `fluid` follows the official server block definition. The model index has complete blockstate and model-reference coverage for the catalogue.

Behavior tags are independent of geometry. `transparent` reflects the upstream block flag and is not an assertion about alpha or renderer translucency. `translucent` remains unresolved. Collision tags indicate any mapped state with that collision kind; a block may have multiple tags as states differ. Light emission and filtering remain exact numeric fields. Unknown values are explicit and are never treated as false.

## Geometry and known gaps

All 1,196 block names and all 32,366 states have collision mappings; all 6,114 shape IDs resolve. The validator checked 59,825 boxes for finite numeric bounds and non-inverted axes. There are 326 unique exact box-list geometries among 6,114 shape IDs; repeated IDs are preserved from source. Collision boxes are not selection/raycast shapes.

Official server reports match all 32,366 state IDs and 2,060 state properties. The report exposes the block-entity type registry and block kinds: all 49 entity types map to 186 blocks. The other 1,010 blocks have no entity type under the official mapping, including all 16 beds. The official item data has 12 `block_state` component records; each corresponds to an exact-name block, and none supplies a mapping for the 154 missing exact-name block entries. Of those 154, 124 have one or more drop candidates (still unverified as placeable block items), 12 are technical/state-only candidates, and 18 have no source mapping. The available reports cannot prove which are obtainable or unobtainable as inventory items; the item report labels every candidate and does not force a relationship.

Exact interaction details, placement/support rules, neighbor-update behavior, selection geometry, render layer/translucency and flammability are not expressed by the available registry/model reports; the audit lists each affected block and the required evidence. Their absence is not treated as proof that a behavior/property is absent.

Block entity types are mapped by matching the official block report kind to the official entity-type registry, with explicit kind aliases recorded in the generator. Every registered entity type maps to at least one block. Beds have no block entity in this version and retain head/foot states.

## 26.2 mechanics recorded

The catalogue includes all Cinnabar and Sulfur block set entries in the pinned registry, plus Potent Sulfur and Sulfur Spike. Potent Sulfur's states, bubbles, noxious gas radius, geyser configurations, entity impulse, and sculk game events are summarized from official 26.2 notes. Sulfur Spike thickness/orientation/waterlogged properties, tip merging, support/fall behavior, and falling damage are summarized from official snapshot/release notes. These summaries are authored notes; the registry and geometry values remain imported source data.

## Future integration

Keep this catalogue out of the live gameplay registry until a later, explicit integration phase defines a stable FineCraft block-ID mapping. Minecraft registry IDs are not FineCraft save IDs. Consume `registry.identifier` and state schemas as the external identity; resolve collisions through the state mapping, and keep rendering and selection geometry as independent future inputs. Do not use a collision shape as a render mesh or raycast shape. This phase introduces no textures, atlas, renderer code, or world-generation changes.
