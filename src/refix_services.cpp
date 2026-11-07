#include "include/refix_services.h"

namespace refix {

static IRefixServices s_services;

IRefixServices& GetRefixServices() {
    return s_services;
}

} // namespace refix
