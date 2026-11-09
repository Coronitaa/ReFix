// Profile sync straight to the player's Google Drive or OneDrive account, without their desktop app.
//
// Least privilege: Google scope drive.appdata and Microsoft scope Files.ReadWrite.AppFolder, so ReFix only sees
// its own folder (hidden in Google Drive, Apps\ReFix in OneDrive), never the player's files, mail or profile.
// Sign-in is OAuth 2.0 with PKCE through the player's browser and a one-shot listener on the loopback address.
// The refresh token is kept encrypted with Windows DPAPI for the current Windows user (%APPDATA%\ReFix\Cloud);
// access tokens live in memory only. No token is ever written to a log.
#pragma once
#include "profile_sync.h"

#include <memory>
#include <string>

namespace refix::overlay::cloud {

enum class Provider { Google, OneDrive };

struct Msg { const char* en; const char* es; };   // the overlay picks the language

const char* Label(Provider p);                     // "Google Drive", "OneDrive"
const wchar_t* Choice(Provider p);                 // value for sync::SetFolder
bool FromChoice(const std::wstring& choice, Provider& out);

bool Available(Provider p);   // this build carries ReFix's client ID for it
bool Connected(Provider p);   // a saved sign-in for this Windows user

// Opens the browser to sign in, on a background thread; when it succeeds, profiles sync to that account.
void SignInAsync(Provider p);
bool SigningIn();
Msg LastError();              // of the last sign-in; en == nullptr when none
void Disconnect(Provider p);  // forgets the sign-in on this PC (and revokes it, where the provider allows)

struct Account { std::string name, email; };   // UTF-8; email may be empty (OneDrive)
Account AccountOf(Provider p);                 // empty until known (fetched in the background after sign-in)

std::unique_ptr<sync::ISyncTarget> MakeTarget(Provider p);

} // namespace refix::overlay::cloud
