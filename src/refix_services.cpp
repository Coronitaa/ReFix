#include "include/refix_services.h"
#include "gc/gc_core.h"
#include "econ/econ_schema.h"
#include "econ/inventory_store.h"
#include "engine/source/source_engine.h"
#include "engine/panorama/panorama_adapter.h"
#include "overlay/game_actions/overlay_actions.h"
#include "entitlements/entitlements_store.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix {

static IRefixServices s_services;

IRefixServices& GetRefixServices() {
    if (!s_services.Log) {
        s_services.Log = &ReFixLog;
        s_services.gc = &gc::GcCore::Get();
        s_services.econ = &econ::EconSchema::Get();
        s_services.inventory = &econ::InventoryStore::Get();
        s_services.engine = &source::SourceEngine::Get();
        s_services.panorama = &panorama::PanoramaAdapter::Get();
        s_services.overlay = &overlay::OverlayActionsRegistry::Get();
        s_services.entitlements = &entitlements::EntitlementsStore::Get();
    }
    return s_services;
}

} // namespace refix
