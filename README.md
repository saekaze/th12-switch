# Touhou 12: Undefined Fantastic Object — Nintendo Switch Port
*(東方星蓮船　～ Undefined Fantastic Object.)*

![Platform](https://img.shields.io/badge/Platform-Nintendo%20Switch-e60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)
![Status](https://img.shields.io/badge/Status-Fully%20Playable-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/Port%20Code-CC0%201.0-blue?style=for-the-badge)

A native homebrew port of ZUN's 2009 bullet hell **Touhou 12: Undefined Fantastic Object** for the **Nintendo Switch** (Horizon OS).

This build runs the C++20 reimplementation of TH12 1.00b by [Tweet-META](https://github.com/Tweet-META/th12) (also published at [Ritosa2333/th12](https://github.com/Ritosa2333/th12) and in [YomotsuHisami/eagler-touhou](https://github.com/YomotsuHisami/eagler-touhou)), compiled **natively for ARM64** (no wasm, no emulation). Its browser front-end is rewritten in C++ for SDL2 on an OpenGL ES 3 context — no Linux, Box64 or Wine involved.

Companion to my other Touhou Switch ports — all of them are listed on the [Touhou Project on Nintendo Switch hub](https://github.com/saekaze/touhou-switch) — with the same `touhou12.nro` + `sd:/switch/touhou/touhou12/` layout.

---

## 🆕 First release (1.00b-r1)

* 🛸 The full game: Reimu, Marisa and Sanae with both shot types, every difficulty, all 6 stages and Extra, the UFO system, Practice, the twelve endings and the staff roll.
* 🎬 Replays, Score Ranking with name entry, Player Data, Music Room and the title demo — the original menus, drawn from your own `th12.dat`.
* 💾 Saves and replays in the original formats: copy `scoreth12.dat`, `th12.cfg` and `replay/` between the PC and the Switch in either direction.
* 📁 Recommended folder: `sd:/switch/touhou/touhou12/`, next to the other Touhou ports.

---

## ✨ Key Features

* Reads your original `th12.exe`, `th12.dat` and `thbgm.dat` in place; nothing is unpacked or converted.
* Saves and replays in the PC formats (`scoreth12.dat`, `th12.cfg`, `replay/`), written next to the game data.
* The 4:3 picture is centred with black bars.
* Japanese text from `msgothic.ttc`, or the Switch's own Japanese font.

---

## 📥 Installation Guide

> ⚠️ **Disclaimer:** In compliance with ZUN's guidelines and copyright law, this repository contains **ONLY the homebrew engine code**. No game assets are distributed. You must legally own a copy of *Touhou 12: Undefined Fantastic Object* **ver 1.00b** (Japanese).

### 1. SD Card File Structure

1. Ensure your Nintendo Switch is running custom firmware (Atmosphère CFW).
2. Download the latest `touhou12.nro` from the [Releases](../../releases) tab (or build from source).
3. Create the folder `sd:/switch/touhou/touhou12/` and copy the following into it:

```text
sd:/switch/touhou/touhou12/
    ├── touhou12.nro          # Nintendo Switch homebrew executable
    ├── th12.exe              # ver 1.00b executable (sound and bullet tables are read from it)
    ├── th12.dat              # Main game archive
    ├── thbgm.dat             # Background music archive
    ├── msgothic.ttc          # MS Gothic (C:\Windows\Fonts) - recommended
    └── msmincho.ttc          # MS Mincho (C:\Windows\Fonts) - optional
```

`thbgm.dat` is optional (the game runs silent without it). Without `msgothic.ttc` the console's built-in Japanese font is used. `th12.exe` must be the original 1.00b executable (not a patcher's launcher): the port reads fixed tables out of it.

Already playing on PC? Copy `scoreth12.dat` (unlocks, rankings, spell card history), `th12.cfg` and the `replay/` folder into the same folder — and back again later.

**Recommended place: `sd:/switch/touhou/touhou12/`.** Keeping every Touhou port in one `sd:/switch/touhou/` folder (`touhou6`, `touhou7`, `touhou8` …) is much tidier than a separate folder per game. Other places still work: the port first looks in its own folder (wherever the NRO is), then for a `th12` / `touhou12` folder (any capitalisation) directly on the SD card, in `switch/`, `touhou/`, `switch/touhou/`, `games/` or `roms/`.

### 2. Launching

Run `touhou12.nro` from the **Homebrew Menu (hbmenu)**, **Sphaira launcher**, or a home screen forwarder. Title-menu **Quit** returns to hbmenu. If something is missing, the port shows what and where it looked, and `th12-switch.log` is written next to the data.

---

## 🕹 Controls

| Nintendo Switch Button | Action |
| :--- | :--- |
| **Left Stick / D-Pad** | Character Movement |
| **B** | Shoot / Confirm |
| **A** | Bomb / Cancel |
| **L / ZL** | Focus (Precision Slow-Motion Movement) |
| **R / ZR** | Skip Dialogue (hold, Ctrl) |
| **+ (Plus)** | Pause |
| **X** | Enter (while not bound in Key Config) |
| **Y** | R key (Retry shortcut in the pause menu, while not bound in Key Config) |
| **R/ZR** or **B** in a replay | Fast-forward ×4 |
| **L/ZL** in a replay | Slow motion ×¼ |
| **− (Minus)** | Snapshot (saved to `snapshot/` next to the game data) |

**The same default layout in every Touhou Switch port:** B shoots, A bombs, L/ZL focuses, R/ZR skips dialogue, + pauses. These are only defaults — the Switch buttons act as the game's own gamepad, so the in-game **Option → Key Config** can rebind shot, bomb, focus, pause and skip (pick a row, let go, press the new button; **Quit** saves, **Default** restores this layout). The D-Pad and sticks only move. Key Config numbers: 0 B, 1 A, 2 L/ZL, 3 R/ZR, 4 +, 5 X, 6 Y. All connected controllers (handheld, grip, Pro Controller) work together.

---

## 🛠 Building from Source

### Automated Build (GitHub Actions)

`.github/workflows/build-switch.yml` compiles `touhou12.nro` inside the official `devkitpro/devkita64` container and uploads it as an artifact; tagging `v*` (or running the workflow with a `release_tag`) also publishes it as a release asset. A second job builds the same sources for Linux and runs the host tests.

### Local Build (Linux / macOS / WSL)

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `devkitA64` and `libnx`.
2. Install the required Switch portlibs:

   ```bash
   sudo dkp-pacman -Syu
   sudo dkp-pacman -S switch-dev switch-mesa switch-libdrm_nouveau switch-sdl2 switch-freetype switch-libpng switch-bzip2 switch-zlib switch-harfbuzz
   ```

3. Build:

   ```bash
   export DEVKITPRO=/opt/devkitpro
   ./scripts/build_switch.sh
   ```

   The result is `build-switch/touhou12.nro`.

Desktop Linux test build and host tests (same sources; needs `libsdl2-dev libfreetype-dev libgles-dev`, and `xvfb` + a Japanese font such as `fonts-ipafont-gothic` for the full test set):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
xvfb-run -a ./build/th12_host_tests
./build/touhou12 /path/to/folder/with/th12.dat   # arrows, Z shot, X bomb, Shift focus, Ctrl skip, Esc pause, Enter, R retry
```

---

## 📂 How It Works

`core/` is upstream's game: ECL/stage/message interpreters, the logical ANM VMs, bullets, lasers, items, the UFO system, spell cards, the full-game stage flow and replay playback (C++20, architecture independent). It is compiled natively with the code generation that matters for determinism (`-ffp-contract=off`, `-fsigned-char`), so replays recorded on PC play back exactly. Upstream drove it from JavaScript in a browser; `src/` is a C++ rewrite of that front-end, reading the core's state directly — see [`upstream/UPSTREAM.md`](upstream/UPSTREAM.md).

| File | Purpose |
| :--- | :--- |
| `core/` | upstream TH12 core (`src/game`, the C ABI `src/wasm/Exports.cpp`, compatibility headers) |
| `src/Archive.*` | `th12.dat` (THA1) read in place, with TH12's cipher table |
| `src/ExeTables.*` | the sound table, bullet visual types, camera planes and Music Room warning from `th12.exe` |
| `src/Anm.*` | ANM v7 entries, sprites, scripts and THTX textures |
| `src/Animation.*` | the presentation-side ANM sampler (`animation.mjs`): bullets, items, HUD, menus |
| `src/Scene.*` | game picture and text overlays (`renderer.mjs`, `stage-background.mjs`, `draw-scheduler.mjs`, …) |
| `src/Renderer.*` | the GLES 3 batch renderer: native blend modes, clips, the stage-surface copy, pillarboxed output |
| `src/TextRaster.*` | dialogue, spell card and menu text with FreeType |
| `src/TitleScenes.*` | the menus' input, timing and scene changes (`title-scenes.mjs`) |
| `src/TitlePresentation.*` | the menus' `title.anm` / `front.anm` / `text.anm` scripts, replay and ranking lists, name grid, Player Data (`title-presentation.mjs`) |
| `src/Profile.*` | `scoreth12.dat` / `th12.cfg` in the original formats (`native-profile.mjs`) |
| `src/Replay.*` | `.rpy` reading, recording and writing (`replay.mjs`, `replay-recorder.mjs`) |
| `src/Ending.*` | the endings and staff roll (`ending-timeline.mjs`) |
| `src/Audio.*` / `src/BgmStream.*` | miniaudio mixer; BGM streamed from `thbgm.dat` with the original loop points, read ahead on its own thread |
| `src/Game.*` | start-up, resource loading, the stage flow, replays, endings and saves (`site/game.mjs`, `stage-loader.mjs`) |
| `src/Platform.*` | SD paths, files, log |
| `src/Memory.cpp` | small-block pool behind `new` / `delete` (newlib's `malloc` is slow for the thousands of small allocations a busy frame makes) |
| `src/main.cpp` | Switch main loop and controls |
| `platform/switch/icon.jpg` | 256×256 NRO icon (the original game cover) |
| `scripts/build_switch.sh` | one-shot Switch build |
| `scripts/nacp_lang.py` | Japanese NACP title slot |
| `scripts/gen_cp932.py` | CP932 → Unicode table for the text renderer |
| `tests/test_host.cpp` | host tests: archive, exe tables, ANM, save formats, sound output, and the whole port on synthetic files |
| `tests/play_real.cpp` | developer tool: scripted play of the Linux build on your own game files |

The game ticks once per vsync (60 Hz); after a real stall it catches up by at most four ticks. It runs at the Switch's stock clocks; raise them with a tool such as sys-clk if you want more headroom.

---

## 🤝 Credits & Acknowledgments

* **ZUN / Team Shanghai Alice** — original creator and developer of the Touhou Project series.
* **[Tweet-META](https://github.com/Tweet-META/th12)** — the TH12 C++20 reimplementation and web port this port is built on (also published by **[Ritosa2333](https://github.com/Ritosa2333/th12)** and in **[YomotsuHisami](https://github.com/YomotsuHisami/eagler-touhou)**'s eagler-touhou collection).
* **[thpatch / thtk](https://github.com/thpatch/thtk)** — the TH12 archive cipher table.
* **Switchbrew & devkitPro Team** — the open-source `libnx` SDK and Switch toolchain.

**Licensing:** the Switch port code (`src/main.cpp`, `src/Platform.*`, `src/Memory.cpp`, `src/Archive.*`, `src/ExeTables.*`, `src/Anm.*`, `src/Renderer.*`, `src/TextRaster.*`, `src/BgmStream.*`, `scripts/`, `tests/`, build files) is CC0 1.0 (see `LICENSE`). `core/` is upstream code by Tweet-META under the **MIT** licence, and `src/Animation.*`, `src/Scene.*`, `src/Audio.*`, `src/Game.*`, `src/TitleScenes.*`, `src/TitlePresentation.*`, `src/Profile.*`, `src/Replay.*` and `src/Ending.*` are ports of its MIT-licensed JavaScript and keep that licence — see [`upstream/LICENSE-upstream-MIT.txt`](upstream/LICENSE-upstream-MIT.txt) and [`upstream/NOTICE-upstream.md`](upstream/NOTICE-upstream.md). Bundled third-party headers keep their own licences (`portable/sdl/third_party/`).
