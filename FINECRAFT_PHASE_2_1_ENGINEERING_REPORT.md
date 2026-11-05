# FineCraft Phase 2.1: Critical Hotfix Engineering Report

**Project Name:** FineCraft  
**Package Name:** `com.ramzstudios.finecraft`  
**Repository:** https://github.com/RabatratheKing/FineCraft-Android_Studio  
**Branch:** `fix/phase-2.1-critical`  
**Baseline Commit:** `d2d93bcbd57e6675fe3ae352575e1c7498a470e3`  
**Report Date:** May 2024  
**Target Environment:** Android Emulator (`emulator-5554`, Medium Phone API 37.0)

---

## 1. Executive Summary

Phase 2.1 successfully resolved all critical defect vectors identified following Phase 2:
1. **Inventory Mutex Deadlocks (P0)**: Eliminated recursive non-recursive mutex locks by introducing unlocked helper methods (`addItemUnlocked`, `clearCraftingGridUnlocked`, `updateCraftingOutputUnlocked`) and making item insertion strictly transactional.
2. **Block Registry Fallback (P0)**: Implemented guaranteed fallback registration for all built-in block IDs 1–15 in `Registry.init()` so that missing or malformed `blocks.json` entries can never result in unknown block definitions.
3. **Continue Inventory Reset (P1)**: Decoupled engine initialization from surface creation (`nativeSurfaceDestroyed` / `nativeSurfaceCreated`), ensuring that surface recreation and Continue (`nativeLoadGame`) never overwrite saved inventory or player state with starter items.
4. **Robust Save File Error Handling**: Upgraded `LoadGame()` with automatic fallback to `NewGame()` on corruption or truncation, preventing ANRs or unplayable states.

---

## 2. Issue-by-Issue Results & Root Cause Analysis

### A. P0: Fix Inventory Mutex Deadlocks
* **Original Defect:** 
  - *Deadlock A:* `SaveGame()` locked `inventoryMutex`, then called `clearCraftingGrid()`, which attempted to acquire the same non-recursive mutex.
  - *Deadlock B:* `takeCraftingOutput()` locked `inventoryMutex`, then called `addItem()`, which attempted to acquire the same mutex.
* **Root Cause:** Re-entrant calls to mutex-locked functions on the same thread without recursive locks.
* **Files Changed:** 
  - [`app/src/main/cpp/gameplay/Inventory.h`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/gameplay/Inventory.h)
  - [`app/src/main/cpp/gameplay/Inventory.cpp`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/gameplay/Inventory.cpp)
  - [`app/src/main/cpp/world/SaveManager.cpp`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/world/SaveManager.cpp)
* **Implemented Solution:**
  - Introduced internal unlocked helpers (`addItemUnlocked`, `clearCraftingGridUnlocked`, `updateCraftingOutputUnlocked`).
  - Public entry points acquire `std::lock_guard<std::mutex> lock(inventoryMutex)` exactly once; internal delegation calls unlocked routines.
  - Made `addItemUnlocked` transactional: simulates insertion across inventory slots on a temporary copy before committing, returning `false` without partial mutation if space is insufficient.
  - Refactored `SaveGame()` to snapshot inventory and crafting slots under lock without mutating live state or prematurely clearing crafting grids.

### B. P0: Restore the Block Registry
* **Original Defect:** Starter block IDs could fall through to unknown registry definitions if `blocks.json` entries were missing or failed to parse.
* **Root Cause:** Sole reliance on external JSON configuration without built-in safety fallbacks for core preset block IDs 1–15.
* **Files Changed:**
  - [`app/src/main/cpp/gameplay/Registry.cpp`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/gameplay/Registry.cpp)
* **Implemented Solution:**
  - Added a guaranteed fallback registration pass in `Registry.init()` verifying that all preset block IDs (1–15: grass, dirt, stone, wood, leaves, water, planks, sand, glass, bricks, gravel, coal ore, iron ore, diamond ore, cobblestone) are registered with valid textures and properties even if `blocks.json` lacks them.

### C. P1: Fix Continue Inventory Reset
* **Original Defect:** `nativeSurfaceCreated()` previously called `initInventory()` whenever `!isRunning`, overwriting loaded save data whenever the surface was created or recreated.
* **Root Cause:** Conflating engine thread startup with inventory initialization.
* **Files Changed:**
  - [`app/src/main/cpp/platform/android/JNI.cpp`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/platform/android/JNI.cpp)
  - [`app/src/main/cpp/world/SaveManager.cpp`](file:///C:/Users/rsroa/OneDrive%20-%20AFG%20Erding/Dokumente/Projects/FineCraft-by-RAMZ-Studios-Inc.-main/app/src/main/cpp/world/SaveManager.cpp)
* **Implemented Solution:**
  - Removed `initInventory()` from `nativeSurfaceCreated()`. `initInventory()` is now strictly invoked on `NewGame()`.
  - Added robust error handling in `LoadGame()` with automatic fallback to `NewGame()` if a save file is missing or corrupted.

---

## 3. Build Results & Verification

* **Gradle Build:** `./gradlew :app:assembleDebug` executed successfully.
* **ABIs Built:** `arm64-v8a` and `x86_64` compiled cleanly via CMake 3.22 and NDK 27.2.12479018 with zero errors or warnings.
* **Emulator Testing:**
  - Tested cold app startup $\to$ Main Menu.
  - Tested clicking "CONTINUE" $\to$ Loaded saved world state instantly without ANR or deadlock.
  - Tested inventory interaction, crafting, saving, and returning to menu.

---

## 4. Remaining Known Issues / Unexecuted Tests
* **ThreadSanitizer (TSan):** Dynamic TSan runtime binary validation on physical ARM64 device was not executed (emulator x86_64 used).
* **Automated C++ Unit Tests:** Unit test harness for C++ inventory transactions was executed via app integration testing rather than standalone GoogleTest executables.
