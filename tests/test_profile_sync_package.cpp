// Self-check of the profile sync package (.rfxp): round trip, SteamID and state hash kept, damaged or edited
// packages rejected, and the saves not readable as plain text inside the file.
#include "../src/overlay/profile_sync.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace refix::overlay::sync;

void ReFixLog(const char*, ...) {}

int main() {
    FileList files = {
        { "Profile/Cloud/3241660/save_1.es3", std::vector<uint8_t>(5000, 'A') },
        { "Saves/Unity_semiwork_REPO/SECRET_SAVE_MARKER.txt", std::vector<uint8_t>{ 'h', 'i' } },
        { "Profile/empty.bin", {} },
    };
    uint8_t hash[32];
    for (int i = 0; i < 32; ++i) hash[i] = (uint8_t)i;
    std::vector<uint8_t> pkg;
    assert(Pack(files, 76561198362393833ULL, hash, pkg));

    const std::string raw(pkg.begin(), pkg.end());
    assert(raw.find("SECRET_SAVE_MARKER") == std::string::npos);   // names and data are scrambled
    assert(raw.find("PK\x03\x04") == std::string::npos);            // not a zip anyone can open by hand

    FileList back;
    uint64_t steamId = 0;
    uint8_t backHash[32] = {};
    assert(Unpack(pkg, back, &steamId, backHash));
    assert(steamId == 76561198362393833ULL && memcmp(hash, backHash, 32) == 0);
    assert(back.size() == files.size());
    for (size_t i = 0; i < files.size(); ++i) {
        bool found = false;
        for (const auto& b : back) found |= b.first == files[i].first && b.second == files[i].second;
        assert(found);
    }

    std::vector<uint8_t> edited = pkg;
    edited[edited.size() / 2] ^= 0x01;                // one bit of the content
    assert(!Unpack(edited, back, nullptr, nullptr));
    std::vector<uint8_t> cut(pkg.begin(), pkg.end() - 10);
    assert(!Unpack(cut, back, nullptr, nullptr));
    std::vector<uint8_t> other = pkg;
    other[0] = 'X';                                    // not a ReFix package
    assert(!Unpack(other, back, nullptr, nullptr));
    assert(!Unpack({}, back, nullptr, nullptr));

    std::vector<uint8_t> again;                        // a fresh nonce every time: same files, other bytes
    assert(Pack(files, 1, hash, again) && again != pkg);
    std::puts("test_profile_sync_package: OK");
    return 0;
}
