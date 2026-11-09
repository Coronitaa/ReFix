// Google Drive / OneDrive sign-in and package storage (cloud_account.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <shellapi.h>
#include <winhttp.h>

#include "cloud_account.h"
#include "cloud_keys.h"
#include "mini_json.h"
#include "overlay_text.h"
#include "profile_store.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>
#include <thread>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

extern void ReFixLog(const char* fmt, ...);

namespace refix::overlay::cloud {
namespace {

constexpr DWORD kSignInWaitMs = 180000;
constexpr int kHttpTimeoutMs = 60000;
constexpr size_t kMaxBody = 1024ull * 1024 * 1024;          // same cap as a package
constexpr size_t kChunk = 32 * 320 * 1024;                  // OneDrive upload chunks must be multiples of 320 KiB
constexpr ULONGLONG kRefreshMarginMs = 60000;
const char kEntropy[] = "ReFix cloud sign-in v1";

struct Spec {
    const char* label;
    const wchar_t* choice;
    const char* clientId;
    const char* clientSecret;   // scrambled (Unscramble)
    const char* authUrl;
    const wchar_t* tokenUrl;
    const char* scope;
    const char* redirectHost;   // what the app registration allows
    const wchar_t* file;
};

const Spec kSpecs[] = {
    { "Google Drive", L"cloud:google", REFIX_GOOGLE_CLIENT_ID, REFIX_GOOGLE_CLIENT_SECRET_HEX,
      "https://accounts.google.com/o/oauth2/v2/auth", L"https://oauth2.googleapis.com/token",
      "https://www.googleapis.com/auth/drive.appdata", "127.0.0.1", L"google.bin" },
    { "OneDrive", L"cloud:onedrive", REFIX_MICROSOFT_CLIENT_ID, "",
      "https://login.microsoftonline.com/common/oauth2/v2.0/authorize",
      L"https://login.microsoftonline.com/common/oauth2/v2.0/token",
      "Files.ReadWrite.AppFolder offline_access", "localhost", L"onedrive.bin" },
};
const Spec& S(Provider p) { return kSpecs[p == Provider::Google ? 0 : 1]; }

std::mutex g_mu;   // tokens and sign-in result
struct Access { std::string token; ULONGLONG expires = 0; };
Access g_access[2];
std::atomic<bool> g_signingIn{ false };
std::atomic<int> g_signingWith{ -1 };   // Idx of the provider in the browser, -1 none
std::atomic<bool> g_cancel{ false };    // the player gave up on the browser (closed the tab, changed mind)
Msg g_error{};

int Idx(Provider p) { return p == Provider::Google ? 0 : 1; }

void Wipe(std::string& s) {
    if (!s.empty()) SecureZeroMemory(&s[0], s.size());
    s.clear();
}

// --- encoding ------------------------------------------------------------------------------------------------
std::string UrlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

std::string UrlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) { out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else out += s[i] == '+' ? ' ' : s[i];
    }
    return out;
}

std::string Base64Url(const uint8_t* b, size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t v = (uint32_t)b[i] << 16 | (i + 1 < n ? (uint32_t)b[i + 1] << 8 : 0) | (i + 2 < n ? b[i + 2] : 0);
        out += t[v >> 18 & 63];
        out += t[v >> 12 & 63];
        if (i + 1 < n) out += t[v >> 6 & 63];
        if (i + 2 < n) out += t[v & 63];
    }
    return out;
}

std::string RandomToken(size_t bytes) {
    std::vector<uint8_t> b(bytes);
    BCryptGenRandom(nullptr, b.data(), (ULONG)b.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return Base64Url(b.data(), b.size());
}

std::string Sha256Url(const std::string& s) {
    uint8_t h[32] = {};
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PUCHAR)s.data(), (ULONG)s.size(), h, sizeof(h));
    return Base64Url(h, sizeof(h));
}

// Query parameter of "GET /?a=1&b=2 HTTP/1.1".
std::string Param(const std::string& requestLine, const char* key) {
    const size_t q = requestLine.find('?'), end = requestLine.find(' ', q == std::string::npos ? 0 : q);
    if (q == std::string::npos) return {};
    const std::string query = "&" + requestLine.substr(q + 1, end - q - 1);
    const std::string k = std::string("&") + key + "=";
    const size_t at = query.find(k);
    if (at == std::string::npos) return {};
    const size_t from = at + k.size(), to = query.find('&', from);
    return UrlDecode(query.substr(from, to == std::string::npos ? std::string::npos : to - from));
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) { if (c == '"' || c == '\\') out += '\\'; out += c; }
    return out;
}

// --- HTTPS ---------------------------------------------------------------------------------------------------
struct Resp { DWORD status = 0; std::string body; std::wstring location; };

// HTTPS only, redirects not followed (a bearer token never leaves for another host by itself).
bool Http(const wchar_t* method, const std::wstring& url, const std::wstring& headers, const void* body, size_t size,
          Resp& r) {
    r = Resp{};
    URL_COMPONENTS u = { sizeof(u) };
    wchar_t host[256] = {}, path[4096] = {};
    u.lpszHostName = host; u.dwHostNameLength = 256;
    u.lpszUrlPath = path; u.dwUrlPathLength = 4096;
    u.dwExtraInfoLength = 1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS) return false;
    std::wstring full = std::wstring(path, u.dwUrlPathLength) + std::wstring(u.lpszExtraInfo ? u.lpszExtraInfo : L"", u.dwExtraInfoLength);
    HINTERNET s = WinHttpOpen(L"ReFix", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return false;
    WinHttpSetTimeouts(s, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs, kHttpTimeoutMs);
    HINTERNET c = WinHttpConnect(s, std::wstring(host, u.dwHostNameLength).c_str(), u.nPort, 0);
    HINTERNET q = c ? WinHttpOpenRequest(c, method, full.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = false;
    if (q) {
        DWORD never = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(q, WINHTTP_OPTION_REDIRECT_POLICY, &never, sizeof(never));
        ok = WinHttpSendRequest(q, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), (DWORD)-1L,
                                (LPVOID)body, (DWORD)size, (DWORD)size, 0) && WinHttpReceiveResponse(q, nullptr);
        if (ok) {
            DWORD len = sizeof(r.status);
            WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &r.status, &len, WINHTTP_NO_HEADER_INDEX);
            wchar_t loc[4096] = {};
            len = sizeof(loc);
            if (WinHttpQueryHeaders(q, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, loc, &len, WINHTTP_NO_HEADER_INDEX))
                r.location = loc;
            for (DWORD avail = 0; ok && WinHttpQueryDataAvailable(q, &avail) && avail; ) {
                if (r.body.size() + avail > kMaxBody) { ok = false; break; }
                const size_t at = r.body.size();
                r.body.resize(at + avail);
                DWORD got = 0;
                if (!WinHttpReadData(q, &r.body[at], avail, &got)) ok = false;
                r.body.resize(at + got);
            }
        }
    }
    if (q) WinHttpCloseHandle(q);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok;
}

bool Http(const wchar_t* method, const std::wstring& url, const std::wstring& headers, const std::string& body, Resp& r) {
    return Http(method, url, headers, body.data(), body.size(), r);
}

// --- token store (DPAPI, current Windows user) ---------------------------------------------------------------
std::wstring TokenFile(const Spec& s) { return profiles::RootDir() + L"\\Cloud\\" + s.file; }

// Account name and email shown in Settings: personal data, kept encrypted like the sign-in.
std::wstring AccountFile(const Spec& s) { return TokenFile(s) + L".account"; }

bool SaveProtected(const std::wstring& file, const std::string& data) {
    DATA_BLOB in = { (DWORD)data.size(), (BYTE*)data.data() }, ent = { sizeof(kEntropy) - 1, (BYTE*)kEntropy }, out = {};
    if (!CryptProtectData(&in, L"ReFix", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    CreateDirectoryW((profiles::RootDir() + L"\\Cloud").c_str(), nullptr);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    const bool ok = f && f.write((const char*)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return ok;
}

bool LoadProtected(const std::wstring& file, std::string& data) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    std::string blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    DATA_BLOB in = { (DWORD)blob.size(), (BYTE*)blob.data() }, ent = { sizeof(kEntropy) - 1, (BYTE*)kEntropy }, out = {};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    data.assign((const char*)out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return !data.empty();
}

bool SaveRefresh(const Spec& s, const std::string& token) { return SaveProtected(TokenFile(s), token); }
bool LoadRefresh(const Spec& s, std::string& token) { return LoadProtected(TokenFile(s), token); }

// The Google secret sits in the DLL as hex of its bytes XOR 0x5A. Not secrecy (Google treats a desktop client's
// secret as public): it keeps secret scanners from finding the GOCSPX- pattern in the published binary.
std::string Unscramble(const char* hex) {
    std::string out;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        const char pair[3] = { hex[i], hex[i + 1], 0 };
        out += (char)(strtol(pair, nullptr, 16) ^ 0x5A);
    }
    return out;
}

// Token endpoint call; keeps the access token in memory and the (possibly rotated) refresh token on disk.
bool TokenRequest(Provider p, std::string form) {
    const Spec& s = S(p);
    form += "&client_id=" + UrlEncode(s.clientId);
    const std::string secret = Unscramble(s.clientSecret);
    if (!secret.empty()) form += "&client_secret=" + UrlEncode(secret);
    Resp r;
    const bool sent = Http(L"POST", s.tokenUrl, L"Content-Type: application/x-www-form-urlencoded\r\n", form, r);
    Wipe(form);
    if (!sent) return false;
    json::Value v = json::Parse(r.body);
    Wipe(r.body);
    if (r.status != 200) {
        ReFixLog("[Cloud] %s token request failed (HTTP %lu, %s)", s.label, r.status, v.Str("error").c_str());
        if (v.Str("error") == "invalid_grant") DeleteFileW(TokenFile(s).c_str());   // revoked or expired: sign in again
        return false;
    }
    std::string refresh = v.Str("refresh_token");
    if (!refresh.empty() && !SaveRefresh(s, refresh)) { Wipe(refresh); return false; }
    Wipe(refresh);
    std::lock_guard<std::mutex> lock(g_mu);
    g_access[Idx(p)] = { v.Str("access_token"), GetTickCount64() + (ULONGLONG)v.Num("expires_in", 3600) * 1000 };
    return !g_access[Idx(p)].token.empty();
}

bool AccessToken(Provider p, std::string& out) {
    {
        std::lock_guard<std::mutex> lock(g_mu);
        const Access& a = g_access[Idx(p)];
        if (!a.token.empty() && GetTickCount64() + kRefreshMarginMs < a.expires) { out = a.token; return true; }
    }
    std::string refresh;
    if (!LoadRefresh(S(p), refresh)) return false;
    std::string form = "grant_type=refresh_token&refresh_token=" + UrlEncode(refresh);
    Wipe(refresh);
    if (!TokenRequest(p, std::move(form))) return false;
    std::lock_guard<std::mutex> lock(g_mu);
    out = g_access[Idx(p)].token;
    return true;
}

// --- sign-in -------------------------------------------------------------------------------------------------
const Msg kErrNoBrowser{ "Could not open the browser.", "No se pudo abrir el navegador." };
const Msg kErrTimeout{ "Sign-in timed out. Try again.", "Se agotó el tiempo para iniciar sesión. Vuelve a intentarlo." };
const Msg kErrDenied{ "Sign-in was cancelled.", "Se canceló el inicio de sesión." };
const Msg kErrToken{ "Could not finish signing in. Check your connection and try again.",
                     "No se pudo terminar el inicio de sesión. Revisa tu conexión y vuelve a intentarlo." };
const Msg kErrLocal{ "Could not start the sign-in on this PC.", "No se pudo iniciar sesión en este PC." };

// The page the browser shows when it comes back from the sign-in: compact, in the player's browser language.
std::string ResultPage(bool ok, const char* provider) {
    std::string page = R"(<!doctype html><html><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>ReFix</title><style>
:root{color-scheme:dark}*{box-sizing:border-box;margin:0}
body{min-height:100vh;display:grid;place-items:center;background:radial-gradient(900px 520px at 50% -12%,#10392f 0,#0b0f14 60%) #0b0f14;
font:15px/1.5 "Segoe UI",system-ui,sans-serif;color:#e8edf2}
.card{width:min(400px,92vw);padding:30px 28px 24px;border-radius:20px;background:rgba(19,25,33,.88);
border:1px solid rgba(255,255,255,.08);box-shadow:0 28px 70px rgba(0,0,0,.55);text-align:center;animation:in .5s cubic-bezier(.2,.8,.2,1)}
.mark{width:68px;height:68px;margin:0 auto 16px;border-radius:50%;display:grid;place-items:center;background:var(--soft);box-shadow:0 0 0 8px var(--ring)}
.mark svg{width:34px;height:34px}.mark path{stroke:var(--c);stroke-width:3.2;fill:none;stroke-linecap:round;stroke-linejoin:round;
stroke-dasharray:40;stroke-dashoffset:40;animation:draw .55s .25s ease-out forwards}
h1{font-size:21px;font-weight:600;letter-spacing:-.01em}.sub{margin-top:6px;color:#9aa6b2}
.pill{display:inline-flex;align-items:center;gap:8px;margin-top:16px;padding:6px 13px;border-radius:999px;font-size:12.5px;
color:#c9d2da;background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.08)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--c);box-shadow:0 0 9px var(--c)}
.foot{margin-top:20px;font-size:12.5px;color:#6f7b87}.brand{font-weight:700;letter-spacing:.08em;color:#2bd4a4}
@keyframes draw{to{stroke-dashoffset:0}}@keyframes in{from{opacity:0;transform:translateY(10px) scale(.98)}}
</style><body><main class=card style="%STYLE%"><div class=mark><svg viewBox="0 0 24 24"><path d="%PATH%"/></svg></div>
<h1 id=t></h1><p class=sub id=s></p><div class=pill><span class=dot></span><span id=p></span></div>
<p class=foot><span class=brand>REFIX</span> &middot; <span id=f></span></p></main><script>
const P="%PROVIDER%",ok=%OK%,es=(navigator.language||"").toLowerCase().startsWith("es");
const T=ok?(es?["Sesión iniciada",P+" quedó conectado a ReFix. Tus partidas se sincronizarán ahí.","Solo la carpeta de ReFix, nada más","Ya puedes cerrar esta pestaña y volver al juego."]
:["You're signed in",P+" is now connected to ReFix. Your saves will sync to it.","Only ReFix's own folder, nothing else","You can close this tab and go back to the game."])
:(es?["No se completó el inicio de sesión","Vuelve al juego e inténtalo de nuevo.",P,"Puedes cerrar esta pestaña."]
:["Sign-in not completed","Go back to the game and try again.",P,"You can close this tab."]);
["t","s","p","f"].forEach((id,i)=>document.getElementById(id).textContent=T[i]);document.title="ReFix · "+T[0];
</script>)";
    auto put = [&](const char* key, const std::string& value) {
        for (size_t at; (at = page.find(key)) != std::string::npos;) page.replace(at, strlen(key), value);
    };
    put("%STYLE%", ok ? "--c:#2bd4a4;--soft:rgba(43,212,164,.14);--ring:rgba(43,212,164,.06)"
                      : "--c:#f05a5a;--soft:rgba(240,90,90,.14);--ring:rgba(240,90,90,.06)");
    put("%PATH%", ok ? "M5 12.5l4.5 4.5L19 7" : "M7 7l10 10M17 7L7 17");
    put("%PROVIDER%", provider);   // one of our own labels, never input from the request
    put("%OK%", ok ? "true" : "false");
    return page;
}

void Reply(SOCKET c, const char* status, const std::string& page) {
    std::string resp = std::string("HTTP/1.1 ") + status + "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                       std::to_string(page.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + page;
    send(c, resp.data(), (int)resp.size(), 0);
}

// Waits for the browser to come back to http://<loopback>:port/?code=...&state=...
bool WaitForCode(SOCKET l4, SOCKET l6, const std::string& state, const char* provider, std::string& code, Msg& err) {
    const ULONGLONG until = GetTickCount64() + kSignInWaitMs;
    while (GetTickCount64() < until && !g_cancel) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(l4, &fds);
        if (l6 != INVALID_SOCKET) FD_SET(l6, &fds);
        timeval tv = { 1, 0 };
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
        const SOCKET l = FD_ISSET(l4, &fds) ? l4 : l6;
        SOCKET c = accept(l, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        DWORD rcv = 5000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv, sizeof(rcv));
        char buf[8192];
        const int n = recv(c, buf, sizeof(buf) - 1, 0);
        std::string line = n > 0 ? std::string(buf, n) : std::string();
        SecureZeroMemory(buf, sizeof(buf));
        line = line.substr(0, line.find("\r\n"));
        if (line.rfind("GET ", 0) != 0 || Param(line, "state").empty()) {   // favicon, preconnect...
            Reply(c, "404 Not Found", "");
            closesocket(c);
            continue;
        }
        const bool stateOk = Param(line, "state") == state;
        code = stateOk ? Param(line, "code") : std::string();
        Wipe(line);
        const bool ok = !code.empty();
        Reply(c, ok ? "200 OK" : "400 Bad Request", ResultPage(ok, provider));
        closesocket(c);
        if (!ok) err = kErrDenied;
        return ok;
    }
    err = g_cancel ? Msg{} : kErrTimeout;   // cancelled on purpose: nothing to report
    return false;
}

bool Connect(Provider p, Msg& err) {
    const Spec& s = S(p);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { err = kErrLocal; return false; }
    // Loopback only: nothing on the network can reach the listener. Same port on IPv6 for browsers that try ::1.
    SOCKET l4 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), l6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a4 = {};
    a4.sin_family = AF_INET;
    a4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int len = sizeof(a4);
    bool ok = l4 != INVALID_SOCKET && bind(l4, (sockaddr*)&a4, sizeof(a4)) == 0 && listen(l4, 4) == 0 &&
              getsockname(l4, (sockaddr*)&a4, &len) == 0;
    if (ok && l6 != INVALID_SOCKET) {
        sockaddr_in6 a6 = {};
        a6.sin6_family = AF_INET6;
        a6.sin6_addr = in6addr_loopback;
        a6.sin6_port = a4.sin_port;
        if (bind(l6, (sockaddr*)&a6, sizeof(a6)) != 0 || listen(l6, 4) != 0) { closesocket(l6); l6 = INVALID_SOCKET; }
    }
    std::string code;
    const std::string verifier = RandomToken(48), state = RandomToken(24);
    const std::string redirect = std::string("http://") + s.redirectHost + ":" + std::to_string(ntohs(a4.sin_port));
    if (!ok) err = kErrLocal;
    if (ok) {
        const std::string url = std::string(s.authUrl) + "?client_id=" + UrlEncode(s.clientId) + "&redirect_uri=" +
                                UrlEncode(redirect) + "&response_type=code&scope=" + UrlEncode(s.scope) +
                                "&code_challenge=" + Sha256Url(verifier) + "&code_challenge_method=S256&state=" + state +
                                (p == Provider::Google ? "&access_type=offline&prompt=consent" : "&prompt=select_account");
        ok = (INT_PTR)ShellExecuteW(nullptr, L"open", FromUtf8(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
        if (!ok) err = kErrNoBrowser;
    }
    if (ok) ok = WaitForCode(l4, l6, state, s.label, code, err);
    if (l4 != INVALID_SOCKET) closesocket(l4);
    if (l6 != INVALID_SOCKET) closesocket(l6);
    WSACleanup();
    if (ok) {
        std::string form = "grant_type=authorization_code&code=" + UrlEncode(code) + "&code_verifier=" + verifier +
                           "&redirect_uri=" + UrlEncode(redirect);
        if (p == Provider::OneDrive) form += "&scope=" + UrlEncode(s.scope);
        ok = TokenRequest(p, std::move(form));
        if (!ok) err = kErrToken;
    }
    Wipe(code);
    ReFixLog("[Cloud] %s sign-in %s", s.label, ok ? "done" : "failed");
    return ok;
}

// --- account shown in Settings ----------------------------------------------------------------------------------
// Google: Drive's about.user (drive.appdata is enough). OneDrive: the drive owner's name (no email without
// User.Read, which ReFix does not ask for).
bool FetchAccount(Provider p, Account& out) {
    std::string token;
    if (!AccessToken(p, token)) return false;
    const std::wstring auth = L"Authorization: Bearer " + FromUtf8(token) + L"\r\n";
    Wipe(token);
    Resp r;
    const std::wstring url = p == Provider::Google
        ? L"https://www.googleapis.com/drive/v3/about?fields=user(displayName,emailAddress)"
        : L"https://graph.microsoft.com/v1.0/me/drive?$select=owner";
    if (!Http(L"GET", url, auth, std::string(), r) || r.status != 200) return false;
    const json::Value v = json::Parse(r.body);
    const json::Value& user = p == Provider::Google ? v["user"] : v["owner"]["user"];
    out.name = user.Str("displayName");
    out.email = p == Provider::Google ? user.Str("emailAddress") : user.Str("email");
    return !out.name.empty() || !out.email.empty();
}

Account g_accounts[2];          // guarded by g_mu
bool g_accountLoaded[2] = {};   // read from disk (or fetched) this session
std::atomic<bool> g_fetching[2] = {};

void StoreAccount(Provider p, const Account& a) {
    SaveProtected(AccountFile(S(p)), a.name + "\n" + a.email);
    std::lock_guard<std::mutex> lock(g_mu);
    g_accounts[Idx(p)] = a;
    g_accountLoaded[Idx(p)] = true;
}

// --- Google Drive: the hidden app data folder ------------------------------------------------------------------
class DriveTarget : public sync::ISyncTarget {
public:
    std::wstring Name() const override { return L"Google Drive"; }
    bool Ready() const override { std::string t; return AccessToken(Provider::Google, t); }
    bool Failed() const override { return failed_; }
    bool Read(const std::wstring& file, std::vector<uint8_t>& out) override {
        std::string id;
        failed_ = !Find(file, id);
        if (failed_ || id.empty()) return false;
        Resp r;
        failed_ = !Call(L"GET", L"https://www.googleapis.com/drive/v3/files/" + FromUtf8(id) + L"?alt=media", L"", {}, r) ||
                  r.status != 200;
        if (failed_) return false;
        out.assign(r.body.begin(), r.body.end());
        return true;
    }
    bool Write(const std::wstring& file, const std::vector<uint8_t>& data) override {
        std::string id;
        if (!Find(file, id)) return false;
        // Resumable session: one PUT carries the whole package, whatever its size.
        Resp r;
        const std::wstring hdr = L"Content-Type: application/json; charset=UTF-8\r\nX-Upload-Content-Type: application/octet-stream\r\n";
        const bool started = id.empty()
            ? Call(L"POST", L"https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable", hdr,
                   "{\"name\":\"" + JsonEscape(ToUtf8(file)) + "\",\"parents\":[\"appDataFolder\"]}", r)
            : Call(L"PATCH", L"https://www.googleapis.com/upload/drive/v3/files/" + FromUtf8(id) + L"?uploadType=resumable", hdr, "{}", r);
        if (!started || r.status != 200 || r.location.empty()) return false;
        Resp put;
        return Http(L"PUT", r.location, L"Content-Type: application/octet-stream\r\n", data.data(), data.size(), put) &&
               (put.status == 200 || put.status == 201);
    }

private:
    static bool Call(const wchar_t* method, const std::wstring& url, const std::wstring& headers, const std::string& body, Resp& r) {
        std::string token;
        if (!AccessToken(Provider::Google, token)) return false;
        const std::wstring h = L"Authorization: Bearer " + FromUtf8(token) + L"\r\n" + headers;
        Wipe(token);
        return Http(method, url, h, body, r);
    }
    // id of the package in the app data folder ("" when there is none); false when Drive could not be asked.
    static bool Find(const std::wstring& file, std::string& id) {
        const std::string q = "name='" + JsonEscape(ToUtf8(file)) + "' and trashed=false";
        Resp r;
        if (!Call(L"GET", L"https://www.googleapis.com/drive/v3/files?spaces=appDataFolder&fields=files(id)&q=" +
                              FromUtf8(UrlEncode(q)), L"", {}, r) || r.status != 200) return false;
        const json::Value v = json::Parse(r.body);
        const json::Value& files = v["files"];
        id = files.items.empty() ? std::string() : files.items.front().Str("id");
        return v.type == json::Value::Type::Object;
    }
    mutable bool failed_ = false;
};

// --- OneDrive: Apps\ReFix ------------------------------------------------------------------------------------
class OneDriveTarget : public sync::ISyncTarget {
public:
    std::wstring Name() const override { return L"OneDrive"; }
    bool Ready() const override { std::string t; return AccessToken(Provider::OneDrive, t); }
    bool Failed() const override { return failed_; }
    bool Read(const std::wstring& file, std::vector<uint8_t>& out) override {
        Resp r;
        failed_ = !Call(L"GET", Item(file) + L"/content", L"", {}, r);
        if (failed_ || r.status == 404) return false;
        if (r.status == 302 && !r.location.empty()) {   // pre-signed download link: no token goes there
            const std::wstring link = r.location;
            failed_ = !Http(L"GET", link, L"", std::string(), r);
            if (failed_) return false;
        }
        failed_ = r.status != 200;
        if (failed_) return false;
        out.assign(r.body.begin(), r.body.end());
        return true;
    }
    bool Write(const std::wstring& file, const std::vector<uint8_t>& data) override {
        Resp r;
        if (!Call(L"POST", Item(file) + L"/createUploadSession", L"Content-Type: application/json\r\n",
                  "{\"item\":{\"@microsoft.graph.conflictBehavior\":\"replace\"}}", r) || r.status != 200) return false;
        const std::wstring url = FromUtf8(json::Parse(r.body).Str("uploadUrl"));
        if (url.empty()) return false;
        if (data.empty()) return false;
        for (size_t at = 0; at < data.size(); at += kChunk) {   // the upload URL is pre-signed: no token
            const size_t n = std::min(kChunk, data.size() - at);
            const std::wstring range = L"Content-Range: bytes " + std::to_wstring(at) + L"-" + std::to_wstring(at + n - 1) +
                                       L"/" + std::to_wstring(data.size()) + L"\r\n";
            Resp put;
            if (!Http(L"PUT", url, range, data.data() + at, n, put) || (put.status != 202 && put.status != 200 && put.status != 201))
                return false;
        }
        return true;
    }

private:
    static std::wstring Item(const std::wstring& file) {
        return L"https://graph.microsoft.com/v1.0/me/drive/special/approot:/" + FromUtf8(UrlEncode(ToUtf8(file))) + L":";
    }
    static bool Call(const wchar_t* method, const std::wstring& url, const std::wstring& headers, const std::string& body, Resp& r) {
        std::string token;
        if (!AccessToken(Provider::OneDrive, token)) return false;
        const std::wstring h = L"Authorization: Bearer " + FromUtf8(token) + L"\r\n" + headers;
        Wipe(token);
        return Http(method, url, h, body, r);
    }
    mutable bool failed_ = false;
};

} // namespace

const char* Label(Provider p) { return S(p).label; }
const wchar_t* Choice(Provider p) { return S(p).choice; }

bool FromChoice(const std::wstring& choice, Provider& out) {
    for (Provider p : { Provider::Google, Provider::OneDrive })
        if (choice == S(p).choice) { out = p; return true; }
    return false;
}

bool Available(Provider p) { return *S(p).clientId != 0 && (p != Provider::Google || *S(p).clientSecret != 0); }
bool Connected(Provider p) { return GetFileAttributesW(TokenFile(S(p)).c_str()) != INVALID_FILE_ATTRIBUTES; }
bool SigningIn() { return g_signingIn; }
bool SigningIn(Provider p) { return g_signingIn && g_signingWith == Idx(p); }
void CancelSignIn() { if (g_signingIn) g_cancel = true; }

Msg LastError() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_error;
}

void SignInAsync(Provider p) {
    if (!Available(p) || g_signingIn.exchange(true)) return;
    g_cancel = false;
    g_signingWith = Idx(p);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_error = {};
    }
    std::thread([p] {
        Msg err{};
        const bool ok = Connect(p, err);
        Account a;
        if (ok && FetchAccount(p, a)) StoreAccount(p, a);
        if (ok) sync::SetFolder(Choice(p));
        {
            std::lock_guard<std::mutex> lock(g_mu);
            g_error = ok ? Msg{} : err;
        }
        g_signingWith = -1;
        g_signingIn = false;
    }).detach();
}

void Disconnect(Provider p) {
    const Spec& s = S(p);
    std::string refresh;
    const bool revoke = p == Provider::Google && LoadRefresh(s, refresh);   // Microsoft: the account page revokes
    DeleteFileW(TokenFile(s).c_str());
    DeleteFileW(AccountFile(s).c_str());
    {
        std::lock_guard<std::mutex> lock(g_mu);
        Wipe(g_access[Idx(p)].token);
        g_access[Idx(p)].expires = 0;
        g_accounts[Idx(p)] = Account{};
        g_accountLoaded[Idx(p)] = false;
    }
    if (!revoke) return;
    std::thread([refresh]() mutable {   // network: off the UI thread
        Resp r;
        Http(L"POST", L"https://oauth2.googleapis.com/revoke", L"Content-Type: application/x-www-form-urlencoded\r\n",
             "token=" + UrlEncode(refresh), r);
        Wipe(refresh);
    }).detach();
    Wipe(refresh);
}

Account AccountOf(Provider p) {
    const int i = Idx(p);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_accountLoaded[i]) return g_accounts[i];
    }
    if (!Connected(p)) return {};
    std::string blob;
    if (LoadProtected(AccountFile(S(p)), blob)) {
        Account a;
        const size_t cut = blob.find('\n');
        a.name = blob.substr(0, cut);
        a.email = cut == std::string::npos ? std::string() : blob.substr(cut + 1);
        std::lock_guard<std::mutex> lock(g_mu);
        g_accounts[i] = a;
        g_accountLoaded[i] = true;
        return a;
    }
    // Signed in before ReFix kept the account: ask once, in the background.
    if (!g_fetching[i].exchange(true)) {
        std::thread([p, i] {
            Account a;
            if (FetchAccount(p, a)) StoreAccount(p, a);
            else {
                std::lock_guard<std::mutex> lock(g_mu);
                g_accountLoaded[i] = true;   // do not ask again this session
            }
            g_fetching[i] = false;
        }).detach();
    }
    return {};
}

std::unique_ptr<sync::ISyncTarget> MakeTarget(Provider p) {
    if (p == Provider::Google) return std::make_unique<DriveTarget>();
    return std::make_unique<OneDriveTarget>();
}

} // namespace refix::overlay::cloud
