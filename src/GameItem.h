#pragma once

#include "GameSim.h"
#include "SynthAudio.h"

#include <QElapsedTimer>
#include <QQuickFramebufferObject>
#include <QTimer>

class GameItem : public QQuickFramebufferObject {
    Q_OBJECT

    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int gameMode READ gameMode WRITE setGameMode NOTIFY gameModeChanged)
    Q_PROPERTY(int difficulty READ difficulty WRITE setDifficulty NOTIFY difficultyChanged)
    Q_PROPERTY(float lookSensitivity READ lookSensitivity WRITE setLookSensitivity NOTIFY settingsChanged)
    Q_PROPERTY(bool gyroEnabled READ gyroEnabled WRITE setGyroEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool hapticsEnabled READ hapticsEnabled WRITE setHapticsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool musicEnabled READ musicEnabled WRITE setMusicEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool sfxEnabled READ sfxEnabled WRITE setSfxEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool autoFire READ autoFire WRITE setAutoFire NOTIFY settingsChanged)

    // Live HUD properties
    Q_PROPERTY(int health READ health NOTIFY hudChanged)
    Q_PROPERTY(int armor READ armor NOTIFY hudChanged)
    Q_PROPERTY(int score READ score NOTIFY hudChanged)
    Q_PROPERTY(int highScore READ highScore NOTIFY hudChanged)
    Q_PROPERTY(int combo READ combo NOTIFY hudChanged)
    Q_PROPERTY(float comboProgress READ comboProgress NOTIFY hudChanged)
    Q_PROPERTY(float timeLeft READ timeLeft NOTIFY hudChanged)
    Q_PROPERTY(bool isTimedMode READ isTimedMode NOTIFY hudChanged)
    Q_PROPERTY(int floorNum READ floorNum NOTIFY hudChanged)
    Q_PROPERTY(QString floorTitle READ floorTitle NOTIFY hudChanged)
    Q_PROPERTY(int currentWeapon READ currentWeapon NOTIFY hudChanged)
    Q_PROPERTY(int currentAmmo READ currentAmmo NOTIFY hudChanged)
    Q_PROPERTY(bool ownShotgun READ ownShotgun NOTIFY hudChanged)
    Q_PROPERTY(bool ownChaingun READ ownChaingun NOTIFY hudChanged)
    Q_PROPERTY(bool ownRocket READ ownRocket NOTIFY hudChanged)
    Q_PROPERTY(bool hasKeycard READ hasKeycard NOTIFY hudChanged)
    Q_PROPERTY(QString calloutText READ calloutText NOTIFY hudChanged)
    Q_PROPERTY(QString contextPrompt READ contextPrompt NOTIFY hudChanged)
    Q_PROPERTY(bool isGameOver READ isGameOver NOTIFY hudChanged)
    Q_PROPERTY(bool isVictory READ isVictory NOTIFY hudChanged)
    Q_PROPERTY(QString endSummary READ endSummary NOTIFY hudChanged)
    Q_PROPERTY(int enemiesAlive READ enemiesAlive NOTIFY hudChanged)
    Q_PROPERTY(float bossHealthRatio READ bossHealthRatio NOTIFY hudChanged)
    Q_PROPERTY(QString bossName READ bossName NOTIFY hudChanged)
    Q_PROPERTY(bool crosshairOnTarget READ crosshairOnTarget NOTIFY hudChanged)

public:
    explicit GameItem(QQuickItem* parent = nullptr);
    Renderer* createRenderer() const override;

    // Input slots callable from QML MultiPointTouchArea, Gyroscope & Keyboard
    Q_INVOKABLE void startGame(int mode, int diff);
    Q_INVOKABLE void restartGame();
    Q_INVOKABLE void setMoveInput(float forward, float strafe);
    Q_INVOKABLE void addTouchLook(float dxPixels, float dyPixels);
    Q_INVOKABLE void addGyroRates(float rxDegPerSec, float ryDegPerSec, float dtSec);
    Q_INVOKABLE void setFireHeld(bool held);
    Q_INVOKABLE void altFire();
    Q_INVOKABLE void bootKick();
    Q_INVOKABLE void jumpOrDash();
    Q_INVOKABLE void interact();
    Q_INVOKABLE void selectWeapon(int index);
    Q_INVOKABLE void cycleWeapon(int dir);

    void syncToSnapshot(RenderSnapshot& dst) const;

    // Property getters & setters
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    int gameMode() const { return m_gameMode; }
    void setGameMode(int m);
    int difficulty() const { return m_difficulty; }
    void setDifficulty(int d);

    float lookSensitivity() const { return m_lookSensitivity; }
    void setLookSensitivity(float s);
    bool gyroEnabled() const { return m_gyroEnabled; }
    void setGyroEnabled(bool on);
    bool hapticsEnabled() const { return m_hapticsEnabled; }
    void setHapticsEnabled(bool on);
    bool musicEnabled() const { return m_musicEnabled; }
    void setMusicEnabled(bool on);
    bool sfxEnabled() const { return m_sfxEnabled; }
    void setSfxEnabled(bool on);
    bool autoFire() const { return m_autoFire; }
    void setAutoFire(bool on);

    int health() const { return m_sim.health(); }
    int armor() const { return m_sim.armor(); }
    int score() const { return m_sim.score(); }
    int highScore() const { return m_sim.highScore(); }
    int combo() const { return m_sim.combo(); }
    float comboProgress() const { return m_sim.comboProgress(); }
    float timeLeft() const { return m_sim.timeLeft(); }
    bool isTimedMode() const { return m_sim.isTimedMode(); }
    int floorNum() const { return m_sim.floorNum(); }
    QString floorTitle() const { return m_sim.floorTitle(); }
    int currentWeapon() const { return m_sim.currentWeapon(); }
    int currentAmmo() const { return m_sim.currentAmmo(); }
    bool ownShotgun() const { return m_sim.weaponOwned(1); }
    bool ownChaingun() const { return m_sim.weaponOwned(2); }
    bool ownRocket() const { return m_sim.weaponOwned(3); }
    bool hasKeycard() const { return m_sim.hasKeycard(); }
    QString calloutText() const { return m_sim.calloutText(); }
    QString contextPrompt() const { return m_sim.contextPrompt(); }
    bool isGameOver() const { return m_sim.isGameOver(); }
    bool isVictory() const { return m_sim.isVictory(); }
    QString endSummary() const { return m_sim.endSummary(); }
    int enemiesAlive() const { return m_sim.enemiesAlive(); }
    float bossHealthRatio() const { return m_sim.bossHealthRatio(); }
    QString bossName() const { return m_sim.bossName(); }
    bool crosshairOnTarget() const { return m_sim.crosshairOnTarget(); }

signals:
    void pausedChanged();
    void gameModeChanged();
    void difficultyChanged();
    void settingsChanged();
    void hudChanged();
    void hapticPulse(bool heavy);

private slots:
    void onTick();

private:
    SynthAudio m_audio;
    GameSim m_sim;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;

    bool m_paused = true;
    int m_gameMode = 0;
    int m_difficulty = 1;
    float m_lookSensitivity = 1.0f;
    bool m_gyroEnabled = false;
    bool m_hapticsEnabled = true;
    bool m_musicEnabled = true;
    bool m_sfxEnabled = true;
    bool m_autoFire = false;

    float m_moveForward = 0.0f;
    float m_moveStrafe = 0.0f;
    bool m_fireHeld = false;
};
