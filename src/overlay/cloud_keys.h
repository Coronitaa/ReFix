// OAuth client IDs of ReFix's own Google and Microsoft apps (docs/cloud-sync-setup.md says how to get them).
// They name the app, they do not open any account: every player still signs in and consents in the browser.
// Kept out of git in cloud_keys.local.h (copy cloud_keys.local.h.example); a build without it hides the buttons.
#pragma once

#if __has_include("cloud_keys.local.h")
#include "cloud_keys.local.h"
#endif

#ifndef REFIX_GOOGLE_CLIENT_ID
#define REFIX_GOOGLE_CLIENT_ID ""
#endif
// Google requires it from "Desktop app" clients; for installed apps Google itself treats it as public.
#ifndef REFIX_GOOGLE_CLIENT_SECRET
#define REFIX_GOOGLE_CLIENT_SECRET ""
#endif
#ifndef REFIX_MICROSOFT_CLIENT_ID
#define REFIX_MICROSOFT_CLIENT_ID ""
#endif
