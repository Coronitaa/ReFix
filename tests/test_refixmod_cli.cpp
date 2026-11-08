#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cassert>

namespace fs = std::filesystem;

static int RunCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

int main() {
    std::cout << "Running test_refixmod_cli..." << std::endl;

    fs::path temp_dir = fs::temp_directory_path() / "refix_cli_test";
    fs::create_directories(temp_dir);

    std::string key_prefix = (temp_dir / "mykey").string();
    std::string mod_dir = (temp_dir / "mymod").string();
    fs::create_directories(mod_dir);
    std::string manifest_file = (fs::path(mod_dir) / "refixmod.json").string();
    std::string rfmod_file = (temp_dir / "output.rfmod").string();

    // 1. Keygen
    std::string cmd_keygen = "bin\\refixmod.exe keygen --out \"" + key_prefix + "\"";
    int res = RunCmd(cmd_keygen);
    assert(res == 0);
    assert(fs::exists(key_prefix + ".pub"));
    assert(fs::exists(key_prefix + ".priv"));
    std::cout << "[PASS] refixmod keygen" << std::endl;

    // Read public key hex
    std::ifstream pub_f(key_prefix + ".pub");
    std::string pub_hex;
    pub_f >> pub_hex;
    pub_f.close();

    // 2. Prepare sample manifest
    std::ofstream mf(manifest_file);
    mf << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"id\": \"com.refix.clitest\",\n"
       << "  \"name\": \"CLI Test Mod\",\n"
       << "  \"version\": \"1.0.0\",\n"
       << "  \"author\": \"Developer\",\n"
       << "  \"author_pubkey\": \"" << pub_hex << "\",\n"
       << "  \"entrypoint\": \"code.wasm\",\n"
       << "  \"execution_tier\": \"sandboxed\",\n"
       << "  \"target_games\": [\n"
       << "    { \"app_id\": 480, \"exe_name\": \"game.exe\", \"engine\": \"unity_mono\" }\n"
       << "  ],\n"
       << "  \"capabilities\": [ \"log\", \"config\" ]\n"
       << "}\n";
    mf.close();

    // Add code.wasm file
    std::ofstream code_f(fs::path(mod_dir) / "code.wasm", std::ios::binary);
    char wasm_header[] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
    code_f.write(wasm_header, sizeof(wasm_header));
    code_f.close();

    // 3. Lint
    std::string cmd_lint = "bin\\refixmod.exe lint \"" + manifest_file + "\"";
    res = RunCmd(cmd_lint);
    assert(res == 0);
    std::cout << "[PASS] refixmod lint" << std::endl;

    // 4. Pack & Sign
    std::string cmd_pack = "bin\\refixmod.exe pack \"" + mod_dir + "\" \"" + rfmod_file + "\" --key \"" + key_prefix + ".priv\"";
    res = RunCmd(cmd_pack);
    assert(res == 0);
    assert(fs::exists(rfmod_file));
    std::cout << "[PASS] refixmod pack" << std::endl;

    // 5. Verify
    std::string cmd_verify = "bin\\refixmod.exe verify \"" + rfmod_file + "\" --key \"" + key_prefix + ".pub\"";
    res = RunCmd(cmd_verify);
    assert(res == 0);
    std::cout << "[PASS] refixmod verify" << std::endl;

    // 6. Inspect
    std::string cmd_inspect = "bin\\refixmod.exe inspect \"" + rfmod_file + "\"";
    res = RunCmd(cmd_inspect);
    assert(res == 0);
    std::cout << "[PASS] refixmod inspect" << std::endl;

    // Cleanup
    fs::remove_all(temp_dir);

    std::cout << "All test_refixmod_cli tests passed!" << std::endl;
    return 0;
}
