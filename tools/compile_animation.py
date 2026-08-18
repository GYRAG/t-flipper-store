#!/usr/bin/env python3
"""
Build a Flipper dolphin animation pack from a folder of PNG frames — so you
never hand-write meta.txt / manifest.txt again.

The T-Embed firmware loads idle animations from the SD card at
    /ext/dolphin/<name>/meta.txt        (animation params, FlipperFormat)
    /ext/dolphin/<name>/frame_<i>.bm    (1-bit frames, icon format)
    /ext/dolphin/manifest.txt           (lists every animation)
This generates all three, with sensible defaults, and updates the manifest
in place (idempotent — re-running replaces that animation's entry).

Usage:
    python tools/compile_animation.py SOURCE --name "MyAnim" [--out DIR] [options]
    python tools/compile_animation.py --selftest

SOURCE: either a .gif file, or a folder of PNG frames (sorted naturally: frame_2 < frame_10).
--out:  the dolphin dir to write into (default: ./dolphin). Copy its contents
        to the SD card's /ext/dolphin/ afterwards.

The dolphin screen is 128x64 monochrome. Color/large sources (e.g. a GIF) are
resized to fit within 128x64 and converted to 1-bit. Override with --width/--height,
and thin out long GIFs with --max-frames.

Frames encode exactly like tools/fam/compile_icons.py (invert -> XBM -> 0x00 header),
so they're byte-for-byte the format the firmware's own icons use.
"""
import argparse
import io
import re
import sys
from pathlib import Path

try:
    from PIL import Image, ImageOps, ImageSequence
except ImportError:
    sys.exit("Pillow required: pip install pillow (or use the ESP-IDF python that has it)")

SCREEN_W, SCREEN_H = 128, 64        # mono dolphin animation
COLOR_W, COLOR_H = 320, 170         # native ST7789 LCD (full-color mode)

# Per-channel RGB565 lookup tables (avoids numpy; big-endian output matches DOOM:
# c = ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3), stored high-byte-first for the ST7789).
_R5 = [(r & 0xF8) << 8 for r in range(256)]
_G6 = [(g & 0xFC) << 3 for g in range(256)]
_B5 = [b >> 3 for b in range(256)]


def image_to_rgb565_be(image: "Image.Image") -> bytes:
    """PIL image -> big-endian RGB565 bytes (2/pixel), the ST7789 blit format."""
    rgb = image.convert("RGB").tobytes()
    out = bytearray(len(rgb) // 3 * 2)
    j = 0
    for i in range(0, len(rgb), 3):
        c = _R5[rgb[i]] | _G6[rgb[i + 1]] | _B5[rgb[i + 2]]
        out[j] = c >> 8          # high byte first (big-endian == DOOM's byte-swap)
        out[j + 1] = c & 0xFF
        j += 2
    return bytes(out)


def write_color_pack(frames, out_dir: Path, name: str, frame_rate: int):
    """Full-color pack: frames.bin (concatenated RGB565-BE) + info.txt."""
    anim_dir = out_dir / name
    anim_dir.mkdir(parents=True, exist_ok=True)
    w, h = frames[0].size
    with (anim_dir / "frames.bin").open("wb") as f:
        for frame in frames:
            f.write(image_to_rgb565_be(frame))
    (anim_dir / "info.txt").write_text(
        "Filetype: T-Embed Color Animation\n"
        "Version: 1\n"
        f"Width: {w}\n"
        f"Height: {h}\n"
        f"Frames: {len(frames)}\n"
        f"Frame rate: {frame_rate}\n",
        encoding="utf-8",
    )
    return w, h, len(frames)


def image_to_bm(image: "Image.Image"):
    """PIL image -> (width, height, bytes) in Flipper .bm format: [0x00] + inverted XBM."""
    with io.BytesIO() as output:
        ImageOps.invert(image.convert("1")).save(output, format="XBM")
        xbm = output.getvalue().decode().strip()
    lines = xbm.splitlines()
    width = int(lines[0].split(" ")[2])
    height = int(lines[1].split(" ")[2])
    data = "".join(lines[2:]).replace(" ", "").split("=")[1][1:-2]
    raw = bytearray.fromhex(data.replace(",", " ").replace("0x", ""))
    return width, height, bytes([0x00]) + bytes(raw)


def load_source_frames(source: Path, width, height, max_frames, fit=(SCREEN_W, SCREEN_H)):
    """Return a list of PIL images from a .gif or a folder of PNGs, resized to fit."""
    if source.is_file() and source.suffix.lower() == ".gif":
        frames = [f.convert("RGB") for f in ImageSequence.Iterator(Image.open(source))]
    elif source.is_dir():
        pngs = sorted([p for p in source.iterdir() if p.suffix.lower() == ".png"], key=natural_key)
        if not pngs:
            sys.exit(f"No PNG frames found in {source}")
        frames = [Image.open(p) for p in pngs]
    else:
        sys.exit(f"SOURCE must be a .gif file or a folder of PNGs: {source}")

    if max_frames and len(frames) > max_frames:  # subsample evenly
        step = len(frames) / max_frames
        frames = [frames[int(i * step)] for i in range(max_frames)]

    # Target size: explicit --width/--height, else fit within 128x64 preserving aspect.
    if width and height:
        target = (width, height)
    else:
        w0, h0 = frames[0].size
        scale = min(fit[0] / w0, fit[1] / h0, 1.0)
        target = (max(1, round(w0 * scale)), max(1, round(h0 * scale)))
    if target != frames[0].size:
        frames = [f.resize(target) for f in frames]
    return frames


def natural_key(p: Path):
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", p.name)]


def write_meta(meta_path: Path, width, height, order, passive, active,
               frame_rate, duration, active_cycles, active_cooldown, bubbles):
    lines = [
        "Filetype: Flipper Animation",
        "Version: 1",
        "",
        f"Width: {width}",
        f"Height: {height}",
        f"Passive frames: {passive}",
        f"Active frames: {active}",
        "Frames order: " + " ".join(str(i) for i in order),
        f"Active cycles: {active_cycles}",
        f"Frame rate: {frame_rate}",
        f"Duration: {duration}",
        f"Active cooldown: {active_cooldown}",
        "",
        f"Bubble slots: {len(bubbles)}",
    ]
    for i, b in enumerate(bubbles):
        lines += [
            f"Slot: {i}",
            f"X: {b['x']}",
            f"Y: {b['y']}",
            "Text: " + b["text"].replace("\n", "\\n"),
            f"AlignH: {b.get('align_h', 'Center')}",
            f"AlignV: {b.get('align_v', 'Bottom')}",
            f"StartFrame: {b['start_frame']}",
            f"EndFrame: {b['end_frame']}",
        ]
    meta_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


MANIFEST_FIELDS = ["Min butthurt", "Max butthurt", "Min level", "Max level", "Weight"]


def _parse_manifest(text):
    """-> dict{name: {field: value}} preserving nothing else (we rewrite the file)."""
    entries, cur = {}, None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("Name:"):
            cur = line.split(":", 1)[1].strip()
            entries[cur] = {}
        elif cur and ":" in line:
            k, v = line.split(":", 1)
            if k.strip() in MANIFEST_FIELDS:
                entries[cur][k.strip()] = v.strip()
    return entries


def update_manifest(manifest_path: Path, name, vals):
    entries = _parse_manifest(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    entries[name] = vals  # add or replace this animation
    out = ["Filetype: Flipper Animation Manifest", "Version: 1", ""]
    for n, v in entries.items():
        out.append(f"Name: {n}")
        for f in MANIFEST_FIELDS:
            out.append(f"{f}: {v[f]}")
        out.append("")
    manifest_path.write_text("\n".join(out).rstrip() + "\n", encoding="utf-8")


def build(source: Path, out_dir: Path, name: str, args, bubbles=None):
    bubbles = bubbles or []
    color = getattr(args, "color", False)
    fit = (COLOR_W, COLOR_H) if color else (SCREEN_W, SCREEN_H)
    frames = load_source_frames(source, getattr(args, "width", None),
                                getattr(args, "height", None), getattr(args, "max_frames", None), fit)

    if color:
        # Full-color native-resolution pack (played by the color-animation FAP via
        # gui_direct_draw + esp_lcd_panel_draw_bitmap). No dolphin manifest.
        return write_color_pack(frames, out_dir, name, args.frame_rate)

    anim_dir = out_dir / name
    anim_dir.mkdir(parents=True, exist_ok=True)
    width = height = None
    for i, frame in enumerate(frames):
        w, h, data = image_to_bm(frame)
        if width is None:
            width, height = w, h
        elif (w, h) != (width, height):
            sys.exit(f"Frame {i} is {w}x{h}, expected {width}x{height} — all frames must match")
        (anim_dir / f"frame_{i}.bm").write_bytes(data)

    n = len(frames)
    active = args.active_frames
    passive = n - active
    if passive < 0:
        sys.exit(f"--active-frames {active} exceeds total frames {n}")
    order = list(range(n))  # 1:1 play order; frame_i.bm plays at step i
    write_meta(anim_dir / "meta.txt", width, height, order, passive, active,
               args.frame_rate, args.duration, args.active_cycles, args.active_cooldown, bubbles)

    update_manifest(out_dir / "manifest.txt", name, {
        "Min butthurt": args.min_butthurt, "Max butthurt": args.max_butthurt,
        "Min level": args.min_level, "Max level": args.max_level, "Weight": args.weight,
    })
    return width, height, n


def selftest():
    import tempfile, os
    tmp = Path(tempfile.mkdtemp())
    frames = tmp / "frames"
    frames.mkdir()
    W, H, N = 128, 64, 3
    for i in range(N):
        Image.new("1", (W, H), color=i % 2).save(frames / f"frame_{i}.png")

    out = tmp / "dolphin"
    args = argparse.Namespace(active_frames=0, frame_rate=2, duration=3600,
                              active_cycles=0, active_cooldown=0, min_butthurt=0,
                              max_butthurt=14, min_level=1, max_level=3, weight=3)
    build(frames, out, "TestAnim", args)

    # frame .bm size must equal ceil(W/8)*H + 1 (the loader's exact expectation)
    expected = ((W + 7) // 8) * H + 1
    for i in range(N):
        sz = (out / "TestAnim" / f"frame_{i}.bm").stat().st_size
        assert sz == expected, f"frame_{i}.bm is {sz}, expected {expected}"
    meta = (out / "TestAnim" / "meta.txt").read_text()
    for key in ("Filetype: Flipper Animation", "Width: 128", "Height: 64",
                "Passive frames: 3", "Frames order: 0 1 2", "Frame rate: 2"):
        assert key in meta, f"meta.txt missing: {key}"
    man = (out / "manifest.txt").read_text()
    assert "Filetype: Flipper Animation Manifest" in man and "Name: TestAnim" in man

    # idempotent re-run replaces, not duplicates
    build(frames, out, "TestAnim", args)
    assert (out / "manifest.txt").read_text().count("Name: TestAnim") == 1
    print("selftest OK — frames, meta.txt, manifest.txt all correct")


def main():
    ap = argparse.ArgumentParser(description="Build a Flipper dolphin animation from a GIF or PNG frames.")
    ap.add_argument("source", nargs="?", type=Path, help="a .gif file or a folder of PNG frames")
    ap.add_argument("--name", help="animation name (folder + manifest entry)")
    ap.add_argument("--out", type=Path, default=Path("dolphin"), help="output dolphin dir (default: ./dolphin)")
    ap.add_argument("--width", type=int, help="force frame width (default: fit within 128x64)")
    ap.add_argument("--height", type=int, help="force frame height (default: fit within 128x64)")
    ap.add_argument("--max-frames", type=int, help="thin a long GIF down to at most N frames")
    ap.add_argument("--color", action="store_true",
                    help="full-color mode: native 320x170 RGB565 pack (frames.bin + info.txt) for the color-animation FAP, instead of the mono dolphin format")
    ap.add_argument("--frame-rate", type=int, default=2, help="fps (default 2, matches stock dolphin)")
    ap.add_argument("--duration", type=int, default=3600, help="passive duration (default 3600)")
    ap.add_argument("--active-frames", type=int, default=0, help="trailing frames that are 'active' (default 0)")
    ap.add_argument("--active-cycles", type=int, default=0)
    ap.add_argument("--active-cooldown", type=int, default=0)
    ap.add_argument("--min-butthurt", type=int, default=0)
    ap.add_argument("--max-butthurt", type=int, default=14)
    ap.add_argument("--min-level", type=int, default=1)
    ap.add_argument("--max-level", type=int, default=3)
    ap.add_argument("--weight", type=int, default=3, help="selection weight (default 3)")
    ap.add_argument("--selftest", action="store_true", help="run internal checks and exit")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        return
    if not args.source or not args.name:
        ap.error("SOURCE and --name are required (or use --selftest)")

    w, h, n = build(args.source, args.out, args.name, args)
    print(f"Animation '{args.name}': {n} frames, {w}x{h} -> {args.out}/{args.name}/")
    if args.color:
        mb = n * w * h * 2 / 1_000_000
        print(f"Full-color pack: frames.bin ({mb:.1f} MB, RGB565) + info.txt")
        print(f"Copy {args.out}/{args.name}/ to the SD (played by the color-animation FAP).")
    else:
        print(f"Updated {args.out}/manifest.txt")
        print(f"Copy the contents of {args.out}/ to the SD card's /ext/dolphin/ and reboot.")


if __name__ == "__main__":
    main()
