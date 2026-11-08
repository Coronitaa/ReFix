// Short, quiet interface sounds for the overlay. Synthesised once in memory (no files), played with
// PlaySound so they never block. Off with [Overlay] Sounds = false.
#pragma once

namespace refix::overlay::sound {

enum class Cue {
    Open,      // overlay opens
    Close,     // overlay closes
    Select,    // a button or tile is activated
    Move,      // keyboard/controller focus moves to another item
    Notify,    // invite or notification
    Denied,    // the action is not possible
    // Achievements: a soft tick for progress, then chimes that grow with the rarity (fewer players, more notes).
    AchievementProgress,
    AchievementCommon,       // more than 50 % of players
    AchievementUncommon,     // 20-50 %
    AchievementRare,         // 10-20 %
    AchievementVeryRare,     // 5-10 %
    AchievementUltraRare,    // 5 % or less
};

void SetEnabled(bool enabled);
void Play(Cue cue);

} // namespace refix::overlay::sound
