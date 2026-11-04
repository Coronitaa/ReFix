#pragma once
#include <cstdint>

// Unity Mono runtime integration for ReFix LAN.
// Supports games like Crush Crush by intercepting in-game purchases and validating them locally.
void ReFixMono_Init();
void ReFixMono_UnlockDLC(uint32_t dlcId);
