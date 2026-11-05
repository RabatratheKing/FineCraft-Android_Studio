# FineCraft Phase 2.5 Engineering Design Document
## Block Storage, State Mapping & Orientation Architecture

**Author:** RAMZ Studios Engineering Team  
**Target Milestone:** Phase 2.5 — Block State & Orientation System  
**Repository Baseline:** Commit `0da73c9` (`feature/phase-2.4-runtime-block-pipeline`)  

---

## 1. Executive Summary

This document presents the finalized technical investigation and architectural design for **FineCraft Phase 2.5**. Building upon the runtime block registry and hybrid geometry pipeline established in Phase 2.4, Phase 2.5 introduces fully state-dependent geometry rendering, precise orientation transformations (for stairs, doors, and torches), robust block state capacity management (`uint16_t` state IDs), inverse-transpose normal transformations, 8-corner AABB collision reconstruction, and backward-compatible v2-to-v3 save migration.

---

## 2. Current Architecture & Source References

- **Runtime Registry:** [`Registry.h`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/gameplay/Registry.h) / [`Registry.cpp`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/gameplay/Registry.cpp) — Parses `blocks_master.json` and `collision_shapes.json`.
- **Geometry Pipeline:** [`GeometryAssetLoader.h`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/geometry/GeometryAssetLoader.h) / [`GeometryAssetLoader.cpp`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/geometry/GeometryAssetLoader.cpp) — Manages `LoadedPrototype` assets and 10-float vertex conversions.
- **World & Chunk Generation:** [`World.h`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/world/World.h) / [`World.cpp`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/world/World.cpp) — Hybrid greedy mesher and prototype geometry inserter.
- **Physics & Collision:** [`Physics.h`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/player/Physics.h) / [`Physics.cpp`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/player/Physics.cpp) — AABB collision detection querying `Registry`.
- **Persistence:** [`SaveManager.h`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/world/SaveManager.h) / [`SaveManager.cpp`](file:///C:/Users/rsroa/Documents/Projects/Finecraft/FineCraft-Android_Studio-feature-phase-2.3-geometry-export/app/src/main/cpp/world/SaveManager.cpp) — Binary save format (`.dat` version 2).

---

## 3. Catalogue State ID Analysis & `uint16_t` Proof

### Empirical Dataset Inspection
Inspection of `collision_shapes.json` and `blocks_master.json` yields the following exact statistics across the official Minecraft Java 26.2 catalogue dataset:
- **Minimum State ID:** `0`
- **Maximum State ID:** `32,365`
- **Total Mapped States:** `32,366`
- **Unique State Count:** `32,366`
- **Duplicate Count:** `0`
- **Missing Ranges:** `0` (Contiguous range from `0` to `32,365`).

### Mathematical Proof of `uint16_t` Sufficiency
- A standard unsigned 16-bit integer (`uint16_t`) supports a numerical range of $[0, 65535]$.
- The catalogue maximum state ID is **$32,365$**.
- Inequality check: $32,365 \le 65,535$.
- **Conclusion:** `uint16_t` is mathematically and empirically sufficient to represent every single one of the 32,366 unique Minecraft states, with an additional headroom of 33,169 values reserved for future expansions or custom modded states.

---

## 4. Accurate Memory Analysis (KiB/MiB & Cache Hierarchy)

Existing chunk dimensions: $16 \times 32 \times 16 = 8,192$ voxels.

| Storage Architecture | Per-Chunk Memory | Across 314 Loaded Chunks | Incremental Memory vs `uint8_t` |
|---|---|---|---|
| **Existing `uint8_t` (Raw IDs)** | $8\text{ KiB}$ ($8,192\text{ bytes}$) | $2,512\text{ KiB}$ ($\approx 2.45\text{ MiB}$) | Baseline ($0\text{ MiB}$) |
| **`uint16_t` State ID (Selected)** | $16\text{ KiB}$ ($16,384\text{ bytes}$) | $5,024\text{ KiB}$ ($\approx 4.91\text{ MiB}$) | $+2.46\text{ MiB}$ |
| **Separate Block + State Arrays** | $32\text{ KiB}$ ($32,768\text{ bytes}$) | $10,048\text{ KiB}$ ($\approx 9.81\text{ MiB}$) | $+7.36\text{ MiB}$ |

### Cache Capacity vs. Cache-Line Size
- **Cache-Line Size:** Modern mobile and desktop CPU cores utilize a standard **64-byte cache-line size**.
- **Cache Capacity:** A single $16\text{ KiB}$ chunk data array fits extremely well within modern CPU L2 cache capacities (typically $256\text{ KiB} - 512\text{ KiB}$ per core) and shared L3 cache ($2\text{ MiB} - 8\text{ MiB}$).
- **Memory Bandwidth & Locality:** Sequential array iteration during greedy meshing and physics queries ensures high spatial locality, enabling hardware prefetchers to stream chunk data with near-zero stalls.

---

## 5. General Transformation Architecture

### Arbitrary Transformations, Pivots & Translations
Phase 2.5 replaces limited Y-axis rotation stubs with a general transformation pipeline:
$$\mathbf{v}' = \mathbf{R} (\mathbf{v} - \mathbf{p}) + \mathbf{p} + \mathbf{t}$$
Where $\mathbf{v}$ is the vertex position, $\mathbf{p}$ is the configurable block-local pivot (defaulting to block center $[0.0, 0.0, 0.0]$ or cell origin), $\mathbf{R}$ is the 3D rotation matrix (supporting arbitrary roll, pitch, and yaw), and $\mathbf{t}$ is the translation vector.

### Inverse-Transpose Normal Transformation
To ensure lighting remains physically correct under non-uniform scaling or general affine transformations, normals are transformed using the inverse-transpose of the upper-left $3 \times 3$ model matrix $\mathbf{M}$:
$$\mathbf{n}' = \left( (\mathbf{M}^{-1})^T \mathbf{n} \right)_{\text{normalized}}$$
For pure rotations, $\mathbf{M}$ is orthogonal, simplifying to $\mathbf{n}' = \mathbf{R} \mathbf{n}$.

### Eight-Corner Collision AABB Reconstruction Algorithm
To transform rotated or translated collision bounding boxes into valid world-space Axis-Aligned Bounding Boxes:
1. Extract all **8 corner vertices** $\mathbf{c}_i$ of the local AABB $[minX, minY, minZ, maxX, maxY, maxZ]$:
   $$\mathbf{c}_i \in \{ \{minX, maxX\} \times \{minY, maxY\} \times \{minZ, maxZ\} \}$$
2. Transform each corner:
   $$\mathbf{c}_i' = \mathbf{R} (\mathbf{c}_i - \mathbf{p}) + \mathbf{p} + \mathbf{t}$$
3. Reconstruct the new world-space AABB by computing extreme bounds across all 8 transformed corners:
   $$min' = (\min_i x_i', \min_i y_i', \min_i z_i') + \mathbf{chunkWorldPos}$$
   $$max' = (\max_i x_i', \max_i y_i', \max_i z_i') + \mathbf{chunkWorldPos}$$

---

## 6. SaveManager.cpp Version 2 Serialization Format & Version 3 Migration

### Version 2 Binary Format (`world.dat`)
1. **Header:** Magic bytes `['F','I','N','C']` (4 bytes), Version `2` (`uint32_t`, 4 bytes).
2. **Global State:** Seed (`int`, 4 bytes), Player Position (`float x, y, z`, 12 bytes), Camera Look (`float x, y`, 8 bytes), Time of Day (`float`, 4 bytes).
3. **Inventory & Crafting:** 36 inventory slots ($\mu16\_t itemId, \mu16\_t count$), Hotbar slot (`int`, 4 bytes), 4 crafting slots.
4. **Chunks:** Chunk count (`uint32_t`), followed by each chunk (`int cx, int cz`, then $8,192$ bytes of raw `uint8_t` voxel data).

### Version 3 Migration Strategy (`SaveManager.cpp`)
- **Detection:** When `LoadGame()` reads version `1` or `2`, it triggers the migration path.
- **Promotion:** Each legacy `uint8_t` voxel value `b` is promoted to its authoritative Minecraft state ID using a deterministic fallback table (`Registry::getDefaultStateIdForLegacyId(b)`).
- **Serialization:** Version 3 writes version `3` header and persists chunk voxel arrays as `uint16_t` state IDs.
- **Safety Guarantee:** Writes occur via a temporary file (`world.dat.tmp`) followed by atomic `std::rename`. Original save files are never overwritten until validation and migration succeed completely.

---

## 7. Registry Lookup Structure, Invalid-State Handling, Air & Custom Blocks

- **Lookup Structure:**
  - `std::unordered_map<int32_t, BlockStateDefinition> stateRegistry;` ($O(1)$ state ID lookup).
  - `std::unordered_map<uint16_t, BlockDefinition> blockRegistry;` ($O(1)$ base block ID lookup).
  - `std::unordered_map<std::string, uint16_t> identifierToId;` ($O(1)$ string identifier lookup).
- **Invalid-State Handling:** Any out-of-bounds or unrecognized state ID safely defaults to Air (State ID `0`, Block ID `0`) with a logged warning.
- **Air Representation:** State ID `0` (`minecraft:air`), non-solid, non-renderable, empty collision boxes.
- **Custom Block Support:** Supported natively by assigning unique namespaced identifiers and numeric IDs in `blocks_master.json`.

---

## 8. Measurable Acceptance Criteria (Phase 2.5A–2.5D)

- **Phase 2.5A (Storage & Migration):**
  - Chunk voxel arrays successfully allocate and store `uint16_t` state IDs.
  - Legacy v2 saves migrate successfully to v3 without data loss.
- **Phase 2.5B (Registry & State Resolution):**
  - Runtime registry successfully parses all 32,366 states and resolves block definitions in $O(1)$ time.
- **Phase 2.5C (Transformation Pipeline):**
  - Arbitrary block rotations, inverse-transpose normal transformations, and 8-corner AABB collision reconstructions render and collide correctly.
- **Phase 2.5D (Verification):**
  - All Python test suites (`test_catalogue.py`, `test_export.py`) pass 100%.
  - Native Android builds (`:app:assembleDebug`) compile successfully for both `arm64-v8a` and `x86_64`.
