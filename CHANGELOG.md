# Changelog — Touhou 12 Switch port

## Touhou 12: Undefined Fantastic Object — Switch Port r2

* **Stage transitions like the original:** the original builds the next stage as its 30-tick field fade ends, so the background fades out and the next one fades in with the player and HUD on screen. The port blacked out the field behind a loading indicator instead. The next stage is now read on a loader thread during the clear bonus and takes over within the frame.
* **Replays** of story and Extra runs play the clear sequence and fades instead of holding the picture still.
* **Smoother boss fights:** BGM is read ahead on its own thread (no SD card reads on the game thread), one vertex buffer per frame instead of one per draw, no per-frame hash maps for bullets, and a small-block pool for the many small allocations of a busy frame.
* `th12-switch.log` adds frame pacing per minute of play (late frames, slowest frame) and stage switch times.
* Display version `1.00b-r2`.

## Touhou 12: Undefined Fantastic Object — Switch Port r1

First release (display version `1.00b-r1`).

* The full game on Tweet-META's C++20 reimplementation of TH12 1.00b, compiled natively for ARM64, with its browser front-end (renderer, ANM sampler, background, HUD, text, sound, music, menus, stage flow) rewritten in C++ for SDL2 + OpenGL ES 3.
* Reads `th12.dat`, `thbgm.dat` and `th12.exe` (1.00b) in place; BGM streamed with the original loop points.
* The original start-up screen, loading indicator, title and menus: Start / Extra Start / Practice Start, Replay, Player Data, Music Room (with the comments and the unheard-track warning), Option, Key Config, the pause menu (with Save Replay and Quit and the Retry shortcut), continues, Score Ranking with name entry, results, and the title demo.
* The twelve endings and the staff roll; a first viewing cannot be skipped, as in the original.
* Replays in the original `.rpy` format (`replay/th12_01..25.rpy` and `th12_ud????.rpy`), watched by stage with fast-forward and slow motion.
* Saves in the original formats: `scoreth12.dat` (rankings, Practice and Extra unlocks, clear and play counts, play time, spell card history and titles, endings seen, Music Room tracks) and `th12.cfg` (volumes, pad buttons). Files that are not TH12 saves are left untouched; progress is also saved when the game is closed from the HOME menu.
* The HUD's high score starts from the score file's best, as in the original.
* Remappable controls through Option → Key Config, with the same defaults as the other ports.
