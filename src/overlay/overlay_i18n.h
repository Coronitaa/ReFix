// Overlay languages: English (default) and Spanish. The choice is per PC, kept in
// %APPDATA%\ReFix\profiles.ini ([General] Language = en | es), and switched from the overlay.
// Strings stay at their call sites as T("English", "Español") pairs: two languages do not need a catalogue.
#pragma once
#include <cwchar>

namespace refix::overlay::i18n {

enum class Lang { En, Es };

Lang Current();
void Set(Lang lang);

inline bool Spanish() { return Current() == Lang::Es; }
inline const char* T(const char* en, const char* es) { return Spanish() ? es : en; }
inline const wchar_t* TW(const wchar_t* en, const wchar_t* es) { return Spanish() ? es : en; }

} // namespace refix::overlay::i18n
