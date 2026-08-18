# T-Embed Port — Custom Work & Session Handoff

This documents the custom additions to this fork of the Flipper-Zero-ESP32-Port
(for the **LilyGo T-Embed CC1101**) and gives a new Claude session the operational
knowledge to continue without re-deriving it. If you're a fresh Claude session,
**read this first.** (Rename or copy this to `CLAUDE.md` if you want Claude Code to
auto-load it every session.)

Owner's GitHub backup of this work: **private repo `github.com/GYRAG/t-flipper`**
(branch pushed to its `main`). Upstream is `github.com/Sor3nt/Flipper-Zero-ESP32-Port`
— **`origin` points at upstream; never push there.** Work branch here: `tembed-ports`.

## At a glance

- **Apps ported** (`applications_user/`): **resistors**, **net_calculator** (IPv4/VLSM subnet
  calculator), **wikiflip** (offline cybersecurity dictionary), **fortune_spinner**.
- **New app built here**: **coloranim** — full-color 320×170 animation player.
- **New tool**: `tools/compile_animation.py` — GIF/PNG → animation pack (mono or color).
- **Firmware improvements**: 432-symbol FAP API export audit; WiFi RAM fix (upstream merge);
  encoder keypad fix (`number_input`); app-browser speedup (thread pin + FAP icon cache).

Details and file paths for each are in §3.

---

## 1. Hardware & UI model (the constraints everything follows from)

- **Board:** LilyGo T-Embed CC1101 — ESP32-S3 (Xtensa), **8 MB PSRAM**, 16 MB flash.
- **Display:** ST7789 **320×170 color** LCD. But the Flipper UI renders a **128×64 mono**
  framebuffer and 2× upscales it. So normal apps are mono; full color/res is possible
  only by taking over the panel directly (see §5 Color animations, and DOOM).
- **Input:** rotary encoder + 2 buttons. The driver
  (`targets/lilygo_t_embed_cc1101/target_input.c`) synthesizes all Flipper keys:
  - turn encoder → **Up/Down**
  - **hold encoder button + turn → Left/Right**
  - short-click encoder → **OK**
  - side key (IO6) → **Back**
  - the encoder button **is** BOOT/IO0 (used for flash download mode).

---

## 2. Build & flash on Windows (the working recipe — non-obvious)

### Firmware
```bash
python winbuild.py build          # cold build; runs set-target (fullclean) + reconfigure
```
- ESP-IDF **v5.4.1** at `C:\Espressif\frameworks\esp-idf-v5.4.1`.
- `set-target` **regenerates `sdkconfig`** from `sdkconfig.defaults*` — so persistent
  config must live in `sdkconfig.defaults.esp32s3`, not just `sdkconfig`.
- **`winbuild.py build` wipes `build_t_embed/`** (incl. all built FAPs) via fullclean.

### FAP (app) build — `buildFap.sh` needs manual env on git-bash
ESP-IDF's `export.sh` refuses to run under MSys/git-bash, and `python3` here is the
Microsoft-Store stub that silently no-ops. So:
```bash
export IDF_PATH="/c/Espressif/frameworks/esp-idf-v5.4.1"
export PATH="$HOME/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH"
export PYTHON="C:/Espressif/python_env/idf5.4_py3.11_env/Scripts/python.exe"
./buildFap.sh applications_user/<appid>     # firmware must be built first (needs build_t_embed/config)
```
- `buildFap.sh` was patched to honor `$PYTHON` (falls back to `python3` on Linux/mac).
- The IDF python has `pillow` installed (for icon/animation tooling).

### Flashing — use the LOCAL web flasher, not the CLI
The S3's native USB CDC won't auto-reset into the bootloader, so `winbuild.py flash`
fails with "No serial data received." The repo's own web flasher works (esptool-js,
Web-Serial reset). To flash a **local** build:
```bash
cp build_t_embed/bootloader/bootloader.bin          release/t-embed/latest/bootloader.bin
cp build_t_embed/partition_table/partition-table.bin release/t-embed/latest/partition-table.bin
cp build_t_embed/furi_esp32.bin                      release/t-embed/latest/furi_esp32.bin
python -m http.server 8777 --bind 127.0.0.1
```
Then in a Chromium browser: open **http://localhost:8777/interface.html** → select
LilyGo T-Embed CC1101 → **enter download mode** (power off → hold encoder in → power on
while holding → release; screen stays **dark**) → Connect (pick the port that appears in
download mode; it may change number) → flash. `release/` and `graphify-out/` are gitignored.

---

## 3. What was added (this work)

### Ported apps (`applications_user/`, all build as FAPs, no firmware change)
- **resistors** — VLSM resistor color-code calculator. Added: short-click OK cycles the
  selected band (so you don't need hold-and-turn).
- **net_calculator** — IPv4/VLSM subnet calculator. Needed 2 `number_input` setters
  exported (see API audit).
- **wikiflip** — offline cybersecurity dictionary (content compiled in).
- **fortune_spinner** — YES/NO/MAYBE spinner (user-added; reviewed).

### Firmware API export audit (commit `572c3a4`)
`components/flipper_application/flipper_application/firmware_api.c` is the FAP symbol
table (hash-sorted). It was maintained reactively and had **432 gaps** → apps "build but
won't load." Audited (public headers × firmware-defined `T` symbols × already-exported),
added all 432 via `tools/add_symbol.py` + 28 module `#include`s. Table **939 → 1372**.
So most ports now resolve out of the box.

### WiFi RAM fix (merge `b196701`)
A from-source rebuild broke WiFi (scan found nothing) because the local checkout predated
upstream `cb6b530`, which moves task stacks to **PSRAM** to free the internal DRAM WiFi's
DMA buffers need (+ `CONFIG_SPI_FLASH_AUTO_SUSPEND`). Fixed by merging `origin/main`.

### Encoder keypad fix (`components/gui/modules/number_input.c`, in `f261bcf`)
Stock `number_input` mapped the encoder's Up/Down to keypad *rows* only, so the dial only
reached `0`/`5`. Rewrote `handle_up/down` to walk the whole keypad linearly. **`text_input`
and `byte_input` still have the stock grid problem** — apply the same fix if a ported app
uses them (needs firmware rebuild + reflash).

### Color animations (commit `67909a6`)
- **`tools/compile_animation.py`** — build animation packs from a **GIF or PNG frames**,
  no hand-written metadata. Mono mode → `/ext/dolphin/` format (meta.txt + frame_N.bm +
  manifest). `--color` → native **320×170 RGB565** pack (`frames.bin` + `info.txt`).
  Flags: `--max-frames`, `--frame-rate`, `--width/--height`, `--selftest`.
- **`applications_user/coloranim`** — FAP that plays color packs at full 320×170 via
  `gui_direct_draw_acquire` + `esp_lcd_panel_draw_bitmap` (same path DOOM uses). Reads
  packs from `/ext/apps_data/coloranim/<name>/`. Single-transaction blit (no scanlines).

### App-browser speedup (commit `62d4c31`)
Opening the app list took 3–4s (per-FAP manifest+icon parse on a slow PSRAM stack).
- `components/furi/core/thread.c` — pins the `"LoaderApplications"` thread's stack to
  internal RAM; everything else stays PSRAM-first (WiFi DRAM untouched).
- `components/loader/fap_icon_cache.{c,h}` (wired into `loader_applications.c`) — caches
  each FAP's name + 10×10 icon in `/ext/.fap_icon_cache` keyed by path+size. First open
  parses+caches; every open after is stat-only hits → near-instant. Rebuilt FAPs
  self-invalidate (size changes).

---

## 4. Porting a new app (quick recipe)

1. Drop upstream app source into `applications_user/<appid>/` (keep its `src/` layout).
2. Its upstream `application.fam` usually works unchanged.
3. Normalize includes to the `<gui/...>` convention. **Do NOT use `<applications/services/gui/...>`**
   — that path is a stale duplicate header tree and double-includes (enum redefinition).
4. Build with the §2 FAP recipe. If it reports **missing API symbols**: they exist in the
   port but aren't exported — add with `PYTHONIOENCODING=utf-8 python tools/add_symbol.py <names…>`,
   add the declaring header's `#include` to `firmware_api.c` if needed, rebuild firmware, reflash.
5. Copy the `.fap` to SD `/ext/apps/<category>/`.

**Input judgment:** apps that only need OK/Back or a menu/list port cleanly. Apps using the
full D-pad work via hold-and-turn, but consider an OK-based shortcut (like the resistors app).

`add_symbol.py` bugs: (a) crashes printing `✓` on the Windows console — run with
`PYTHONIOENCODING=utf-8`; (b) a new **global-max-hash** symbol lands *after* the array's
`};` — move the terminator down by hand if it happens.

---

## 5. Animation format reference

Idle "dolphin" animations are **SD-loaded** (loader: `applications/services/desktop/animations/
animation_storage.c`). Under `/ext/dolphin/`:
- `manifest.txt` (FlipperFormat, `Filetype: Flipper Animation Manifest`): per animation
  `Name`, `Min/Max butthurt`, `Min/Max level`, `Weight`.
- `<name>/meta.txt` (`Filetype: Flipper Animation`): `Width`, `Height`, `Passive frames`,
  `Active frames`, `Frames order`, `Active cycles`, `Frame rate`, `Duration`,
  `Active cooldown`, `Bubble slots` (+ bubbles).
- `<name>/frame_<i>.bm` — 1-bit, **same format as icons**: `[0x00 header] + inverted-XBM`.
  Uncompressed size must be exactly `ceil(width/8)*height + 1`.

Full-color animations are a **separate, custom** thing (not dolphin): 320×170 RGB565
big-endian packs played by the `coloranim` FAP. All display symbols needed
(`gui_direct_draw_acquire/release`, `furi_hal_display_get_panel_handle/get_h_res/get_v_res`,
`furi_hal_spi_bus_lock/unlock`, `esp_lcd_panel_draw_bitmap`) are exported.

---

## 6. Open ideas / possible next steps

- **coloranim → on-device manager**: add preview thumbnails + set-active to the FAP.
- **Color idle/boot animation**: hook the color player into the desktop idle (firmware
  work) so a color animation plays *as* the idle screen, not just on-demand.
- **BLE unknown-tracker detector**: strongest "signature app" for this board (native BLE).
- **`text_input`/`byte_input` keypad fix**: same linear-encoder fix as `number_input`.
- **On-device app store over WiFi**: feasible only for FAPs **pre-compiled for Xtensa** —
  the official Flipper catalog ships ARM binaries that cannot run here (wrong CPU).

---

## 7. Gotchas cheat-sheet

| Symptom | Cause / fix |
|---|---|
| App "builds but won't load" | Symbol not in `firmware_api.c` → `add_symbol.py` + reflash |
| Enum redefinition on build | App used `<applications/services/gui/...>` → switch to `<gui/...>` |
| WiFi scans nothing after a rebuild | Checkout behind upstream, missing PSRAM-stacks fix → merge `origin/main` |
| App list slow (seconds) | FAP icon cache (done); also trim `/ext/apps/`; fewer FAPs = faster |
| CLI flash "No serial data received" | Use the local web flasher + manual download mode (§2) |
| `python3` does nothing on Windows | MS-Store stub → use `$PYTHON` / the IDF python |
| number_input dial only hits 0/5 | Fixed; `text_input`/`byte_input` still need it |
