#pragma once

#include <QAudioOutput>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <array>
#include <atomic>
#include <cstdint>

enum class SfxId : int {
    Revolver = 0,
    Shotgun = 1,
    Chaingun = 2,
    RocketLaunch = 3,
    Explosion = 4,
    BootKick = 5,
    EnemyHit = 6,
    EnemyGib = 7,
    PlayerHurt = 8,
    Pickup = 9,
    SodaCan = 10,
    DoorOpen = 11,
    LevelClear = 12,
    LockOn = 13
};

class SynthStreamDevice : public QIODevice {
    Q_OBJECT
public:
    explicit SynthStreamDevice(QObject* parent = nullptr);

    void triggerSfx(SfxId id, float volumeScale = 1.0f, float pitchScale = 1.0f);
    void setMusicEnabled(bool on) { m_musicEnabled.store(on); }
    void setSfxEnabled(bool on) { m_sfxEnabled.store(on); }
    void setPaused(bool paused) { m_paused.store(paused); }
    void setUrgency(bool urgent) { m_urgent.store(urgent); }
    void setMasterVolume(float v) { m_masterVol.store(v); }

protected:
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;
    qint64 bytesAvailable() const override;

private:
    struct Voice {
        bool active = false;
        SfxId type = SfxId::Revolver;
        float t = 0.0f;
        float dur = 0.2f;
        float phase = 0.0f;
        float phase2 = 0.0f;
        float vol = 1.0f;
        float pitch = 1.0f;
    };

    float nextNoise();
    float sampleVoice(Voice& v, float dt);
    float sampleMusic(float dt);

    QMutex m_mutex;
    std::array<Voice, 16> m_voices{};
    uint32_t m_rng = 0xCAFEBABEu;

    // Cyberpunk 138 BPM EBM synth state
    double m_musicBeatPos = 0.0;
    float m_bassPhase = 0.0f;
    float m_leadPhase = 0.0f;
    float m_kickPhase = 0.0f;

    std::atomic<bool> m_musicEnabled{true};
    std::atomic<bool> m_sfxEnabled{true};
    std::atomic<bool> m_paused{true};
    std::atomic<bool> m_urgent{false};
    std::atomic<float> m_masterVol{0.85f};
};

class SynthAudio : public QObject {
    Q_OBJECT
public:
    explicit SynthAudio(QObject* parent = nullptr);
    ~SynthAudio() override;

    void play(SfxId id, float vol = 1.0f, float pitch = 1.0f);
    void setMusicEnabled(bool on);
    void setSfxEnabled(bool on);
    void setPaused(bool paused);
    void setUrgency(bool urgent);
    void setMasterVolume(float v);

private:
    QAudioOutput* m_output = nullptr;
    SynthStreamDevice* m_device = nullptr;
};
