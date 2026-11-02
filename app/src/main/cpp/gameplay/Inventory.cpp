#include "Inventory.h"
#include <unordered_map>

InventorySlot inventory[INVENTORY_SIZE];
int selectedHotbarSlot = 0;
bool isInventoryOpen = false;


void initInventory() {
    Registry::init();
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        inventory[i].itemId = 0;
        inventory[i].count = 0;
    }
    
    // Hotbar slots (27-35)
    inventory[27] = {1, 64}; // Grass
    inventory[28] = {2, 64}; // Dirt
    inventory[29] = {3, 64}; // Stone
    inventory[30] = {4, 64}; // Wood
    inventory[31] = {5, 64}; // Leaves
    inventory[32] = {6, 64}; // Water
    inventory[33] = {7, 64}; // Planks
    inventory[34] = {8, 64}; // Sand
    inventory[35] = {9, 64}; // Glass
    
    // Top inventory slots for the rest
    inventory[0] = {10, 64}; // Bricks
    inventory[1] = {11, 64}; // Gravel
    inventory[2] = {12, 64}; // Coal Ore
    inventory[3] = {13, 64}; // Iron Ore
    inventory[4] = {14, 64}; // Diamond Ore
    inventory[5] = {15, 64}; // Cobblestone
    
    // Additional items
    inventory[6] = {101, 1}; // Diamond Sword
    inventory[7] = {102, 1}; // Diamond Pickaxe
    inventory[8] = {103, 1}; // Wooden Pickaxe
    inventory[9] = {100, 16}; // Apple
}

const ItemDefinition& getBlockMetadata(uint16_t id) {
    return Registry::getItem(id);
}

bool addItem(uint16_t itemId, uint16_t count) {
    if (itemId == 0) return false;
    uint16_t maxStack = getBlockMetadata(itemId).maxStack;
    if (maxStack == 0) return false;

    // First pass: try to add to existing partial stacks
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory[i].itemId == itemId && inventory[i].count < maxStack) {
            uint16_t space = maxStack - inventory[i].count;
            if (count <= space) {
                inventory[i].count += count;
                return true;
            } else {
                inventory[i].count += space;
                count -= space;
            }
        }
    }

    // Second pass: find empty slots
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory[i].count == 0 || inventory[i].itemId == 0) {
            if (count <= maxStack) {
                inventory[i].itemId = itemId;
                inventory[i].count = count;
                return true;
            } else {
                inventory[i].itemId = itemId;
                inventory[i].count = maxStack;
                count -= maxStack;
            }
        }
    }

    return false; // Could not fit all items
}

bool consumeItem(int slotIndex, uint16_t count) {
    if (slotIndex < 0 || slotIndex >= INVENTORY_SIZE) return false;
    if (inventory[slotIndex].count >= count) {
        inventory[slotIndex].count -= count;
        if (inventory[slotIndex].count == 0) {
            inventory[slotIndex].itemId = 0;
        }
        return true;
    }
    return false;
}

void swapSlots(int slotA, int slotB) {
    if (slotA < 0 || slotA >= INVENTORY_SIZE || slotB < 0 || slotB >= INVENTORY_SIZE) return;
    
    if (inventory[slotA].itemId == inventory[slotB].itemId && inventory[slotA].itemId != 0) {
        // Try to merge
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
            return;
        }
    }
    
    InventorySlot temp = inventory[slotA];
    inventory[slotA] = inventory[slotB];
    inventory[slotB] = temp;
}

InventorySlot craftingGrid[4] = {{0,0}, {0,0}, {0,0}, {0,0}};
InventorySlot craftingOutput = {0,0};

void updateCraftingOutput() {
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

void takeCraftingOutput() {
    updateCraftingOutput();
    if(craftingOutput.itemId != 0) {
        if(addItem(craftingOutput.itemId, craftingOutput.count)) {
            // consume ingredients
            if(craftingOutput.itemId == 7) { // wood -> planks
                int needed = 1;
                for(int i=0; i<4 && needed>0; i++) {
                    if(craftingGrid[i].itemId == 4) {
                        int take = std::min((int)craftingGrid[i].count, needed);
                        craftingGrid[i].count -= take;
                        if(craftingGrid[i].count == 0) craftingGrid[i].itemId = 0;
                        needed -= take;
                    }
                }
            } else if(craftingOutput.itemId == 9) { // planks -> sticks
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
            updateCraftingOutput();
        }
    }
}

void swapCraftingSlot(int invSlot, int craftSlot) {
    if(invSlot < 0 || invSlot >= INVENTORY_SIZE || craftSlot < 0 || craftSlot >= 4) return;
    
    if (inventory[invSlot].itemId == craftingGrid[craftSlot].itemId && inventory[invSlot].itemId != 0) {
        // Try to merge
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
            updateCraftingOutput();
            return;
        }
    }
    
    InventorySlot temp = inventory[invSlot];
    inventory[invSlot] = craftingGrid[craftSlot];
    craftingGrid[craftSlot] = temp;
    updateCraftingOutput();
}

void swapCraftingToCrafting(int craftSlotA, int craftSlotB) {
    if(craftSlotA < 0 || craftSlotA >= 4 || craftSlotB < 0 || craftSlotB >= 4) return;
    InventorySlot temp = craftingGrid[craftSlotA];
    craftingGrid[craftSlotA] = craftingGrid[craftSlotB];
    craftingGrid[craftSlotB] = temp;
    updateCraftingOutput();
}

void clearCraftingGrid() {
    for(int i=0; i<4; i++) {
        if(craftingGrid[i].itemId != 0) {
            addItem(craftingGrid[i].itemId, craftingGrid[i].count);
            craftingGrid[i] = {0, 0};
        }
    }
    updateCraftingOutput();
}

bool moveItems(int srcType, int srcSlot, int destType, int destSlot, int amount) {
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
        updateCraftingOutput();
        return true;
    } else if (dest->itemId == src->itemId) {
        uint16_t maxStack = getBlockMetadata(dest->itemId).maxStack;
        if (dest->count < maxStack) {
            uint16_t space = maxStack - dest->count;
            int move = (amount < space) ? amount : space;
            dest->count += move;
            src->count -= move;
            if (src->count == 0) src->itemId = 0;
            updateCraftingOutput();
            return true;
        }
    }
    return false;
}
