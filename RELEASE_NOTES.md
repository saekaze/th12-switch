<img width="256" height="256" alt="Touhou 12" src="https://raw.githubusercontent.com/saekaze/th12-switch/main/platform/switch/icon.jpg" />

## Touhou 12 — 1.00b-r2

* 🎬 **Stage transitions like the original.** After a stage clear the field fades out and the next stage fades in, with no black loading screen in between: it now loads in the background during the clear bonus. Replays play the clear sequence too.
* ⚡ **Smoother boss fights.** Fewer frame drops in dense patterns: music is read ahead in the background instead of from the SD card mid-frame, and drawing and memory allocation got lighter.

Update: replace `touhou12.nro` in `sd:/switch/touhou/touhou12/`. Saves and replays are kept.

---

## Touhou 12 — 1.00b-r1 (first release)

A native Nintendo Switch port of Touhou 12: Undefined Fantastic Object ver 1.00b, built on Tweet-META's TH12 C++20 reimplementation and compiled natively for ARM64.

* 🛸 **The full game:** Reimu, Marisa and Sanae with both shot types, every difficulty, all 6 stages and Extra, the UFO system, Practice, the twelve endings and the staff roll.
* 🏮 **The original menus:** title, selection, pause, continue and result pages, Music Room, Option and Key Config, drawn from your own `th12.dat`.
* 🎬 **Replays, Score Ranking with name entry, Player Data** and the title demo.
* 💾 **PC-compatible saves:** `scoreth12.dat`, `th12.cfg` and `replay/` can be copied between the PC and the Switch in either direction.
* 🔊 BGM streamed straight from `thbgm.dat` with the original loop points.
* 📁 Recommended folder: `sd:/switch/touhou/touhou12/`, next to the other ports.

Put `touhou12.nro`, `th12.exe`, `th12.dat`, `thbgm.dat` and `msgothic.ttc` (ver 1.00b, Japanese) in the folder and launch from hbmenu.

Controls: B shoot/confirm, A bomb/cancel, L/ZL focus, R/ZR skip dialogue, + pause, Y retry (pause menu), − snapshot; rebindable in Option → Key Config. In replays: R/ZR or B fast-forward, L/ZL slow motion.
