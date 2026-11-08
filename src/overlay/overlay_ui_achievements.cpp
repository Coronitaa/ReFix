// Achievements in the overlay: the corner notification when one unlocks (or progresses), the card with the
// most recent ones and the modal with all of them. Data comes from achievements.cpp (same store the game
// writes through ISteamUserStats), names and descriptions follow the overlay language.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "overlay_app.h"
#include "overlay_i18n.h"
#include "overlay_settings.h"
#include "overlay_sound.h"
#include "overlay_text.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace refix::overlay {
namespace {

constexpr ULONGLONG kAchToastMs = 5000;   // on screen; time with the game in the background does not count
// Reveal of the notification, in seconds from its birth: the frame unfolds from the corner, the icon pops,
// rare ones get their border traced and (very rare and up) one light sweep. Everything plays once.
constexpr float kUnfoldSec = 0.34f;
constexpr float kIconPopStart = 0.10f;
constexpr float kIconPopSec = 0.38f;
constexpr float kTraceStart = 0.30f;
constexpr float kTraceSec = 0.70f;
constexpr float kSweepStart = 0.55f;
constexpr float kSweepSec = 0.75f;
constexpr float kSparkleSec = 1.1f;
constexpr ULONGLONG kModalFadeMs = 180;
constexpr float kBarGrowSec = 0.5f;
constexpr float kCardRowHeight = 56.0f;
constexpr float kCardIcon = 40.0f;
constexpr float kModalRowHeight = 84.0f;
constexpr float kModalIcon = 60.0f;
constexpr float kModalMarginX = 56.0f;
constexpr float kModalMarginY = 40.0f;
constexpr float kModalMaxWidth = 980.0f;
constexpr float kRowGap = 6.0f;
constexpr float kSpecialPercent = 10.0f;   // Steam's "rare achievement" glow

// Tiers of the share of Steam players who have it.
struct Rarity {
    const char* en;
    const char* es;
    ImU32 col;
};

Rarity RarityOf(float pct) {
    if (pct < 0) return { nullptr, nullptr, ui::color::Muted };
    if (pct <= 5.0f) return { "Ultra rare", "Ultra raro", IM_COL32(255, 184, 64, 255) };
    if (pct <= kSpecialPercent) return { "Very rare", "Muy raro", IM_COL32(196, 128, 255, 255) };
    if (pct <= 20.0f) return { "Rare", "Raro", ui::color::Invite };
    if (pct <= 50.0f) return { "Uncommon", "Poco común", ui::color::Accent };
    return { "Common", "Común", IM_COL32(170, 180, 194, 255) };
}

std::string RarityText(float pct) {
    const Rarity r = RarityOf(pct);
    if (!r.en) return {};
    char buf[96];
    snprintf(buf, sizeof(buf), "%s · %.1f%%", i18n::T(r.en, r.es), pct);
    return buf;
}

sound::Cue CueFor(float pct) {
    if (pct < 0 || pct > 50.0f) return sound::Cue::AchievementCommon;
    if (pct > 20.0f) return sound::Cue::AchievementUncommon;
    if (pct > kSpecialPercent) return sound::Cue::AchievementRare;
    if (pct > 5.0f) return sound::Cue::AchievementVeryRare;
    return sound::Cue::AchievementUltraRare;
}

float Clamp01(float v) { return v < 0 ? 0.0f : v > 1 ? 1.0f : v; }
float EaseOutCubic(float t) { t = Clamp01(t); return 1 - (1 - t) * (1 - t) * (1 - t); }
float EaseOutBack(float t) {
    t = Clamp01(t);
    const float c1 = 1.4f, c3 = c1 + 1;
    return 1 + c3 * (t - 1) * (t - 1) * (t - 1) + c1 * (t - 1) * (t - 1);
}

// The card's own rounded outline, drawn from its top-left corner clockwise up to `f` (0..1) of its length:
// the same path AddRect strokes, so the trace follows the corners and ends exactly on the final outline.
void TraceRoundedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float f, ImU32 col, float thick) {
    dl->PathClear();
    dl->PathRect(ImVec2(a.x + 0.5f, a.y + 0.5f), ImVec2(b.x - 0.49f, b.y - 0.49f), rounding);
    ImVector<ImVec2> path = dl->_Path;
    dl->PathClear();
    if (path.Size < 2) return;
    path.push_back(path[0]);   // closed loop
    float total = 0;
    for (int i = 1; i < path.Size; ++i) total += std::hypot(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
    float left = total * Clamp01(f);
    ImVector<ImVec2> out;
    out.push_back(path[0]);
    for (int i = 1; i < path.Size && left > 0; ++i) {
        const ImVec2 d(path[i].x - path[i - 1].x, path[i].y - path[i - 1].y);
        const float len = std::hypot(d.x, d.y);
        if (len >= left) {
            const float k = len > 0 ? left / len : 0;
            out.push_back(ImVec2(path[i - 1].x + d.x * k, path[i - 1].y + d.y * k));
            break;
        }
        out.push_back(path[i]);
        left -= len;
    }
    if (out.Size >= 2) dl->AddPolyline(out.Data, out.Size, col, ImDrawFlags_None, thick);
}

int Lang() { return i18n::Spanish() ? ach::kSpanish : ach::kEnglish; }

bool Concealed(const ach::Entry& e) { return e.def.hidden && !e.unlocked; }

std::string Title(const ach::Entry& e) {
    if (Concealed(e)) return i18n::T("Hidden achievement", "Logro oculto");
    const std::string& local = e.def.name[Lang()];
    if (!local.empty()) return local;
    return e.def.name[ach::kEnglish].empty() ? e.def.api : e.def.name[ach::kEnglish];
}

std::string Description(const ach::Entry& e) {
    if (Concealed(e)) return i18n::T("Keep playing to reveal it.", "Sigue jugando para descubrirlo.");
    const std::string& local = e.def.desc[Lang()];
    return local.empty() ? e.def.desc[ach::kEnglish] : local;
}

// Progress the game reported, or none.
bool HasProgress(const ach::Entry& e) { return !e.unlocked && e.progressMax > 0; }
float Fraction(const ach::Entry& e) { return e.progressMax ? std::min(1.0f, (float)e.progress / (float)e.progressMax) : 0.0f; }

std::string FormatDate(uint32_t unix) {
    if (!unix) return {};
    static const char* const kEn[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    static const char* const kEs[] = { "ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic" };
    const time_t t = (time_t)unix;
    tm lt{};
    if (localtime_s(&lt, &t) != 0) return {};
    char buf[64];
    if (i18n::Spanish()) snprintf(buf, sizeof(buf), "%d %s %d, %02d:%02d", lt.tm_mday, kEs[lt.tm_mon], lt.tm_year + 1900, lt.tm_hour, lt.tm_min);
    else snprintf(buf, sizeof(buf), "%s %d, %d, %02d:%02d", kEn[lt.tm_mon], lt.tm_mday, lt.tm_year + 1900, lt.tm_hour, lt.tm_min);
    return buf;
}

void Text(ImDrawList* dl, ImFont* font, ImVec2 pos, ImU32 col, const std::string& text) {
    dl->AddText(font, font->FontSize, pos, col, text.c_str());
}

std::string Fit(ImFont* font, const std::string& text, float width) {
    ImGui::PushFont(font);
    std::string out = ui::Ellipsize(text, width);
    ImGui::PopFont();
    return out;
}

float TextWidth(ImFont* font, const std::string& text) {
    return font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, text.c_str()).x;
}

void Bar(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float frac, ImU32 col, float alpha = 1.0f) {
    const float r = (p1.y - p0.y) * 0.5f;
    dl->AddRectFilled(p0, p1, ui::Fade(IM_COL32(255, 255, 255, 30), alpha), r);
    if (frac > 0) dl->AddRectFilled(p0, ImVec2(p0.x + std::max(p1.y - p0.y, (p1.x - p0.x) * frac), p1.y), ui::Fade(col, alpha), r);
}

// Icon square; a trophy glyph while the picture downloads (or when there is none).
void Icon(ImDrawList* dl, ImTextureID tex, ImVec2 p0, float size, bool locked, float alpha) {
    const ImVec2 p1(p0.x + size, p0.y + size);
    const float r = ui::S(8);
    if (tex) {
        const ImU32 tint = locked ? IM_COL32(255, 255, 255, (int)(170 * alpha)) : IM_COL32(255, 255, 255, (int)(255 * alpha));
        dl->AddImageRounded(tex, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint, r);
    } else {
        dl->AddRectFilled(p0, p1, ui::Fade(ui::color::Row, alpha), r);
        ui::GlyphCentered(dl, ui::F().body, size * 0.45f, ImVec2(p0.x + size * 0.5f, p0.y + size * 0.5f),
                          ui::Fade(locked ? ui::color::Faint : ui::color::Star, alpha), ui::icon::Trophy);
    }
    if (locked)
        dl->AddRect(p0, p1, ui::Fade(IM_COL32(255, 255, 255, 26), alpha), r);
}

// Rarity chip; returns its width. `right` is the right edge.
float Chip(ImDrawList* dl, float right, float centerY, float pct) {
    const std::string text = RarityText(pct);
    if (text.empty()) return 0;
    const ui::Fonts& f = ui::F();
    const float w = TextWidth(f.caption, text) + ui::S(20);
    const float h = f.caption->FontSize + ui::S(8);
    const ImVec2 p0(right - w, centerY - h * 0.5f), p1(right, centerY + h * 0.5f);
    const ImU32 col = RarityOf(pct).col;
    dl->AddRectFilled(p0, p1, ui::Fade(col, 0.16f), h * 0.5f);
    Text(dl, f.caption, ImVec2(p0.x + ui::S(10), centerY - f.caption->FontSize * 0.5f), col, text);
    return w;
}

// Newest activity first: unlocked, then the ones with progress.
std::vector<const ach::Entry*> Recent(const std::vector<ach::Entry>& all) {
    std::vector<const ach::Entry*> out;
    for (const ach::Entry& e : all) {
        if (e.unlocked || (HasProgress(e) && e.progress > 0)) out.push_back(&e);
    }
    std::stable_sort(out.begin(), out.end(), [](const ach::Entry* a, const ach::Entry* b) { return a->ActivityTime() > b->ActivityTime(); });
    return out;
}

// Modal order: unlocked (newest first), with progress (closest first), locked (most common first), hidden.
std::vector<const ach::Entry*> Ordered(const std::vector<ach::Entry>& all) {
    std::vector<const ach::Entry*> out;
    for (const ach::Entry& e : all) out.push_back(&e);
    auto group = [](const ach::Entry* e) { return e->unlocked ? 0 : HasProgress(*e) ? 1 : Concealed(*e) ? 3 : 2; };
    std::stable_sort(out.begin(), out.end(), [&](const ach::Entry* a, const ach::Entry* b) {
        const int ga = group(a), gb = group(b);
        if (ga != gb) return ga < gb;
        if (ga == 0) return a->unlockTime > b->unlockTime;
        if (ga == 1) return Fraction(*a) > Fraction(*b);
        return a->def.percent > b->def.percent;
    });
    return out;
}

struct Totals {
    int total = 0;
    int unlocked = 0;
};

Totals Count(const std::vector<ach::Entry>& all) {
    Totals t;
    for (const ach::Entry& e : all) {
        if (!e.inSchema) continue;
        ++t.total;
        if (e.unlocked) ++t.unlocked;
    }
    return t;
}

} // namespace

void App::PollAchievements() {
    const uint64_t v = ach::Version();
    if (v != achVersion_) {
        achVersion_ = v;
        achievements_ = ach::Snapshot();
        // Icons download once in the background, so a notification already has its picture.
        for (const ach::Entry& e : achievements_) {
            ach::IconFile(e.def.icon);
            ach::IconFile(e.def.iconGray);
        }
    }
    const ULONGLONG now = GetTickCount64();
    for (ach::Event& e : ach::TakeEvents()) {
        float pct = -1.0f;
        bool known = false;
        for (const ach::Entry& x : achievements_) {
            if (x.def.api == e.api) { pct = x.def.percent; known = true; break; }
        }
        if (!known) continue;   // not shown anywhere (the game has no Steam schema): no notification either
        const settings::Values& prefs = settings::Get();
        if (e.unlocked ? !prefs.achievementToasts : !prefs.progressToasts) continue;
        // Progress of the same achievement updates the notification already on screen (no replayed reveal).
        auto it = std::find_if(toasts_.begin(), toasts_.end(), [&](const Toast& t) {
            return t.kind == Toast::Kind::Achievement && !t.closing && t.achievement.api == e.api && !t.achievement.unlocked;
        });
        if (it != toasts_.end()) {
            it->achievement = e;
            it->life = (now > it->born ? now - it->born : 0) + kAchToastMs;
        } else {
            Toast t;
            t.kind = Toast::Kind::Achievement;
            t.achievement = e;
            t.born = now;
            t.life = kAchToastMs;
            toasts_.push_back(std::move(t));
        }
        if (prefs.achievementSounds) sound::Play(e.unlocked ? CueFor(pct) : sound::Cue::AchievementProgress);
        host::Wake();
    }
}

ImTextureID App::AchievementIcon(const ach::Entry& e) {
    const std::string& icon = !e.unlocked && !e.def.iconGray.empty() ? e.def.iconGray : e.def.icon;
    if (icon.empty()) return 0;
    const std::string key = "ach:" + icon;
    if (host::HasTexture(key)) return host::Texture(key, nullptr);
    const std::wstring path = ach::IconFile(icon);
    if (path.empty()) return 0;   // downloading
    return host::Texture(key, [&](image::Rgba& img) { return image::LoadFile(path, img); });
}

void App::DrawAchievementToast(ImDrawList* dl, const Toast& t, ImVec2 p0, ImVec2 p1, float a) {
    const ui::Fonts& f = ui::F();
    ach::Entry e;
    e.def.api = t.achievement.api;
    for (const ach::Entry& x : achievements_) {
        if (x.def.api == t.achievement.api) { e = x; break; }
    }
    const bool unlocked = t.achievement.unlocked;
    const float pct = e.def.percent;
    const Rarity rarity = RarityOf(pct);
    const bool tiered = unlocked && rarity.en != nullptr;
    const bool rare = tiered && pct <= 20.0f;
    const bool veryRare = tiered && pct <= kSpecialPercent;
    const bool ultra = tiered && pct <= 5.0f;
    const float age = (float)(GetTickCount64() - t.born) / 1000.0f;
    const float h = p1.y - p0.y;
    const float rounding = ui::S(14);

    // The frame unfolds leftwards from the corner it is anchored to.
    const float unfold = EaseOutCubic(age / kUnfoldSec);
    const ImVec2 c0(p1.x - (h + (p1.x - p0.x - h) * unfold), p0.y);
    ui::Card(dl, c0, p1, ui::Fade(ui::color::PanelRaised, a), 14.0f);
    if (rare) {
        // Rarity border: traced once around the card, then kept as a plain outline.
        const float trace = (age - kTraceStart) / kTraceSec;
        const ImU32 edge = ui::Fade(rarity.col, a * 0.85f);
        if (trace >= 1.0f) dl->AddRect(c0, p1, edge, rounding, 0, ui::S(1.5f));
        else if (trace > 0) TraceRoundedRect(dl, c0, p1, rounding, trace, edge, ui::S(1.5f));
    }
    dl->PushClipRect(c0, p1, true);

    const float icon = ui::S(56);
    const ImVec2 ic(c0.x + ui::S(16) + icon * 0.5f, p0.y + h * 0.5f);
    const float pop = age < kIconPopStart ? 0.0f : EaseOutBack((age - kIconPopStart) / kIconPopSec);
    const float is = icon * (0.55f + 0.45f * pop);
    e.unlocked = unlocked || e.unlocked;
    if (pop > 0) Icon(dl, AchievementIcon(e), ImVec2(ic.x - is * 0.5f, ic.y - is * 0.5f), is, !unlocked, a * Clamp01(pop * 1.5f));

    const float tx = c0.x + ui::S(16) + icon + ui::S(14);
    const float tw = p1.x - ui::S(16) - tx;
    const float lineGap = ui::S(3);
    const float block = f.caption->FontSize * 2 + f.strong->FontSize + lineGap * 2;
    float y = p0.y + (h - block) * 0.5f;
    const ImU32 headCol = unlocked ? (tiered ? rarity.col : ui::color::Accent) : ui::color::Muted;
    Text(dl, f.caption, ImVec2(tx, y), ui::Fade(headCol, a),
         Fit(f.caption, unlocked ? i18n::T("Achievement unlocked", "Logro desbloqueado") : i18n::T("Achievement progress", "Progreso de logro"), tw));
    y += f.caption->FontSize + lineGap;
    e.def.hidden = e.def.hidden && !unlocked;
    Text(dl, f.strong, ImVec2(tx, y), ui::Fade(ui::color::Text, a), Fit(f.strong, Title(e), tw));
    y += f.strong->FontSize + lineGap;
    if (unlocked) {
        std::string line = RarityText(pct);
        if (line.empty()) line = Description(e);
        else line += i18n::T(" of players", " de los jugadores");
        Text(dl, f.caption, ImVec2(tx, y), ui::Fade(tiered ? rarity.col : ui::color::Muted, a), Fit(f.caption, line, tw));
    } else {
        char count[48];
        snprintf(count, sizeof(count), "%u / %u", t.achievement.progress, t.achievement.progressMax);
        const float cw = TextWidth(f.caption, count);
        const float by = y + f.caption->FontSize * 0.5f;
        const float frac = t.achievement.progressMax ? (float)t.achievement.progress / (float)t.achievement.progressMax : 0.0f;
        Bar(dl, ImVec2(tx, by - ui::S(3)), ImVec2(tx + tw - cw - ui::S(10), by + ui::S(3)), frac * EaseOutCubic(age / kBarGrowSec),
            ui::color::Accent, a);
        Text(dl, f.caption, ImVec2(tx + tw - cw, y), ui::Fade(ui::color::Muted, a), count);
    }

    // Very rare and up: one diagonal light sweep across the card.
    const float sweep = (age - kSweepStart) / kSweepSec;
    if (veryRare && sweep > 0 && sweep < 1) {
        const float bandW = ui::S(70), skew = h * 0.6f;
        const float x = c0.x - bandW - skew + (p1.x - c0.x + 2 * bandW + skew) * EaseOutCubic(sweep);
        const ImU32 clear = IM_COL32(255, 255, 255, 0);
        const ImU32 glint = ui::Fade(IM_COL32(255, 244, 214, 255), 0.16f * a * (1 - sweep));
        dl->AddRectFilledMultiColor(ImVec2(x, p0.y), ImVec2(x + bandW * 0.5f, p1.y), clear, glint, glint, clear);
        dl->AddRectFilledMultiColor(ImVec2(x + bandW * 0.5f, p0.y), ImVec2(x + bandW, p1.y), glint, clear, clear, glint);
    }
    dl->PopClipRect();

    // Ultra rare: a few sparkles burst once around the icon.
    const float sp = (age - kIconPopStart) / kSparkleSec;
    if (ultra && sp > 0 && sp < 1) {
        for (int i = 0; i < 6; ++i) {
            const float ang = i * 1.047f + 0.4f;
            const float dist = icon * (0.45f + 0.35f * EaseOutCubic(sp));
            const ImVec2 c(ic.x + std::cos(ang) * dist, ic.y + std::sin(ang) * dist);
            const float r = ui::S(4.5f) * (1 - sp);
            const ImU32 col = ui::Fade(rarity.col, a * (1 - sp));
            dl->AddQuadFilled(ImVec2(c.x, c.y - r * 2), ImVec2(c.x + r * 0.6f, c.y), ImVec2(c.x, c.y + r * 2), ImVec2(c.x - r * 0.6f, c.y), col);
            dl->AddQuadFilled(ImVec2(c.x - r * 2, c.y), ImVec2(c.x, c.y - r * 0.6f), ImVec2(c.x + r * 2, c.y), ImVec2(c.x, c.y + r * 0.6f), col);
        }
    }
}

void App::DrawAchievementsCard(ImVec2 pos, ImVec2 size) {
    const ui::Fonts& f = ui::F();
    ui::BeginCard("##achievements", pos, size);
    const Totals totals = Count(achievements_);

    // Header row: title on the left, "View all" on the right, both inside the card's content box (the
    // card is a child window that clips at its edges, so nothing may sit above or beside that box).
    const ImVec2 row = ImGui::GetCursorPos();
    const float rowW = ImGui::GetContentRegionAvail().x;
    const char* label = i18n::T("View all", "Ver todos");
    ImGui::PushFont(f.strong);
    const ImVec2 bs(ImGui::CalcTextSize(label).x + ui::S(32), ui::S(34));
    ImGui::PopFont();
    const float rowH = (std::max)(bs.y, f.heading->FontSize);
    ImGui::SetCursorPos(ImVec2(row.x, row.y + (rowH - f.heading->FontSize) * 0.5f));
    ImGui::PushFont(f.heading);
    ui::TextColored(ui::color::Star, ui::icon::Trophy);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Achievements", "Logros"));
    ImGui::PopFont();
    if (!achievements_.empty()) {
        ImGui::SetCursorPos(ImVec2(row.x + rowW - bs.x - ui::S(2), row.y));   // 2 px: room for the focus ring
        if (ui::SecondaryButton(label, bs)) achModalRequested_ = true;
    }
    ImGui::SetCursorPos(ImVec2(row.x, row.y + rowH + ImGui::GetStyle().ItemSpacing.y));

    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (totals.total > 0) {
        char line[96];
        snprintf(line, sizeof(line), i18n::T("%d of %d unlocked", "%d de %d desbloqueados"), totals.unlocked, totals.total);
        const ImVec2 c = ImGui::GetCursorScreenPos();
        Text(dl, f.caption, c, ui::color::Muted, line);
        char pctText[16];
        snprintf(pctText, sizeof(pctText), "%d%%", totals.unlocked * 100 / totals.total);
        Text(dl, f.caption, ImVec2(c.x + width - TextWidth(f.caption, pctText), c.y), ui::color::Text, pctText);
        const float by = c.y + f.caption->FontSize + ui::S(6);
        Bar(dl, ImVec2(c.x, by), ImVec2(c.x + width, by + ui::S(6)), (float)totals.unlocked / totals.total, ui::color::Star);
        ImGui::Dummy(ImVec2(width, f.caption->FontSize + ui::S(16)));
    }
    if (active_ && active_->guest) {
        ImGui::PushFont(f.caption);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ui::TextColored(ui::color::Muted, i18n::T("Guest: these achievements are deleted when the game closes.",
                                                  "Invitado: estos logros se borran al cerrar el juego."));
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
    }

    const std::vector<const ach::Entry*> recent = Recent(achievements_);
    if (recent.empty()) {
        ImGui::Dummy(ImVec2(0, ui::S(10)));
        ImGui::PushFont(f.caption);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ui::TextColored(ui::color::Muted,
                        achievements_.empty()
                            ? i18n::T("This game has no Steam achievements, or their list could not be downloaded yet.",
                                      "Este juego no tiene logros de Steam, o su lista aún no se pudo descargar.")
                            : i18n::T("Your unlocked achievements and their progress will show up here.",
                                      "Aquí verás los logros que consigas y su progreso."));
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ui::EndCard();
        return;
    }

    ImGui::Dummy(ImVec2(0, ui::S(2)));
    ui::SectionLabel(i18n::T("Recent", "Recientes"));
    ImGui::BeginChild("##achrecent", ImVec2(0, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = ui::S(kCardRowHeight);
    const float icon = ui::S(kCardIcon);
    for (const ach::Entry* e : recent) {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + w, p0.y + h);
        ImGui::PushID(e->def.api.c_str());
        if (ui::Clicked(ui::NavButton("##row", ImVec2(w, h)))) achModalRequested_ = true;
        const bool hover = ImGui::IsItemHovered();
        ui::Tooltip(Description(*e).c_str());
        ImGui::PopID();
        dl->AddRectFilled(p0, p1, hover ? ui::color::RowHover : ui::color::Row, ui::S(10));
        Icon(dl, AchievementIcon(*e), ImVec2(p0.x + ui::S(8), p0.y + (h - icon) * 0.5f), icon, !e->unlocked, 1.0f);
        const float tx = p0.x + ui::S(8) + icon + ui::S(12);
        const float right = p1.x - ui::S(10);
        const float ty = p0.y + (h - f.strong->FontSize - f.caption->FontSize - ui::S(3)) * 0.5f;
        const float sub = ty + f.strong->FontSize + ui::S(3);
        float nameRight = right;
        if (e->unlocked) {
            const Rarity r = RarityOf(e->def.percent);
            if (r.en) {
                char pct[16];
                snprintf(pct, sizeof(pct), "%.1f%%", e->def.percent);
                const float pw = TextWidth(f.caption, pct);
                Text(dl, f.caption, ImVec2(right - pw, ty + ui::S(1)), r.col, pct);
                nameRight = right - pw - ui::S(8);
            }
            Text(dl, f.caption, ImVec2(tx, sub), ui::color::Muted, Fit(f.caption, FormatDate(e->unlockTime), right - tx));
        } else {
            char count[32];
            snprintf(count, sizeof(count), "%u / %u", e->progress, e->progressMax);
            const float cw = TextWidth(f.caption, count);
            const float by = sub + f.caption->FontSize * 0.5f;
            Bar(dl, ImVec2(tx, by - ui::S(3)), ImVec2(right - cw - ui::S(10), by + ui::S(3)), Fraction(*e), ui::color::Accent);
            Text(dl, f.caption, ImVec2(right - cw, sub), ui::color::Muted, count);
        }
        Text(dl, f.strong, ImVec2(tx, ty), ui::color::Text, Fit(f.strong, Title(*e), nameRight - tx));
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(kRowGap)));
        ImGui::Dummy(ImVec2(0, 0));
    }
    ImGui::EndChild();
    ui::EndCard();
}

void App::DrawAchievementsModal(ImVec2 viewport) {
    if (achModalRequested_) {
        achModalRequested_ = false;
        achModalOpen_ = true;
        achModalOpenedAt_ = GetTickCount64();
    }
    if (!achModalOpen_) return;
    if (!ImGui::IsPopupOpen("##achall")) ImGui::OpenPopup("##achall");
    const ImVec2 size(std::min(ui::S(kModalMaxWidth), viewport.x - 2 * ui::S(kModalMarginX)),
                      viewport.y - 2 * ui::S(kModalMarginY));
    const float intro = EaseOutCubic((float)(GetTickCount64() - achModalOpenedAt_) / kModalFadeMs);
    ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f + (1 - intro) * ui::S(18)), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowBgAlpha(intro);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.2f + 0.8f * intro);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ui::S(24), ui::S(20)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, ui::S(14));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ui::color::Panel);
    const bool open = ImGui::BeginPopupModal("##achall", nullptr,
                                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!open) {
        achModalOpen_ = false;
        return;
    }
    const float grow = EaseOutCubic((float)(GetTickCount64() - achModalOpenedAt_) / 1000.0f / kBarGrowSec);

    const ui::Fonts& f = ui::F();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const Totals totals = Count(achievements_);
    const float width = ImGui::GetContentRegionAvail().x;
    bool close = false;

    // Header: title and count, close button, overall progress.
    const ImVec2 h0 = ImGui::GetCursorScreenPos();
    ImGui::PushFont(f.title);
    ui::TextColored(ui::color::Star, ui::icon::Trophy);
    ImGui::SameLine();
    ImGui::TextUnformatted(i18n::T("Achievements", "Logros"));
    ImGui::PopFont();
    const float btn = ui::S(36);
    ImGui::SetCursorScreenPos(ImVec2(h0.x + width - btn, h0.y));
    if (ui::IconButton("achclose", ui::icon::Close, i18n::T("Close (Esc)", "Cerrar (Esc)"), ui::color::Text, true, btn)) close = true;
    ImGui::SetCursorScreenPos(ImVec2(h0.x, h0.y + f.title->FontSize + ui::S(10)));
    if (totals.total > 0) {
        const ImVec2 c = ImGui::GetCursorScreenPos();
        char line[128];
        snprintf(line, sizeof(line), i18n::T("%d of %d unlocked (%d%%)", "%d de %d desbloqueados (%d%%)"), totals.unlocked, totals.total,
                 totals.unlocked * 100 / totals.total);
        Text(dl, f.strong, c, ui::color::Text, line);
        const std::string game = Fit(f.caption, ToUtf8(gameTitle_), width * 0.5f);
        Text(dl, f.caption, ImVec2(c.x + width - TextWidth(f.caption, game), c.y + ui::S(2)), ui::color::Muted, game);
        const float by = c.y + f.strong->FontSize + ui::S(8);
        Bar(dl, ImVec2(c.x, by), ImVec2(c.x + width, by + ui::S(8)), grow * totals.unlocked / totals.total, ui::color::Star);
        ImGui::Dummy(ImVec2(width, f.strong->FontSize + ui::S(26)));
    }

    ImGui::BeginChild("##achlist", ImVec2(0, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
    dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x - ui::S(4);
    const float h = ui::S(kModalRowHeight);
    const float icon = ui::S(kModalIcon);
    int group = -1;
    for (const ach::Entry* e : Ordered(achievements_)) {
        const int g = e->unlocked ? 0 : 1;
        if (g != group) {
            if (group != -1) ImGui::Dummy(ImVec2(0, ui::S(8)));
            group = g;
            int n = 0;
            for (const ach::Entry& x : achievements_) n += (x.unlocked ? 0 : 1) == g ? 1 : 0;
            ui::SectionLabel(g == 0 ? i18n::T("Unlocked", "Desbloqueados") : i18n::T("Locked", "Bloqueados"), n);
        }
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + w, p0.y + h);
        ImGui::PushID(e->def.api.c_str());
        ui::NavButton("##row", ImVec2(w, h));
        const bool hover = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        ImGui::PopID();
        dl->AddRectFilled(p0, p1, hover ? ui::color::RowHover : ui::color::Row, ui::S(12));
        Icon(dl, AchievementIcon(*e), ImVec2(p0.x + ui::S(12), p0.y + (h - icon) * 0.5f), icon, !e->unlocked, 1.0f);

        const float tx = p0.x + ui::S(12) + icon + ui::S(16);
        float right = p1.x - ui::S(16);
        const float midY = p0.y + h * 0.5f;
        const float chipW = Chip(dl, right, p0.y + ui::S(22), e->def.percent);
        const float textRight = right - (chipW > 0 ? chipW + ui::S(12) : 0);
        const float lines = f.strong->FontSize + f.caption->FontSize * 2 + ui::S(8);
        float y = midY - lines * 0.5f;
        Text(dl, f.strong, ImVec2(tx, y), e->unlocked ? ui::color::Text : ui::Fade(ui::color::Text, 0.8f), Fit(f.strong, Title(*e), textRight - tx));
        y += f.strong->FontSize + ui::S(4);
        const std::string desc = Description(*e);
        Text(dl, f.caption, ImVec2(tx, y), ui::color::Muted, Fit(f.caption, desc, right - tx));
        if (hover && TextWidth(f.caption, desc) > right - tx) ui::Tooltip(desc.c_str());
        y += f.caption->FontSize + ui::S(4);
        if (e->unlocked) {
            const std::string when = std::string(ui::icon::Check) + "  " + i18n::T("Unlocked ", "Desbloqueado el ") + FormatDate(e->unlockTime);
            Text(dl, f.caption, ImVec2(tx, y), ui::color::Accent, Fit(f.caption, when, right - tx));
        } else if (HasProgress(*e)) {
            char count[48];
            snprintf(count, sizeof(count), "%u / %u", e->progress, e->progressMax);
            const float cw = TextWidth(f.caption, count);
            const float by = y + f.caption->FontSize * 0.5f;
            const float barRight = std::min(right - cw - ui::S(12), tx + ui::S(360));
            Bar(dl, ImVec2(tx, by - ui::S(3)), ImVec2(barRight, by + ui::S(3)), grow * Fraction(*e), ui::color::Accent);
            Text(dl, f.caption, ImVec2(barRight + ui::S(12), y), ui::color::Muted, count);
        } else if (e->def.maxProgress > 0) {
            char count[64];
            snprintf(count, sizeof(count), i18n::T("Progress: 0 / %d", "Progreso: 0 / %d"), e->def.maxProgress);
            Text(dl, f.caption, ImVec2(tx, y), ui::color::Muted, count);
        }
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + ui::S(kRowGap)));
        ImGui::Dummy(ImVec2(0, 0));
    }
    if (achievements_.empty()) {
        ImGui::PushFont(f.caption);
        ui::TextColored(ui::color::Muted, i18n::T("No achievements to show.", "No hay logros para mostrar."));
        ImGui::PopFont();
    }
    ImGui::EndChild();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) close = true;
    if (close) {
        achModalOpen_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace refix::overlay
