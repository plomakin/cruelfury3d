#include "SynthAudio.h"

#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr int kSampleRate = 44100;
constexpr float kTwoPi = 6.28318530718f;

inline float fastSaw(float phase) {
    float p = phase / kTwoPi;
    p -= std::floor(p);
    return 2.0f * p - 1.0f;
}

inline float fastSquare(float phase, float duty = 0.5f) {
    float p = phase / kTwoPi;
    p -= std::floor(p);
    return (p < duty) ? 1.0f : -1.0f;
}
} // namespace

SynthStreamDevice::SynthStreamDevice(QObject* parent) : QIODevice(parent) {
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

void SynthStreamDevice::triggerSfx(SfxId id, float volumeScale, float pitchScale) {
    if (!m_sfxEnabled.load()) return;
    QMutexLocker lock(&m_mutex);

    int slot = 0;
    float oldest = -1.0f;
    for (int i = 0; i < static_cast<int>(m_voices.size()); ++i) {
        if (!m_voices[i].active) {
            slot = i;
            oldest = 999.0f;
            break;
        }
        if (m_voices[i].t > oldest) {
            oldest = m_voices[i].t;
            slot = i;
        }
    }

    Voice& v = m_voices[slot];
    v.active = true;
    v.type = id;
    v.t = 0.0f;
    v.phase = 0.0f;
    v.phase2 = 0.0f;
    v.vol = std::clamp(volumeScale, 0.05f, 1.5f);
    v.pitch = std::clamp(pitchScale, 0.5f, 2.0f);

    switch (id) {
        case SfxId::Revolver:     v.dur = 0.22f; break;
        case SfxId::Shotgun:      v.dur = 0.46f; break;
        case SfxId::Chaingun:     v.dur = 0.11f; break;
        case SfxId::RocketLaunch: v.dur = 0.34f; break;
        case SfxId::Explosion:    v.dur = 0.68f; break;
        case SfxId::BootKick:     v.dur = 0.24f; break;
        case SfxId::EnemyHit:     v.dur = 0.14f; break;
        case SfxId::EnemyGib:     v.dur = 0.36f; break;
        case SfxId::PlayerHurt:   v.dur = 0.26f; break;
        case SfxId::Pickup:       v.dur = 0.24f; break;
        case SfxId::SodaCan:      v.dur = 0.32f; break;
        case SfxId::DoorOpen:     v.dur = 0.35f; break;
        case SfxId::LevelClear:   v.dur = 0.85f; break;
        case SfxId::LockOn:       v.dur = 0.10f; break;
    }
}

float SynthStreamDevice::nextNoise() {
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return (static_cast<float>(m_rng & 0xFFFFu) / 32768.0f) - 1.0f;
}

float SynthStreamDevice::sampleVoice(Voice& v, float dt) {
    v.t += dt;
    if (v.t >= v.dur) {
        v.active = false;
        return 0.0f;
    }
    const float u = v.t / v.dur;
    const float env = (1.0f - u) * (1.0f - u);
    const float nz = nextNoise();
    float out = 0.0f;

    switch (v.type) {
        case SfxId::Revolver: {
            // Sharp .44 crack + metallic cylinder ping
            float freq = (480.0f * std::exp(-16.0f * v.t) + 95.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            v.phase2 += kTwoPi * 1650.0f * v.pitch * dt;
            float crack = (v.t < 0.035f) ? nz * 1.1f : nz * 0.35f;
            out = (0.65f * std::sin(v.phase) + 0.55f * crack + 0.18f * std::sin(v.phase2) * std::exp(-22.0f * v.t)) * env * 1.1f;
            break;
        }
        case SfxId::Shotgun: {
            // Heavy boom + pump rack "chk-chk" at 0.24s and 0.33s
            float freq = (240.0f * std::exp(-12.0f * v.t) + 52.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            float blastEnv = std::exp(-8.5f * v.t);
            float blast = (0.75f * std::sin(v.phase) + 0.85f * nz) * blastEnv;
            float rack = 0.0f;
            if ((v.t > 0.24f && v.t < 0.285f) || (v.t > 0.32f && v.t < 0.37f)) {
                v.phase2 += kTwoPi * 1200.0f * dt;
                rack = (0.38f * nz + 0.2f * fastSquare(v.phase2)) * 0.7f;
            }
            out = blast + rack;
            break;
        }
        case SfxId::Chaingun: {
            float freq = (310.0f * std::exp(-22.0f * v.t) + 85.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = (0.55f * fastSquare(v.phase, 0.35f) + 0.65f * nz) * env * 0.85f;
            break;
        }
        case SfxId::RocketLaunch: {
            float freq = (190.0f + 420.0f * (1.0f - u)) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = (0.45f * fastSaw(v.phase) + 0.65f * nz) * (1.0f - u) * 0.8f;
            break;
        }
        case SfxId::Explosion: {
            float freq = 95.0f * std::exp(-4.5f * v.t) + 28.0f;
            v.phase += kTwoPi * freq * dt;
            float rumble = std::tanh(1.6f * std::sin(v.phase) + 1.1f * nz * (1.0f - 0.5f * u));
            out = rumble * std::pow(1.0f - u, 1.4f) * 1.15f;
            break;
        }
        case SfxId::BootKick: {
            // Sub-bass impact + leather/air whoosh
            float freq = (260.0f * std::exp(-18.0f * v.t) + 46.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = (0.85f * std::sin(v.phase) + 0.35f * nz * std::exp(-14.0f * v.t)) * env * 1.1f;
            break;
        }
        case SfxId::EnemyHit: {
            float freq = (320.0f - 180.0f * u) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = (0.5f * fastSaw(v.phase) + 0.4f * nz) * env * 0.7f;
            break;
        }
        case SfxId::EnemyGib: {
            float freq = (190.0f * std::exp(-7.0f * v.t) + 55.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            v.phase2 += kTwoPi * (540.0f + 190.0f * std::sin(v.t * 55.0f)) * dt;
            out = (0.55f * std::sin(v.phase) + 0.45f * nz + 0.25f * std::sin(v.phase2)) * (1.0f - u) * 0.9f;
            break;
        }
        case SfxId::PlayerHurt: {
            float freq = 185.0f - 95.0f * u;
            v.phase += kTwoPi * freq * dt;
            out = (0.65f * fastSaw(v.phase) + 0.4f * nz) * env * 0.95f;
            break;
        }
        case SfxId::Pickup: {
            float freq = ((u < 0.45f) ? 587.33f : 880.0f) * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = 0.45f * fastSquare(v.phase, 0.25f) * (1.0f - u);
            break;
        }
        case SfxId::SodaCan: {
            // Can pop-tab + fizzy heal arpeggio
            float freq = (u < 0.25f) ? 980.0f : (u < 0.55f ? 784.0f : 1174.6f);
            v.phase += kTwoPi * freq * dt;
            float fizz = (u < 0.35f) ? 0.45f * nz : 0.1f * nz;
            out = (0.38f * fastSquare(v.phase, 0.3f) + fizz) * (1.0f - u) * 0.85f;
            break;
        }
        case SfxId::DoorOpen: {
            float freq = 110.0f + 85.0f * u;
            v.phase += kTwoPi * freq * dt;
            out = (0.42f * fastSaw(v.phase) + 0.18f * nz) * (1.0f - 0.6f * u) * 0.65f;
            break;
        }
        case SfxId::LevelClear: {
            static const float notes[4] = {440.0f, 554.37f, 659.25f, 880.0f};
            int idx = std::min(3, static_cast<int>(u * 4.0f));
            v.phase += kTwoPi * notes[idx] * dt;
            out = 0.45f * fastSaw(v.phase) * (1.0f - 0.5f * u);
            break;
        }
        case SfxId::LockOn: {
            float freq = 1320.0f * v.pitch;
            v.phase += kTwoPi * freq * dt;
            out = 0.35f * fastSquare(v.phase, 0.5f) * (1.0f - u);
            break;
        }
    }
    return out * v.vol;
}

float SynthStreamDevice::sampleMusic(float dt) {
    // 140 BPM (162 BPM in CRUEL low-time urgency!) darksynth industrial loop
    const float bpm = m_urgent.load() ? 164.0f : 140.0f;
    const double stepRate = (bpm / 60.0) * 4.0; // 16th notes per second
    const double prevStep = std::floor(m_musicBeatPos);
    m_musicBeatPos += stepRate * dt;
    if (m_musicBeatPos >= 64.0) m_musicBeatPos -= 64.0;
    const int step = static_cast<int>(m_musicBeatPos) & 63;
    const float stepFrac = static_cast<float>(m_musicBeatPos - std::floor(m_musicBeatPos));
    if (std::floor(m_musicBeatPos) != prevStep && (step % 4 == 0)) {
        m_kickPhase = 0.0f;
    }

    // Driving E-minor / Phrygian cyberpunk bassline (MIDI notes relative to E1 = 41.20 Hz)
    static const int bassSemis[16] = {
        0, 0, 12, 0,  0, 0, 10, 0,
        3, 3, 15, 3,  1, 1, 13, 7
    };
    int barShift = (step >= 48) ? 3 : 0;
    int semi = bassSemis[step & 15] + barShift;
    float bassFreq = 41.203f * std::pow(2.0f, semi / 12.0f);
    m_bassPhase += kTwoPi * bassFreq * dt;
    if (m_bassPhase > kTwoPi) m_bassPhase -= kTwoPi;
    float bassEnv = std::max(0.0f, 1.0f - stepFrac * 1.15f);
    float bass = (0.6f * fastSaw(m_bassPhase) + 0.4f * fastSquare(m_bassPhase, 0.28f)) * bassEnv * 0.22f;

    // Four-on-the-floor industrial kick + off-beat snare/hat
    float drums = 0.0f;
    if ((step % 4) == 0) {
        float kt = stepFrac * (1.0f / static_cast<float>(stepRate));
        float kf = 145.0f * std::exp(-28.0f * kt) + 42.0f;
        m_kickPhase += kTwoPi * kf * dt;
        drums += std::sin(m_kickPhase) * std::exp(-12.0f * kt) * 0.34f;
    }
    if ((step % 8) == 4) {
        drums += nextNoise() * std::max(0.0f, 1.0f - stepFrac * 1.8f) * 0.16f;
    } else if ((step % 2) == 1) {
        drums += nextNoise() * std::max(0.0f, 1.0f - stepFrac * 4.2f) * 0.06f;
    }

    // Cyberpunk synth arpeggio hook on bars 2 & 4
    float lead = 0.0f;
    if (step >= 16) {
        static const int arpSemis[16] = {
            12, 15, 19, 24,  23, 19, 15, 12,
            13, 16, 20, 24,  19, 15, 13, 12
        };
        float leadFreq = 164.81f * std::pow(2.0f, arpSemis[step & 15] / 12.0f);
        m_leadPhase += kTwoPi * leadFreq * dt;
        if (m_leadPhase > kTwoPi) m_leadPhase -= kTwoPi;
        float leadEnv = std::max(0.0f, 1.0f - stepFrac * 1.35f);
        lead = fastSquare(m_leadPhase, 0.22f) * leadEnv * 0.095f;
    }

    return bass + drums + lead;
}

qint64 SynthStreamDevice::readData(char* data, qint64 maxlen) {
    const int samples = static_cast<int>(maxlen / sizeof(int16_t));
    int16_t* out = reinterpret_cast<int16_t*>(data);
    if (m_paused.load()) {
        std::memset(data, 0, static_cast<size_t>(samples) * sizeof(int16_t));
        return static_cast<qint64>(samples) * sizeof(int16_t);
    }

    const float dt = 1.0f / static_cast<float>(kSampleRate);
    const bool playMusic = m_musicEnabled.load();
    const float master = m_masterVol.load();

    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < samples; ++i) {
        float mix = playMusic ? sampleMusic(dt) : 0.0f;
        for (Voice& v : m_voices) {
            if (v.active) {
                mix += sampleVoice(v, dt);
            }
        }
        mix = std::tanh(mix * master);
        out[i] = static_cast<int16_t>(std::clamp(mix * 30000.0f, -32700.0f, 32700.0f));
    }
    return static_cast<qint64>(samples) * sizeof(int16_t);
}

qint64 SynthStreamDevice::writeData(const char*, qint64) { return 0; }

qint64 SynthStreamDevice::bytesAvailable() const {
    return 4096 + QIODevice::bytesAvailable();
}

SynthAudio::SynthAudio(QObject* parent) : QObject(parent) {
    QAudioFormat fmt;
    fmt.setSampleRate(kSampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleSize(16);
    fmt.setCodec("audio/pcm");
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setSampleType(QAudioFormat::SignedInt);

    m_device = new SynthStreamDevice(this);
    QAudioDeviceInfo chosen;
    const auto devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (const auto& dev : devices) {
        if (dev.deviceName().contains(QStringLiteral("pulse"), Qt::CaseInsensitive) ||
            dev.deviceName() == QStringLiteral("default")) {
            if (dev.isFormatSupported(fmt)) {
                chosen = dev;
                break;
            }
        }
    }
    if (chosen.isNull()) {
        QAudioDeviceInfo def = QAudioDeviceInfo::defaultOutputDevice();
        if (!def.isNull() && def.isFormatSupported(fmt)) {
            chosen = def;
        }
    }
    if (!chosen.isNull()) {
        m_output = new QAudioOutput(chosen, fmt, this);
        m_output->setBufferSize(4096); // ~46 ms low-latency ring buffer
        m_output->start(m_device);
    }
}

SynthAudio::~SynthAudio() {
    if (m_output) {
        m_output->stop();
    }
}

void SynthAudio::play(SfxId id, float vol, float pitch) {
    if (m_device) m_device->triggerSfx(id, vol, pitch);
}
void SynthAudio::setMusicEnabled(bool on) {
    if (m_device) m_device->setMusicEnabled(on);
}
void SynthAudio::setSfxEnabled(bool on) {
    if (m_device) m_device->setSfxEnabled(on);
}
void SynthAudio::setPaused(bool paused) {
    if (m_device) m_device->setPaused(paused);
}
void SynthAudio::setUrgency(bool urgent) {
    if (m_device) m_device->setUrgency(urgent);
}
void SynthAudio::setMasterVolume(float v) {
    if (m_device) m_device->setMasterVolume(v);
}
