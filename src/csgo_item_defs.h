#pragma once
#include <cstdint>
#include <cstddef>

namespace ReFixGC {
namespace CSGO {

// 31 Authentic CS:GO Weapon Cases matching items_game.txt of CS:GO
constexpr uint32_t kCases[] = {
    4001, // CS:GO Weapon Case (Caja de armas de CS:GO)
    4002, // eSports 2013 Case (Maletín eSports 2013)
    4003, // Operation Bravo Case (Caja de la Operación Bravo)
    4004, // CS:GO Weapon Case 2 (Caja de armas de CS:GO 2)
    4005, // eSports 2013 Winter Case (Maletín eSports de invierno 2013)
    4009, // Winter Offensive Weapon Case (Caja de armas de la Ofensiva Invernal)
    4010, // CS:GO Weapon Case 3 (Caja de armas de CS:GO 3)
    4011, // Operation Phoenix Weapon Case (Caja de armas de la Operación Phoenix)
    4017, // Huntsman Weapon Case (Caja de armas del Cazador)
    4018, // Operation Breakout Weapon Case (Caja de armas de la Operación Breakout)
    4019, // eSports 2014 Summer Case (Maletín eSports de verano 2014)
    4029, // Operation Vanguard Weapon Case (Caja de armas de la Operación Vanguard)
    4061, // Chroma Case (Caja Croma)
    4089, // Chroma 2 Case (Caja Croma 2)
    4091, // Falchion Case (Caja Alfanje)
    4138, // Shadow Case (Caja Sombría)
    4186, // Revolver Case (Caja Revólver)
    4187, // Operation Wildfire Case (Caja de armas de la Operación Wildfire)
    4233, // Chroma 3 Case (Caja Croma 3)
    4236, // Gamma Case (Caja Gamma)
    4281, // Gamma 2 Case (Caja Gamma 2)
    4288, // Glove Case (Caja de Guantes)
    4351, // Spectrum Case (Caja Espectro)
    4352, // Operation Hydra Case (Caja de la Operación Hydra)
    4403, // Spectrum 2 Case (Caja Espectro 2)
    4471, // Clutch Case (Caja Clutch)
    4482, // Horizon Case (Caja Horizonte)
    4548, // Danger Zone Case (Caja Danger Zone)
    4598, // Prisma Case (Caja Prisma)
    4620, // CS:GO Weapon Case Shattered Web (Caja de Shattered Web)
    4669  // CS20 Case (Caja CS20)
};
constexpr size_t kNumCases = sizeof(kCases) / sizeof(kCases[0]);

// Matching authentic Case Keys
constexpr uint32_t kKeys[] = {
    1203, // CS:GO Case Key
    1204, // eSports Key
    1214, // Winter Offensive Key
    1303, // Operation Phoenix Key
    1307, // Huntsman Key
    1313, // Operation Breakout Key
    1322, // Operation Vanguard Key
    1323, // Chroma Key
    1325, // Chroma 2 Key
    1330, // Falchion Key
    1333, // Shadow Key
    1334, // Revolver Key
    1347, // Chroma 3 Key
    1350, // Gamma Key
    1351, // Gamma 2 Key
    1356, // Glove Key
    1364, // Spectrum Key
    1365, // Operation Hydra Key
    1366, // Spectrum 2 Key
    1373, // Clutch Key
    1374, // Horizon Key
    1375, // Danger Zone Key
    1383, // Prisma Key
    1384, // Shattered Web Key
    1385, // CS20 Key
    7000  // Operation Wildfire Key
};
constexpr size_t kNumKeys = sizeof(kKeys) / sizeof(kKeys[0]);

// Iconic starter skins and knives
constexpr uint32_t kWeaponSkinDefs[] = {
    7,   // AK-47
    4,   // Glock-18
    16,  // M4A4
    9,   // AWP
    1,   // Desert Eagle
    61,  // USP-S
    507, // Karambit
    515, // Butterfly Knife (Cuchillo Mariposa)
    508, // M9 Bayonet (Bayoneta M9)
    523, // Talon Knife (Cuchillo talón)
    500, // Bayonet (Bayoneta)
    505, // Flip Knife (Cuchillo plegable)
    506, // Gut Knife (Cuchillo Destripador)
    509, // Huntsman Knife (Cuchillo del Cazador)
    514, // Bowie Knife (Cuchillo Bowie)
    516, // Shadow Daggers (Dagas Sombrías)
    519, // Ursus Knife (Cuchillo ursus)
    520, // Navaja Knife (Navaja)
    522  // Stiletto Knife (Estilete)
};
constexpr size_t kNumWeaponSkinDefs = sizeof(kWeaponSkinDefs) / sizeof(kWeaponSkinDefs[0]);

// Coupons / Store offers for Main Menu
constexpr uint32_t kCoupons[] = {
    20000, 20004, 20007, 20020
};
constexpr size_t kNumCoupons = sizeof(kCoupons) / sizeof(kCoupons[0]);

} // namespace CSGO
} // namespace ReFixGC
