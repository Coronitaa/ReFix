// Profile sync: every file of a profile (definition, picture, emulated Steam Cloud, stats and achievements,
// inventory, and the save folders it owns) in one package, kept in a destination the player chooses: a folder
// on this PC, a USB drive, or the folder of a cloud app (OneDrive, Google Drive, Dropbox), which uploads it.
// Steam profiles sync on their own (package named after the SteamID, so any PC with that account finds it);
// local profiles only when the player turns that on. Free at any scale: no ReFix server, the player's storage.
//
// Package (.rfxp): 128-byte header, then a zip XChaCha20-scrambled with a fixed key so it is not opened and
// edited by hand by mistake (this is not secrecy), and a BLAKE2b digest that rejects damaged packages.
#pragma once
#include "profile_store.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace refix::overlay::sync {

enum class State {
    Off,          // this profile does not sync (local profile with the option off, guest, no profile)
    Unavailable,  // no destination chosen, or it cannot be reached now (cloud app closed, USB unplugged)
    Pending,      // changes not uploaded yet
    Syncing,
    Synced,
    Failed,
};

struct Status {
    State state = State::Off;
    std::string error;            // UTF-8, English (overlay translates the known ones)
    uint64_t lastSync = 0;        // FILETIME of the last good sync on this PC
    std::wstring destination;     // shown to the player
};

// Where packages live. A cloud API (Google Drive, OneDrive) can implement this later.
class ISyncTarget {
public:
    virtual ~ISyncTarget() = default;
    virtual std::wstring Name() const = 0;
    virtual bool Ready() const = 0;
    virtual bool Read(const std::wstring& file, std::vector<uint8_t>& out) = 0;   // false when missing
    virtual bool Write(const std::wstring& file, const std::vector<uint8_t>& data) = 0;
};
std::unique_ptr<ISyncTarget> MakeFolderTarget(const std::wstring& folder);

struct CloudFolder {
    std::wstring label;           // "OneDrive", "Google Drive", "Dropbox"
    std::wstring path;
};
std::vector<CloudFolder> CloudFolders();         // the ones installed on this PC
constexpr const wchar_t* kLocalOnly = L"-";     // SetFolder value: keep profiles on this PC only
std::wstring ChosenFolder();                     // player's choice, "" = automatic, kLocalOnly = none
std::wstring EffectiveFolder();                  // choice, else first cloud folder + \ReFix Sync, else ""
void SetFolder(const std::wstring& folder);      // "" = automatic
bool SyncLocalProfiles();
void SetSyncLocalProfiles(bool on);

// Package format (no I/O).
using FileList = std::vector<std::pair<std::string, std::vector<uint8_t>>>;   // UTF-8 path with '/', bytes
bool Pack(const FileList& files, uint64_t steamId, const uint8_t stateHash[32], std::vector<uint8_t>& out);
bool Unpack(const std::vector<uint8_t>& pkg, FileList& files, uint64_t* steamId, uint8_t stateHash[32]);

// A profile's package as a file anywhere (USB, mail, another PC), and back. An imported package always becomes
// a new local profile, so no profile, save or backup that exists is ever replaced.
bool ExportTo(const Profile& p, const std::wstring& file, std::string& error);
std::optional<Profile> ImportAsNewProfile(const std::wstring& file, std::string& error);

void Begin(const Profile& active);   // game start, before the save folders switch: takes a newer package
Status Get();
void SyncNow();                      // asynchronous
void Shutdown();                     // game exit: last upload (bounded wait)

} // namespace refix::overlay::sync
