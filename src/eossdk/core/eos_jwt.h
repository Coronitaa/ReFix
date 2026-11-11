// ReFix EOS v3 - ID tokens.
//
// EOS hands a title its player's identity as a JSON Web Token
// (EOS_Connect_CopyIdToken / EOS_Auth_CopyIdToken). Titles rarely verify it
// themselves; they pass it on - to Photon as custom-auth credentials, to their
// own backend, to a voice service - and some decode the payload first to read
// the subject or the expiry. So the token has to be a well-formed JWT with the
// claims real EOS puts in it, even though nobody can check its signature.
//
// The signature is a SHA-256 over header.payload: three non-empty segments,
// stable for the same input, and plainly not an Epic signature.
#pragma once
#include "refix_common.h"
#include "refix_hash.h"

namespace refix {
namespace jwt {

inline std::string Base64Url(const void* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const uint8_t* p = (const uint8_t*)data;
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < len; i += 3) {
        uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i + 1] << 8) | p[i + 2];
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];  out += tbl[v & 63];
    }
    if (i + 1 == len) {
        uint32_t v = (uint32_t)p[i] << 16;
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
    } else if (i + 2 == len) {
        uint32_t v = ((uint32_t)p[i] << 16) | ((uint32_t)p[i + 1] << 8);
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63]; out += tbl[(v >> 6) & 63];
    }
    return out;  // JWT segments carry no '=' padding
}

inline std::string Base64Url(const std::string& s) { return Base64Url(s.data(), s.size()); }

// A JSON string literal, quotes included.
inline std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    out += "\"";
    return out;
}

// The external account type as EOS spells it in the "eat" claim.
inline const char* ExternalAccountTypeClaim(EOS_EExternalAccountType type) {
    switch (type) {
        case EAT::EOS_EAT_EPIC:      return "epicgames";
        case EAT::EOS_EAT_STEAM:     return "steam";
        case EAT::EOS_EAT_PSN:       return "psn";
        case EAT::EOS_EAT_XBL:       return "xbl";
        case EAT::EOS_EAT_DISCORD:   return "discord";
        case EAT::EOS_EAT_GOG:       return "gog";
        case EAT::EOS_EAT_NINTENDO:  return "nintendo";
        case EAT::EOS_EAT_UPLAY:     return "uplay";
        case EAT::EOS_EAT_OPENID:    return "openid";
        case EAT::EOS_EAT_APPLE:     return "apple";
        case EAT::EOS_EAT_GOOGLE:    return "google";
        case EAT::EOS_EAT_OCULUS:    return "oculus";
        case EAT::EOS_EAT_ITCHIO:    return "itchio";
        case EAT::EOS_EAT_AMAZON:    return "amazon";
        default:                     return "steam";
    }
}

struct Claims {
    std::string Issuer;
    std::string Subject;       // ProductUserId (Connect) or EpicAccountId (Auth)
    std::string Audience;      // client id
    std::string ProductId;
    std::string SandboxId;
    std::string DeploymentId;
    // Connect tokens describe the account the player logged in with.
    bool        HasAccount = false;
    std::string AccountType;   // "eat"
    std::string AccountId;     // "eaid"
    std::string Platform = "other";
    std::string DeviceType = "PC";
    int64_t     IssuedAt = 0;
    int64_t     ExpiresAt = 0;
};

inline std::string Build(const Claims& c) {
    const std::string header = "{\"alg\":\"RS256\",\"kid\":\"refix-local\",\"typ\":\"JWT\"}";

    std::string payload = "{";
    payload += "\"iss\":" + Quote(c.Issuer);
    payload += ",\"sub\":" + Quote(c.Subject);
    payload += ",\"aud\":" + Quote(c.Audience);
    payload += ",\"pfpid\":" + Quote(c.ProductId);
    payload += ",\"pfsid\":" + Quote(c.SandboxId);
    payload += ",\"pfdid\":" + Quote(c.DeploymentId);
    if (c.HasAccount) {
        payload += ",\"act\":{\"eat\":" + Quote(c.AccountType) +
                   ",\"eaid\":" + Quote(c.AccountId) +
                   ",\"pltfm\":" + Quote(c.Platform) +
                   ",\"dty\":" + Quote(c.DeviceType) + "}";
    } else {
        payload += ",\"act\":{\"pltfm\":" + Quote(c.Platform) +
                   ",\"dty\":" + Quote(c.DeviceType) + "}";
    }
    payload += ",\"iat\":" + std::to_string(c.IssuedAt);
    payload += ",\"exp\":" + std::to_string(c.ExpiresAt);
    payload += ",\"jti\":" + Quote(DerivedId(c.Subject + "|" + std::to_string(c.IssuedAt)));
    payload += "}";

    const std::string signingInput = Base64Url(header) + "." + Base64Url(payload);
    uint8_t digest[32];
    Sha256(signingInput.data(), signingInput.size(), digest);
    return signingInput + "." + Base64Url(digest, sizeof(digest));
}

} // namespace jwt
} // namespace refix
