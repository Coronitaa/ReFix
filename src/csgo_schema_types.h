// Portions of this file are derived from csgo_gc:
// Copyright (c) 2024-2026, Mikko Kokko
// Copyright (c) 2024-2026, Theeto
// Licensed under the BSD-2-Clause License (see THIRD_PARTY_NOTICES)

#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

template<typename Enum>
constexpr std::underlying_type_t<Enum> FromEnum(Enum value) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

template<typename Enum>
constexpr Enum ToEnum(std::underlying_type_t<Enum> value) noexcept
{
    return static_cast<Enum>(value);
}

enum class Rarity : uint32_t
{
    Default = 0,
    Common = 1,
    Uncommon = 2,
    Rare = 3,       // Mil-Spec (Blue)
    Mythical = 4,   // Restricted (Purple)
    Legendary = 5,  // Classified (Pink)
    Ancient = 6,    // Covert (Red)
    Immortal = 7,   // Contraband / Gold / Knife
    Unusual = 99
};

enum class Quality : uint32_t
{
    Normal = 0,
    Genuine = 1,
    Vintage = 2,
    Unusual = 3,    // ★ Knives / Gloves
    Unique = 4,     // Standard weapon skins
    Community = 5,
    Developer = 6,
    Selfmade = 7,
    Customized = 8,
    Strange = 9,    // StatTrak™
    Completed = 10,
    Haunted = 11,
    Tournament = 12 // Souvenir
};

enum class ItemDefIndex : uint32_t
{
    Casket = 1201,
    Sticker = 1209,
    MusicKit = 1314,
    Spray = 1348,
    SprayPaint = 1349,
    Patch = 4609
};

enum class AttributeDefIndex : uint32_t
{
    TexturePrefab = 6,
    TextureSeed = 7,
    TextureWear = 8,
    SupplyCrateSeries = 68,
    AlternateIcon = 70,
    KillEater = 80,
    KillEaterScoreType = 81,
    CustomName = 111,
    StickerId0 = 113,
    MusicId = 166,
    QuestId = 168,
    SpraysRemaining = 232,
    SprayTintId = 233
};

enum ItemOrigin : uint32_t
{
    ItemOriginDropped = 0,
    ItemOriginPurchased = 2,
    ItemOriginCrate = 8,
    ItemOriginBaseItem = 22
};

enum UnacknowledgedType : uint32_t
{
    UnacknowledgedInvalid = 0,
    UnacknowledgedDropped = 1,
    UnacknowledgedCrafted = 2,
    UnacknowledgedTraded = 3,
    UnacknowledgedPurchased = 4,
    UnacknowledgedFoundInCrate = 5
};

constexpr uint32_t InventoryUnacknowledged(UnacknowledgedType type)
{
    constexpr uint32_t InventoryUnacknowledgedMask = (1u << 30);
    return FromEnum(type) | InventoryUnacknowledgedMask;
}
