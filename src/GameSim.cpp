#include "GameSim.h"
#include "TextureAtlas.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kEyeHeight = 1.08f;
constexpr float kPlayerRadius = 0.44f;
constexpr float kPi = 3.14159265359f;

// Cell encoding helpers
constexpr uint8_t C_EMPTY        = 0;
constexpr uint8_t C_STEEL        = TexTile::SteelWall + 1;
constexpr uint8_t C_NEON         = TexTile::NeonSignWall + 1;
constexpr uint8_t C_HAZARD       = TexTile::HazardWall + 1;
constexpr uint8_t C_CLUB         = TexTile::ClubWall + 1;
constexpr uint8_t C_PILLAR       = TexTile::ServerPillar + 1;
constexpr uint8_t C_DOOR         = 20;
constexpr uint8_t C_LOCKED_DOOR  = 21;
constexpr uint8_t C_SECRET       = 22;
constexpr uint8_t C_SODA         = 23;
constexpr uint8_t C_SODA_SMASHED = 24;
constexpr uint8_t C_EXIT         = 25;
constexpr uint8_t C_CRATE        = 26;

const char* kFloor1[24] = {
    "########################",
    "#P..S..#...B....D..M...#",
    "#......#..e...e.#......#",
    "#..##..D........#..K...#",
    "#..##..#..B.....########",
    "#......#........X..W..A#",
    "####D#######D###########",
    "#..........#...........#",
    "#.N....B...#..s....B...#",
    "#....e.....D.......e...#",
    "#..........#...........#",
    "#.S..c..c..#...##..##..#",
    "#..........#...##..##..#",
    "########L###...........#",
    "#..........#....B..s...#",
    "#..e...B...#...........#",
    "#..........#####D#######",
    "#..M...s...#...........#",
    "#..........#..e..B..e..#",
    "#####D######...........#",
    "#..........D..s.....A..#",
    "#..S..B..d.#.....##....#",
    "#..........#....E##....#",
    "########################"
};

const char* kFloor2[24] = {
    "########################",
    "#P.....D...e...#..S...K#",
    "#..c...#.......D...d...#",
    "#......#...B...#.......#",
    "####D#######D#######L###",
    "#..........#...........#",
    "#..d...B...#...C...M...#",
    "#..........#....B......#",
    "#..##..##..D......s..d.#",
    "#..##..##..#...........#",
    "#..........#####D#######",
    "#...B..s...#...........#",
    "#..........#..d....B...#",
    "####D#######.....r.....#",
    "#..........#...B....d..#",
    "#.S..e..B..#...........#",
    "#..........X..R..A..M..#",
    "#..e...s...#############",
    "#..........D...........#",
    "#..B.......#..s..B..e..#",
    "#####D######...........#",
    "#...M......D....r......#",
    "#..S....d..#.........E.#",
    "########################"
};

const char* kFloor3[24] = {
    "########################",
    "#P..S..D....s....#..M..#",
    "#......#...B.....D..K..#",
    "#..A...#......e..#.....#",
    "####D#######D###########",
    "#......................#",
    "#..N....s....B....s..N.#",
    "#......................#",
    "#....B....r.....B......#",
    "#......................#",
    "#..S....d........d...S.#",
    "#...........R..........#",
    "##########L#############",
    "#..........#...........#",
    "#..s...B...D...e...B...#",
    "#..........#...........#",
    "#....r.....#.....d.....#",
    "#####D##########D#######",
    "#......................#",
    "#..S..B..s....s..B..M..#",
    "#..........r...........#",
    "#....N..........N......#",
    "#..A................E..#",
    "########################"
};

const char* kFloor4Boss[24] = {
    "########################",
    "#P..S..M..A..C..R......#",
    "#......................#",
    "#...##............##...#",
    "#...##.....B......##...#",
    "#......................#",
    "#..S................S..#",
    "#......B........B......#",
    "#......................#",
    "#...##............##...#",
    "#...##.....Z......##...#",
    "#......................#",
    "#......s........s......#",
    "#......................#",
    "#...##............##...#",
    "#...##.....B......##...#",
    "#......................#",
    "#..S................S..#",
    "#......d........d......#",
    "#......................#",
    "#...##.....A......##...#",
    "#...##............##...#",
    "#..M.......B........A..#",
    "########################"
};
} // namespace

GameSim::GameSim(SynthAudio* audio) : m_audio(audio) {
    loadHighScore();
    startNewGame(GameMode::CruelTimed, 1);
}

float GameSim::rand01() {
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return static_cast<float>(m_rng & 0xFFFFu) / 65535.0f;
}

float GameSim::randSigned() {
    return rand01() * 2.0f - 1.0f;
}

void GameSim::loadHighScore() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile f(dir + QStringLiteral("/scores.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        m_highScore = doc.object().value(QStringLiteral("highScore")).toInt(0);
    }
}

void GameSim::saveHighScore() {
    if (m_score > m_highScore) {
        m_highScore = m_score;
    }
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/scores.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject obj;
        obj.insert(QStringLiteral("highScore"), m_highScore);
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}

void GameSim::startNewGame(GameMode mode, int difficulty) {
    m_mode = mode;
    m_difficulty = std::clamp(difficulty, 0, 2);
    m_health = 100;
    m_armor = (m_difficulty == 0) ? 50 : 25;
    m_score = 0;
    m_combo = 1;
    m_comboTimer = 0.0f;
    m_totalKills = 0;
    m_weapon = 0;
    m_weaponOwned = {true, true, false, false}; // Start with Loverboy .44 & Disperser Shotgun!
    m_shells = 20;
    m_bullets = 100;
    m_rockets = 4;
    m_gameOver = false;
    m_victory = false;
    m_endSummary.clear();
    m_timeLeft = (m_difficulty == 0) ? 38.0f : (m_difficulty == 1 ? 30.0f : 24.0f);

    loadFloor(1);
    pushCallout(QStringLiteral("LET'S ROCK!"), 2.0f);
}

void GameSim::loadFloor(int floorNumber) {
    m_floor = floorNumber;
    m_cells.assign(kMapW * kMapH, C_EMPTY);
    m_doorOpen.assign(kMapW * kMapH, 0.0f);
    m_doorOpening.assign(kMapW * kMapH, false);
    m_staticLights.clear();
    m_entities.clear();
    m_projectiles.clear();
    m_particles.clear();
    m_hasKeycard = false;
    m_bossActive = false;
    m_fanBurstShotsLeft = 0;
    m_lockedEnemyIndices.clear();
    ++m_mapRevision;

    uint8_t primaryWall = C_STEEL;
    uint8_t accentWall = C_NEON;

    if (floorNumber == 1 && m_mode != GameMode::EndlessArena) {
        m_floorTitle = QStringLiteral("FL 01 // NEON ARCADE");
        m_floorTile = TexTile::GrateFloor;
        m_ceilTile = TexTile::CeilingLight;
        m_ceilHeight = 3.1f;
        primaryWall = C_STEEL;
        accentWall = C_NEON;
    } else if (floorNumber == 2 && m_mode != GameMode::EndlessArena) {
        m_floorTitle = QStringLiteral("FL 02 // CRYO-REACTOR");
        m_floorTile = TexTile::GrateFloor;
        m_ceilTile = TexTile::CeilingLight;
        m_ceilHeight = 3.3f;
        primaryWall = C_HAZARD;
        accentWall = C_STEEL;
    } else if (floorNumber == 3 && m_mode != GameMode::EndlessArena) {
        m_floorTitle = QStringLiteral("FL 03 // SYNTH-CLUB");
        m_floorTile = TexTile::ClubFloor;
        m_ceilTile = TexTile::CeilingLight;
        m_ceilHeight = 3.5f;
        primaryWall = C_CLUB;
        accentWall = C_NEON;
    } else if (floorNumber == 4 && m_mode != GameMode::EndlessArena) {
        m_floorTitle = QStringLiteral("FL 04 // HELIPAD BOSS");
        m_floorTile = TexTile::ClubFloor;
        m_ceilTile = TexTile::CeilingLight;
        m_ceilHeight = 4.2f;
        primaryWall = C_NEON;
        accentWall = C_PILLAR;
    } else {
        m_floorTitle = QStringLiteral("ARENA WAVE %1").arg(floorNumber);
        m_floorTile = (floorNumber & 1) ? TexTile::ClubFloor : TexTile::GrateFloor;
        m_ceilTile = TexTile::CeilingLight;
        m_ceilHeight = 3.6f;
        primaryWall = (floorNumber % 3 == 0) ? C_HAZARD : ((floorNumber & 1) ? C_CLUB : C_STEEL);
        accentWall = C_NEON;
    }

    auto spawnEnemy = [&](EnemyType t, float cx, float cz, bool boss = false) {
        WorldEntity e;
        e.kind = EntityKind::Enemy;
        e.enemyType = t;
        e.isBoss = boss;
        e.x = (cx + 0.5f) * kCell;
        e.z = (cz + 0.5f) * kCell;
        e.y = (t == EnemyType::Drone) ? 0.65f : 0.0f;
        e.animPhase = rand01() * 6.28f;
        float diffMul = (m_difficulty == 0) ? 0.8f : (m_difficulty == 1 ? 1.0f : 1.3f);
        switch (t) {
            case EnemyType::Punk:
                e.hp = e.maxHp = static_cast<int>(45 * diffMul);
                e.radius = 0.45f; e.height = 1.75f;
                break;
            case EnemyType::Sawbot:
                e.hp = e.maxHp = static_cast<int>(38 * diffMul);
                e.radius = 0.42f; e.height = 1.55f;
                break;
            case EnemyType::Drone:
                e.hp = e.maxHp = static_cast<int>(52 * diffMul);
                e.radius = 0.48f; e.height = 1.45f;
                break;
            case EnemyType::Brute:
                e.hp = e.maxHp = boss ? static_cast<int>(850 * diffMul) : static_cast<int>(140 * diffMul);
                e.radius = boss ? 0.85f : 0.62f;
                e.height = boss ? 2.85f : 2.15f;
                break;
        }
        e.attackCooldown = 0.6f + rand01() * 0.8f;
        m_entities.push_back(e);
        if (boss) m_bossActive = true;
    };

    auto spawnItem = [&](EntityKind k, float cx, float cz) {
        WorldEntity it;
        it.kind = k;
        it.x = (cx + 0.5f) * kCell;
        it.z = (cz + 0.5f) * kCell;
        it.y = 0.0f;
        it.radius = (k == EntityKind::Barrel) ? 0.42f : 0.5f;
        it.height = (k == EntityKind::Barrel) ? 1.25f : 0.85f;
        it.hp = it.maxHp = 22;
        m_entities.push_back(it);
    };

    if (m_mode != GameMode::EndlessArena && floorNumber >= 1 && floorNumber <= 4) {
        const char** rows = (floorNumber == 1) ? kFloor1 :
                            (floorNumber == 2) ? kFloor2 :
                            (floorNumber == 3) ? kFloor3 : kFloor4Boss;

        for (int z = 0; z < kMapH; ++z) {
            for (int x = 0; x < kMapW; ++x) {
                char ch = rows[z][x];
                const int idx = z * kMapW + x;
                const float wx = (x + 0.5f) * kCell;
                const float wz = (z + 0.5f) * kCell;
                switch (ch) {
                    case '#':
                        m_cells[idx] = ((x + z) % 5 == 0) ? accentWall : primaryWall;
                        break;
                    case 'N':
                        m_cells[idx] = C_PILLAR;
                        m_staticLights.push_back({wx, 1.8f, wz, 1.0f, 0.2f, 0.85f, 7.5f});
                        break;
                    case 'D': m_cells[idx] = C_DOOR; break;
                    case 'L': m_cells[idx] = C_LOCKED_DOOR; break;
                    case 'X': m_cells[idx] = C_SECRET; break;
                    case 'S':
                        m_cells[idx] = C_SODA;
                        m_staticLights.push_back({wx, 1.4f, wz, 0.15f, 0.9f, 1.0f, 6.0f});
                        break;
                    case 'E':
                        m_cells[idx] = C_EXIT;
                        m_staticLights.push_back({wx, 1.5f, wz, 0.2f, 1.0f, 0.45f, 7.5f});
                        break;
                    case 'c': m_cells[idx] = C_CRATE; break;
                    case 'P':
                        m_px = wx; m_pz = wz; m_py = 0.0f;
                        m_yaw = 0.0f; m_pitch = 0.0f;
                        break;
                    case 'B': spawnItem(EntityKind::Barrel, x, z); break;
                    case 'M': spawnItem(EntityKind::PickupMedkit, x, z); break;
                    case 'A': spawnItem(EntityKind::PickupAmmo, x, z); break;
                    case 'K': spawnItem(EntityKind::PickupKeycard, x, z); break;
                    case 'W': spawnItem(EntityKind::PickupWeaponShotgun, x, z); break;
                    case 'C': spawnItem(EntityKind::PickupWeaponChaingun, x, z); break;
                    case 'R': spawnItem(EntityKind::PickupWeaponRocket, x, z); break;
                    case 'e': spawnEnemy(EnemyType::Punk, x, z); break;
                    case 's': spawnEnemy(EnemyType::Sawbot, x, z); break;
                    case 'd': spawnEnemy(EnemyType::Drone, x, z); break;
                    case 'r': spawnEnemy(EnemyType::Brute, x, z, false); break;
                    case 'Z': spawnEnemy(EnemyType::Brute, x, z, true); break;
                    default: break;
                }
            }
        }
    } else {
        // Endless procedural neon arena with pillars, soda machines, barrels & escalating waves!
        m_weaponOwned = {true, true, true, true};
        for (int z = 0; z < kMapH; ++z) {
            for (int x = 0; x < kMapW; ++x) {
                int idx = z * kMapW + x;
                if (x == 0 || z == 0 || x == kMapW - 1 || z == kMapH - 1) {
                    m_cells[idx] = ((x + z) % 4 == 0) ? C_NEON : primaryWall;
                } else if ((x == 6 || x == 17) && (z == 6 || z == 17)) {
                    m_cells[idx] = C_PILLAR;
                    m_staticLights.push_back({(x + 0.5f) * kCell, 1.8f, (z + 0.5f) * kCell, 0.25f, 0.9f, 1.0f, 8.0f});
                } else if ((x == 11 && z == 1) || (x == 11 && z == kMapH - 2)) {
                    m_cells[idx] = C_SODA;
                }
            }
        }
        m_px = 3.5f * kCell;
        m_pz = 3.5f * kCell;
        m_yaw = 0.78f;
        int count = std::min(22, 7 + floorNumber * 3);
        for (int i = 0; i < count; ++i) {
            int gx = 4 + static_cast<int>(rand01() * 16.0f);
            int gz = 4 + static_cast<int>(rand01() * 16.0f);
            if (m_cells[gz * kMapW + gx] != C_EMPTY) continue;
            if (std::hypot(gx - 3.5f, gz - 3.5f) < 5.0f) continue;
            float r = rand01();
            EnemyType t = (r < 0.35f) ? EnemyType::Punk :
                          (r < 0.65f) ? EnemyType::Sawbot :
                          (r < 0.88f) ? EnemyType::Drone : EnemyType::Brute;
            spawnEnemy(t, gx, gz, false);
        }
        for (int b = 0; b < 7; ++b) {
            int gx = 3 + static_cast<int>(rand01() * 18.0f);
            int gz = 3 + static_cast<int>(rand01() * 18.0f);
            if (m_cells[gz * kMapW + gx] == C_EMPTY) spawnItem(EntityKind::Barrel, gx, gz);
        }
        spawnItem(EntityKind::PickupAmmo, 12, 11);
        spawnItem(EntityKind::PickupMedkit, 11, 12);
    }

    // Add ambient ceiling lights across open rooms
    for (int z = 3; z < kMapH - 2; z += 6) {
        for (int x = 3; x < kMapW - 2; x += 6) {
            if (m_cells[z * kMapW + x] == C_EMPTY && m_staticLights.size() < 12) {
                bool warm = ((x + z) & 2) != 0;
                m_staticLights.push_back({
                    (x + 0.5f) * kCell, m_ceilHeight - 0.3f, (z + 0.5f) * kCell,
                    warm ? 0.95f : 0.25f, warm ? 0.30f : 0.85f, warm ? 0.85f : 1.0f,
                    8.5f
                });
            }
        }
    }

    if (m_mode == GameMode::CruelTimed && floorNumber > 1) {
        m_timeLeft = std::min(45.0f, m_timeLeft + 15.0f);
    }
}

bool GameSim::isCellSolidForMove(int cx, int cz) const {
    if (cx < 0 || cz < 0 || cx >= kMapW || cz >= kMapH) return true;
    const int idx = cz * kMapW + cx;
    const uint8_t c = m_cells[idx];
    if (c == C_EMPTY) return false;
    if (c == C_DOOR || c == C_LOCKED_DOOR) return m_doorOpen[idx] < 0.78f;
    return true;
}

bool GameSim::isCellSolidForRay(int cx, int cz) const {
    if (cx < 0 || cz < 0 || cx >= kMapW || cz >= kMapH) return true;
    const int idx = cz * kMapW + cx;
    const uint8_t c = m_cells[idx];
    if (c == C_EMPTY) return false;
    if (c == C_DOOR || c == C_LOCKED_DOOR) return m_doorOpen[idx] < 0.75f;
    return true;
}

void GameSim::moveBodyWithSlide(float& x, float& z, float dx, float dz, float radius) const {
    auto collides = [&](float nx, float nz) -> bool {
        int minCx = static_cast<int>(std::floor((nx - radius) / kCell));
        int maxCx = static_cast<int>(std::floor((nx + radius) / kCell));
        int minCz = static_cast<int>(std::floor((nz - radius) / kCell));
        int maxCz = static_cast<int>(std::floor((nz + radius) / kCell));
        for (int cz = minCz; cz <= maxCz; ++cz) {
            for (int cx = minCx; cx <= maxCx; ++cx) {
                if (!isCellSolidForMove(cx, cz)) continue;
                float closestX = std::clamp(nx, cx * kCell, (cx + 1) * kCell);
                float closestZ = std::clamp(nz, cz * kCell, (cz + 1) * kCell);
                float ox = nx - closestX;
                float oz = nz - closestZ;
                if (ox * ox + oz * oz < radius * radius) return true;
            }
        }
        return false;
    };

    if (!collides(x + dx, z)) x += dx;
    if (!collides(x, z + dz)) z += dz;
}

float GameSim::castWallRay(float ox, float oy, float oz, float dx, float dy, float dz, int* hitCx, int* hitCz) const {
    const float maxDist = 48.0f;
    const float step = 0.08f;
    for (float t = 0.08f; t < maxDist; t += step) {
        float px = ox + dx * t;
        float py = oy + dy * t;
        float pz = oz + dz * t;
        if (py <= 0.0f || py >= m_ceilHeight) return t;
        int cx = static_cast<int>(std::floor(px / kCell));
        int cz = static_cast<int>(std::floor(pz / kCell));
        if (isCellSolidForRay(cx, cz)) {
            if (hitCx) *hitCx = cx;
            if (hitCz) *hitCz = cz;
            return t;
        }
    }
    return maxDist;
}

bool GameSim::hasLineOfSight(float x0, float z0, float x1, float z1) const {
    float dx = x1 - x0;
    float dz = z1 - z0;
    float dist = std::hypot(dx, dz);
    if (dist < 0.05f) return true;
    dx /= dist; dz /= dist;
    for (float t = 0.25f; t < dist - 0.2f; t += 0.22f) {
        int cx = static_cast<int>(std::floor((x0 + dx * t) / kCell));
        int cz = static_cast<int>(std::floor((z0 + dz * t) / kCell));
        if (isCellSolidForRay(cx, cz)) return false;
    }
    return true;
}

void GameSim::addLookDelta(float dYawRad, float dPitchRad) {
    m_yaw += dYawRad;
    if (m_yaw > kPi) m_yaw -= 2.0f * kPi;
    if (m_yaw < -kPi) m_yaw += 2.0f * kPi;
    m_pitch = std::clamp(m_pitch + dPitchRad, -1.08f, 1.08f);
}

void GameSim::selectWeapon(int idx) {
    if (idx >= 0 && idx < 4 && m_weaponOwned[idx]) {
        m_weapon = idx;
    }
}

void GameSim::cycleWeapon(int dir) {
    for (int i = 1; i <= 4; ++i) {
        int cand = (m_weapon + dir * i + 16) % 4;
        if (m_weaponOwned[cand]) {
            m_weapon = cand;
            break;
        }
    }
}

int GameSim::currentAmmo() const {
    switch (m_weapon) {
        case 0: return 999; // Infinite Loverboy .44!
        case 1: return m_shells;
        case 2: return m_bullets;
        case 3: return m_rockets;
    }
    return 0;
}

int GameSim::enemiesAlive() const {
    int c = 0;
    for (const auto& e : m_entities)
        if (e.active && e.kind == EntityKind::Enemy) ++c;
    return c;
}

float GameSim::bossHealthRatio() const {
    for (const auto& e : m_entities) {
        if (e.active && e.kind == EntityKind::Enemy && e.isBoss && e.maxHp > 0) {
            return std::clamp(static_cast<float>(e.hp) / static_cast<float>(e.maxHp), 0.0f, 1.0f);
        }
    }
    return 0.0f;
}

void GameSim::pushCallout(const QString& text, float duration) {
    m_callout = text;
    m_calloutTimer = duration;
}

void GameSim::addKillReward(int baseScore, float bonusSeconds, const QString& customCallout) {
    ++m_totalKills;
    m_score += baseScore * m_combo;
    m_combo = std::min(10, m_combo + 1);
    m_comboTimer = 4.0f;
    if (m_mode == GameMode::CruelTimed) {
        m_timeLeft = std::min(45.0f, m_timeLeft + bonusSeconds);
    }
    if (!customCallout.isEmpty()) {
        pushCallout(customCallout, 1.4f);
    } else if (m_combo == 4) {
        pushCallout(QStringLiteral("COME GET SOME! x4"));
    } else if (m_combo == 7) {
        pushCallout(QStringLiteral("REST IN PIECES! x7"));
    } else if (m_combo == 10) {
        pushCallout(QStringLiteral("MAXIMUM CRUELTY! x10"));
    }
    saveHighScore();
}

void GameSim::spawnGibs(float x, float y, float z, int count) {
    for (int i = 0; i < count && m_particles.size() < 260; ++i) {
        Particle3D p{};
        p.x = x + randSigned() * 0.25f;
        p.y = y + 0.4f + rand01() * 0.8f;
        p.z = z + randSigned() * 0.25f;
        p.vx = randSigned() * 5.2f;
        p.vy = 2.4f + rand01() * 4.5f;
        p.vz = randSigned() * 5.2f;
        p.r = 1.0f; p.g = 0.25f; p.b = 0.3f;
        p.size = 0.32f + rand01() * 0.18f;
        p.life = p.maxLife = 0.75f + rand01() * 0.5f;
        p.tile = TexTile::GibChunk;
        p.gravity = true;
        m_particles.push_back(p);
    }
}

void GameSim::spawnSparks(float x, float y, float z, float r, float g, float b, int count) {
    for (int i = 0; i < count && m_particles.size() < 280; ++i) {
        Particle3D p{};
        p.x = x; p.y = y; p.z = z;
        p.vx = randSigned() * 4.4f;
        p.vy = 1.0f + rand01() * 3.2f;
        p.vz = randSigned() * 4.4f;
        p.r = r; p.g = g; p.b = b;
        p.size = 0.14f + rand01() * 0.10f;
        p.life = p.maxLife = 0.28f + rand01() * 0.22f;
        p.tile = -1;
        p.gravity = true;
        m_particles.push_back(p);
    }
}

void GameSim::damageEnemy(WorldEntity& e, int dmg, float kx, float kz, bool headshot, SimEvents& ev) {
    if (!e.active) return;
    e.hp -= dmg;
    e.painTimer = 0.16f;
    e.alerted = true;
    e.vx += kx;
    e.vz += kz;
    spawnSparks(e.x, e.y + e.height * 0.65f, e.z, 1.0f, headshot ? 0.95f : 0.15f, headshot ? 0.2f : 0.35f, headshot ? 10 : 5);

    if (e.hp <= 0) {
        e.active = false;
        ev.triggerHapticHeavy = true;
        if (m_audio) m_audio->play(SfxId::EnemyGib, 0.95f, 0.9f + rand01() * 0.25f);
        spawnGibs(e.x, e.y, e.z, e.isBoss ? 26 : 9);

        if (e.isBoss) {
            addKillReward(5000, 15.0f, QStringLiteral("HESKEL-X OBLITERATED!"));
            m_victory = true;
            saveHighScore();
            m_endSummary = QStringLiteral("MEGACORP TOWER CLEARED!\nSCORE: %1  |  KILLS: %2").arg(m_score).arg(m_totalKills);
            if (m_audio) m_audio->play(SfxId::LevelClear, 1.1f, 1.0f);
        } else {
            int pts = (e.enemyType == EnemyType::Brute) ? 450 : (e.enemyType == EnemyType::Drone ? 200 : 150);
            float sec = (e.enemyType == EnemyType::Brute) ? 4.5f : 2.6f;
            addKillReward(headshot ? pts * 2 : pts, sec, headshot ? QStringLiteral("HEADSHOT! +2X") : QString());
        }
    } else {
        if (m_audio) m_audio->play(SfxId::EnemyHit, 0.65f, 0.9f + rand01() * 0.2f);
    }
}

void GameSim::detonateExplosion(float ex, float ey, float ez, float radius, int maxDmg, bool playerCredit, SimEvents& ev) {
    if (m_audio) m_audio->play(SfxId::Explosion, 1.15f, 0.92f + rand01() * 0.15f);
    ev.triggerHapticHeavy = true;
    m_shakeTimer = 0.32f;
    m_shakeMag = 0.085f;

    // Fireball sprite particles
    for (int i = 0; i < 10 && m_particles.size() < 280; ++i) {
        Particle3D p{};
        p.x = ex + randSigned() * 0.6f;
        p.y = std::max(0.3f, ey + randSigned() * 0.5f);
        p.z = ez + randSigned() * 0.6f;
        p.vx = randSigned() * 2.6f;
        p.vy = 0.8f + rand01() * 2.0f;
        p.vz = randSigned() * 2.6f;
        p.r = 1.0f; p.g = 0.8f; p.b = 0.3f;
        p.size = 1.35f + rand01() * 0.6f;
        p.life = p.maxLife = 0.42f + rand01() * 0.18f;
        p.tile = TexTile::Explo0;
        p.gravity = false;
        m_particles.push_back(p);
    }
    spawnSparks(ex, ey + 0.4f, ez, 1.0f, 0.65f, 0.15f, 18);

    // Crack nearby secret walls or crates!
    int minCx = std::max(0, static_cast<int>((ex - radius) / kCell));
    int maxCx = std::min(kMapW - 1, static_cast<int>((ex + radius) / kCell));
    int minCz = std::max(0, static_cast<int>((ez - radius) / kCell));
    int maxCz = std::min(kMapH - 1, static_cast<int>((ez + radius) / kCell));
    for (int cz = minCz; cz <= maxCz; ++cz) {
        for (int cx = minCx; cx <= maxCx; ++cx) {
            int idx = cz * kMapW + cx;
            float wx = (cx + 0.5f) * kCell;
            float wz = (cz + 0.5f) * kCell;
            if (std::hypot(wx - ex, wz - ez) <= radius) {
                if (m_cells[idx] == C_SECRET) {
                    m_cells[idx] = C_EMPTY;
                    ++m_mapRevision;
                    addKillReward(1000, 5.0f, QStringLiteral("SECRET WALL BLOWN OPEN!"));
                } else if (m_cells[idx] == C_CRATE) {
                    m_cells[idx] = C_EMPTY;
                    ++m_mapRevision;
                }
            }
        }
    }

    // Splash damage to enemies & chain-detonate barrels!
    for (size_t i = 0; i < m_entities.size(); ++i) {
        WorldEntity& e = m_entities[i];
        if (!e.active) continue;
        float dx = e.x - ex;
        float dz = e.z - ez;
        float d = std::hypot(dx, dz);
        if (d > radius) continue;
        float falloff = 1.0f - (d / radius) * 0.7f;
        if (e.kind == EntityKind::Enemy) {
            float inv = (d > 0.05f) ? (1.0f / d) : 0.0f;
            int dmg = static_cast<int>(maxDmg * falloff);
            damageEnemy(e, dmg, dx * inv * 9.0f * falloff, dz * inv * 9.0f * falloff, false, ev);
        } else if (e.kind == EntityKind::Barrel) {
            e.active = false;
            if (playerCredit) addKillReward(100, 1.5f, QStringLiteral("CHAIN BLAST!"));
            detonateExplosion(e.x, 0.6f, e.z, 4.4f, 125, playerCredit, ev);
        }
    }

    // Mild self-splash if standing right on top of explosion
    float pd = std::hypot(m_px - ex, m_pz - ez);
    if (pd < radius * 0.65f) {
        int selfDmg = static_cast<int>(24.0f * (1.0f - pd / (radius * 0.65f)));
        if (selfDmg > 0) {
            int absorb = std::min(m_armor, selfDmg / 2);
            m_armor -= absorb;
            m_health -= (selfDmg - absorb);
            m_hurtFlash = 0.45f;
        }
    }
}

void GameSim::fireHitscanPellet(float dirX, float dirY, float dirZ, int dmg, float knockback, bool headshotCapable, SimEvents& ev) {
    const float ox = m_px;
    const float oy = m_py + kEyeHeight;
    const float oz = m_pz;

    int hitCx = -1, hitCz = -1;
    float bestT = castWallRay(ox, oy, oz, dirX, dirY, dirZ, &hitCx, &hitCz);
    int hitEntityIdx = -1;
    bool isHeadshot = false;

    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i) {
        const WorldEntity& e = m_entities[i];
        if (!e.active || (e.kind != EntityKind::Enemy && e.kind != EntityKind::Barrel)) continue;

        // Ray-cylinder intersection in XZ, then check Y bounds
        float rx = e.x - ox;
        float rz = e.z - oz;
        float horizLen2 = dirX * dirX + dirZ * dirZ;
        if (horizLen2 < 1e-5f) continue;
        float t = (rx * dirX + rz * dirZ) / horizLen2;
        if (t < 0.15f || t >= bestT) continue;
        float closestX = ox + dirX * t - e.x;
        float closestZ = oz + dirZ * t - e.z;
        float hitRad = e.radius * 1.18f; // Generous mobile hitbox!
        if (closestX * closestX + closestZ * closestZ <= hitRad * hitRad) {
            float hitY = oy + dirY * t;
            if (hitY >= e.y - 0.1f && hitY <= e.y + e.height + 0.12f) {
                bestT = t;
                hitEntityIdx = i;
                isHeadshot = headshotCapable && (hitY >= e.y + e.height * 0.68f);
            }
        }
    }

    if (hitEntityIdx >= 0) {
        WorldEntity& target = m_entities[hitEntityIdx];
        if (target.kind == EntityKind::Enemy) {
            damageEnemy(target, isHeadshot ? dmg * 2 : dmg, dirX * knockback, dirZ * knockback, isHeadshot, ev);
        } else if (target.kind == EntityKind::Barrel) {
            target.hp -= dmg;
            spawnSparks(target.x, 0.7f, target.z, 1.0f, 0.85f, 0.2f, 6);
            if (target.hp <= 0) {
                target.active = false;
                addKillReward(100, 1.8f, QStringLiteral("BOOM!"));
                detonateExplosion(target.x, 0.6f, target.z, 4.5f, 130, true, ev);
            }
        }
    } else {
        // Wall spark impact + breakable crate check
        float hx = ox + dirX * (bestT - 0.08f);
        float hy = std::clamp(oy + dirY * (bestT - 0.08f), 0.1f, m_ceilHeight - 0.1f);
        float hz = oz + dirZ * (bestT - 0.08f);
        spawnSparks(hx, hy, hz, 0.35f, 0.95f, 1.0f, 4);
        if (hitCx >= 0 && hitCz >= 0) {
            int idx = hitCz * kMapW + hitCx;
            if (m_cells[idx] == C_CRATE) {
                m_cells[idx] = C_EMPTY;
                ++m_mapRevision;
                spawnSparks((hitCx + 0.5f) * kCell, 0.8f, (hitCz + 0.5f) * kCell, 1.0f, 0.75f, 0.3f, 12);
                // Pop a Cola Can or Ammo out of smashed crate!
                WorldEntity drop{};
                drop.kind = (rand01() < 0.65f) ? EntityKind::PickupCola : EntityKind::PickupAmmo;
                drop.x = (hitCx + 0.5f) * kCell;
                drop.z = (hitCz + 0.5f) * kCell;
                drop.radius = 0.5f; drop.height = 0.8f;
                m_entities.push_back(drop);
            }
        }
    }
}

void GameSim::fireCurrentWeapon(SimEvents& ev) {
    const float cy = std::cos(m_yaw), sy = std::sin(m_yaw);
    const float cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    const float fwdX = sy * cp;
    const float fwdY = sp;
    const float fwdZ = -cy * cp;
    const float rightX = cy;
    const float rightZ = sy;

    if (m_weapon == 0) {
        // [1] LOVERBOY .44 TRIPLE-REVOLVER
        m_fireCooldown = 0.21f;
        m_recoil = 1.0f;
        m_muzzleTimer = 0.075f;
        m_shakeTimer = 0.08f; m_shakeMag = 0.018f;
        ev.triggerHapticLight = true;
        if (m_audio) m_audio->play(SfxId::Revolver, 0.9f, 0.96f + rand01() * 0.08f);
        fireHitscanPellet(fwdX, fwdY, fwdZ, 28, 3.2f, true, ev);
    } else if (m_weapon == 1) {
        // [2] DISPERSER RIOT SHOTGUN
        if (m_shells <= 0) { selectWeapon(0); return; }
        --m_shells;
        m_fireCooldown = 0.58f;
        m_recoil = 1.45f;
        m_muzzleTimer = 0.10f;
        m_shakeTimer = 0.18f; m_shakeMag = 0.048f;
        ev.triggerHapticHeavy = true;
        if (m_audio) m_audio->play(SfxId::Shotgun, 1.05f, 0.96f + rand01() * 0.08f);
        for (int p = 0; p < 8; ++p) {
            float sx = randSigned() * 0.068f;
            float syOff = randSigned() * 0.045f;
            float dx = fwdX + rightX * sx;
            float dy = fwdY + syOff;
            float dz = fwdZ + rightZ * sx;
            float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
            fireHitscanPellet(dx * inv, dy * inv, dz * inv, 14, 2.2f, false, ev);
        }
    } else if (m_weapon == 2) {
        // [3] RIPPER TRIPLE-CHAINGUN
        if (m_bullets <= 0) { selectWeapon(0); return; }
        --m_bullets;
        m_fireCooldown = 0.088f;
        m_recoil = 0.65f;
        m_muzzleTimer = 0.055f;
        m_shakeTimer = 0.06f; m_shakeMag = 0.015f;
        ev.triggerHapticLight = true;
        if (m_audio) m_audio->play(SfxId::Chaingun, 0.75f, 0.92f + rand01() * 0.16f);
        float sx = randSigned() * 0.032f;
        float syOff = randSigned() * 0.024f;
        float dx = fwdX + rightX * sx;
        float dy = fwdY + syOff;
        float dz = fwdZ + rightZ * sx;
        float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
        fireHitscanPellet(dx * inv, dy * inv, dz * inv, 15, 1.5f, false, ev);
    } else if (m_weapon == 3) {
        // [4] DEVASTATOR MICRO-RPG
        if (m_rockets <= 0) { selectWeapon(1); return; }
        --m_rockets;
        m_fireCooldown = 0.68f;
        m_recoil = 1.5f;
        m_muzzleTimer = 0.11f;
        m_shakeTimer = 0.16f; m_shakeMag = 0.04f;
        ev.triggerHapticHeavy = true;
        if (m_audio) m_audio->play(SfxId::RocketLaunch, 1.0f, 1.0f);
        Projectile pr{};
        pr.fromPlayer = true;
        pr.explosive = true;
        pr.x = m_px + fwdX * 0.55f;
        pr.y = m_py + kEyeHeight - 0.12f;
        pr.z = m_pz + fwdZ * 0.55f;
        pr.vx = fwdX * 18.5f;
        pr.vy = fwdY * 18.5f;
        pr.vz = fwdZ * 18.5f;
        pr.damage = 140;
        pr.life = 3.5f;
        pr.tile = TexTile::RocketProj;
        m_projectiles.push_back(pr);
    }
}

void GameSim::triggerAltFire() {
    if (m_gameOver || m_victory || m_fanBurstShotsLeft > 0) return;

    // Ion Fury tribute: LOVERBOY FAN-THE-HAMMER Lock-On 6-shot auto-burst!
    m_weapon = 0;
    m_lockedEnemyIndices.clear();
    const float fwdX = std::sin(m_yaw);
    const float fwdZ = -std::cos(m_yaw);

    for (int i = 0; i < static_cast<int>(m_entities.size()); ++i) {
        const auto& e = m_entities[i];
        if (!e.active || e.kind != EntityKind::Enemy) continue;
        float dx = e.x - m_px;
        float dz = e.z - m_pz;
        float dist = std::hypot(dx, dz);
        if (dist > 18.0f || dist < 0.2f) continue;
        float dot = (dx * fwdX + dz * fwdZ) / dist;
        if (dot > 0.55f && hasLineOfSight(m_px, m_pz, e.x, e.z)) {
            m_lockedEnemyIndices.push_back(i);
            if (m_lockedEnemyIndices.size() >= 4) break;
        }
    }

    m_fanBurstShotsLeft = 6;
    m_fanBurstTimer = 0.01f;
    if (m_audio) m_audio->play(SfxId::LockOn, 0.9f, 1.15f);
    if (!m_lockedEnemyIndices.empty()) {
        pushCallout(QStringLiteral("FAN THE HAMMER!"), 1.1f);
    }
}

void GameSim::triggerBootKick() {
    if (m_gameOver || m_victory || m_bootCooldown > 0.0f) return;
    m_bootTimer = 0.30f;
    m_bootCooldown = 0.36f;
    m_shakeTimer = 0.12f;
    m_shakeMag = 0.032f;
    if (m_audio) m_audio->play(SfxId::BootKick, 1.0f, 1.0f);

    const float fwdX = std::sin(m_yaw);
    const float fwdZ = -std::cos(m_yaw);

    // 1) Check walls right in front of boot (Soda Machine, Secret Wall, Crate, Door, Exit!)
    for (float d = 0.7f; d <= 2.35f; d += 0.45f) {
        int cx = static_cast<int>(std::floor((m_px + fwdX * d) / kCell));
        int cz = static_cast<int>(std::floor((m_pz + fwdZ * d) / kCell));
        if (cx < 0 || cz < 0 || cx >= kMapW || cz >= kMapH) break;
        int idx = cz * kMapW + cx;
        uint8_t c = m_cells[idx];
        if (c == C_SODA) {
            m_cells[idx] = C_SODA_SMASHED;
            ++m_mapRevision;
            m_health = std::min(150, m_health + 20);
            m_pickupFlash = 0.35f;
            addKillReward(250, 4.0f, QStringLiteral("CRUEL COLA! +20 HP +4s"));
            if (m_audio) m_audio->play(SfxId::SodaCan, 1.0f, 1.0f);
            spawnSparks((cx + 0.5f) * kCell, 1.1f, (cz + 0.5f) * kCell, 0.2f, 0.95f, 1.0f, 16);
            break;
        } else if (c == C_SECRET) {
            m_cells[idx] = C_EMPTY;
            ++m_mapRevision;
            addKillReward(1000, 5.0f, QStringLiteral("SECRET KICKED OPEN!"));
            spawnSparks((cx + 0.5f) * kCell, 1.1f, (cz + 0.5f) * kCell, 1.0f, 0.8f, 0.25f, 18);
            break;
        } else if (c == C_CRATE) {
            m_cells[idx] = C_EMPTY;
            ++m_mapRevision;
            WorldEntity can{};
            can.kind = EntityKind::PickupCola;
            can.x = (cx + 0.5f) * kCell;
            can.z = (cz + 0.5f) * kCell;
            can.radius = 0.5f; can.height = 0.8f;
            m_entities.push_back(can);
            spawnSparks(can.x, 0.8f, can.z, 1.0f, 0.7f, 0.25f, 14);
            break;
        } else if (c == C_DOOR) {
            m_doorOpening[idx] = true;
            if (m_audio) m_audio->play(SfxId::DoorOpen, 0.9f, 1.0f);
            break;
        } else if (c == C_LOCKED_DOOR) {
            if (m_hasKeycard) {
                m_doorOpening[idx] = true;
                pushCallout(QStringLiteral("KEYCARD ACCEPTED!"));
                if (m_audio) m_audio->play(SfxId::DoorOpen, 1.0f, 1.15f);
            } else {
                pushCallout(QStringLiteral("NEED GOLD KEYCARD!"));
            }
            break;
        } else if (c == C_EXIT) {
            triggerInteract();
            return;
        }
    }

    // 2) Kick enemies & launch explosive barrels forward like bowling bombs!
    SimEvents dummyEv;
    for (auto& e : m_entities) {
        if (!e.active) continue;
        float dx = e.x - m_px;
        float dz = e.z - m_pz;
        float dist = std::hypot(dx, dz);
        if (dist > 2.85f || dist < 0.05f) continue;
        float dot = (dx * fwdX + dz * fwdZ) / dist;
        if (dot < 0.45f) continue;

        if (e.kind == EntityKind::Barrel) {
            e.kickedFlying = true;
            e.vx = fwdX * 16.5f;
            e.vz = fwdZ * 16.5f;
            pushCallout(QStringLiteral("BARREL BOWLING!"), 1.1f);
        } else if (e.kind == EntityKind::Enemy) {
            e.kickedFlying = true;
            damageEnemy(e, 42, fwdX * 14.0f, fwdZ * 14.0f, false, dummyEv);
            if (e.active) pushCallout(QStringLiteral("MIGHTY BOOT!"), 1.0f);
        }
    }
}

void GameSim::triggerJumpOrDash(float moveForward, float moveStrafe) {
    if (m_gameOver || m_victory) return;
    if (m_onGround) {
        m_pvy = 6.2f;
        m_onGround = false;
    }
    if (m_dashCooldown <= 0.0f) {
        float cy = std::cos(m_yaw), sy = std::sin(m_yaw);
        float fx = (std::abs(moveForward) > 0.1f || std::abs(moveStrafe) > 0.1f)
                       ? (sy * moveForward + cy * moveStrafe)
                       : sy;
        float fz = (std::abs(moveForward) > 0.1f || std::abs(moveStrafe) > 0.1f)
                       ? (-cy * moveForward + sy * moveStrafe)
                       : -cy;
        float len = std::hypot(fx, fz);
        if (len > 0.01f) { fx /= len; fz /= len; }
        m_pvx += fx * 9.5f;
        m_pvz += fz * 9.5f;
        m_dashCooldown = 0.85f;
        if (m_audio) m_audio->play(SfxId::BootKick, 0.55f, 1.45f);
    }
}

void GameSim::triggerInteract() {
    if (m_gameOver || m_victory) return;
    const float fwdX = std::sin(m_yaw);
    const float fwdZ = -std::cos(m_yaw);

    for (float d = 0.6f; d <= 2.5f; d += 0.45f) {
        int cx = static_cast<int>(std::floor((m_px + fwdX * d) / kCell));
        int cz = static_cast<int>(std::floor((m_pz + fwdZ * d) / kCell));
        if (cx < 0 || cz < 0 || cx >= kMapW || cz >= kMapH) break;
        int idx = cz * kMapW + cx;
        uint8_t c = m_cells[idx];
        if (c == C_DOOR) {
            m_doorOpening[idx] = true;
            if (m_audio) m_audio->play(SfxId::DoorOpen, 0.9f, 1.0f);
            return;
        } else if (c == C_LOCKED_DOOR) {
            if (m_hasKeycard) {
                m_doorOpening[idx] = true;
                pushCallout(QStringLiteral("KEYCARD UNLOCKED!"));
                if (m_audio) m_audio->play(SfxId::DoorOpen, 1.0f, 1.15f);
            } else {
                pushCallout(QStringLiteral("LOCKED — FIND GOLD KEYCARD!"));
            }
            return;
        } else if (c == C_EXIT) {
            if (m_audio) m_audio->play(SfxId::LevelClear, 1.0f, 1.0f);
            m_score += 1000 * m_floor;
            saveHighScore();
            loadFloor(m_floor + 1);
            pushCallout(QStringLiteral("ENTERING %1").arg(m_floorTitle), 2.2f);
            return;
        } else if (c == C_SODA || c == C_SECRET) {
            triggerBootKick();
            return;
        }
    }
}

void GameSim::updateContextPrompt() {
    m_contextPrompt.clear();
    const float fwdX = std::sin(m_yaw);
    const float fwdZ = -std::cos(m_yaw);
    for (float d = 0.6f; d <= 2.4f; d += 0.45f) {
        int cx = static_cast<int>(std::floor((m_px + fwdX * d) / kCell));
        int cz = static_cast<int>(std::floor((m_pz + fwdZ * d) / kCell));
        if (cx < 0 || cz < 0 || cx >= kMapW || cz >= kMapH) break;
        uint8_t c = m_cells[cz * kMapW + cx];
        if (c == C_EXIT) { m_contextPrompt = QStringLiteral("TAP [BOOT/USE] — NEXT FLOOR!"); return; }
        if (c == C_SODA) { m_contextPrompt = QStringLiteral("KICK SODA MACHINE (+HP +TIME)"); return; }
        if (c == C_LOCKED_DOOR && !m_hasKeycard) { m_contextPrompt = QStringLiteral("GOLD KEYCARD REQUIRED"); return; }
        if (c == C_LOCKED_DOOR && m_hasKeycard)  { m_contextPrompt = QStringLiteral("TAP [BOOT/USE] — UNLOCK GATE"); return; }
        if (c == C_SECRET) { m_contextPrompt = QStringLiteral("CRACKED WALL — KICK IT!"); return; }
    }
}

SimEvents GameSim::update(float dt, float moveForward, float moveStrafe, bool fireHeld, bool autoFire) {
    SimEvents ev{};
    dt = std::min(dt, 0.05f);
    if (m_gameOver || m_victory) return ev;

    // 1) Timers & CRUEL Adrenaline Clock
    if (m_fireCooldown > 0.0f) m_fireCooldown -= dt;
    if (m_muzzleTimer > 0.0f) m_muzzleTimer -= dt;
    if (m_bootTimer > 0.0f) m_bootTimer -= dt;
    if (m_bootCooldown > 0.0f) m_bootCooldown -= dt;
    if (m_dashCooldown > 0.0f) m_dashCooldown -= dt;
    if (m_shakeTimer > 0.0f) m_shakeTimer -= dt;
    if (m_hurtFlash > 0.0f) m_hurtFlash = std::max(0.0f, m_hurtFlash - dt * 1.8f);
    if (m_pickupFlash > 0.0f) m_pickupFlash = std::max(0.0f, m_pickupFlash - dt * 2.2f);
    if (m_calloutTimer > 0.0f) m_calloutTimer -= dt;
    m_recoil = std::max(0.0f, m_recoil - dt * 5.2f);

    if (m_combo > 1) {
        m_comboTimer -= dt;
        if (m_comboTimer <= 0.0f) {
            m_combo = 1;
        }
    }

    if (m_mode == GameMode::CruelTimed) {
        m_timeLeft -= dt;
        if (m_audio) m_audio->setUrgency(m_timeLeft < 7.5f);
        if (m_timeLeft <= 0.0f) {
            m_timeLeft = 0.0f;
            m_gameOver = true;
            saveHighScore();
            m_endSummary = QStringLiteral("OUT OF ADRENALINE TIME!\nSCORE: %1  |  KILLS: %2").arg(m_score).arg(m_totalKills);
            if (m_audio) m_audio->play(SfxId::PlayerHurt, 1.1f, 0.7f);
            return ev;
        }
    } else if (m_audio) {
        m_audio->setUrgency(m_bossActive);
    }

    // Endless arena wave clear progression!
    if (m_mode == GameMode::EndlessArena && enemiesAlive() == 0) {
        if (m_audio) m_audio->play(SfxId::LevelClear, 1.0f, 1.05f);
        m_score += 1200 * m_floor;
        m_health = std::min(150, m_health + 25);
        saveHighScore();
        loadFloor(m_floor + 1);
        pushCallout(QStringLiteral("WAVE %1 — GET SOME!").arg(m_floor), 2.0f);
        return ev;
    }

    // 2) Auto-open unlocked doors when walking right up to them (huge mobile QoL!)
    {
        int pcx = static_cast<int>(std::floor(m_px / kCell));
        int pcz = static_cast<int>(std::floor(m_pz / kCell));
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = pcx + dx, nz = pcz + dz;
                if (nx < 0 || nz < 0 || nx >= kMapW || nz >= kMapH) continue;
                int idx = nz * kMapW + nx;
                uint8_t c = m_cells[idx];
                if (c == C_DOOR || (c == C_LOCKED_DOOR && m_hasKeycard)) {
                    float wx = (nx + 0.5f) * kCell;
                    float wz = (nz + 0.5f) * kCell;
                    if (std::hypot(wx - m_px, wz - m_pz) < 2.25f && !m_doorOpening[idx]) {
                        m_doorOpening[idx] = true;
                        if (m_audio) m_audio->play(SfxId::DoorOpen, 0.85f, 1.0f);
                        if (c == C_LOCKED_DOOR) pushCallout(QStringLiteral("KEYCARD GATE OPENED!"));
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < m_doorOpening.size(); ++i) {
        if (m_doorOpening[i] && m_doorOpen[i] < 1.0f) {
            m_doorOpen[i] = std::min(1.0f, m_doorOpen[i] + dt * 2.4f);
        }
    }

    // 3) Player movement, jump/gravity, strafe-tilt & view-bob
    const float cy = std::cos(m_yaw), sy = std::sin(m_yaw);
    const float speed = 7.4f;
    float wishX = (sy * moveForward + cy * moveStrafe) * speed;
    float wishZ = (-cy * moveForward + sy * moveStrafe) * speed;

    m_pvx *= std::exp(-8.5f * dt);
    m_pvz *= std::exp(-8.5f * dt);

    moveBodyWithSlide(m_px, m_pz, (wishX + m_pvx) * dt, (wishZ + m_pvz) * dt, kPlayerRadius);

    // Smooth strafe camera roll (Quake / Ion Fury feel!)
    float targetRoll = -moveStrafe * 0.042f;
    m_roll += (targetRoll - m_roll) * std::min(1.0f, dt * 12.0f);

    float moveMag = std::min(1.0f, std::hypot(moveForward, moveStrafe));
    if (m_onGround && moveMag > 0.08f) {
        m_walkBobPhase += dt * (8.5f + 4.0f * moveMag);
    }

    if (!m_onGround || m_pvy != 0.0f) {
        m_py += m_pvy * dt;
        m_pvy -= 17.5f * dt;
        if (m_py <= 0.0f) {
            m_py = 0.0f;
            m_pvy = 0.0f;
            m_onGround = true;
        }
    }

    // 4) Check if crosshair is over a live enemy (for reticle glow & optional auto-fire)
    m_crosshairTarget = false;
    {
        float cp = std::cos(m_pitch), sp = std::sin(m_pitch);
        float dx = sy * cp, dy = sp, dz = -cy * cp;
        float wallDist = castWallRay(m_px, m_py + kEyeHeight, m_pz, dx, dy, dz);
        for (const auto& e : m_entities) {
            if (!e.active || e.kind != EntityKind::Enemy) continue;
            float rx = e.x - m_px, rz = e.z - m_pz;
            float dist = std::hypot(rx, rz);
            if (dist >= wallDist || dist < 0.2f) continue;
            float proj = (rx * sy + rz * (-cy));
            if (proj <= 0.0f) continue;
            float perp = std::abs(rx * cy + rz * sy);
            if (perp <= e.radius * 1.15f) {
                m_crosshairTarget = true;
                break;
            }
        }
    }

    // 5) Fan-the-Hammer burst or primary firing
    if (m_fanBurstShotsLeft > 0) {
        m_fanBurstTimer -= dt;
        if (m_fanBurstTimer <= 0.0f) {
            m_fanBurstTimer = 0.075f;
            --m_fanBurstShotsLeft;
            m_recoil = 1.1f;
            m_muzzleTimer = 0.055f;
            ev.triggerHapticLight = true;
            if (m_audio) m_audio->play(SfxId::Revolver, 0.85f, 1.05f + (6 - m_fanBurstShotsLeft) * 0.04f);

            int targetIdx = -1;
            if (!m_lockedEnemyIndices.empty()) {
                for (int tries = 0; tries < 4; ++tries) {
                    int cand = m_lockedEnemyIndices[(6 - m_fanBurstShotsLeft + tries) % m_lockedEnemyIndices.size()];
                    if (cand >= 0 && cand < static_cast<int>(m_entities.size()) && m_entities[cand].active) {
                        targetIdx = cand;
                        break;
                    }
                }
            }
            if (targetIdx >= 0) {
                const auto& te = m_entities[targetIdx];
                float dx = te.x - m_px;
                float dy = (te.y + te.height * 0.72f) - (m_py + kEyeHeight);
                float dz = te.z - m_pz;
                float inv = 1.0f / std::sqrt(dx * dx + dy * dy + dz * dz);
                fireHitscanPellet(dx * inv, dy * inv, dz * inv, 26, 3.0f, true, ev);
            } else {
                float cp = std::cos(m_pitch), sp = std::sin(m_pitch);
                fireHitscanPellet(sy * cp, sp, -cy * cp, 26, 3.0f, true, ev);
            }
            if (m_fanBurstShotsLeft == 0) {
                m_lockedEnemyIndices.clear();
                m_fireCooldown = 0.28f;
            }
        }
    } else if ((fireHeld || (autoFire && m_crosshairTarget)) && m_fireCooldown <= 0.0f) {
        fireCurrentWeapon(ev);
    }

    // 6) Update entities: Pickups, Flying Barrels, and Enemy AI!
    for (size_t i = 0; i < m_entities.size(); ++i) {
        WorldEntity& e = m_entities[i];
        if (!e.active) continue;
        e.animPhase += dt * 7.5f;
        if (e.painTimer > 0.0f) e.painTimer -= dt;

        float dxP = m_px - e.x;
        float dzP = m_pz - e.z;
        float distP = std::hypot(dxP, dzP);

        // Pickups
        if (e.kind >= EntityKind::PickupMedkit) {
            if (distP < 1.05f) {
                bool consumed = true;
                switch (e.kind) {
                    case EntityKind::PickupMedkit:
                        if (m_health >= 150) consumed = false;
                        else { m_health = std::min(150, m_health + 30); pushCallout(QStringLiteral("MEDKIT +30 HP")); }
                        break;
                    case EntityKind::PickupCola:
                        m_health = std::min(150, m_health + 15);
                        if (m_mode == GameMode::CruelTimed) m_timeLeft = std::min(45.0f, m_timeLeft + 3.5f);
                        pushCallout(QStringLiteral("CRUEL COLA! +15 HP +3.5s"));
                        if (m_audio) m_audio->play(SfxId::SodaCan, 0.95f, 1.05f);
                        break;
                    case EntityKind::PickupArmor:
                        m_armor = std::min(150, m_armor + 35);
                        pushCallout(QStringLiteral("CYBER ARMOR +35"));
                        break;
                    case EntityKind::PickupAmmo:
                        m_shells = std::min(80, m_shells + 10);
                        m_bullets = std::min(300, m_bullets + 45);
                        m_rockets = std::min(40, m_rockets + 4);
                        pushCallout(QStringLiteral("AMMO PACK LOADED!"));
                        break;
                    case EntityKind::PickupKeycard:
                        m_hasKeycard = true;
                        m_score += 500;
                        pushCallout(QStringLiteral("GOLD KEYCARD ACQUIRED!"), 2.0f);
                        break;
                    case EntityKind::PickupWeaponShotgun:
                        m_weaponOwned[1] = true; m_shells += 12; m_weapon = 1;
                        pushCallout(QStringLiteral("DISPERSER SHOTGUN!"), 1.8f);
                        break;
                    case EntityKind::PickupWeaponChaingun:
                        m_weaponOwned[2] = true; m_bullets += 80; m_weapon = 2;
                        pushCallout(QStringLiteral("RIPPER CHAINGUN UNLOCKED!"), 2.0f);
                        break;
                    case EntityKind::PickupWeaponRocket:
                        m_weaponOwned[3] = true; m_rockets += 8; m_weapon = 3;
                        pushCallout(QStringLiteral("DEVASTATOR MICRO-RPG!"), 2.0f);
                        break;
                    default: break;
                }
                if (consumed) {
                    e.active = false;
                    m_pickupFlash = 0.28f;
                    ev.triggerHapticLight = true;
                    if (e.kind != EntityKind::PickupCola && m_audio) m_audio->play(SfxId::Pickup, 0.85f, 1.0f);
                }
            }
            continue;
        }

        // Kicked / sliding barrels!
        if (e.kind == EntityKind::Barrel) {
            if (std::hypot(e.vx, e.vz) > 0.4f) {
                float prevX = e.x, prevZ = e.z;
                moveBodyWithSlide(e.x, e.z, e.vx * dt, e.vz * dt, e.radius);
                bool hitWall = (std::hypot(e.x - prevX, e.z - prevZ) < std::hypot(e.vx * dt, e.vz * dt) * 0.5f);
                bool hitEnemy = false;
                for (const auto& other : m_entities) {
                    if (other.active && other.kind == EntityKind::Enemy &&
                        std::hypot(other.x - e.x, other.z - e.z) < (e.radius + other.radius + 0.2f)) {
                        hitEnemy = true;
                        break;
                    }
                }
                if (e.kickedFlying && (hitWall || hitEnemy)) {
                    e.active = false;
                    addKillReward(200, 2.5f, QStringLiteral("STRIKE!!"));
                    detonateExplosion(e.x, 0.6f, e.z, 4.8f, 150, true, ev);
                    continue;
                }
                e.vx *= std::exp(-4.0f * dt);
                e.vz *= std::exp(-4.0f * dt);
            }
            continue;
        }

        // Enemy physics + AI
        if (std::hypot(e.vx, e.vz) > 0.1f) {
            float prevX = e.x, prevZ = e.z;
            moveBodyWithSlide(e.x, e.z, e.vx * dt, e.vz * dt, e.radius);
            if (e.kickedFlying && std::hypot(e.x - prevX, e.z - prevZ) < std::hypot(e.vx * dt, e.vz * dt) * 0.45f) {
                e.kickedFlying = false;
                damageEnemy(e, 25, 0.0f, 0.0f, false, ev);
                if (!e.active) continue;
            }
            e.vx *= std::exp(-6.5f * dt);
            e.vz *= std::exp(-6.5f * dt);
            if (std::hypot(e.vx, e.vz) < 1.2f) e.kickedFlying = false;
        }

        bool canSee = (distP < 22.0f) && hasLineOfSight(e.x, e.z, m_px, m_pz);
        if (canSee || distP < 5.0f) e.alerted = true;
        if (!e.alerted) continue;

        // Move toward player (Sawbots sprint fast; others weave & maintain range)
        float moveSpd = (e.enemyType == EnemyType::Sawbot) ? 5.3f :
                        (e.enemyType == EnemyType::Drone)  ? 3.8f :
                        (e.isBoss ? 3.2f : 3.4f);
        float minRange = (e.enemyType == EnemyType::Sawbot) ? 1.15f : 3.8f;

        if (distP > minRange && e.painTimer <= 0.0f) {
            float nx = dxP / distP;
            float nz = dzP / distP;
            float strafe = std::sin(e.animPhase * 0.45f + static_cast<float>(i)) * 0.45f;
            float mx = (nx - nz * strafe) * moveSpd * dt;
            float mz = (nz + nx * strafe) * moveSpd * dt;
            moveBodyWithSlide(e.x, e.z, mx, mz, e.radius);
        }

        // Attacks
        e.attackCooldown -= dt;
        if (e.attackCooldown <= 0.0f && canSee) {
            const float diffDmg = (m_difficulty == 0) ? 0.65f : (m_difficulty == 1 ? 1.0f : 1.3f);
            if (e.enemyType == EnemyType::Sawbot) {
                if (distP < 1.85f) {
                    e.attackCooldown = 0.75f;
                    e.stateTimer = 0.22f;
                    int dmg = static_cast<int>(14 * diffDmg);
                    int absA = std::min(m_armor, dmg / 2);
                    m_armor -= absA; m_health -= (dmg - absA);
                    m_hurtFlash = 0.55f;
                    ev.triggerHapticHeavy = true;
                    if (m_audio) m_audio->play(SfxId::PlayerHurt, 0.9f, 1.0f);
                } else {
                    e.attackCooldown = 0.25f;
                }
            } else if (e.enemyType == EnemyType::Punk) {
                e.attackCooldown = 1.45f + rand01() * 0.5f;
                e.stateTimer = 0.25f;
                // Fires visible plasma-buckshot bolt so player can dodge on touch controls!
                Projectile pr{};
                pr.fromPlayer = false;
                pr.x = e.x; pr.y = 1.05f; pr.z = e.z;
                float inv = 1.0f / std::max(0.2f, distP);
                pr.vx = dxP * inv * 11.5f + randSigned() * 0.9f;
                pr.vy = ((m_py + 0.95f) - 1.05f) * inv * 11.5f;
                pr.vz = dzP * inv * 11.5f + randSigned() * 0.9f;
                pr.damage = static_cast<int>(11 * diffDmg);
                pr.life = 2.8f;
                pr.tile = TexTile::PlasmaOrb;
                m_projectiles.push_back(pr);
                if (m_audio) m_audio->play(SfxId::Shotgun, 0.45f, 1.25f);
            } else if (e.enemyType == EnemyType::Drone) {
                e.attackCooldown = 1.25f + rand01() * 0.4f;
                e.stateTimer = 0.25f;
                Projectile pr{};
                pr.fromPlayer = false;
                pr.x = e.x; pr.y = e.y + 0.7f; pr.z = e.z;
                float inv = 1.0f / std::max(0.2f, distP);
                pr.vx = dxP * inv * 13.0f;
                pr.vy = ((m_py + 0.95f) - pr.y) * inv * 13.0f;
                pr.vz = dzP * inv * 13.0f;
                pr.damage = static_cast<int>(13 * diffDmg);
                pr.life = 3.0f;
                pr.tile = TexTile::PlasmaOrb;
                m_projectiles.push_back(pr);
            } else if (e.enemyType == EnemyType::Brute) {
                e.attackCooldown = e.isBoss ? 1.15f : 1.75f;
                e.stateTimer = 0.30f;
                int fanCount = e.isBoss ? 5 : 2;
                float baseAng = std::atan2(dxP, dzP);
                for (int k = 0; k < fanCount; ++k) {
                    float off = (k - (fanCount - 1) * 0.5f) * 0.18f;
                    float ang = baseAng + off;
                    Projectile pr{};
                    pr.fromPlayer = false;
                    pr.explosive = !e.isBoss && (k == 0);
                    pr.x = e.x; pr.y = 1.25f; pr.z = e.z;
                    pr.vx = std::sin(ang) * 12.0f;
                    pr.vy = 0.0f;
                    pr.vz = std::cos(ang) * 12.0f;
                    pr.damage = static_cast<int>((e.isBoss ? 15 : 18) * diffDmg);
                    pr.life = 3.4f;
                    pr.tile = e.isBoss ? TexTile::PlasmaOrb : TexTile::RocketProj;
                    m_projectiles.push_back(pr);
                }
                if (m_audio) m_audio->play(SfxId::RocketLaunch, 0.65f, 0.85f);
            }
        }
        if (e.stateTimer > 0.0f) e.stateTimer -= dt;
    }

    // 7) Update 3D Projectiles
    for (auto& pr : m_projectiles) {
        if (!pr.active) continue;
        pr.life -= dt;
        if (pr.life <= 0.0f) { pr.active = false; continue; }

        pr.x += pr.vx * dt;
        pr.y += pr.vy * dt;
        pr.z += pr.vz * dt;

        int cx = static_cast<int>(std::floor(pr.x / kCell));
        int cz = static_cast<int>(std::floor(pr.z / kCell));
        if (pr.y <= 0.05f || pr.y >= m_ceilHeight - 0.05f || isCellSolidForRay(cx, cz)) {
            pr.active = false;
            if (pr.explosive) {
                detonateExplosion(pr.x - pr.vx * dt, std::clamp(pr.y, 0.3f, m_ceilHeight - 0.3f), pr.z - pr.vz * dt, 4.4f, pr.damage, pr.fromPlayer, ev);
            } else {
                spawnSparks(pr.x - pr.vx * dt, pr.y, pr.z - pr.vz * dt, 1.0f, 0.25f, 0.85f, 6);
            }
            continue;
        }

        if (pr.fromPlayer) {
            for (auto& e : m_entities) {
                if (!e.active || (e.kind != EntityKind::Enemy && e.kind != EntityKind::Barrel)) continue;
                if (std::hypot(e.x - pr.x, e.z - pr.z) < e.radius + 0.35f &&
                    pr.y >= e.y && pr.y <= e.y + e.height + 0.2f) {
                    pr.active = false;
                    if (pr.explosive) {
                        detonateExplosion(pr.x, pr.y, pr.z, 4.5f, pr.damage, true, ev);
                    } else if (e.kind == EntityKind::Enemy) {
                        damageEnemy(e, pr.damage, pr.vx * 0.25f, pr.vz * 0.25f, false, ev);
                    }
                    break;
                }
            }
        } else {
            if (std::hypot(m_px - pr.x, m_pz - pr.z) < kPlayerRadius + 0.22f &&
                pr.y >= m_py && pr.y <= m_py + 1.65f) {
                pr.active = false;
                int absA = std::min(m_armor, pr.damage / 2);
                m_armor -= absA;
                m_health -= (pr.damage - absA);
                m_hurtFlash = 0.55f;
                m_shakeTimer = 0.14f; m_shakeMag = 0.04f;
                ev.triggerHapticHeavy = true;
                if (m_audio) m_audio->play(SfxId::PlayerHurt, 0.95f, 1.0f);
            }
        }
    }

    // 8) Update 3D Particles
    for (auto& p : m_particles) {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        if (p.gravity) {
            p.vy -= 12.5f * dt;
            if (p.y < 0.08f) {
                p.y = 0.08f;
                p.vy = -p.vy * 0.35f;
                p.vx *= 0.65f;
                p.vz *= 0.65f;
            }
        } else if (p.tile >= TexTile::Explo0 && p.tile <= TexTile::Explo2) {
            float u = 1.0f - (p.life / p.maxLife);
            p.tile = TexTile::Explo0 + std::min(2, static_cast<int>(u * 3.0f));
        }
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                     [](const Particle3D& p) { return p.life <= 0.0f; }),
                      m_particles.end());
    m_projectiles.erase(std::remove_if(m_projectiles.begin(), m_projectiles.end(),
                                       [](const Projectile& p) { return !p.active; }),
                        m_projectiles.end());

    // 9) Check player death
    if (m_health <= 0) {
        m_health = 0;
        m_gameOver = true;
        saveHighScore();
        m_endSummary = QStringLiteral("FLATLINED ON %1\nSCORE: %2  |  KILLS: %3").arg(m_floorTitle).arg(m_score).arg(m_totalKills);
        if (m_audio) m_audio->play(SfxId::Explosion, 1.0f, 0.7f);
    }

    updateContextPrompt();
    return ev;
}

void GameSim::buildSnapshot(RenderSnapshot& out) const {
    out.mapW = kMapW;
    out.mapH = kMapH;
    if (out.mapRevision != m_mapRevision) {
        out.mapRevision = m_mapRevision;
        out.cells = m_cells;
    }
    out.doorOpen = m_doorOpen;
    out.floorTile = m_floorTile;
    out.ceilTile = m_ceilTile;
    out.ceilHeight = m_ceilHeight;

    float bobY = std::sin(m_walkBobPhase) * 0.055f;
    float shakeX = (m_shakeTimer > 0.0f) ? std::sin(m_shakeTimer * 95.0f) * m_shakeMag : 0.0f;
    float shakeY = (m_shakeTimer > 0.0f) ? std::cos(m_shakeTimer * 120.0f) * m_shakeMag : 0.0f;

    out.camX = m_px + shakeX;
    out.camY = m_py + kEyeHeight + bobY + shakeY;
    out.camZ = m_pz;
    out.camYaw = m_yaw;
    out.camPitch = m_pitch + m_recoil * 0.035f;
    out.camRoll = m_roll;
    out.fovDeg = 78.0f + (m_dashCooldown > 0.55f ? 8.0f : 0.0f);

    out.currentWeapon = m_weapon;
    out.weaponRecoil = m_recoil;
    out.weaponBobX = std::cos(m_walkBobPhase * 0.5f) * 0.028f;
    out.weaponBobY = std::abs(std::sin(m_walkBobPhase * 0.5f)) * 0.032f;
    out.muzzleTimer = m_muzzleTimer;
    out.bootTimer = m_bootTimer;
    out.hurtFlash = m_hurtFlash;
    out.pickupFlash = m_pickupFlash;
    out.overdriveGlow = (m_combo >= 5) ? 0.18f : 0.0f;

    out.billboards.clear();
    out.billboards.reserve(m_entities.size() + m_projectiles.size() + m_particles.size());

    for (size_t i = 0; i < m_entities.size(); ++i) {
        const auto& e = m_entities[i];
        if (!e.active) continue;
        RenderBillboard b{};
        b.x = e.x;
        b.z = e.z;
        if (e.kind == EntityKind::Enemy) {
            int baseTile = TexTile::PunkBase + static_cast<int>(e.enemyType) * 4;
            int frame = (e.painTimer > 0.0f) ? 3 :
                        (e.stateTimer > 0.0f) ? 2 :
                        (static_cast<int>(e.animPhase) & 1);
            b.tile = baseTile + frame;
            b.width = e.isBoss ? 2.5f : (e.enemyType == EnemyType::Brute ? 1.85f : 1.45f);
            b.height = e.height;
            b.y = e.y + b.height * 0.5f;
            b.lockedOn = std::find(m_lockedEnemyIndices.begin(), m_lockedEnemyIndices.end(), static_cast<int>(i)) != m_lockedEnemyIndices.end();
            if (b.lockedOn) { b.tintR = 0.5f; b.tintG = 1.35f; b.tintB = 1.45f; }
        } else if (e.kind == EntityKind::Barrel) {
            b.tile = TexTile::Barrel;
            b.width = 1.15f; b.height = 1.30f;
            b.y = 0.65f;
        } else {
            float hover = 0.08f * std::sin(e.animPhase * 0.6f);
            b.width = 0.95f; b.height = 0.95f;
            b.y = 0.52f + hover;
            b.fullbright = true;
            switch (e.kind) {
                case EntityKind::PickupMedkit:  b.tile = TexTile::Medkit; break;
                case EntityKind::PickupCola:    b.tile = TexTile::ColaCan; break;
                case EntityKind::PickupArmor:   b.tile = TexTile::ArmorShard; break;
                case EntityKind::PickupAmmo:    b.tile = TexTile::AmmoBox; break;
                case EntityKind::PickupKeycard: b.tile = TexTile::Keycard; break;
                case EntityKind::PickupWeaponShotgun:  b.tile = TexTile::WepShotgun; b.width = 1.25f; break;
                case EntityKind::PickupWeaponChaingun: b.tile = TexTile::WepChaingun; b.width = 1.25f; break;
                case EntityKind::PickupWeaponRocket:   b.tile = TexTile::WepRocket; b.width = 1.25f; break;
                default: b.tile = TexTile::Medkit; break;
            }
        }
        out.billboards.push_back(b);
    }

    for (const auto& pr : m_projectiles) {
        RenderBillboard b{};
        b.x = pr.x; b.y = pr.y; b.z = pr.z;
        b.width = pr.explosive ? 0.65f : 0.52f;
        b.height = b.width;
        b.tile = pr.tile;
        b.fullbright = true;
        out.billboards.push_back(b);
    }

    for (const auto& p : m_particles) {
        RenderBillboard b{};
        b.x = p.x; b.y = p.y; b.z = p.z;
        b.width = p.size; b.height = p.size;
        b.tile = (p.tile >= 0) ? p.tile : TexTile::PlasmaOrb;
        b.tintR = p.r; b.tintG = p.g; b.tintB = p.b;
        b.fullbright = (p.tile != TexTile::GibChunk);
        out.billboards.push_back(b);
    }

    // Gather closest 8 dynamic + static point lights for the GLSL shader!
    out.lights.clear();
    if (m_muzzleTimer > 0.0f) {
        out.lights.push_back({m_px, m_py + 1.1f, m_pz, 1.0f, 0.78f, 0.35f, 9.5f});
    }
    for (const auto& pr : m_projectiles) {
        if (out.lights.size() >= 8) break;
        if (std::hypot(pr.x - m_px, pr.z - m_pz) < 18.0f) {
            out.lights.push_back({
                pr.x, pr.y, pr.z,
                pr.explosive ? 1.0f : 0.25f,
                pr.explosive ? 0.55f : 0.9f,
                pr.explosive ? 0.15f : 1.0f,
                5.5f
            });
        }
    }
    // Sort static lights by proximity to player so nearby rooms are always vividly lit
    std::vector<PointLight3D> sortedStatic = m_staticLights;
    std::sort(sortedStatic.begin(), sortedStatic.end(), [&](const PointLight3D& a, const PointLight3D& b) {
        return std::hypot(a.x - m_px, a.z - m_pz) < std::hypot(b.x - m_px, b.z - m_pz);
    });
    for (const auto& sl : sortedStatic) {
        if (out.lights.size() >= 8) break;
        out.lights.push_back(sl);
    }
}
