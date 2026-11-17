# Cruel Fury 3D (`cruelfury.plm`)

<p align="center">
  <img src="click/assets/icon.svg" width="144" alt="Cruel Fury 3D Icon"/>
</p>

**Neon cyberpunk 3D boomer-shooter built natively for Ubuntu Touch 24.04 (`OpenGL ES 2.0` + `Qt5` + `Lomiri.Components` + `Haptics`).**

Inspired by *Duke Nukem 3D*, *CRUEL*, and *Ion Fury*, **Cruel Fury 3D** puts you in the armored combat boots of **Jade "Nuke" Vance** storming up the neon-soaked corridors of **Heskel Plaza** with dual-thumb mobile FPS controls, a dedicated **Mighty Boot** kick, procedural pixel-art textures, dynamic point lighting, and a real-time C++ PCM darksynth soundtrack.

---

## Features

- **3 Game Modes:**
  - **CRUEL CLOCK:** High-adrenaline time-attack — kills, smashing Neon Soda Machines, barrel chain-reactions, and secrets add seconds to your ticking clock while building up to `x10` Overdrive combos.
  - **DUKE CLASSIC:** Untimed 4-floor campaign (`Neon Arcade` → `Cryo-Reactor` → `Synth-Club` → `Helipad Boss: Warlord Heskel-X`).
  - **ENDLESS WAVE:** Escalating procedural neon arenas.
- **Arsenal + Mighty Boot:**
  - **`1` Loverboy .44 Triple-Revolver** — `2x` Headshot crits + **`FAN`** 6-shot lock-on burst
  - **`2` Disperser Riot Shotgun** — 8-pellet spread & heavy knockback
  - **`3` Ripper Triple-Chaingun** — high-RPM crowd shredder
  - **`4` Devastator Micro-RPG** — explosive splash rockets
  - **`BOOT` (Mighty Cyber-Boot)** — kick explosive hazmat barrels into enemies like bowling bombs, smash **Neon Soda Machines** (`+20 HP` + `+4s` time), and break **Cracked Secret Walls**!
- **First-Class Ubuntu Touch / Lomiri Integration:**
  - Simultaneous 4-finger `MultiPointTouchArea`: floating left thumbstick, precision right-half freelook, **aim-while-dragging `FIRE`**, and quick weapon strip
  - Instant **`90°` Widescreen Landscape** toggle (works even when Ubuntu Touch portrait rotation-lock is on)
  - Optional **Gyroscope Aim Assist** (`QtSensors`) & **Lomiri Haptic Rumble** (`Haptics`)
  - Zero external asset bloat — 100% procedural 512×512 pixel-art texture atlas + real-time 44.1 kHz 16-bit PCM synth engine (`~150 KB` `.click` package!)

---

## Project Structure

```text
cruelfury3d/
├── CMakeLists.txt
├── clickable.yaml
├── LICENSE
├── src/
│   ├── main.cpp           # Qt5 / QML entry point & Click path resolver
│   ├── GameItem.h/.cpp    # QQuickFramebufferObject OpenGL ES 2.0 3D renderer + QML bridge
│   ├── GameSim.h/.cpp     # 3D physics, AI, 4 floors + boss, weapons, barrels & CRUEL clock
│   ├── TextureAtlas.h/.cpp# Procedural 512x512 crisp pixel-art texture & sprite generator
│   └── SynthAudio.h/.cpp  # Real-time 44.1 kHz PCM darksynth music & 14 retro SFX voices
├── click/
│   ├── manifest.json      # cruelfury.plm (ubuntu-touch-24.04-1.x)
│   ├── cruelfury.apparmor # Confined AppArmor profile (audio, keep-display-on, sensors)
│   ├── cruelfury.desktop  # Lomiri launcher metadata
│   ├── assets/            # Vector icon.svg & 512x512 icon.png
│   └── qml/Main.qml       # Lomiri UI, HUD, MultiPointTouchArea & Settings overlay
└── scripts/
    ├── build_and_package.sh
    └── publish_openstore.sh
```

---

## Building

### Option A: Native On-Device Build (Ubuntu Touch 24.04 LXC / `devbox`)
Built and tested directly on an Ubuntu Touch 24.04 ARM64 phone (`Moto G 5G 2025`):

```bash
# Install build dependencies inside an Ubuntu 24.04 arm64 container:
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev libqt5svg5-dev libgles2-mesa-dev

# Compile & stage into click/:
./scripts/build_and_package.sh

# Build .click package on Ubuntu Touch host:
mkdir -p release && cd release
click build --no-validate ../click

# Install locally into Lomiri App Drawer:
gdbus call --system --dest com.lomiri.click --object-path /com/lomiri/click \
  --method com.lomiri.click.Install "$PWD/cruelfury.plm_1.0.0_arm64.click"
```

### Option B: Desktop Cross-Build with Clickable
```bash
clickable build --arch arm64
```

---

## License

Released under the [MIT License](LICENSE).
