# FineCraft Phase 2.5 Engineering Design Document
## Block Storage, State Mapping & Orientation Architecture

**Author:** RAMZ Studios Engineering Team  
**Target Milestone:** Phase 2.5 — Block State & Orientation System  
**Repository Baseline:** Commit `0da73c9` (`feature/phase-2.4-runtime-block-pipeline`)  

---

## 1. Executive Summary

This document presents the technical investigation and architectural design for **FineCraft Phase 2.5**. Building upon the runtime block registry and hybrid geometry pipeline established in Phase 2.4, Phase 2.5 introduces fully state-dependent geometry rendering, precise orientation transformations (for stairs, doors, and torches), and robust block ID / state capacity management.

Following our critical architecture design review, this document establishes the canonical per-voxel representation, accurate memory bandwidth calculations, state-to-block lookup designs, geometry coverage limits, shared transformation architecture (vertex positions, normals, and 8-corner AABB collision reconstruction), and backward-compatible v2-to-v3 save migration.

---

## 2. Canonical Per-Voxel Representation

### Architectural Evaluation of Alternatives
To support full block state fidelity (facing, half, shape, open/closed, hinge) across 32,366 possible Minecraft states while supporting 1,196 block definitions, we evaluated three chunk storage architectures:

- **Alternative A (uint16_t Block ID + Separate State Array):** Stores a 16-bit block type ID per voxel, requiring a secondary data structure or lookup table for block states. This introduces pointer indirection and complicates chunk serialization.
- **Alternative B (uint16_t Minecraft State ID Only):** Stores the direct Minecraft State ID (`uint16_t`) per voxel. Because every state ID maps uniquely to a parent block ID via the registry, storing the State ID implicitly provides the block type while preserving exact property fidelity.
- **Alternative C (Packed 32-bit Integer):** Packs both block ID and state ID into a 32-bit word, doubling chunk memory consumption unnecessarily.

### Recommendation & Justification
**Alternative B (`uint16_t` Minecraft State ID per voxel) is selected as the canonical per-voxel representation.**
- **Rationale:** Minecraft's internal world model is inherently state-driven. Storing state IDs (`uint16_t`, supporting up to 65,535 distinct states—comfortably housing the 32,366 catalogue states) natively preserves facing, shape, half, and hinge properties without requiring parallel chunk arrays. `Registry::getBlockForState(stateId)` instantly resolves the parent block definition in $O(1)$ time.

---

## 3. Accurate Memory Calculations

Existing chunk dimensions: $16 \times 32 \times 16 = 8,192$ voxels.

| Storage Architecture | Per-Chunk Memory | Across 314 Loaded Chunks | Incremental Memory vs `uint8_t` |
|---|---|---|---|
| **Existing `uint8_t` (Raw IDs)** | 8 KB | 2.45 MB | Baseline (0 MB) |
| **Alternative A (`uint16_t` Block ID)** | 16 KB | 4.91 MB | +2.46 MB |
| **Alternative B (`uint16_t` State ID)** | 16 KB | 4.91 MB | +2.46 MB |
| **Separate Block + State Arrays** | 32 KB | 9.81 MB | +7.36 MB |
| **Packed Representation (`uint32_t`)** | 32 KB | 9.81 MB | +7.36 MB |

### Memory Bandwidth & Cache Locality Analysis
- Storing `uint16_t` state IDs (Alternative B) results in **16 KB per chunk**. This fits cleanly within modern mobile CPU L2 cache lines (typically 256 KB – 512 KB per core), ensuring high cache hit rates during greedy meshing and physics queries.
- Contiguous linear memory layout avoids heap fragmentation and pointer chasing.

---

## 4. Geometry State Coverage Analysis

### Oak Stairs
- **Rotations:** Y-axis rotations (0°, 90°, 180°, 270°) cover straight stairs facing north, south, east, and west.
- **Inner/Outer Corners:** Corner shapes (`shape: inner_left`, `shape: outer_left`, etc.) have distinct geometric profiles that cannot be obtained through pure 90° rotation of a straight stair model. Phase 2.5 will map corner states to dedicated variant components or fall back safely to straight geometry where variant models are absent.

### Oak Doors
- **Halves & Hinges:** Represented by two vertical half-blocks: upper (`5658`) and lower (`5666`). Hinge placement (`left` vs `right`) and open state (`true`/`false`) define rotation pivot offsets and swing angles.

### Torches
- **Attachments:** Floor placement vs wall-mounted. Wall-mounted torches require translation and surface-normal rotation.

---

## 5. Shared Transformation Architecture

### Unified Transform Pipeline
Vertex positions (`glm::vec3`), surface normals (`glm::vec3`), and collision AABBs (`AABB`) share a common transformation function:
```cpp
struct TransformContext {
    int rotationYDegrees = 0; // 0, 90, 180, 270
    glm::vec3 translationOffset{0.0f};
};
```

### 8-Corner AABB Reconstruction Algorithm
To transform non-axis-aligned or rotated collision AABBs into valid Axis-Aligned Bounding Boxes:
1. Extract all **8 corner vertices** of the local box:
   $$\mathbf{c}_i \in \{ [minX, maxX] \times [minY, maxY] \times [minZ, maxZ] \}$$
2. Apply the rotation and translation transformation matrix to each corner:
   $$\mathbf{c}_i' = \mathbf{M} \mathbf{c}_i + \mathbf{t}$$
3. Reconstruct the new world-space Axis-Aligned Bounding Box by taking the minimum and maximum bounds across all 8 transformed corners:
   $$min' = (\min_i x_i', \min_i y_i', \min_i z_i')$$
   $$max' = (\max_i x_i', \max_i y_i', \max_i z_i')$$

---

## 6. Save Compatibility & Migration Strategy

- **Version Increment:** Bump save format version from `2` to `3` in `SaveManager.cpp`.
- **Migration Logic:**
  - When loading version `1` or `2` saves containing `uint8_t` block IDs, automatically promote them to default Minecraft state IDs using a deterministic migration mapping table (`legacyBlockId -> defaultStateId`).
  - Write temporary files (`world.dat.tmp`) and perform atomic renames (`std::rename`), ensuring original save files are never corrupted or overwritten prematurely.

---

## 7. Updated Testing & Acceptance Criteria

### Test Plan
1. **Capacity Tests:** Verify `uint16_t` state ID storage for state IDs $>255$ up to 32,366.
2. **Migration Tests:** Load legacy v2 saves and confirm correct state promotion.
3. **Transformation Tests:** Verify 8-corner AABB reconstruction for rotated stairs and doors.
4. **Automated Validation:** Run `tools/catalogue/test_catalogue.py` and `tools/geometry_export/tests/test_export.py`.

### Acceptance Criteria
- All 32,366 catalogue states representable in chunk storage via `uint16_t`.
- Stair, door, and torch orientations render and collide correctly.
- Legacy v2 saves migrate successfully to v3.
- All Python test suites and native builds pass without errors.
