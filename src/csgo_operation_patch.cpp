// Brings the operation missions that later CS:GO builds unlocked (Shattered Web weeks 5-16) into builds that still
// ship them as "locked": merges the embedded patch (tools/gen_csgo_operation_patch.py) into items_game.txt, the
// English/Spanish resource files and maps/cfg. Runs once per process for csgo.exe, keeps a .refix_orig backup of
// every file it changes, and is a no-op once the files are up to date.
#include "csgo_operation_patch.h"

#include <windows.h>
#include <atomic>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "mods/miniz/miniz.h"
#include "games/csgo/operation_patch_data.h"

extern void ReFixLog(const char* fmt, ...);

namespace refix::csgo {

namespace {

std::wstring GameDir() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1) + L"csgo\\";
}

bool ReadAll(const std::wstring& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

// Writes the new contents, keeping the first original as <file>.refix_orig.
bool WriteWithBackup(const std::wstring& path, const std::string& data) {
    std::wstring backup = path + L".refix_orig";
    if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES && !CopyFileW(path.c_str(), backup.c_str(), TRUE)) {
        ReFixLog("[CSGOPatch] Could not back up %ls, leaving it untouched", path.c_str());
        return false;
    }
    std::wstring tmp = path + L".refix_tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), (std::streamsize)data.size());
        if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

std::wstring Utf8ToWide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Body end (index of the closing brace) of the last top-level `"name" {` block, or npos.
size_t LastSectionEnd(const std::string& text, const std::string& name, const std::string& nl) {
    std::string header = nl + "\t\"" + name + "\"" + nl + "\t{";
    size_t start = text.rfind(header);
    if (start == std::string::npos) return std::string::npos;
    int depth = 0;
    for (size_t i = start + header.size() - 1; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}' && --depth == 0) return i;
    }
    return std::string::npos;
}

bool PatchItemsGame(const nlohmann::json& patch) {
    std::wstring path = GameDir() + L"scripts\\items\\items_game.txt";
    std::string text;
    if (!ReadAll(path, text)) return false;
    const std::string nl = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    bool changed = false;

    // Mission cards: "quests" "locked" -> the quest list of the later build.
    size_t ops = text.rfind("\"seasonaloperations\"");
    if (ops != std::string::npos) {
        for (auto it = patch["cards"].begin(); it != patch["cards"].end(); ++it) {
            size_t id = text.find("\"id\"\t\t\"" + it.key() + "\"", ops);
            if (id == std::string::npos) continue;
            size_t q = text.find("\"quests\"", id);
            size_t v1 = q == std::string::npos ? q : text.find('"', q + 8);
            size_t v2 = v1 == std::string::npos ? v1 : text.find('"', v1 + 1);
            if (v2 == std::string::npos || text.compare(v1 + 1, v2 - v1 - 1, "locked") != 0) continue;
            text.replace(v1 + 1, v2 - v1 - 1, it.value().get<std::string>());
            changed = true;
        }
    }

    // Quest definitions the cards now point at. They are added together with the cards, so a file whose cards were
    // already unlocked (patched before, or a later build) is left alone.
    std::string blocks;
    for (auto it = patch["quests"].begin(); changed && it != patch["quests"].end(); ++it) {
        std::string block = it.value().get<std::string>();
        if (nl == "\r\n") {
            std::string conv;
            for (char c : block) { if (c == '\n') conv += '\r'; conv += c; }
            block = conv;
        }
        blocks += block + nl;
    }
    if (!blocks.empty()) {
        size_t end = LastSectionEnd(text, "quest_definitions", nl);
        if (end != std::string::npos) {
            size_t lineStart = text.rfind(nl, end);
            text.insert(lineStart + nl.size(), blocks);
            changed = true;
        }
    }
    if (!changed) return true;
    bool ok = WriteWithBackup(path, text);
    ReFixLog("[CSGOPatch] items_game.txt: operation missions %s", ok ? "added" : "NOT written");
    return ok;
}

bool PatchResource(const std::string& lang, const nlohmann::json& tokens) {
    if (!tokens.is_object() || tokens.empty()) return true;
    std::wstring path = GameDir() + L"resource\\csgo_" + Utf8ToWide(lang) + L".txt";
    std::string raw;
    if (!ReadAll(path, raw) || raw.size() < 4 || (unsigned char)raw[0] != 0xFF || (unsigned char)raw[1] != 0xFE) return false;
    std::wstring text((const wchar_t*)(raw.data() + 2), (raw.size() - 2) / 2);

    std::wstring lines;
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        std::wstring key = Utf8ToWide(it.key());
        if (text.find(L"\"" + key + L"\"") != std::wstring::npos) continue;
        lines += L"\"" + key + L"\"\t\"" + Utf8ToWide(it.value().get<std::string>()) + L"\"\r\n";
    }
    if (lines.empty()) return true;
    // The file ends with the Tokens block and the lang block: insert before the Tokens block's closing brace.
    size_t langClose = text.rfind(L'}');
    size_t tokensClose = langClose == std::wstring::npos ? langClose : text.rfind(L'}', langClose - 1);
    if (tokensClose == std::wstring::npos) return false;
    text.insert(tokensClose, lines);

    std::string out("\xFF\xFE", 2);
    out.append((const char*)text.data(), text.size() * 2);
    bool ok = WriteWithBackup(path, out);
    ReFixLog("[CSGOPatch] csgo_%s.txt: mission strings %s", lang.c_str(), ok ? "added" : "NOT written");
    return ok;
}

void WriteCfgs(const nlohmann::json& cfgs) {
    for (auto it = cfgs.begin(); it != cfgs.end(); ++it) {
        std::wstring rel = Utf8ToWide(it.key());
        for (auto& c : rel) if (c == L'/') c = L'\\';
        std::wstring path = GameDir() + rel;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        std::ofstream f(path, std::ios::binary);
        std::string body = it.value().get<std::string>();
        f.write(body.data(), (std::streamsize)body.size());
        ReFixLog("[CSGOPatch] Wrote %s", it.key().c_str());
    }
}

} // namespace

void ApplyOperationPatch() {
    static std::atomic<bool> s_done{ false };
    if (s_done.exchange(true)) return;
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring name = exe;
    name = name.substr(name.find_last_of(L"\\/") + 1);
    if (_wcsicmp(name.c_str(), L"csgo.exe") != 0) return;

    std::string json(kOperationPatchRawSize, '\0');
    mz_ulong len = (mz_ulong)json.size();
    if (mz_uncompress((unsigned char*)json.data(), &len, kOperationPatchZ, (mz_ulong)sizeof(kOperationPatchZ)) != MZ_OK) {
        ReFixLog("[CSGOPatch] Embedded operation patch is corrupt");
        return;
    }
    json.resize(len);
    nlohmann::json patch = nlohmann::json::parse(json, nullptr, false);
    if (!patch.is_object()) return;

    try {
        if (!PatchItemsGame(patch)) return;  // strings and cfgs only matter once the missions exist
        for (auto it = patch["tokens"].begin(); it != patch["tokens"].end(); ++it) PatchResource(it.key(), it.value());
        WriteCfgs(patch["cfgs"]);
    } catch (...) {
        ReFixLog("[CSGOPatch] Failed to apply the operation patch");
    }
}

} // namespace refix::csgo
