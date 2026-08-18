# Known Bugs

Tracking app crashes on the T-Embed port. Both need a **coredump** to diagnose
(the ESP32 panic prints to UART0 pins, not the USB CDC on COM16, so a live serial
monitor can't capture the backtrace — see below).

## 1. Tetris crashes during play
- **Symptom:** crashes a few seconds into a game.
- **Status:** OPEN. A first fix (commit `62d4c31`… actually in the tetris working
  change) added `y` bounds to `tetris_game_is_valid_pos` — the rotation wall-kick
  could push a block's `y` out of range and index `playField[y][x]` out of bounds.
  **That did not fully fix it — it still crashes.** So there is at least one more
  fault path (candidates: `tetris_game_piece_at_bottom` reads `playField[pos.y][pos.x]`
  with no `x` bound and no `y<0` guard; line-clear index math; or a stack/heap issue).
- **Next step:** enable coredump-to-flash, reproduce, decode the backtrace.

## 2. Fortune Spinner crashes after ~7 spins
- **Symptom:** crashes after roughly 7 spins.
- **Status:** OPEN. Source reviewed 3×: fixed-size model, no arrays, no per-spin
  allocation, RNG is a one-line `esp_random()` — **nothing in the code should crash.**
  Likely a firmware/runtime issue (stack, heap, or GUI redraw churn — it commits the
  view model every 50 ms tick even when idle).
- **Next step:** coredump backtrace (same as above).

## Diagnosis blocker → the fix for it
The panic backtrace isn't visible over USB (goes to UART0; the S3's USB-Serial/JTAG
console can't share the PHY with the TinyUSB CDC the firmware uses). **A coredump
partition already exists** (`0xa10000`, 128 KB) but is disabled (`CONFIG_ESP_COREDUMP_ENABLE_TO_NONE`).
Enabling `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` + rebuild + reflash would let us extract
a full register/stack dump from flash with `espcoredump.py` and decode the exact fault
for **any** app crash. Recommended once we circle back to these.
