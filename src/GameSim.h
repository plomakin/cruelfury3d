#pragma once

#include "SynthAudio.h"
#include <QString>
#include <array>
#include <cstdint>
#include <vector>

enum class GameMode : int {
    CruelTimed = 0,   // CRUEL-style ticking adrenaline clock (+time on kills/soda/secrets)
    DukeClassic = 1,  // Classic untimed Duke/Ion Fury exploration & action
    EndlessArena = 2  // Endless procedural neon kill-floors
};

enum class EnemyType : int {
    Punk = 0,
    Sawbot = 1,
    Drone = 2,
    Brute = 3
};

enum class EntityKind : int {
    Enemy = 0,
    Barrel = 1,
    PickupMedkit = 2,
    PickupCola = 3,
    PickupArmor = 4,
    PickupAmmo = 5,
    PickupKeycard = 6,
    PickupWeaponShotgun = 7,
    PickupWeaponChaingun = 8,
    PickupWeaponRocket = 9
};

struct WorldEntity {
    bool active = true;
    EntityKind kind = EntityKind::Enemy;
    EnemyType enemyType = EnemyType::Punk;
    bool isBoss = false;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vz = 0.0f;
    float radius = 0.45f;
    float height = 1.75f;

    int hp = 40;
    int maxHp = 40;
    float stateTimer = 0.0f;
    float attackCooldown = 1.0f;
    float painTimer = 0.0f;
    float animPhase = 0.0f;
    bool alerted = false;
    bool kickedFlying = false;
};

struct Projectile {
    bool active = true;
    bool fromPlayer = false;
    bool explosive = false;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    int damage = 20;
    float life = 3.0f;
    int tile = 38;
};

struct Particle3D {
    float x, y, z;
    float vx, vy, vz;
    float r, g, b;
    float size;
    float life;
    float maxLife;
    int tile = -1; // -1 = colored spark quad, >=0 = textured sprite (Explo / Gib)
    bool gravity = true;
};

struct PointLight3D {
    float x, y, z;
    float r, g, b;
    float radius;
};

struct RenderBillboard {
    float x, y, z;
    float width, height;
    int tile;
    float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
    bool fullbright = false;
    bool lockedOn = false;
};

struct RenderSnapshot {
    int mapW = 24;
    int mapH = 24;
    uint32_t mapRevision = 0;
    std::vector<uint8_t> cells;     // 0 = empty, >0 = wall tile + 1, special doors/switches
    std::vector<float> doorOpen;    // 0.0 closed .. 1.0 open per cell
    int floorTile = 8;
    int ceilTile = 9;
    float ceilHeight = 3.1f;

    float camX = 3.0f, camY = 1.05f, camZ = 3.0f;
    float camYaw = 0.0f, camPitch = 0.0f, camRoll = 0.0f;
    float fovDeg = 78.0f;

    int currentWeapon = 0;
    float weaponRecoil = 0.0f;
    float weaponBobX = 0.0f, weaponBobY = 0.0f;
    float muzzleTimer = 0.0f;
    float bootTimer = 0.0f;

    float hurtFlash = 0.0f;
    float pickupFlash = 0.0f;
    float overdriveGlow = 0.0f;

    std::vector<RenderBillboard> billboards;
    std::vector<PointLight3D> lights;
};

struct SimEvents {
    bool triggerHapticLight = false;
    bool triggerHapticHeavy = false;
};

class GameSim {
public:
    static constexpr int kMapW = 24;
    static constexpr int kMapH = 24;
    static constexpr float kCell = 2.0f; // 2 meters per grid cell

    explicit GameSim(SynthAudio* audio);

    void startNewGame(GameMode mode, int difficulty);
    void loadFloor(int floorNumber);
    SimEvents update(float dt, float moveForward, float moveStrafe, bool fireHeld, bool autoFire);

    void addLookDelta(float dYawRad, float dPitchRad);
    void triggerAltFire();
    void triggerBootKick();
    void triggerJumpOrDash(float moveForward, float moveStrafe);
    void triggerInteract();
    void selectWeapon(int idx);
    void cycleWeapon(int dir);

    void buildSnapshot(RenderSnapshot& out) const;

    // HUD getters
    int health() const { return m_health; }
    int armor() const { return m_armor; }
    int score() const { return m_score; }
    int highScore() const { return m_highScore; }
    int combo() const { return m_combo; }
    float comboProgress() const { return m_comboTimer / 4.0f; }
    float timeLeft() const { return m_timeLeft; }
    bool isTimedMode() const { return m_mode == GameMode::CruelTimed; }
    int floorNum() const { return m_floor; }
    QString floorTitle() const { return m_floorTitle; }
    int currentWeapon() const { return m_weapon; }
    int currentAmmo() const;
    bool weaponOwned(int idx) const { return idx >= 0 && idx < 4 ? m_weaponOwned[idx] : false; }
    bool hasKeycard() const { return m_hasKeycard; }
    QString calloutText() const { return m_calloutTimer > 0.0f ? m_callout : QString(); }
    QString contextPrompt() const { return m_contextPrompt; }
    bool isGameOver() const { return m_gameOver; }
    bool isVictory() const { return m_victory; }
    QString endSummary() const { return m_endSummary; }
    int enemiesAlive() const;
    float bossHealthRatio() const;
    QString bossName() const { return m_bossActive ? QStringLiteral("WARLORD HESKEL-X") : QString(); }
    bool crosshairOnTarget() const { return m_crosshairTarget; }

    void saveHighScore();
    void loadHighScore();

private:
    // Cell values:
    // 0 = empty floor
    // 1..16 = solid wall using tile (cell - 1)
    // 20 = normal sliding door
    // 21 = keycard locked door
    // 22 = cracked secret wall
    // 23 = soda machine intact (wall block)
    // 24 = soda machine smashed (wall block)
    // 25 = exit elevator switch
    // 26 = destructible crate block
    bool isCellSolidForMove(int cx, int cz) const;
    bool isCellSolidForRay(int cx, int cz) const;
    void moveBodyWithSlide(float& x, float& z, float dx, float dz, float radius) const;
    bool hasLineOfSight(float x0, float z0, float x1, float z1) const;
    float castWallRay(float ox, float oy, float oz, float dx, float dy, float dz, int* hitCx = nullptr, int* hitCz = nullptr) const;

    void fireCurrentWeapon(SimEvents& ev);
    void fireHitscanPellet(float dirX, float dirY, float dirZ, int dmg, float knockback, bool headshotCapable, SimEvents& ev);
    void detonateExplosion(float ex, float ey, float ez, float radius, int maxDmg, bool playerCredit, SimEvents& ev);
    void damageEnemy(WorldEntity& e, int dmg, float kx, float kz, bool headshot, SimEvents& ev);
    void addKillReward(int baseScore, float bonusSeconds, const QString& customCallout);
    void pushCallout(const QString& text, float duration = 1.6f);
    void spawnGibs(float x, float y, float z, int count);
    void spawnSparks(float x, float y, float z, float r, float g, float b, int count);
    void updateContextPrompt();

    SynthAudio* m_audio = nullptr;
    GameMode m_mode = GameMode::CruelTimed;
    int m_difficulty = 1; // 0=Easy, 1=Normal, 2=Hard

    int m_floor = 1;
    QString m_floorTitle;
    std::vector<uint8_t> m_cells;
    std::vector<float> m_doorOpen;
    std::vector<bool> m_doorOpening;
    std::vector<PointLight3D> m_staticLights;
    uint32_t m_mapRevision = 1;
    int m_floorTile = 8;
    int m_ceilTile = 9;
    float m_ceilHeight = 3.1f;

    // Player state
    float m_px = 3.0f, m_py = 0.0f, m_pz = 3.0f;
    float m_pvx = 0.0f, m_pvy = 0.0f, m_pvz = 0.0f;
    float m_yaw = 0.0f, m_pitch = 0.0f, m_roll = 0.0f;
    bool m_onGround = true;
    float m_dashCooldown = 0.0f;
    float m_walkBobPhase = 0.0f;
    float m_shakeTimer = 0.0f;
    float m_shakeMag = 0.0f;

    int m_health = 100;
    int m_armor = 25;
    bool m_hasKeycard = false;
    int m_score = 0;
    int m_highScore = 0;
    int m_combo = 1;
    float m_comboTimer = 0.0f;
    float m_timeLeft = 30.0f;
    int m_totalKills = 0;

    // Arsenal: 0=Revolver, 1=Shotgun, 2=Chaingun, 3=Rocket
    int m_weapon = 0;
    std::array<bool, 4> m_weaponOwned{{true, true, false, false}};
    int m_shells = 16;
    int m_bullets = 90;
    int m_rockets = 6;
    float m_fireCooldown = 0.0f;
    float m_recoil = 0.0f;
    float m_muzzleTimer = 0.0f;
    float m_bootTimer = 0.0f;
    float m_bootCooldown = 0.0f;

    // Fan-the-Hammer lock-on burst queue
    int m_fanBurstShotsLeft = 0;
    float m_fanBurstTimer = 0.0f;
    std::vector<int> m_lockedEnemyIndices;

    float m_hurtFlash = 0.0f;
    float m_pickupFlash = 0.0f;
    QString m_callout;
    float m_calloutTimer = 0.0f;
    QString m_contextPrompt;
    bool m_crosshairTarget = false;
    bool m_bossActive = false;

    bool m_gameOver = false;
    bool m_victory = false;
    QString m_endSummary;

    std::vector<WorldEntity> m_entities;
    std::vector<Projectile> m_projectiles;
    std::vector<Particle3D> m_particles;
    uint32_t m_rng = 0x1337BEEFu;

    float rand01();
    float randSigned();
};
