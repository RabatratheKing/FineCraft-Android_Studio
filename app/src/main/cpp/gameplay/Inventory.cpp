#include "Inventory.h"
#include "../core/Globals.h"
#include <unordered_map>
#include <algorithm>

InventorySlot inventory[INVENTORY_SIZE];
int selectedHotbarSlot = 0;

void initInventory() {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    Registry::init();
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        inventory[i].itemId = 0;
        inventory[i].count = 0;
    }
    inventoryVersion++;
}

const ItemDefinition& getBlockMetadata(uint16_t id) {
    return Registry::getItem(id);
}

bool addItemUnlocked(uint16_t itemId, uint16_t count) {
    if (itemId == 0 || count == 0) return false;
    uint16_t maxStack = getBlockMetadata(itemId).maxStack;
    if (maxStack == 0) return false;

    // Transactional check: simulate adding across inventory slots using temp copy
    InventorySlot tempInv[INVENTORY_SIZE];
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        tempInv[i] = inventory[i];
    }

    uint16_t remaining = count;
    // Pass 1: existing partial stacks
    for (int i = 0; i < INVENTORY_SIZE && remaining > 0; i++) {
        if (tempInv[i].itemId == itemId && tempInv[i].count < maxStack) {
            uint16_t space = maxStack - tempInv[i].count;
            uint16_t take = std::min(remaining, space);
            tempInv[i].count += take;
            remaining -= take;
        }
    }
    // Pass 2: empty slots
    for (int i = 0; i < INVENTORY_SIZE && remaining > 0; i++) {
        if (tempInv[i].itemId == 0 || tempInv[i].count == 0) {
            uint16_t take = std::min(remaining, maxStack);
            tempInv[i].itemId = itemId;
            tempInv[i].count = take;
            remaining -= take;
        }
    }

    if (remaining == 0) {
        // Commit changes
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            inventory[i] = tempInv[i];
        }
        inventoryVersion++;
        return true;
    }
    return false; // Transaction failed: inventory not modified
}

bool addItem(uint16_t itemId, uint16_t count) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    return addItemUnlocked(itemId, count);
}

bool consumeItem(int slotIndex, uint16_t count) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    if (slotIndex < 0 || slotIndex >= INVENTORY_SIZE) return false;
    if (inventory[slotIndex].count >= count) {
        inventory[slotIndex].count -= count;
        if (inventory[slotIndex].count == 0) {
            inventory[slotIndex].itemId = 0;
        }
        inventoryVersion++;
        return true;
    }
    return false;
}

void swapSlots(int slotA, int slotB) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    if (slotA < 0 || slotA >= INVENTORY_SIZE || slotB < 0 || slotB >= INVENTORY_SIZE) return;
    
    if (inventory[slotA].itemId == inventory[slotB].itemId && inventory[slotA].itemId != 0) {
        uint16_t maxStack = getBlockMetadata(inventory[slotA].itemId).maxStack;
        if (inventory[slotB].count < maxStack) {
            uint16_t space = maxStack - inventory[slotB].count;
            if (inventory[slotA].count <= space) {
                inventory[slotB].count += inventory[slotA].count;
                inventory[slotA].itemId = 0;
                inventory[slotA].count = 0;
            } else {
                inventory[slotB].count += space;
                inventory[slotA].count -= space;
            }
            inventoryVersion++;
            return;
        }
    }
    
    InventorySlot temp = inventory[slotA];
    inventory[slotA] = inventory[slotB];
    inventory[slotB] = temp;
    inventoryVersion++;
}

InventorySlot craftingGrid[4] = {{0,0}, {0,0}, {0,0}, {0,0}};
InventorySlot craftingOutput = {0,0};

void updateCraftingOutputUnlocked() {
    int woodCount = 0;
    int planksCount = 0;
    int otherCount = 0;
    for(int i=0; i<4; i++) {
        if(craftingGrid[i].itemId == 4) woodCount += craftingGrid[i].count;
        else if(craftingGrid[i].itemId == 7) planksCount += craftingGrid[i].count;
        else if(craftingGrid[i].itemId != 0) otherCount++;
    }
    
    craftingOutput = {0, 0};
    
    // Wood -> 4 Planks
    if(otherCount == 0 && planksCount == 0 && woodCount >= 1) {
        craftingOutput = {7, 4};
    }
    // 2 Planks -> 4 Sticks
    else if(otherCount == 0 && woodCount == 0 && planksCount >= 2) {
        craftingOutput = {9, 4};
    }
}

void updateCraftingOutput() {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    updateCraftingOutputUnlocked();
}

void takeCraftingOutput() {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    updateCraftingOutputUnlocked();
    if(craftingOutput.itemId != 0) {
        uint16_t outId = craftingOutput.itemId;
        uint16_t outCount = craftingOutput.count;

        // Transactional add: ensure entire output fits before consuming ingredients
        if (addItemUnlocked(outId, outCount)) {
            if(outId == 7) { // wood -> planks
                int needed = 1;
                for(int i=0; i<4 && needed>0; i++) {
                    if(craftingGrid[i].itemId == 4) {
                        int take = std::min((int)craftingGrid[i].count, needed);
                        craftingGrid[i].count -= take;
                        if(craftingGrid[i].count == 0) craftingGrid[i].itemId = 0;
                        needed -= take;
                    }
                }
            } else if(outId == 9) { // planks -> sticks
                int needed = 2;
                for(int i=0; i<4 && needed>0; i++) {
                    if(craftingGrid[i].itemId == 7) {
                        int take = std::min((int)craftingGrid[i].count, needed);
                        craftingGrid[i].count -= take;
                        if(craftingGrid[i].count == 0) craftingGrid[i].itemId = 0;
                        needed -= take;
                    }
                }
            }
            updateCraftingOutputUnlocked();
            inventoryVersion++;
        }
    }
}

void swapCraftingSlot(int invSlot, int craftSlot) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    if(invSlot < 0 || invSlot >= INVENTORY_SIZE || craftSlot < 0 || craftSlot >= 4) return;
    
    if (inventory[invSlot].itemId == craftingGrid[craftSlot].itemId && inventory[invSlot].itemId != 0) {
        uint16_t maxStack = getBlockMetadata(inventory[invSlot].itemId).maxStack;
        if (craftingGrid[craftSlot].count < maxStack) {
            uint16_t space = maxStack - craftingGrid[craftSlot].count;
            if (inventory[invSlot].count <= space) {
                craftingGrid[craftSlot].count += inventory[invSlot].count;
                inventory[invSlot].itemId = 0;
                inventory[invSlot].count = 0;
            } else {
                craftingGrid[craftSlot].count += space;
                inventory[invSlot].count -= space;
            }
            updateCraftingOutputUnlocked();
            inventoryVersion++;
            return;
        }
    }
    
    InventorySlot temp = inventory[invSlot];
    inventory[invSlot] = craftingGrid[craftSlot];
    craftingGrid[craftSlot] = temp;
    updateCraftingOutputUnlocked();
    inventoryVersion++;
}

void swapCraftingToCrafting(int craftSlotA, int craftSlotB) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    if(craftSlotA < 0 || craftSlotA >= 4 || craftSlotB < 0 || craftSlotB >= 4) return;
    InventorySlot temp = craftingGrid[craftSlotA];
    craftingGrid[craftSlotA] = craftingGrid[craftSlotB];
    craftingGrid[craftSlotB] = temp;
    updateCraftingOutputUnlocked();
    inventoryVersion++;
}

void clearCraftingGridUnlocked() {
    for(int i=0; i<4; i++) {
        if(craftingGrid[i].itemId != 0 && craftingGrid[i].count > 0) {
            uint16_t id = craftingGrid[i].itemId;
            uint16_t count = craftingGrid[i].count;
            uint16_t maxStack = getBlockMetadata(id).maxStack;
            if (maxStack == 0) maxStack = 64;

            uint16_t remaining = count;
            for (int s = 0; s < INVENTORY_SIZE && remaining > 0; s++) {
                if (inventory[s].itemId == id && inventory[s].count < maxStack) {
                    uint16_t space = maxStack - inventory[s].count;
                    uint16_t take = std::min(remaining, space);
                    inventory[s].count += take;
                    remaining -= take;
                }
            }
            for (int s = 0; s < INVENTORY_SIZE && remaining > 0; s++) {
                if (inventory[s].itemId == 0 || inventory[s].count == 0) {
                    uint16_t take = std::min(remaining, maxStack);
                    inventory[s].itemId = id;
                    inventory[s].count = take;
                    remaining -= take;
                }
            }
            if (remaining == 0) {
                craftingGrid[i] = {0, 0};
            } else {
                craftingGrid[i].count = remaining;
            }
        }
    }
    updateCraftingOutputUnlocked();
    inventoryVersion++;
}

void clearCraftingGrid() {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    clearCraftingGridUnlocked();
}

bool moveItems(int srcType, int srcSlot, int destType, int destSlot, int amount) {
    std::lock_guard<std::mutex> lock(inventoryMutex);
    if (amount <= 0) return false;
    
    InventorySlot* src = nullptr;
    InventorySlot* dest = nullptr;
    
    if (srcType == 0) {
        if (srcSlot < 0 || srcSlot >= INVENTORY_SIZE) return false;
        src = &inventory[srcSlot];
    } else if (srcType == 1) {
        if (srcSlot < 0 || srcSlot >= 4) return false;
        src = &craftingGrid[srcSlot];
    } else return false;
    
    if (destType == 0) {
        if (destSlot < 0 || destSlot >= INVENTORY_SIZE) return false;
        dest = &inventory[destSlot];
    } else if (destType == 1) {
        if (destSlot < 0 || destSlot >= 4) return false;
        dest = &craftingGrid[destSlot];
    } else return false;
    
    if (src->count < amount) amount = src->count;
    if (amount <= 0) return false;
    
    if (dest->itemId == 0) {
        dest->itemId = src->itemId;
        dest->count = amount;
        src->count -= amount;
        if (src->count == 0) src->itemId = 0;
        updateCraftingOutputUnlocked();
        inventoryVersion++;
        return true;
    } else if (dest->itemId == src->itemId) {
        uint16_t maxStack = getBlockMetadata(dest->itemId).maxStack;
        if (dest->count < maxStack) {
            uint16_t space = maxStack - dest->count;
            int move = (amount < space) ? amount : space;
            dest->count += move;
            src->count -= move;
            if (src->count == 0) src->itemId = 0;
            updateCraftingOutputUnlocked();
            inventoryVersion++;
            return true;
        }
    }
    return false;
}
