# FineCraft Minecraft Java 26.2 Block Catalogue

This is a data-only foundation for future FineCraft block work. It does not change the renderer, world generation, save format, JNI, gameplay registry, textures, or atlas.

## Source and reproducibility

The registry source is [Complexity-ML/minecraft-data-26.2](https://github.com/Complexity-ML/minecraft-data-26.2), Java Edition data at `data/pc/26.2`, pinned to commit `2a6a5fd9ebb0d964a73312d11beef596b7ec029b`. The source contained **1,196** blocks at this commit. The four source JSON files are copied unchanged into `tools/catalogue/source/`; their SHA-256 digests and byte lengths are in `source_manifest.json`.

This distribution states that it is MIT licensed and derived from PrismarineJS minecraft-data, retaining upstream history and contributor attribution. Complexity-ML states it is not affiliated with Mojang or Microsoft. The dataset notes some data may be extracted from wiki.vg and minecraft.gamepedia.com; see the source repository README for those upstream notices. Minecraft is a Mojang/Microsoft property.

The official [Minecraft Java Edition 26.2 release notes](https://www.minecraft.net/en-us/article/minecraft-java-edition-26-2) were used to describe Potent Sulfur and Sulfur Spike mechanics and verify the Cinnabar/Sulfur block families. Snapshot-specific Sulfur Spike behavior was cross-checked against the [official 26.2 Snapshot 3 notes](https://feedback.minecraft.net/hc/en-us/articles/45491765809037-Minecraft-Java-Edition-26-2-Snapshot-3). No Bedrock or modded registry data is used.

Regenerate with Python 3 and no third-party dependencies:

```powershell
python tools/catalogue/generate_catalogue.py
python tools/catalogue/validate_catalogue.py
```

Use `--source-dir` and `--output-dir` to work with another source snapshot or staging directory. Do not change the source commit without reviewing the corresponding manifest and output changes.

## Files

- `blocks_master.json`: one definition per source block type; state properties remain attached to the type and are not expanded into duplicate definitions.
- `blocks_by_category.json`: identifier references grouped by physical classification and behavior tag.
- `collision_shapes.json`: source shape boxes and per-state shape mappings. Coordinates preserve the upstream floating point values and are ordered `x_min,y_min,z_min,x_max,y_max,z_max` in block-local units. Shapes may extend beyond a unit cell.
- `block_behaviours.json`: reusable behavior tags and separate gravity, support, lighting, waterlogging, interaction, physics, multipart, block entity, and known 26.2 mechanic data.
- `catalogue_statistics.json`: registry and property counts and coverage measures.
- `catalogue_audit.json`: generation-time omissions, warnings, and unresolved field inventories.
- `catalogue_validation_report.json`: validator results, warnings, and unresolved inventories.
- `source_manifest.json`: pinned commit and hashes for copied source inputs.

## Schema and classification rules

`registry.id` and state ranges are the upstream Minecraft registry values. `identifier` is namespaced as `minecraft:<internal name>`. State `values` and `num_values` are retained; the source omits the two boolean values, so the generator expands the boolean type to explicit `[false, true]`. A block is never repeated for each state.

Physical classification is an array because structures can belong to multiple groups. `full_cube` means at least one state has a full unit collision box; `partial_geometry` means at least one mapped collision state has a nonempty, non-full collision shape. `non_solid` means at least one mapped state has an empty collision shape, except where more precise fluid or technical classifications also apply. `gravity_affected` and `multipart_structure` are independent memberships. `primary_geometry` is a convenience choice that leaves gravity as an additional membership; consumers should inspect the full array. These classifications use collision data as the physical evidence and do not describe render models.

Behavior tags are independent of physical classification. `transparent` reflects the upstream `transparent` field; it does not assert translucency. `translucent` remains null because the source does not separate transparency from translucency. Collision tags indicate any mapped state with that collision kind; a block can carry both full and partial collision tags when its states differ. Light emission and filtering remain separate exact numeric fields. Unknown values are null or explicit `unknown`/`requires_further_investigation` labels, not guessed values.

## Geometry and known gaps

The selected source provides collision shapes for every block name and state in this snapshot. Collision shapes are indexed by block and state using the `blocks` table in `blockCollisionShapes.json`; the master catalogue references those mappings. Collision boxes are not render geometry or selection/raycast geometry. The source documentation says neighbor-dependent behavior is not encoded; it is left unresolved here. Render models, selection shapes, and a block entity registry are not supplied by the four required datasets. Flammability and a distinct translucency field are also absent. The audit enumerates these gaps for every block where that data is unavailable, and lists blocks that lack an exact-name item registry entry. Interactions and support/fall rules are only partially described.

The block entity type is not inferred from a name. As a result, type coverage remains unresolved pending a matching 26.2 source. Beds are handled as head/foot multipart block states; 26.2 removed the bed block entity, so no obsolete bed entity is recorded.

## 26.2 mechanics recorded

The catalogue includes all Cinnabar and Sulfur block set entries in the pinned registry, plus Potent Sulfur and Sulfur Spike. Potent Sulfur's states, bubbles, noxious gas radius, geyser configurations, entity impulse, and sculk game events are summarized from official 26.2 notes. Sulfur Spike thickness/orientation/waterlogged properties, tip merging, support/fall behavior, and falling damage are summarized from official snapshot/release notes. These summaries are authored notes; the registry and geometry values remain imported source data.

## Future integration

Keep this catalogue out of the live gameplay registry until a later, explicit integration phase defines a stable FineCraft block-ID mapping. Minecraft registry IDs are not FineCraft save IDs. Consume `registry.identifier` and state schemas as the external identity; resolve collisions through the state mapping, and keep rendering and selection geometry as independent future inputs. Do not use a collision shape as a render mesh or raycast shape. This phase introduces no textures, atlas, renderer code, or world-generation changes.
