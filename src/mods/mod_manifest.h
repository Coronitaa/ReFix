#pragma once

#include "mod_types.h"
#include <string>

namespace refix::mods {

// Strictly parses a JSON string into a ModManifest. Returns false and populates out_error on failure.
bool ParseManifest(const std::string& json_text, ModManifest& out_manifest, std::string& out_error);

// Validates semantic constraints of the manifest (id format, tier restrictions, valid capabilities, limits).
bool ValidateManifest(const ModManifest& manifest, std::string& out_error);

// Generates a canonical, reproducible JSON string representation (sorted keys, no extraneous whitespace)
// suitable for cryptographic signing with Ed25519.
std::string ToCanonicalJson(const ModManifest& manifest);

// Security check against Zip-Slip path traversal attacks.
// Rejects paths with "..", absolute drives ("C:"), leading slashes, backslashes outside subdirs.
bool IsSafeRelativePath(const std::string& path);

} // namespace refix::mods
