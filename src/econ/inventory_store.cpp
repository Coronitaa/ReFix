#include "inventory_store.h"
#include "../steam_gc_emu.h"

namespace refix::econ {

InventoryStore& InventoryStore::Get() {
    static InventoryStore s_instance;
    return s_instance;
}

InventoryStore::InventoryStore() {
    refix::GetRefixServices().inventory = this;
}

// The GC emulator loads on first use and saves on every change.
bool InventoryStore::Load(uint64_t) { return true; }
bool InventoryStore::Save(uint64_t) { return true; }

std::vector<InventoryItem> InventoryStore::GetItems(uint64_t) {
    return ReFixGC::GetLocalInventory();
}

// Granted on the game's main thread (persisted and pushed to the client as SO Create); the new id is not known yet.
uint64_t InventoryStore::GrantItem(uint64_t, uint32_t defIndex, uint32_t, uint32_t quality, uint32_t rarity, uint32_t paintKit, float wear, uint32_t seed, const char* customName) {
    CSGO_GrantItemToLocal(defIndex, 1, quality, rarity, paintKit, wear, seed, customName);
    return 0;
}

// Equipping goes through the game's own 1059 message, which the GC emulator handles.
bool InventoryStore::SetEquipped(uint64_t, uint64_t, uint32_t, uint32_t) { return false; }

uint64_t InventoryStore::AllocateItemId(uint64_t) { return 0; }

} // namespace refix::econ
