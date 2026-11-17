#include "TextureAtlas.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct Canvas {
    std::vector<uint8_t>& buf;
    int ox = 0;
    int oy = 0;

    void put(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        if (x < 0 || x >= 64 || y < 0 || y >= 64) return;
        const int idx = ((oy + y) * TextureAtlas::kAtlasDim + (ox + x)) * 4;
        buf[idx + 0] = r;
        buf[idx + 1] = g;
        buf[idx + 2] = b;
        buf[idx + 3] = a;
    }

    void rect(int x0, int y0, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x)
                put(x, y, r, g, b, a);
    }

    void circle(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        for (int y = cy - rad; y <= cy + rad; ++y) {
            for (int x = cx - rad; x <= cx + rad; ++x) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= rad * rad) put(x, y, r, g, b, a);
            }
        }
    }

    void line(int x0, int y0, int x1, int y1, int thick, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        if (steps == 0) { circle(x0, y0, thick / 2, r, g, b, a); return; }
        for (int i = 0; i <= steps; ++i) {
            int x = x0 + (x1 - x0) * i / steps;
            int y = y0 + (y1 - y0) * i / steps;
            if (thick <= 1) put(x, y, r, g, b, a);
            else circle(x, y, thick / 2, r, g, b, a);
        }
    }
};

uint32_t hash2d(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 static_cast<uint32_t>(seed) * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void drawEnemyHumanoid(Canvas& c, int variant, int frame) {
    // Transparent background
    c.rect(0, 0, 64, 64, 0, 0, 0, 0);
    const int bob = (frame == 1) ? -2 : 0;
    const int legShift = (frame == 0) ? -3 : (frame == 1 ? 3 : 0);
    const bool attack = (frame == 2);
    const bool pain = (frame == 3);

    auto tint = [&](uint8_t& r, uint8_t& g, uint8_t& b) {
        if (pain) {
            r = static_cast<uint8_t>(std::min(255, int(r) + 120));
            g = static_cast<uint8_t>(std::min(255, int(g) + 70));
            b = static_cast<uint8_t>(std::min(255, int(b) + 70));
        }
    };

    if (variant == 0) {
        // PUNK ENFORCER (Mohawk + glowing magenta visor + trenchcoat + shotgun)
        uint8_t coatR = 38, coatG = 42, coatB = 62;
        uint8_t skinR = 195, skinG = 145, skinB = 120;
        tint(coatR, coatG, coatB); tint(skinR, skinG, skinB);

        // Boots & legs
        c.rect(22 + legShift, 44, 7, 17, 25, 25, 32);
        c.rect(35 - legShift, 44, 7, 17, 25, 25, 32);
        c.rect(21 + legShift, 57, 9, 5, 15, 15, 20);
        c.rect(34 - legShift, 57, 9, 5, 15, 15, 20);
        // Torso & neon lapel trim
        c.rect(19, 23 + bob, 26, 23, coatR, coatG, coatB);
        c.rect(25, 25 + bob, 14, 18, 70, 30, 55);
        c.line(20, 24 + bob, 25, 42 + bob, 1, 255, 40, 190);
        c.line(43, 24 + bob, 38, 42 + bob, 1, 255, 40, 190);
        // Arms
        c.rect(13, 25 + bob, 6, 15, coatR, coatG, coatB);
        c.rect(45, 25 + bob, 6, 15, coatR, coatG, coatB);
        // Head + Neon Mohawk + Glowing Visor
        c.rect(25, 11 + bob, 14, 12, skinR, skinG, skinB);
        c.rect(29, 3 + bob, 6, 9, 255, 30, 165); // Hot-pink mohawk!
        c.rect(26, 14 + bob, 12, 4, 40, 245, 255); // Cyan cyber-visor!
        // Shotgun across chest + muzzle flash on attack
        c.rect(16, 32 + bob, 32, 5, 65, 70, 80);
        c.rect(42, 31 + bob, 9, 4, 95, 100, 112);
        if (attack) {
            c.circle(52, 33 + bob, 7, 255, 230, 80);
            c.circle(52, 33 + bob, 4, 255, 255, 240);
        }
    } else if (variant == 1) {
        // SAWBOT STALKER (Hunched chrome rusher + dual neon-orange buzzsaws!)
        uint8_t chR = 95, chG = 105, chB = 122;
        tint(chR, chG, chB);
        c.rect(21 + legShift, 45, 7, 16, 55, 60, 72);
        c.rect(36 - legShift, 45, 7, 16, 55, 60, 72);
        c.circle(32, 31 + bob, 13, chR, chG, chB);
        c.rect(22, 22 + bob, 20, 18, 48, 52, 66);
        // Glowing crimson tri-eyes
        c.circle(28, 25 + bob, 2, 255, 45, 30);
        c.circle(36, 25 + bob, 2, 255, 45, 30);
        c.circle(32, 20 + bob, 2, 255, 150, 30);
        // Dual buzzsaws
        int sawY = attack ? (28 + bob) : (35 + bob);
        c.circle(11, sawY, 8, 210, 220, 235);
        c.circle(11, sawY, 5, 255, 95, 20);
        c.circle(53, sawY, 8, 210, 220, 235);
        c.circle(53, sawY, 5, 255, 95, 20);
    } else if (variant == 2) {
        // AERO-DRONE (Floating armored orb + glowing plasma core + fins)
        int hover = (frame & 1) ? -3 : 2;
        uint8_t bodyR = 48, bodyG = 62, bodyB = 92;
        tint(bodyR, bodyG, bodyB);
        c.circle(32, 30 + hover, 16, bodyR, bodyG, bodyB);
        c.circle(32, 30 + hover, 12, 72, 92, 130);
        // Side thruster pods
        c.rect(8, 25 + hover, 9, 12, 38, 45, 65);
        c.rect(47, 25 + hover, 9, 12, 38, 45, 65);
        c.rect(9, 37 + hover, 7, 4, 40, 235, 255);
        c.rect(48, 37 + hover, 7, 4, 40, 235, 255);
        // Central cyclops eye
        c.circle(32, 30 + hover, 7, 20, 25, 35);
        c.circle(32, 30 + hover, 5, attack ? 255 : 40, attack ? 70 : 245, 255);
        c.circle(30, 28 + hover, 2, 255, 255, 255);
    } else {
        // WARLORD BRUTE / BOSS HESKEL-X (Massive crimson/gold mech juggernaut!)
        uint8_t armR = 145, armG = 32, armB = 42;
        tint(armR, armG, armB);
        c.rect(17 + legShift, 43, 11, 19, 85, 24, 30);
        c.rect(36 - legShift, 43, 11, 19, 85, 24, 30);
        // Massive armored torso + shoulder missile pods
        c.rect(13, 17 + bob, 38, 28, armR, armG, armB);
        c.rect(7, 13 + bob, 11, 16, 210, 165, 45);
        c.rect(46, 13 + bob, 11, 16, 210, 165, 45);
        // Horned cyber-helm
        c.rect(24, 7 + bob, 16, 12, 60, 20, 28);
        c.line(24, 10 + bob, 17, 3 + bob, 2, 240, 200, 70);
        c.line(40, 10 + bob, 47, 3 + bob, 2, 240, 200, 70);
        c.rect(26, 11 + bob, 12, 4, 255, 220, 40);
        // Glowing chest reactor core
        c.circle(32, 30 + bob, 6, 255, 90, 30);
        c.circle(32, 30 + bob, 3, 255, 245, 160);
        // Twin arm cannons
        c.rect(6, 29 + bob, 9, 13, 55, 58, 68);
        c.rect(49, 29 + bob, 9, 13, 55, 58, 68);
        if (attack) {
            c.circle(10, 41 + bob, 6, 255, 190, 50);
            c.circle(54, 41 + bob, 6, 255, 190, 50);
        }
    }
}

} // namespace

AtlasUV TextureAtlas::tileUV(int tileIndex) {
    const int gx = tileIndex % kGridDim;
    const int gy = tileIndex / kGridDim;
    // Half-texel inset prevents neighboring tile bleed at edges
    const float pad = 0.5f / static_cast<float>(kAtlasDim);
    const float step = 1.0f / static_cast<float>(kGridDim);
    return {
        gx * step + pad,
        gy * step + pad,
        (gx + 1) * step - pad,
        (gy + 1) * step - pad
    };
}

std::vector<uint8_t> TextureAtlas::generateRGBA() {
    std::vector<uint8_t> buf(kAtlasDim * kAtlasDim * 4, 0);

    auto tile = [&](int idx) -> Canvas {
        return Canvas{buf, (idx % kGridDim) * kTileSize, (idx / kGridDim) * kTileSize};
    };

    // --- 0: Cyber Steel Wall ---
    {
        Canvas c = tile(TexTile::SteelWall);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                int n = static_cast<int>(hash2d(x, y, 1) & 15) - 7;
                bool seam = (x == 0 || x == 31 || y == 0 || y == 31 || y == 48);
                int base = seam ? 24 : 52;
                c.put(x, y,
                      static_cast<uint8_t>(std::clamp(base + n, 10, 255)),
                      static_cast<uint8_t>(std::clamp(base + 6 + n, 10, 255)),
                      static_cast<uint8_t>(std::clamp(base + 18 + n, 15, 255)));
            }
        }
        // Glowing cyan neon trim line
        c.rect(0, 46, 64, 2, 30, 235, 255);
        // Corner rivets
        for (int rx : {4, 27, 36, 59})
            for (int ry : {4, 27})
                c.rect(rx, ry, 2, 2, 110, 125, 145);
    }

    // --- 1: Neon Billboard Wall ---
    {
        Canvas c = tile(TexTile::NeonSignWall);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                bool mortar = ((y % 8) == 0) || (((x + (y / 8) * 16) % 32) == 0);
                int n = static_cast<int>(hash2d(x, y, 2) & 11) - 5;
                int v = mortar ? 20 : 38 + n;
                c.put(x, y, v + 8, v, v + 18);
            }
        }
        // Framed neon sign board
        c.rect(8, 12, 48, 34, 16, 12, 28);
        c.rect(8, 12, 48, 2, 255, 35, 185);
        c.rect(8, 44, 48, 2, 255, 35, 185);
        c.rect(8, 12, 2, 34, 35, 240, 255);
        c.rect(54, 12, 2, 34, 35, 240, 255);
        // Neon cyber glyphs inside sign
        c.rect(15, 19, 10, 3, 255, 50, 200);
        c.rect(15, 19, 3, 18, 255, 50, 200);
        c.rect(15, 27, 8, 3, 255, 50, 200);
        c.rect(29, 19, 3, 18, 45, 245, 255);
        c.rect(29, 34, 10, 3, 45, 245, 255);
        c.rect(36, 19, 3, 18, 45, 245, 255);
        c.circle(46, 28, 5, 255, 210, 45);
        c.circle(46, 28, 2, 16, 12, 28);
    }

    // --- 2: Hazard Reactor Bulkhead ---
    {
        Canvas c = tile(TexTile::HazardWall);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                int n = static_cast<int>(hash2d(x, y, 3) & 15) - 7;
                if (y >= 48 || y < 10) {
                    bool yellow = (((x + y) / 6) % 2) == 0;
                    if (yellow) c.put(x, y, 220, 175, 20);
                    else        c.put(x, y, 25, 25, 28);
                } else {
                    c.put(x, y, 48 + n, 54 + n, 58 + n);
                }
            }
        }
        // Reactor core window
        c.rect(20, 18, 24, 22, 18, 32, 22);
        c.rect(23, 21, 18, 16, 50, 255, 95);
        c.rect(26, 24, 12, 10, 190, 255, 200);
    }

    // --- 3: Nightclub Synthwave Wall ---
    {
        Canvas c = tile(TexTile::ClubWall);
        c.rect(0, 0, 64, 64, 24, 12, 42);
        for (int i = 0; i < 64; i += 16) {
            c.rect(i, 0, 1, 64, 90, 25, 140);
            c.rect(0, i, 64, 1, 90, 25, 140);
        }
        // Equalizer neon bars
        const int heights[7] = {18, 32, 24, 40, 28, 36, 20};
        for (int b = 0; b < 7; ++b) {
            int bx = 6 + b * 8;
            int bh = heights[b];
            for (int y = 54 - bh; y < 54; y += 4) {
                bool top = (y < 28);
                c.rect(bx, y, 6, 3, top ? 255 : 40, top ? 45 : 235, top ? 195 : 255);
            }
        }
    }

    // --- 4 & 5: Blast Door & Keycard Security Gate ---
    for (int locked = 0; locked <= 1; ++locked) {
        Canvas c = tile(locked ? TexTile::LockedDoor : TexTile::Door);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                int n = static_cast<int>(hash2d(x, y, 5) & 11) - 5;
                int v = (x == 31 || x == 32) ? 18 : 66 + n;
                c.put(x, y, v, v + 4, v + 12);
            }
        }
        c.rect(4, 4, 24, 56, 52, 56, 68);
        c.rect(36, 4, 24, 56, 52, 56, 68);
        if (locked) {
            c.rect(0, 28, 64, 8, 255, 185, 25);
            c.circle(32, 32, 6, 255, 50, 40);
        } else {
            c.rect(28, 24, 8, 16, 30, 245, 140);
        }
    }

    // --- 6: Secret Cracked Wall ---
    {
        Canvas c = tile(TexTile::SecretWall);
        // Base matches SteelWall closely, with dark fracture lines + subtle amber glint
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                int n = static_cast<int>(hash2d(x, y, 1) & 15) - 7;
                c.put(x, y, 50 + n, 55 + n, 66 + n);
            }
        }
        c.rect(0, 46, 64, 2, 255, 165, 40);
        c.line(28, 8, 35, 26, 2, 16, 18, 24);
        c.line(35, 26, 24, 44, 2, 16, 18, 24);
        c.line(24, 44, 38, 58, 2, 16, 18, 24);
        c.line(35, 26, 48, 34, 1, 16, 18, 24);
    }

    // --- 7: Elevator Exit Switch ---
    {
        Canvas c = tile(TexTile::ExitSwitch);
        c.rect(0, 0, 64, 64, 34, 40, 52);
        c.rect(12, 10, 40, 44, 20, 26, 34);
        c.rect(16, 14, 32, 14, 35, 255, 120); // Glowing green EXIT panel
        c.rect(26, 32, 12, 18, 85, 90, 100);
        c.rect(28, 34, 8, 8, 255, 70, 50);
    }

    // --- 8: Industrial Grate Floor ---
    {
        Canvas c = tile(TexTile::GrateFloor);
        for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
                bool grid = ((x % 16) == 0) || ((y % 16) == 0);
                bool hole = ((x % 8 == 4) && (y % 8 == 4));
                int n = static_cast<int>(hash2d(x, y, 8) & 11) - 5;
                int v = hole ? 18 : (grid ? 58 : 38 + n);
                c.put(x, y, v, v + 3, v + 10);
            }
        }
    }

    // --- 9: Ceiling Light Tile ---
    {
        Canvas c = tile(TexTile::CeilingLight);
        c.rect(0, 0, 64, 64, 24, 26, 36);
        c.rect(0, 0, 64, 2, 16, 18, 26);
        c.rect(0, 0, 2, 64, 16, 18, 26);
        c.rect(24, 10, 16, 44, 60, 180, 210);
        c.rect(27, 13, 10, 38, 215, 250, 255);
    }

    // --- 10 & 11: Neon Soda Vending Machine (Fresh & Smashed!) ---
    {
        Canvas c = tile(TexTile::SodaMachine);
        c.rect(0, 0, 64, 64, 32, 20, 68);
        c.rect(6, 5, 52, 42, 35, 235, 255);
        c.rect(10, 9, 44, 34, 255, 45, 180);
        c.circle(32, 26, 10, 255, 240, 90);
        c.rect(14, 50, 36, 10, 18, 18, 26);
    }
    {
        Canvas c = tile(TexTile::SodaSmashed);
        c.rect(0, 0, 64, 64, 26, 20, 40);
        c.rect(6, 5, 52, 42, 16, 16, 22);
        c.line(8, 8, 40, 38, 2, 70, 160, 180);
        c.line(52, 10, 22, 42, 2, 70, 160, 180);
        c.rect(14, 50, 36, 10, 12, 12, 16);
    }

    // --- 12: Supply Crate ---
    {
        Canvas c = tile(TexTile::Crate);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                int n = static_cast<int>(hash2d(x, y, 12) & 15) - 7;
                bool border = (x < 6 || x >= 58 || y < 6 || y >= 58 || std::abs(x - y) < 4);
                if (border) c.put(x, y, 135 + n, 95 + n, 45 + n);
                else        c.put(x, y, 105 + n, 72 + n, 34 + n);
            }
    }

    // --- 13: Acid / Plasma Floor ---
    {
        Canvas c = tile(TexTile::AcidFloor);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                float w = std::sin(x * 0.22f) + std::cos(y * 0.22f);
                int g = static_cast<int>(185 + 55 * (0.5f * w));
                c.put(x, y, 40, static_cast<uint8_t>(std::clamp(g, 90, 255)), 65);
            }
    }

    // --- 14: Club Floor ---
    {
        Canvas c = tile(TexTile::ClubFloor);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                bool alt = (((x / 16) + (y / 16)) & 1) != 0;
                bool edge = (x % 16 == 0 || y % 16 == 0);
                if (edge) c.put(x, y, 150, 35, 185);
                else if (alt) c.put(x, y, 36, 22, 58);
                else c.put(x, y, 22, 16, 38);
            }
    }

    // --- 15: Server Pillar ---
    {
        Canvas c = tile(TexTile::ServerPillar);
        c.rect(0, 0, 64, 64, 28, 32, 42);
        for (int y = 6; y < 60; y += 8) {
            c.rect(8, y, 48, 5, 16, 18, 24);
            c.rect(12, y + 1, 4, 3, (y % 16 == 6) ? 40 : 255, (y % 16 == 6) ? 245 : 60, 190);
            c.rect(20, y + 1, 4, 3, 40, 245, 255);
        }
    }

    // --- 16..31: 4 Enemies x 4 Frames ---
    for (int v = 0; v < 4; ++v) {
        for (int f = 0; f < 4; ++f) {
            Canvas c = tile(TexTile::PunkBase + v * 4 + f);
            drawEnemyHumanoid(c, v, f);
        }
    }

    // --- 32: Explosive Hazmat Barrel ---
    {
        Canvas c = tile(TexTile::Barrel);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(16, 10, 32, 50, 195, 32, 28);
        c.rect(14, 14, 36, 5, 235, 190, 30);
        c.rect(14, 48, 36, 5, 235, 190, 30);
        c.circle(32, 33, 8, 245, 210, 35);
        c.circle(32, 33, 3, 25, 25, 25);
        c.rect(22, 12, 5, 44, 235, 75, 65);
    }

    // --- 33: Medkit ---
    {
        Canvas c = tile(TexTile::Medkit);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(14, 22, 36, 30, 235, 240, 248);
        c.rect(24, 16, 16, 6, 160, 170, 185);
        c.rect(29, 27, 6, 20, 235, 35, 45);
        c.rect(22, 34, 20, 6, 235, 35, 45);
    }

    // --- 34: Cruel Cola Can (+HP +Time!) ---
    {
        Canvas c = tile(TexTile::ColaCan);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(22, 16, 20, 38, 35, 235, 255);
        c.rect(22, 26, 20, 16, 255, 45, 190);
        c.rect(24, 13, 16, 4, 210, 220, 230);
    }

    // --- 35: Armor Vest ---
    {
        Canvas c = tile(TexTile::ArmorShard);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(16, 16, 32, 36, 35, 125, 245);
        c.rect(22, 22, 20, 22, 75, 215, 255);
    }

    // --- 36: Ammo Crate ---
    {
        Canvas c = tile(TexTile::AmmoBox);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(14, 24, 36, 28, 72, 88, 56);
        c.rect(18, 28, 6, 16, 235, 190, 45);
        c.rect(29, 28, 6, 16, 235, 55, 45);
        c.rect(40, 28, 6, 16, 235, 190, 45);
    }

    // --- 37: Keycard ---
    {
        Canvas c = tile(TexTile::Keycard);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(16, 18, 32, 34, 255, 205, 35);
        c.rect(20, 24, 24, 8, 40, 245, 255);
        c.rect(22, 38, 12, 6, 35, 35, 45);
    }

    // --- 38 & 39: Projectiles ---
    {
        Canvas c = tile(TexTile::PlasmaOrb);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.circle(32, 32, 16, 255, 40, 205);
        c.circle(32, 32, 10, 60, 245, 255);
        c.circle(32, 32, 5, 255, 255, 255);
    }
    {
        Canvas c = tile(TexTile::RocketProj);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.circle(32, 32, 14, 255, 120, 25);
        c.circle(32, 32, 8, 255, 235, 95);
    }

    // --- 40..42: Explosion Frames ---
    for (int f = 0; f < 3; ++f) {
        Canvas c = tile(TexTile::Explo0 + f);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        int rOuter = 16 + f * 6;
        c.circle(32, 32, rOuter, 220 - f * 45, 70 - f * 15, 25);
        c.circle(26 + f * 3, 28, rOuter - 5, 255, 155 - f * 35, 30);
        if (f < 2) c.circle(32, 32, 10 - f * 3, 255, 250, 190);
    }

    // --- 43: Gib Chunk ---
    {
        Canvas c = tile(TexTile::GibChunk);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.circle(32, 32, 11, 210, 25, 38);
        c.circle(29, 29, 5, 255, 95, 60);
    }

    // --- 48..53: First-Person Weapon Viewmodels ---
    // 48: Loverboy .44 Triple-Revolver
    {
        Canvas c = tile(TexTile::WepRevolver);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        // Barrel assembly (angled perspective toward center crosshair)
        c.line(30, 16, 36, 44, 9, 130, 142, 162);
        c.line(29, 14, 34, 40, 4, 200, 212, 230);
        c.rect(27, 13, 6, 4, 40, 245, 255); // Neon front sight!
        // Cylinder with 3 glowing chambers
        c.circle(35, 39, 8, 78, 86, 102);
        c.circle(33, 37, 2, 255, 60, 195);
        c.circle(37, 37, 2, 40, 245, 255);
        // Cyber-gloved hand & grip
        c.rect(31, 45, 16, 19, 36, 38, 48);
        c.rect(32, 46, 14, 3, 255, 45, 185);
    }
    // 49: Disperser Riot Shotgun
    {
        Canvas c = tile(TexTile::WepShotgun);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.line(28, 12, 34, 58, 12, 58, 64, 76);
        c.line(27, 12, 32, 48, 5, 115, 124, 140);
        c.line(29, 28, 33, 48, 14, 95, 55, 32); // Pump forend
        c.rect(25, 10, 6, 3, 255, 85, 30);      // Glowing orange sight
        c.rect(26, 48, 18, 16, 34, 36, 46);
    }
    // 50: Ripper Triple-Chaingun
    {
        Canvas c = tile(TexTile::WepChaingun);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(18, 18, 28, 34, 62, 68, 84);
        c.rect(21, 12, 5, 28, 145, 155, 175);
        c.rect(29, 10, 6, 30, 175, 185, 205);
        c.rect(38, 12, 5, 28, 145, 155, 175);
        c.rect(18, 32, 28, 5, 40, 240, 255); // Glowing cooling collar
        c.rect(16, 46, 32, 18, 38, 42, 54);
    }
    // 51: Devastator Micro-RPG
    {
        Canvas c = tile(TexTile::WepRocket);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.rect(20, 10, 26, 54, 68, 76, 58);
        c.circle(27, 16, 4, 24, 24, 28);
        c.circle(39, 16, 4, 24, 24, 28);
        c.rect(22, 28, 22, 10, 22, 34, 28);
        c.rect(24, 30, 18, 6, 55, 255, 110); // Digital warhead counter screen
        c.rect(20, 10, 26, 3, 235, 185, 35);
    }
    // 52: Muzzle Flash
    {
        Canvas c = tile(TexTile::MuzzleFlash);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        c.line(32, 4, 32, 60, 8, 255, 165, 35);
        c.line(4, 32, 60, 32, 8, 255, 165, 35);
        c.line(12, 12, 52, 52, 6, 255, 90, 185);
        c.line(52, 12, 12, 52, 6, 255, 90, 185);
        c.circle(32, 32, 12, 255, 250, 210);
    }
    // 53: Mighty Cyber-Boot
    {
        Canvas c = tile(TexTile::MightyBoot);
        c.rect(0, 0, 64, 64, 0, 0, 0, 0);
        // Shin coming from bottom-left + massive sole facing enemy!
        c.line(10, 62, 30, 34, 14, 36, 42, 58);
        c.rect(20, 14, 26, 32, 28, 30, 38);
        c.rect(22, 16, 22, 28, 52, 56, 68);
        // Neon cyan sole treads!
        for (int ty = 18; ty <= 38; ty += 5) {
            c.rect(24, ty, 18, 3, 40, 245, 255);
        }
    }

    return buf;
}
