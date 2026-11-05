#include "../src/mods/mod_package.h"
#include <iostream>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;
using namespace refix::mods;

int main() {
    std::cout << "Running test_mod_package..." << std::endl;

    fs::path temp_dir = fs::temp_directory_path() / "refix_test_pkg";
    fs::create_directories(temp_dir);
    std::string pkg_path = (temp_dir / "sample.rfmod").string();
    std::string extract_dir = (temp_dir / "extracted").string();

    // 1. Generate keypair
    uint8_t pubkey[32];
    uint8_t privkey[64];
    CryptoGenerateKeypair(pubkey, privkey);
    std::string pubkey_hex = ToHex(pubkey, 32);

    // 2. Prepare manifest & files
    ModManifest manifest;
    manifest.id = "com.refix.testpkg";
    manifest.name = "Test Package";
    manifest.version = "1.0.0";
    manifest.author = "TestAuthor";
    manifest.author_pubkey = pubkey_hex;
    manifest.entrypoint = "main.wasm";
    manifest.execution_tier = ExecutionTier::Sandboxed;

    TargetGame tg;
    tg.app_id = 480;
    tg.exe_name = "test.exe";
    tg.engine = "native";
    manifest.target_games.push_back(tg);

    std::vector<PackageFileEntry> files;
    PackageFileEntry f1;
    f1.relative_path = "main.wasm";
    f1.data = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 }; // wasm magic
    files.push_back(f1);

    PackageFileEntry f2;
    f2.relative_path = "data/config.txt";
    std::string cfg = "key=value\n";
    f2.data.assign(cfg.begin(), cfg.end());
    files.push_back(f2);

    std::string error;
    bool create_ok = ModPackage::CreatePackage(pkg_path, manifest, files, privkey, error);
    if (!create_ok) {
        std::cerr << "Failed to create package: " << error << std::endl;
        assert(false);
    }
    std::cout << "[PASS] Package created successfully" << std::endl;

    // 3. Inspect package
    PackageInspectInfo info;
    bool inspect_ok = ModPackage::InspectPackage(pkg_path, pubkey, nullptr, info, error);
    assert(inspect_ok);
    assert(info.is_valid);
    assert(info.manifest.id == "com.refix.testpkg");
    assert(info.manifest.files.size() == 2);
    std::cout << "[PASS] Package inspected and validated" << std::endl;

    // 4. Verify and extract
    bool extract_ok = ModPackage::VerifyAndExtract(pkg_path, extract_dir, pubkey, nullptr, info, error);
    assert(extract_ok);
    assert(fs::exists(fs::path(extract_dir) / "main.wasm"));
    assert(fs::exists(fs::path(extract_dir) / "data/config.txt"));
    assert(fs::exists(fs::path(extract_dir) / "refixmod.json"));
    std::cout << "[PASS] Package extracted successfully" << std::endl;

    // 5. Test Zip-Slip rejection
    PackageFileEntry evil_file;
    evil_file.relative_path = "../evil.txt";
    evil_file.data = { 'e', 'v', 'i', 'l' };
    std::vector<PackageFileEntry> evil_files = files;
    evil_files.push_back(evil_file);

    std::string evil_pkg = (temp_dir / "evil.rfmod").string();
    bool evil_create = ModPackage::CreatePackage(evil_pkg, manifest, evil_files, privkey, error);
    assert(!evil_create); // Must be rejected at creation
    std::cout << "[PASS] Zip-slip relative traversal rejected during packaging" << std::endl;

    // 6. Test Revocation rejection
    RevocationList rev_list;
    rev_list.RevokeKey(pubkey_hex);
    PackageInspectInfo rev_info;
    bool rev_extract = ModPackage::VerifyAndExtract(pkg_path, (temp_dir / "rev_out").string(), pubkey, &rev_list, rev_info, error);
    assert(!rev_extract);
    assert(!rev_info.is_valid);
    std::cout << "[PASS] Revoked key package rejected" << std::endl;

    // Cleanup
    fs::remove_all(temp_dir);

    std::cout << "All test_mod_package tests passed!" << std::endl;
    return 0;
}
