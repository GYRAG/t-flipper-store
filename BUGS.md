# Known Bugs

Tracking crashes on the T-Embed port. All of them need a **coredump** to diagnose
(the ESP32 panic prints to UART0 pins, not the USB CDC on COM16, so a live serial
monitor can't capture the backtrace — see below).

**Coredump is now enabled** in `sdkconfig.defaults.esp32s3`
(`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`). It needs a rebuild + reflash to take
effect; after that, reproduce any crash below and decode it (commands at the end).

## 1. Tetris "crashes" during play — LIKELY FIXED by the #3 fix
- **Symptom (historic):** stopped responding a few seconds into a game.
- **Status:** NOT REPRODUCIBLE since the deadlock fixes in #3 landed
  (2026-08-23). Almost certainly the same system-level deadlock, not a Tetris
  bug — see "Why these were never app bugs" below.
- Earlier work still stands on its own merits: `tetris_game_is_valid_pos` gained
  `y` bounds because a rotation wall-kick could push a block's `y` out of range
  and index `playField[y][x]` out of bounds. That was a real latent defect; it
  just was not what made the device stop responding.
- **If it ever recurs:** the coredump pipeline works now — reproduce and decode
  (commands at the end). Remaining unguarded candidate:
  `tetris_game_piece_at_bottom` reads `playField[pos.y][pos.x]` with no `x` bound
  and no `y<0` guard.

## 2. Fortune Spinner "crashes" after ~7 spins — LIKELY FIXED by the #3 fix
- **Symptom (historic):** stopped responding after roughly 7 spins.
- **Status:** NOT REPRODUCIBLE since the #3 fixes landed (2026-08-23).
- The original note here was right and worth keeping: the source was reviewed 3×
  — fixed-size model, no arrays, no per-spin allocation, RNG is a one-line
  `esp_random()` — and **nothing in the code should crash.** It never was an app
  bug. It commits the view model every 50 ms tick even when idle, which is
  exactly the timer + view-model churn that fed the deadlock.

## Why these were never app bugs
Both were filed as "crashes", and both were almost certainly the #3 deadlock:

- **Neither ever wrote a coredump.** This file used to assume panics were merely
  invisible over USB. Now that coredump-to-flash is on, a genuine panic demonstrably
  *does* get captured — so the absence of a dump means there was never a panic.
  They were freezes, and a frozen device that needs the reset button looks exactly
  like a crash from the outside.
- **The broken lock order was system-wide, not per-app.** `desktop_clock_timer_callback`
  blocking on the GUI mutex stalled `Tmr Svc`, the only task that drains the
  FreeRTOS timer command queue — so *every* `furi_timer_start/stop` in the
  firmware stalled behind it. Any app leaning on timers could wedge, in its own
  seemingly unrelated way. That is why the failures looked app-specific and why
  reading each app's source found nothing.

**Lesson for the next one of these:** a reproducible "crash" that leaves no
coredump is not a crash. Check for a deadlock before reading app source.

## 3. Device freezes when the dial is rotated fast — ROOT CAUSE FOUND
- **Symptom:** rotate the encoder quickly in a menu → everything stops. All
  buttons dead, only the reset button does anything. Sometimes it freezes
  silently, sometimes it reboots — same bug either way (see below).
- **Status:** DIAGNOSED from a coredump on 2026-08-23. **Not a crash — a
  three-way circular deadlock.** That is why it never produced a coredump on
  its own: no panic ever happened, and the task watchdog only checks the *idle*
  task, which keeps running happily while everyone else is blocked.

### The cycle (all three confirmed in the dump)

| Task | Holds | Blocked waiting for |
|---|---|---|
| **LoaderMenu** | view-model mutex | FreeRTOS timer command queue |
| **Tmr Svc** | — (the only drainer of that queue) | GUI mutex |
| **GuiSrv** | GUI mutex | view-model mutex |

1. `menu_process_down` → `menu_set_position` takes the **view model lock**
   (`with_view_model`) and, still holding it, calls `icon_animation_stop/start`
   → `furi_timer_stop/start` → `xQueueGenericSend(xStaticTimerQueue, WaitForever)`.
   `CONFIG_FREERTOS_TIMER_QUEUE_LENGTH` is **10**, and every detent issues two
   timer commands, so a fast turn fills it and this call blocks.
2. The only task that drains that queue is **Tmr Svc**, and it is busy running
   `desktop_clock_timer_callback` (`applications/services/desktop/desktop.c:362`),
   which calls `gui_active_view_port_count` → `gui_lock()` and blocks on the
   **GUI mutex**.
3. **GuiSrv** holds the GUI mutex inside `gui_redraw_fs` → `view_draw` →
   `view_get_model`, blocked on the **view model mutex** — held by LoaderMenu.

Closed loop. Collateral damage: `NotificationSrv` is also stuck on the timer
queue, and `InputSrv` piles up behind the GUI's 8-entry input queue.

### Two independent fixes, either of which breaks the cycle

- **(A) Don't take the GUI lock from a timer callback.**
  `desktop_clock_timer_callback` runs on **Tmr Svc**, the one task that must
  never block — while it waits, no timer command from any task can complete.
  Defer the work to the desktop thread instead. This is the clearest defect.
- **(B) Don't start/stop timers while holding the view model lock.**
  `menu_set_position` (`components/gui/modules/menu.c:463`) does exactly that.
  Moving the `icon_animation_*` calls outside `with_view_model` removes the
  other half of the inversion.

Doing **both** is the durable fix; (A) alone is the minimal one.

### Why it looked inconsistent
The "freeze vs reboot" difference is just *where* InputSrv happened to be when
the cycle closed. Blocked inside `furi_pubsub_publish` waiting on the pubsub
mutex → silent freeze. Blocked inside the GUI queue put → the diagnostic
`furi_crash("GUI input queue stuck")` fires after 2 s and reboots. Visiting the
Apps browser first is *not* required; it just makes the stall easier to hit.

### The diagnostic that caught it
`components/gui/gui.c` currently bounds the input-queue put at
`GUI_INPUT_QUEUE_TIMEOUT_MS` and calls `furi_crash` on timeout, to turn the
silent deadlock into a capturable panic. **Keep this only until (A)/(B) land** —
it converts a freeze into a reboot, which is right for debugging and wrong for
daily use.

## Diagnosis blocker → now fixed
The panic backtrace isn't visible over USB (goes to UART0; the S3's USB-Serial/JTAG
console can't share the PHY with the TinyUSB CDC the firmware uses). A coredump
partition already existed (`0xa10000`, 128 KB) but the build defaulted to
`CONFIG_ESP_COREDUMP_ENABLE_TO_NONE`, so nothing was ever written to it.

`sdkconfig.defaults.esp32s3` now sets `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH` (+ ELF
format and a CRC32 checksum). Note the config has to live in
`sdkconfig.defaults.esp32s3`, not `sdkconfig` — `set-target` regenerates
`sdkconfig` from the defaults on every cold build (HANDOFF.md §2).

### Getting a backtrace

Rebuild and reflash first — the setting only exists in a new image:

```bash
python winbuild.py build
```

Then flash via the local web flasher (HANDOFF.md §2), or the store site's
**Flash Firmware** button.

Reproduce the crash, then pull and decode the dump. The exact invocation matters —
three things bite (all verified on 2026-08-23):

- **Use the IDF python, not `python`.** A bare `python` picks up whatever is first
  on PATH and fails with `No module named esp_coredump`. Same class of trap as the
  `python3` MS-Store stub in HANDOFF.md §2.
- **`--port` is a GLOBAL option** — it goes *before* the subcommand, not after.
- **`IDF_PATH` and the xtensa gdb must be in the environment**, or it stops with
  "Please set up ESP-IDF to complete the action".

`--core-format` is only for a dump file passed with `-c`; reading from flash
doesn't take it. **No download mode needed** — esptool auto-resets over the native
USB CDC here, so the device's normal COM port works while the firmware is running.

```bash
export IDF_PATH="C:/Espressif/frameworks/esp-idf-v5.4.1"
export PATH="C:/Espressif/tools/xtensa-esp-elf-gdb/14.2_20240403/xtensa-esp-elf-gdb/bin:$PATH"
"C:/Espressif/python_env/idf5.4_py3.11_env/Scripts/esp-coredump.exe" --port COM15 --chip esp32s3 info_corefile build_t_embed/furi_esp32.elf
```

The ELF must be the exact one that was flashed or the symbols won't line up, and
`winbuild.py build` wipes `build_t_embed/` — so decode before rebuilding.

**Reading `Core dump version "0xffff" is not supported!`** (with size
`4294967295`) just means the partition is still erased: no crash has been captured
since the last flash. Reproduce first.

To save the raw dump instead of decoding immediately, add `--save-core dump.elf`,
or use `dbg_corefile` for an interactive gdb session.

Once decoded, the faulting task, PC and stack trace identify the bug directly —
for **any** of the three crashes above, and any future one.
