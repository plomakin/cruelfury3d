#pragma once

#include <cstdint>
#include <vector>

namespace TexTile {
    constexpr int SteelWall       = 0;
    constexpr int NeonSignWall    = 1;
    constexpr int HazardWall      = 2;
    constexpr int ClubWall        = 3;
    constexpr int Door            = 4;
    constexpr int LockedDoor      = 5;
    constexpr int SecretWall      = 6;
    constexpr int ExitSwitch      = 7;
    constexpr int GrateFloor      = 8;
    constexpr int CeilingLight    = 9;
    constexpr int SodaMachine     = 10;
    constexpr int SodaSmashed     = 11;
    constexpr int Crate           = 12;
    constexpr int AcidFloor       = 13;
    constexpr int ClubFloor       = 14;
    constexpr int ServerPillar    = 15;

    // Enemies: 4 frames each (WalkA, WalkB, Attack, Pain)
    constexpr int PunkBase        = 16; // 16..19
    constexpr int SawbotBase      = 20; // 20..23
    constexpr int DroneBase       = 24; // 24..27
    constexpr int BruteBase       = 28; // 28..31

    // Props, Pickups & FX
    constexpr int Barrel          = 32;
    constexpr int Medkit          = 33;
    constexpr int ColaCan         = 34;
    constexpr int ArmorShard      = 35;
    constexpr int AmmoBox         = 36;
    constexpr int Keycard         = 37;
    constexpr int PlasmaOrb       = 38;
    constexpr int RocketProj      = 39;
    constexpr int Explo0          = 40;
    constexpr int Explo1          = 41;
    constexpr int Explo2          = 42;
    constexpr int GibChunk        = 43;

    // First-person weapon viewmodels
    constexpr int WepRevolver     = 48;
    constexpr int WepShotgun      = 49;
    constexpr int WepChaingun     = 50;
    constexpr int WepRocket       = 51;
    constexpr int MuzzleFlash     = 52;
    constexpr int MightyBoot      = 53;
}

struct AtlasUV {
    float u0, v0, u1, v1;
};

class TextureAtlas {
public:
    static constexpr int kTileSize = 64;
    static constexpr int kGridDim  = 8;
    static constexpr int kAtlasDim = kTileSize * kGridDim; // 512x512

    static std::vector<uint8_t> generateRGBA();
    static AtlasUV tileUV(int tileIndex);
};
