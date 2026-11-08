#pragma once

#include "mod_types.h"
#include "mod_manifest.h"
#include "mod_signature.h"
#include <string>
#include <vector>

namespace refix::mods {

struct PackageFileEntry {
    std::string relative_path;
    std::vector<uint8_t> data;
    std::string sha256_hex;
};

struct PackageInspectInfo {
    ModManifest manifest;
    TrustLevel trust_level = TrustLevel::Untrusted;
    bool signature_valid = false;
    std::string error_message;
    std::vector<std::string> files_in_zip;
    std::vector<std::string> validation_errors;
    bool is_valid = false;
};

class ModPackage {
public:
    // Create an .rfmod package file from directory or in-memory files.
    // If private_key_64 is provided, it signs the canonical manifest and adds refixmod.sig.
    static bool CreatePackage(const std::string& output_rfmod_path,
                             ModManifest manifest,
                             const std::vector<PackageFileEntry>& files,
                             const uint8_t private_key_64[64],
                             std::string& out_error);

    // Inspect an .rfmod package without extracting to disk
    static bool InspectPackage(const std::string& rfmod_path,
                              const uint8_t expected_public_key_32[32],
                              const RevocationList* revocation_list,
                              PackageInspectInfo& out_info,
                              std::string& out_error);

    // Verify cryptographic integrity and extract safely to dest_dir.
    // Guaranteed Zip-Slip prevention, strict file hash matching, no rogue files.
    static bool VerifyAndExtract(const std::string& rfmod_path,
                                const std::string& dest_dir,
                                const uint8_t expected_public_key_32[32],
                                const RevocationList* revocation_list,
                                PackageInspectInfo& out_info,
                                std::string& out_error);

    // Read full contents of a file from disk
    static bool ReadDiskFile(const std::string& path, std::vector<uint8_t>& out_data);
    static bool WriteDiskFile(const std::string& path, const uint8_t* data, size_t size);
};

} // namespace refix::mods
