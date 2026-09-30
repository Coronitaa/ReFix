// ReFix EOS v3 - module wiring shared by the API translation units.
#pragma once
#include "core/refix_common.h"

namespace refix {

// Binds implemented entry points into the export table by name. A name that is
// not part of this SDK's export set is reported rather than silently dropped,
// so a typo cannot quietly disable an interface.
class Registrar {
public:
    void Bind(const char* name, void* fn);
    int  Bound() const { return m_bound; }
    int  Missing() const { return m_missing; }
private:
    int m_bound = 0;
    int m_missing = 0;
};

#define REFIX_BIND(reg, fn) (reg).Bind(#fn, (void*)&fn)

int         FindExport(const char* name);
const char* ExportName(int index);
int         ExportCount();
void        InitialiseModule();

void RegisterPlatformApi(Registrar&);
void RegisterConnectApi(Registrar&);
void RegisterAuthApi(Registrar&);
void RegisterUserApi(Registrar&);
void RegisterLobbyApi(Registrar&);
void RegisterSessionsApi(Registrar&);
void RegisterP2PApi(Registrar&);
void RegisterStorageApi(Registrar&);

} // namespace refix
