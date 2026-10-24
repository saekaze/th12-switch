# Upstream

* **Project:** Tweet-META/th12 — TH12 1.00b C++20 reimplementation and web port (MIT).
* **Taken from:** [YomotsuHisami/eagler-touhou](https://github.com/YomotsuHisami/eagler-touhou) `ports/th12`, commit `445eb6ce2b0220db166d0d334bef67a77488346e` (2026-10-24), a subtree of Tweet-META/th12 branch `eagler` (base `c322e8ad7a3fa1276dd6dc377c0b5184d4f834bd`).
* Also published as [Ritosa2333/th12](https://github.com/Ritosa2333/th12) (an earlier state of the same code).

## What is used

| Upstream path | Here | Changes |
| :--- | :--- | :--- |
| `src/game/*` | `core/game/` | none |
| `src/wasm/Exports.cpp`, `src/wasm/Api.hpp` | `core/wasm/` | none |
| `src/*.hpp` (compatibility headers) | `core/` | none |
| `LICENSE`, `NOTICE.md` | `upstream/` | renamed |

Not used directly: the browser front-end (`site/`, `src/*.mjs`, rewritten below), content preparation (`scripts/`, done at run time), THPrac, the browser-only manual, the replay motion extension and workspace, the replay verifier and frame inspector, and `decompiled/`.

## The front-end rewrite

Upstream's JavaScript is rewritten in C++ in `src/`, following its behaviour:

| Upstream | Here |
| :--- | :--- |
| `src/animation.mjs`, `src/easing.mjs` | `src/Animation.*` |
| `src/renderer.mjs`, `src/stage-background.mjs`, `src/draw-scheduler.mjs`, `src/vertex-batch.mjs` | `src/Scene.*`, `src/Renderer.*` |
| `src/ascii-presentation.mjs`, `src/spell-presentation.mjs`, `src/ufo-presentation.mjs`, `src/fps-presentation.mjs`, `src/stage-clear-presentation.mjs` | `src/Scene.cpp` |
| `src/sound-player.mjs`, `src/sound-events.mjs`, `src/music.mjs` | `src/Audio.*` |
| `src/stage-loader.mjs`, `src/loading-presentation.mjs`, `site/game.mjs` (session flow) | `src/Game.*` |
| `src/title-scenes.mjs` | `src/TitleScenes.*` |
| `src/title-presentation.mjs`, `src/native-replay-presentation.mjs`, `src/native-data-presentation.mjs`, `src/menu-resources.mjs` (music comments) | `src/TitlePresentation.*` |
| `src/native-profile.mjs`, `src/game-profile.mjs` (rules) | `src/Profile.*` |
| `src/replay.mjs`, `src/replay-recorder.mjs`, `src/replay-session.mjs` | `src/Replay.*` |
| `src/ending-timeline.mjs`, `src/menu-resources.mjs` (ending MSG) | `src/Ending.*` |
| `scripts/prepare-assets.py` (ANM), `prepare-presentation.py` (STD, bullet types), `prepare-sounds.py`, `prepare-music.py` | read at run time from `th12.dat` / `th12.exe` |

Where upstream read the core's JSON snapshots (message, spell, UFO, boss HUD, stage flow, background), the host reads the same core fields directly. Binary records (poses, schedule, meshes, sprites, sound events, HUD) use the same exported C ABI.

Known deviation: upstream's scroll-velocity interpolation (ANM ops 93/94) starts from an undefined value in JavaScript; here it starts from the current scroll velocity.
