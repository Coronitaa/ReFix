// ReFix EOS v3 - EOS_PlayerDataStorage_* and EOS_TitleStorage_*.
//
// "Cloud" saves are kept in a folder next to the game, one directory per
// ProductUserId, so a title that gates its main menu on a cloud-save query gets
// a real answer instead of an error. The chunked transfer protocol is honoured
// exactly - the title's data callback is invoked chunk by chunk on the tick
// thread and its return value is respected - because titles use it to stream
// straight into their own buffers.
//
// The cloud slot is also kept in step with the engine's own local save. An
// Unreal title writes its progress to %LOCALAPPDATA%\<Project>\Saved\SaveGames
// under a slot named after the player ("cLeon_Default_<UserId>.sav") and then
// mirrors that blob into the cloud under the bare slot name ("cLeon_Default").
// Without a real service behind it the cloud half of that pair never appears,
// so the title reports its cloud state as an error even though the save on disk
// is perfectly healthy. SaveGameBridge closes the loop: whenever the title asks
// what is in the cloud, any local save belonging to this player that is newer
// than the cloud copy is promoted into it first. The player's own file is the
// only source - nothing is ever invented - and the local save is never written
// to, so the engine stays the sole owner of its own storage.
#include "../core/refix_common.h"
#include "../core/refix_log.h"
#include "../core/refix_config.h"
#include "../core/eos_handles.h"
#include "../core/eos_ids.h"
#include "../core/eos_identity.h"
#include "../core/eos_dispatch.h"
#include "../eos_module.h"

#include <fstream>
#include <algorithm>

// The SDK declares these as opaque; the emulator gives them meaning.
struct EOS_PlayerDataStorageFileTransferRequestHandle {
    uint32_t    Magic;
    std::string Filename;
    bool        Cancelled;
};
struct EOS_TitleStorageFileTransferRequestHandle {
    uint32_t    Magic;
    std::string Filename;
    bool        Cancelled;
};

using namespace refix;

namespace {

const uint32_t kMagicTransfer = 0x52465854u;   // 'RFXT'

struct FileEntry {
    std::string Name;
    uint32_t    Size = 0;
    int64_t     Modified = 0;
};

std::mutex               g_mutex;
std::vector<FileEntry>   g_files;          // last query result
std::string              g_rootCache;

// One directory per player keeps two local test instances from sharing saves.
const std::string& StorageRoot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_rootCache.empty()) {
        std::string dir = Config::Get().GetString("Storage", "LocalSave", "saves");
        std::string base = (dir.size() > 1 && dir[1] == ':') ? dir : GameDirectory() + dir;
        CreateDirectoryA(base.c_str(), nullptr);
        base += "\\" + Identity::Get().LocalUser().Puid;
        CreateDirectoryA(base.c_str(), nullptr);
        g_rootCache = base + "\\";
    }
    return g_rootCache;
}

// Filenames come from the title and are used to build a path, so anything that
// could escape the storage directory is rejected outright.
bool SafeName(const char* name, std::string& out) {
    if (!name || !*name) return false;
    out = name;
    if (out.size() > 255) return false;
    if (out.find("..") != std::string::npos) return false;
    for (char c : out) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 0x20)
            return false;
    }
    return true;
}

std::vector<uint8_t> ReadWholeFile(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    ok = f.is_open();
    if (!ok) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// --- bridge to the engine's own save directory -----------------------------

uint64_t FileTimeToU64(const FILETIME& ft) {
    ULARGE_INTEGER t{};
    t.LowPart  = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    return t.QuadPart;
}

// Size and last-write time of a file, or false if it is not there.
bool StatFile(const std::string& path, uint64_t& size, uint64_t& written) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fad)) return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return false;
    size    = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    written = FileTimeToU64(fad.ftLastWriteTime);
    return true;
}

// The Unreal project name. The shipping executable lives in
// <Project>\Binaries\<Platform>\, and the engine derives its user directory
// from that same project name, so one can be read off the other.
std::string ProjectName() {
    std::string cfg = Config::Get().GetString("Storage", "ProjectName", "");
    if (!cfg.empty()) return cfg;

    std::string dir = GameDirectory();
    while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/')) dir.pop_back();
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= dir.size(); ++i) {
        if (i == dir.size() || dir[i] == '\\' || dir[i] == '/') {
            if (i > start) parts.push_back(dir.substr(start, i - start));
            start = i + 1;
        }
    }
    for (size_t i = parts.size(); i-- > 1; ) {
        if (ToLower(parts[i]) == "binaries") return parts[i - 1];
    }
    return "";
}

// %LOCALAPPDATA%\<Project>\Saved\SaveGames\ - where the engine actually keeps
// the player's progress.
const std::string& SaveGamesDir() {
    static std::string cache = [] {
        std::string cfg = Config::Get().GetString("Storage", "SaveGamesDir", "");
        if (!cfg.empty()) {
            if (cfg.back() != '\\' && cfg.back() != '/') cfg += "\\";
            return cfg;
        }
        std::string project = ProjectName();
        if (project.empty()) return std::string();
        char local[MAX_PATH] = {0};
        DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", local, (DWORD)sizeof(local));
        if (n == 0 || n >= sizeof(local)) return std::string();
        return std::string(local) + "\\" + project + "\\Saved\\SaveGames\\";
    }();
    return cache;
}

// The suffixes the engine appends to a slot name for this player, most specific
// first: the ProductUserId this session logged in as, and - for a title that was
// played before on the same machine through Steam - the SteamID64 behind it.
std::vector<std::string> LocalSlotSuffixes() {
    std::vector<std::string> out;
    const UserRecord& me = Identity::Get().LocalUser();
    if (!me.Puid.empty()) out.push_back("_" + me.Puid);
    if (me.External.Valid && !me.External.AccountId.empty())
        out.push_back("_" + me.External.AccountId);
    return out;
}

bool g_bridgeAnnounced = false;

// Promotes the engine's own save into the cloud slot whenever the cloud copy is
// missing, empty, or older. Never writes to the engine's directory.
void SyncFromSaveGames() {
    if (!Config::Get().GetBool("Storage", "MirrorLocalSaveGames", true)) return;
    const std::string& saves = SaveGamesDir();
    if (saves.empty()) return;

    std::vector<std::string> suffixes = LocalSlotSuffixes();
    if (suffixes.empty()) return;   // nobody is logged in yet

    if (!g_bridgeAnnounced) {
        g_bridgeAnnounced = true;
        RFLOG(Core, "SaveGameBridge: engine save directory is %s (slot suffix '%s')",
              saves.c_str(), suffixes[0].c_str());
    }

    // slot name -> (source path, priority) with priority 0 = best match.
    std::map<std::string, std::pair<std::string, size_t>> best;

    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA((saves + "*.sav").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (fd.nFileSizeLow == 0 && fd.nFileSizeHigh == 0) continue;
        std::string file = fd.cFileName;
        if (file.size() < 5) continue;
        std::string stem = file.substr(0, file.size() - 4);          // drop ".sav"
        for (size_t p = 0; p < suffixes.size(); ++p) {
            const std::string& suffix = suffixes[p];
            if (stem.size() <= suffix.size()) continue;
            if (ToLower(stem.substr(stem.size() - suffix.size())) != ToLower(suffix)) continue;
            std::string slot = stem.substr(0, stem.size() - suffix.size());
            std::string src;
            if (!SafeName(slot.c_str(), src)) break;
            auto it = best.find(slot);
            if (it == best.end() || p < it->second.second)
                best[slot] = { saves + file, p };
            break;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    for (const auto& kv : best) {
        const std::string& slot = kv.first;
        const std::string& src  = kv.second.first;
        std::string dst = StorageRoot() + slot;

        uint64_t srcSize = 0, srcTime = 0;
        if (!StatFile(src, srcSize, srcTime) || srcSize == 0) continue;

        uint64_t dstSize = 0, dstTime = 0;
        const bool haveDst = StatFile(dst, dstSize, dstTime);
        // An empty cloud file is not a save; it is the residue of an older
        // build that answered "missing" with an empty blob. Drop it.
        if (haveDst && dstSize == 0) {
            DeleteFileA(dst.c_str());
            RFLOG(Core, "SaveGameBridge: discarded empty cloud file '%s'", slot.c_str());
        }
        if (haveDst && dstSize != 0 && dstTime >= srcTime) continue;   // cloud is current

        if (CopyFileA(src.c_str(), dst.c_str(), FALSE)) {
            RFLOG(Core, "SaveGameBridge: '%s' <- %s (%llu bytes)", slot.c_str(), src.c_str(),
                  (unsigned long long)srcSize);
        } else {
            RFLOG(Core, "SaveGameBridge: could not copy %s to '%s' (error %lu)",
                  src.c_str(), slot.c_str(), (unsigned long)GetLastError());
        }
    }
}

void RefreshFileList() {
    SyncFromSaveGames();
    std::vector<FileEntry> found;
    WIN32_FIND_DATAA fd{};
    std::string pattern = StorageRoot() + "*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            // An empty file is not a save. Listing one would tell a title its
            // cloud slot is populated and then hand it nothing.
            if (fd.nFileSizeLow == 0 && fd.nFileSizeHigh == 0) continue;
            FileEntry e;
            e.Name = fd.cFileName;
            e.Size = fd.nFileSizeLow;
            ULARGE_INTEGER t{};
            t.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
            t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            e.Modified = (int64_t)(t.QuadPart / 10000000ULL) - 11644473600LL;  // FILETIME -> Unix
            found.push_back(std::move(e));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_files.swap(found);
}

struct MetadataBlock {
    EOS_PlayerDataStorage_FileMetadata Meta;
    std::string Filename;
    std::string Hash;
};
std::mutex                                                     g_metaMutex;
std::map<EOS_PlayerDataStorage_FileMetadata*, MetadataBlock*>  g_metaBlocks;

EOS_PlayerDataStorage_FileMetadata* MakeMetadata(const FileEntry& e) {
    auto* block = new MetadataBlock();
    block->Filename = e.Name;
    block->Hash     = "";                    // no integrity service to mirror
    std::memset(&block->Meta, 0, sizeof(block->Meta));
    block->Meta.ApiVersion               = EOS_PLAYERDATASTORAGE_FILEMETADATA_API_LATEST;
    block->Meta.FileSizeBytes            = e.Size;
    block->Meta.UnencryptedDataSizeBytes = e.Size;
    block->Meta.MD5Hash                  = block->Hash.c_str();
    block->Meta.Filename                 = block->Filename.c_str();
    block->Meta.LastModifiedTime         = e.Modified;
    std::lock_guard<std::mutex> lock(g_metaMutex);
    g_metaBlocks[&block->Meta] = block;
    return &block->Meta;
}

} // namespace

// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(void) EOS_PlayerDataStorage_QueryFileList(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileListOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnQueryFileListCompleteCallback CompletionCallback) {
    RefreshFileList();
    uint32_t count;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        count = (uint32_t)g_files.size();
    }
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    RFLOG(Core, "PlayerDataStorage_QueryFileList -> %u file(s) in %s", count, StorageRoot().c_str());
    if (!CompletionCallback) return;
    Dispatcher::Get().Post([CompletionCallback, ClientData, user, count]() {
        EOS_PlayerDataStorage_QueryFileListCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = user;
        info.FileCount   = count;
        CompletionCallback(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_PlayerDataStorage_QueryFile(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnQueryFileCompleteCallback CompletionCallback) {
    RefreshFileList();
    std::string name;
    bool exists = false;
    if (Options && SafeName(Options->Filename, name)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& f : g_files) if (f.Name == name) { exists = true; break; }
    }
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    if (!CompletionCallback) return;
    Dispatcher::Get().Post([CompletionCallback, ClientData, user, exists]() {
        EOS_PlayerDataStorage_QueryFileCallbackInfo info{};
        info.ResultCode  = exists ? ER::EOS_Success : ER::EOS_NotFound;
        info.ClientData  = ClientData;
        info.LocalUserId = user;
        CompletionCallback(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorage_GetFileMetadataCount(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_GetFileMetadataCountOptions* Options, int32_t* OutFileMetadataCount) {
    if (!OutFileMetadataCount) return ER::EOS_InvalidParameters;
    std::lock_guard<std::mutex> lock(g_mutex);
    *OutFileMetadataCount = (int32_t)g_files.size();
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorage_CopyFileMetadataAtIndex(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_CopyFileMetadataAtIndexOptions* Options, EOS_PlayerDataStorage_FileMetadata** OutMetadata) {
    if (!Options || !OutMetadata) return ER::EOS_InvalidParameters;
    *OutMetadata = nullptr;
    FileEntry entry;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (Options->Index >= (uint32_t)g_files.size()) return ER::EOS_NotFound;
        entry = g_files[Options->Index];
    }
    *OutMetadata = MakeMetadata(entry);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorage_CopyFileMetadataByFilename(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_CopyFileMetadataByFilenameOptions* Options, EOS_PlayerDataStorage_FileMetadata** OutMetadata) {
    if (!Options || !OutMetadata) return ER::EOS_InvalidParameters;
    *OutMetadata = nullptr;
    std::string name;
    if (!SafeName(Options->Filename, name)) return ER::EOS_InvalidParameters;
    FileEntry entry;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = std::find_if(g_files.begin(), g_files.end(),
                               [&](const FileEntry& f) { return f.Name == name; });
        if (it == g_files.end()) return ER::EOS_NotFound;
        entry = *it;
    }
    *OutMetadata = MakeMetadata(entry);
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_PlayerDataStorage_FileMetadata_Release(EOS_PlayerDataStorage_FileMetadata* Metadata) {
    if (!Metadata) return;
    MetadataBlock* block = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_metaMutex);
        auto it = g_metaBlocks.find(Metadata);
        if (it == g_metaBlocks.end()) return;
        block = it->second;
        g_metaBlocks.erase(it);
    }
    delete block;
}

EOS_DECLARE_FUNC(EOS_HPlayerDataStorageFileTransferRequest) EOS_PlayerDataStorage_ReadFile(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_ReadFileOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnReadFileCompleteCallback CompletionCallback) {
    std::string name;
    const bool valid = Options && SafeName(Options->Filename, name);

    auto* request = new EOS_PlayerDataStorageFileTransferRequestHandle();
    request->Magic     = kMagicTransfer;
    request->Filename  = valid ? name : "";
    request->Cancelled = false;

    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    auto dataCallback = Options ? Options->ReadFileDataCallback : nullptr;
    uint32_t chunkSize = (Options && Options->ReadChunkLengthBytes) ? Options->ReadChunkLengthBytes : 64 * 1024;

    // Pick up anything the engine has written to its own save directory since
    // the last time we looked, so the cloud slot answers with the player's real
    // progress rather than with nothing.
    SyncFromSaveGames();

    bool ok = false;
    std::vector<uint8_t> data;
    if (valid) data = ReadWholeFile(StorageRoot() + name, ok);
    // A zero-length file is not a save. Older builds left one behind when they
    // answered a missing file with an empty blob; reading it back as a success
    // hands the title an empty buffer to deserialise, which is exactly what it
    // reports as a cloud error. Treat it as what it is: nothing.
    if (ok && data.empty()) {
        ok = false;
        DeleteFileA((StorageRoot() + name).c_str());
    }

    // A brand-new player has no cloud file yet, and the real service answers
    // EOS_NotFound - which is what we answer too. Handing back an empty file
    // instead is offered for titles that treat NotFound as a hard failure, but
    // it is off by default: an empty blob is not a save, and a title that
    // deserialises one is worse off than a title told the file is absent.
    const bool missingIsEmpty = ok ? false
        : Config::Get().GetBool("Storage", "TreatMissingFileAsEmpty", false);
    if (missingIsEmpty && valid) ok = true;

    Dispatcher::Get().Post([=]() mutable {
        ER result = ER::EOS_Success;
        if (!valid)      result = ER::EOS_InvalidParameters;
        else if (!ok)    result = ER::EOS_NotFound;
        else if (dataCallback) {
            // Hand the file over chunk by chunk, exactly as EOS does, and stop
            // if the title asks us to.
            size_t offset = 0;
            do {
                size_t remaining = data.size() - offset;
                size_t take = remaining < chunkSize ? remaining : chunkSize;
                EOS_PlayerDataStorage_ReadFileDataCallbackInfo chunk{};
                chunk.ClientData           = ClientData;
                chunk.LocalUserId          = user;
                chunk.Filename             = name.c_str();
                chunk.TotalFileSizeBytes   = (uint32_t)data.size();
                chunk.DataChunkLengthBytes = (uint32_t)take;
                chunk.DataChunk            = data.empty() ? nullptr : data.data() + offset;
                offset += take;
                chunk.bIsLastChunk = (offset >= data.size()) ? EOS_TRUE : EOS_FALSE;
                auto verdict = dataCallback(&chunk);
                if (verdict == EOS_PlayerDataStorage_EReadResult::EOS_RR_FailRequest) { result = ER::EOS_UnexpectedError; break; }
                if (verdict == EOS_PlayerDataStorage_EReadResult::EOS_RR_CancelRequest) { result = ER::EOS_Canceled; break; }
            } while (offset < data.size());
        }
        if (request->Cancelled) result = ER::EOS_Canceled;
        RFLOG(Core, "PlayerDataStorage_ReadFile('%s') -> %d (%zu bytes)%s", name.c_str(), (int)result, data.size(),
              (result == ER::EOS_NotFound) ? " [no cloud file yet - the same answer the real service gives]"
              : (missingIsEmpty && data.empty()) ? " [reported as empty by configuration]" : "");
        if (CompletionCallback) {
            EOS_PlayerDataStorage_ReadFileCallbackInfo info{};
            info.ResultCode  = result;
            info.ClientData  = ClientData;
            info.LocalUserId = user;
            info.Filename    = name.c_str();
            CompletionCallback(&info);
        }
    });
    return (EOS_HPlayerDataStorageFileTransferRequest)request;
}

EOS_DECLARE_FUNC(EOS_HPlayerDataStorageFileTransferRequest) EOS_PlayerDataStorage_WriteFile(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_WriteFileOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnWriteFileCompleteCallback CompletionCallback) {
    std::string name;
    const bool valid = Options && SafeName(Options->Filename, name);

    auto* request = new EOS_PlayerDataStorageFileTransferRequestHandle();
    request->Magic     = kMagicTransfer;
    request->Filename  = valid ? name : "";
    request->Cancelled = false;

    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    auto dataCallback = Options ? Options->WriteFileDataCallback : nullptr;
    uint32_t chunkSize = (Options && Options->ChunkLengthBytes) ? Options->ChunkLengthBytes : 64 * 1024;

    Dispatcher::Get().Post([=]() mutable {
        ER result = valid ? ER::EOS_Success : ER::EOS_InvalidParameters;
        if (valid && dataCallback) {
            std::vector<uint8_t> buffer(chunkSize);
            std::vector<uint8_t> whole;
            bool done = false;
            while (!done) {
                EOS_PlayerDataStorage_WriteFileDataCallbackInfo info{};
                info.ClientData            = ClientData;
                info.LocalUserId           = user;
                info.Filename              = name.c_str();
                info.DataBufferLengthBytes = chunkSize;
                uint32_t written = 0;
                auto verdict = dataCallback(&info, buffer.data(), &written);
                if (written > chunkSize) { result = ER::EOS_UnexpectedError; break; }
                whole.insert(whole.end(), buffer.begin(), buffer.begin() + written);
                if (verdict == EOS_PlayerDataStorage_EWriteResult::EOS_WR_FailRequest)   { result = ER::EOS_UnexpectedError; break; }
                if (verdict == EOS_PlayerDataStorage_EWriteResult::EOS_WR_CancelRequest) { result = ER::EOS_Canceled; break; }
                if (verdict == EOS_PlayerDataStorage_EWriteResult::EOS_WR_CompleteRequest) done = true;
            }
            if (result == ER::EOS_Success) {
                std::ofstream f(StorageRoot() + name, std::ios::binary | std::ios::trunc);
                if (!f.is_open()) result = ER::EOS_UnexpectedError;
                else if (!whole.empty()) f.write((const char*)whole.data(), (std::streamsize)whole.size());
            }
            RFLOG(Core, "PlayerDataStorage_WriteFile('%s') -> %d (%zu bytes)", name.c_str(), (int)result, whole.size());
        }
        if (CompletionCallback) {
            EOS_PlayerDataStorage_WriteFileCallbackInfo info{};
            info.ResultCode  = result;
            info.ClientData  = ClientData;
            info.LocalUserId = user;
            info.Filename    = name.c_str();
            CompletionCallback(&info);
        }
    });
    return (EOS_HPlayerDataStorageFileTransferRequest)request;
}

EOS_DECLARE_FUNC(void) EOS_PlayerDataStorage_DeleteFile(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_DeleteFileOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnDeleteFileCompleteCallback CompletionCallback) {
    std::string name;
    ER result = ER::EOS_InvalidParameters;
    if (Options && SafeName(Options->Filename, name))
        result = DeleteFileA((StorageRoot() + name).c_str()) ? ER::EOS_Success : ER::EOS_NotFound;
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    RefreshFileList();
    if (!CompletionCallback) return;
    Dispatcher::Get().Post([CompletionCallback, ClientData, user, result]() {
        EOS_PlayerDataStorage_DeleteFileCallbackInfo info{};
        info.ResultCode  = result;
        info.ClientData  = ClientData;
        info.LocalUserId = user;
        CompletionCallback(&info);
    });
}

EOS_DECLARE_FUNC(void) EOS_PlayerDataStorage_DuplicateFile(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_DuplicateFileOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnDuplicateFileCompleteCallback CompletionCallback) {
    std::string src, dst;
    ER result = ER::EOS_InvalidParameters;
    if (Options && SafeName(Options->SourceFilename, src) && SafeName(Options->DestinationFilename, dst))
        result = CopyFileA((StorageRoot() + src).c_str(), (StorageRoot() + dst).c_str(), FALSE)
                     ? ER::EOS_Success : ER::EOS_NotFound;
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    RefreshFileList();
    if (!CompletionCallback) return;
    Dispatcher::Get().Post([CompletionCallback, ClientData, user, result]() {
        EOS_PlayerDataStorage_DuplicateFileCallbackInfo info{};
        info.ResultCode  = result;
        info.ClientData  = ClientData;
        info.LocalUserId = user;
        CompletionCallback(&info);
    });
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorage_DeleteCache(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_DeleteCacheOptions* Options, void* ClientData, const EOS_PlayerDataStorage_OnDeleteCacheCompleteCallback CompletionCallback) {
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    if (CompletionCallback) {
        Dispatcher::Get().Post([CompletionCallback, ClientData, user]() {
            EOS_PlayerDataStorage_DeleteCacheCallbackInfo info{};
            info.ResultCode  = ER::EOS_Success;
            info.ClientData  = ClientData;
            info.LocalUserId = user;
            CompletionCallback(&info);
        });
    }
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorageFileTransferRequest_GetFileRequestState(EOS_HPlayerDataStorageFileTransferRequest Handle) {
    auto* r = (EOS_PlayerDataStorageFileTransferRequestHandle*)Handle;
    if (!r || r->Magic != kMagicTransfer) return ER::EOS_InvalidParameters;
    return r->Cancelled ? ER::EOS_Canceled : ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorageFileTransferRequest_GetFilename(EOS_HPlayerDataStorageFileTransferRequest Handle, uint32_t FilenameStringBufferSizeBytes, char* OutStringBuffer, int32_t* OutStringLength) {
    auto* r = (EOS_PlayerDataStorageFileTransferRequestHandle*)Handle;
    if (!r || r->Magic != kMagicTransfer || !OutStringBuffer || !OutStringLength) return ER::EOS_InvalidParameters;
    int32_t needed = (int32_t)r->Filename.size() + 1;
    if ((int32_t)FilenameStringBufferSizeBytes < needed) { *OutStringLength = needed; return ER::EOS_LimitExceeded; }
    std::memcpy(OutStringBuffer, r->Filename.c_str(), needed);
    *OutStringLength = needed;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(EOS_EResult) EOS_PlayerDataStorageFileTransferRequest_CancelRequest(EOS_HPlayerDataStorageFileTransferRequest Handle) {
    auto* r = (EOS_PlayerDataStorageFileTransferRequestHandle*)Handle;
    if (!r || r->Magic != kMagicTransfer) return ER::EOS_InvalidParameters;
    r->Cancelled = true;
    return ER::EOS_Success;
}

EOS_DECLARE_FUNC(void) EOS_PlayerDataStorageFileTransferRequest_Release(EOS_HPlayerDataStorageFileTransferRequest PlayerDataStorageFileTransferHandle) {
    auto* r = (EOS_PlayerDataStorageFileTransferRequestHandle*)PlayerDataStorageFileTransferHandle;
    if (!r || r->Magic != kMagicTransfer) return;
    r->Magic = 0;
    delete r;
}

// ---------------------------------------------------------------------------
// Title storage: read-only content published by the developer. There is none
// here, and an empty list is the correct, complete answer.
// ---------------------------------------------------------------------------
EOS_DECLARE_FUNC(void) EOS_TitleStorage_QueryFileList(EOS_HTitleStorage Handle, const EOS_TitleStorage_QueryFileListOptions* Options, void* ClientData, const EOS_TitleStorage_OnQueryFileListCompleteCallback CompletionCallback) {
    EOS_ProductUserId user = Options ? Options->LocalUserId : nullptr;
    if (!CompletionCallback) return;
    Dispatcher::Get().Post([CompletionCallback, ClientData, user]() {
        EOS_TitleStorage_QueryFileListCallbackInfo info{};
        info.ResultCode  = ER::EOS_Success;
        info.ClientData  = ClientData;
        info.LocalUserId = user;
        info.FileCount   = 0;
        CompletionCallback(&info);
    });
}

EOS_DECLARE_FUNC(uint32_t) EOS_TitleStorage_GetFileMetadataCount(EOS_HTitleStorage Handle, const EOS_TitleStorage_GetFileMetadataCountOptions* Options) {
    return 0;
}

namespace refix {

void RegisterStorageApi(Registrar& reg) {
    REFIX_BIND(reg, EOS_PlayerDataStorage_QueryFileList);
    REFIX_BIND(reg, EOS_PlayerDataStorage_QueryFile);
    REFIX_BIND(reg, EOS_PlayerDataStorage_GetFileMetadataCount);
    REFIX_BIND(reg, EOS_PlayerDataStorage_CopyFileMetadataAtIndex);
    REFIX_BIND(reg, EOS_PlayerDataStorage_CopyFileMetadataByFilename);
    REFIX_BIND(reg, EOS_PlayerDataStorage_FileMetadata_Release);
    REFIX_BIND(reg, EOS_PlayerDataStorage_ReadFile);
    REFIX_BIND(reg, EOS_PlayerDataStorage_WriteFile);
    REFIX_BIND(reg, EOS_PlayerDataStorage_DeleteFile);
    REFIX_BIND(reg, EOS_PlayerDataStorage_DuplicateFile);
    REFIX_BIND(reg, EOS_PlayerDataStorage_DeleteCache);
    REFIX_BIND(reg, EOS_PlayerDataStorageFileTransferRequest_GetFileRequestState);
    REFIX_BIND(reg, EOS_PlayerDataStorageFileTransferRequest_GetFilename);
    REFIX_BIND(reg, EOS_PlayerDataStorageFileTransferRequest_CancelRequest);
    REFIX_BIND(reg, EOS_PlayerDataStorageFileTransferRequest_Release);
    REFIX_BIND(reg, EOS_TitleStorage_QueryFileList);
    REFIX_BIND(reg, EOS_TitleStorage_GetFileMetadataCount);
}

} // namespace refix
