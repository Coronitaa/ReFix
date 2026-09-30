// ReFix EOS v3 - account id interning.
//
// EOS_ProductUserId / EOS_EpicAccountId are opaque pointers. Real EOS guarantees
// that the same account always maps to the same pointer for the lifetime of the
// process, and titles rely on that: Redpoint stores them in maps keyed by
// pointer and compares them with ==. So ids are interned here, created once and
// never destroyed, and the string form is the single source of truth.
#pragma once
#include "refix_common.h"

// Concrete definitions for the SDK's opaque id handles.
struct EOS_ProductUserIdDetails { char id[EOS_PRODUCTUSERID_MAX_LENGTH + 1]; };
struct EOS_EpicAccountIdDetails { char id[EOS_EPICACCOUNTID_MAX_LENGTH + 1]; };

namespace refix {

class IdRegistry {
public:
    static IdRegistry& Get();

    EOS_ProductUserId  Puid(const std::string& id);
    EOS_EpicAccountId  Eaid(const std::string& id);

    static bool        IsValidPuid(EOS_ProductUserId id);
    static bool        IsValidEaid(EOS_EpicAccountId id);
    static const char* ToString(EOS_ProductUserId id);
    static const char* ToString(EOS_EpicAccountId id);

private:
    std::mutex m_mutex;
    std::map<std::string, EOS_ProductUserIdDetails*> m_puids;
    std::map<std::string, EOS_EpicAccountIdDetails*> m_eaids;
};

} // namespace refix
