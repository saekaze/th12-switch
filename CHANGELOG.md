# Changelog — Touhou 12 Switch port

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
