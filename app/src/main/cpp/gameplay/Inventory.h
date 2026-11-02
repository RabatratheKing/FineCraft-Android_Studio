#pragma once
#include <cstdint>
#include <string>
#include "Registry.h"

const int INVENTORY_SIZE = 36;
const int HOTBAR_SIZE = 9;

struct InventorySlot {
    uint16_t itemId;
    uint16_t count;
};

extern InventorySlot inventory[INVENTORY_SIZE];
extern int selectedHotbarSlot;
extern bool isInventoryOpen;

void initInventory();
bool addItem(uint16_t itemId, uint16_t count);
bool consumeItem(int slotIndex, uint16_t count);
void swapSlots(int slotA, int slotB);

const ItemDefinition& getBlockMetadata(uint16_t id); // Keeping this for backward compatibility

extern InventorySlot craftingGrid[4];
extern InventorySlot craftingOutput;

void updateCraftingOutput();
void takeCraftingOutput();
void swapCraftingSlot(int invSlot, int craftSlot); // Moves item between inv and 2x2 grid
void swapCraftingToCrafting(int craftSlotA, int craftSlotB);
bool moveItems(int srcType, int srcSlot, int destType, int destSlot, int amount);
void clearCraftingGrid(); // put items back to inventory if closed
