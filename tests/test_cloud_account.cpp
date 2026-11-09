// Offline checks of the cloud sign-in pieces: PKCE (RFC 7636 appendix B), the callback parser, and the DPAPI
// token store round trip. No network.
#include "../src/overlay/cloud_account.cpp"

#include <cstdio>

void ReFixLog(const char*, ...) {}
namespace refix::overlay::sync { void SetFolder(const std::wstring&) {} }
namespace refix::overlay::profiles {
std::wstring RootDir() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    return std::wstring(tmp) + L"refix_cloud_test";
}
}

using namespace refix::overlay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main() {
    CHECK(cloud::Sha256Url("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
    CHECK(cloud::RandomToken(48).size() == 64);   // RFC 7636: verifier 43..128 chars
    CHECK(cloud::RandomToken(48) != cloud::RandomToken(48));

    const std::string line = "GET /?state=abc&code=4%2F0Ab_x&scope=s HTTP/1.1";
    CHECK(cloud::Param(line, "code") == "4/0Ab_x");
    CHECK(cloud::Param(line, "state") == "abc");
    CHECK(cloud::Param("GET /favicon.ico HTTP/1.1", "state").empty());
    CHECK(cloud::UrlEncode("a b/'") == "a%20b%2F%27");
    CHECK(cloud::Unscramble("1d1519") == "GOC");   // secret stored as hex of bytes XOR 0x5A

    CreateDirectoryW(profiles::RootDir().c_str(), nullptr);
    const cloud::Spec& g = cloud::S(cloud::Provider::Google);
    CHECK(cloud::SaveRefresh(g, "1//refresh-token"));
    std::string back;
    CHECK(cloud::LoadRefresh(g, back) && back == "1//refresh-token");
    std::ifstream raw(cloud::TokenFile(g), std::ios::binary);
    std::string disk((std::istreambuf_iterator<char>(raw)), std::istreambuf_iterator<char>());
    raw.close();
    CHECK(disk.find("refresh-token") == std::string::npos);   // encrypted on disk
    CHECK(cloud::Connected(cloud::Provider::Google));
    DeleteFileW(cloud::TokenFile(g).c_str());
    CHECK(!cloud::Connected(cloud::Provider::Google));

    std::printf(g_fail ? "test_cloud_account: %d FAILED\n" : "test_cloud_account: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
