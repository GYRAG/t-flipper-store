#!/usr/bin/env python3
"""Generate store/catalog.json from store/apps.json + the FAPs built by buildFap.sh.

Split of responsibilities:
  store/apps.json    curated by a human (id, dir, category, author, description)
  this script        machine facts (name, size, firmware-compat tag, fap path)

The `fw` tag needs no new logic. buildFap.sh already dumps every FAP's undefined
symbols to <build>/fap/<dir>/undef_syms.txt, and check_fap_symbols.py already
answers "is every one of these in the API table". Point that at UPSTREAM's
firmware_api.c (937 symbols) instead of ours (1372): all resolved -> "stock"
(runs on the original port firmware too), anything missing -> "modified"
(needs our build).

Usage:
    python tools/make_catalog.py [--stock-api upstream_firmware_api.c]
    python tools/make_catalog.py --selftest

With no --stock-api it reads upstream's table straight out of git
(git show origin/main:...), which is what you want for a local run.
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from check_fap_symbols import elf_gnu_hash  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API_REL = "components/flipper_application/flipper_application/firmware_api.c"


class _AnyEnum:
    """Stand-in for FlipperAppType.* — any attribute access returns a marker."""

    def __getattr__(self, name):
        return "FlipperAppType." + name


def fam_name(app_dir):
    """Display name from application.fam — the string that shows on the device.

    A .fam is plain Python, so exec it with a stub App() rather than regexing it
    (same trick as tools/fap_app_info.py). First non-plugin App is the main app.
    """
    fam_path = os.path.join(app_dir, "application.fam")
    apps = []
    g = {
        "App": lambda **kw: apps.append(kw),
        "Lib": lambda **kw: kw,
        "ExtFile": lambda **kw: kw,
        "FlipperAppType": _AnyEnum(),
        "app_manifest_path": fam_path,
    }
    with open(fam_path, encoding="utf-8") as f:
        exec(compile(f.read(), fam_path, "exec"), g)  # .fam is trusted repo source
    for a in apps:
        if "PLUGIN" not in str(a.get("apptype", "")):
            return a.get("name") or os.path.basename(app_dir)
    raise SystemExit(fam_path + ": no non-plugin App() found")


def read_stock_api(path=None):
    """Upstream's firmware_api.c text — from a file, or straight out of git."""
    if path:
        with open(path, encoding="utf-8") as f:
            return f.read()
    return subprocess.run(
        ["git", "show", "origin/main:" + API_REL],
        cwd=ROOT, capture_output=True, text=True, check=True,
    ).stdout


def api_hashes(src):
    """Every .hash value in a firmware_api.c."""
    return {int(m, 16) for m in re.findall(r"\.hash\s*=\s*(0x[0-9a-fA-F]+)", src)}


def build_catalog(apps_json, build_dir, stock_hashes, apps_root):
    """Merge curated metadata with facts read off the built FAPs."""
    with open(apps_json, encoding="utf-8") as f:
        curated = json.load(f)

    out = []
    for app in curated["apps"]:
        app_id, dir_name = app["id"], app["dir"]
        app_dir = os.path.join(apps_root, dir_name)
        fap = os.path.join(build_dir, "fap", app_id + ".fap")
        syms = os.path.join(build_dir, "fap", dir_name, "undef_syms.txt")

        # A silently-missing app is worse than a red build.
        if not os.path.isfile(fap):
            raise SystemExit(
                app_id + ": no FAP at " + fap + " — did buildFap.sh run for " + dir_name + "?"
            )
        if not os.path.isfile(syms):
            raise SystemExit(app_id + ": no undef_syms.txt at " + syms + " — stale build?")

        with open(syms, encoding="utf-8") as f:
            needed = [s.strip() for s in f if s.strip()]
        missing = [s for s in needed if elf_gnu_hash(s) not in stock_hashes]

        entry = {
            "id": app_id,
            "name": fam_name(app_dir),
            "category": app["category"],
            "author": app.get("author", ""),
            "description": app.get("description", ""),
            "size": os.path.getsize(fap),
            "fw": "modified" if missing else "stock",
            "fap": "faps/" + app_id + ".fap",
        }
        # Credit link to where the app originally came from. Optional: apps
        # written here (coloranim) have no upstream to point at.
        if app.get("url"):
            entry["url"] = app["url"]
        out.append(entry)
        note = (
            "needs " + str(len(missing)) + " symbol(s) beyond stock"
            if missing else "resolves on stock fw"
        )
        print("  %-16s %7d B  %-8s (%s)" % (app_id, entry["size"], entry["fw"], note))

    return {
        "name": "T-Embed App Store",
        "note": "fw: 'stock' = runs on the original creator's port firmware too; "
                "'modified' = needs our firmware (extra exported symbols).",
        "apps": out,
    }


def selftest():
    """The fw tag is the only non-trivial logic here — check it resolves both ways."""
    import tempfile

    with tempfile.TemporaryDirectory() as td:
        apps_root = os.path.join(td, "apps")
        build = os.path.join(td, "build")
        fapdir = os.path.join(build, "fap")
        for d in ("plain", "fancy"):
            os.makedirs(os.path.join(apps_root, d))
            os.makedirs(os.path.join(fapdir, d))
            with open(os.path.join(apps_root, d, "application.fam"), "w") as f:
                f.write(
                    'App(appid="%s", name="Name Of %s", apptype=FlipperAppType.EXTERNAL)\n'
                    % (d, d)
                )
            with open(os.path.join(fapdir, d + ".fap"), "wb") as f:
                f.write(b"\0" * 100)

        # 'plain' uses only a symbol the stock table has; 'fancy' also needs one it lacks.
        with open(os.path.join(fapdir, "plain", "undef_syms.txt"), "w") as f:
            f.write("furi_delay_ms\n")
        with open(os.path.join(fapdir, "fancy", "undef_syms.txt"), "w") as f:
            f.write("furi_delay_ms\nnumber_input_set_header_text\n")

        stock = {elf_gnu_hash("furi_delay_ms")}
        apps_json = os.path.join(td, "apps.json")
        with open(apps_json, "w") as f:
            json.dump({"apps": [
                {"id": "plain", "dir": "plain", "category": "Tools", "description": "d"},
                {"id": "fancy", "dir": "fancy", "category": "Tools", "description": "d",
                 "url": "https://example.invalid/app"},
            ]}, f)

        cat = build_catalog(apps_json, build, stock, apps_root)
        by_id = {a["id"]: a for a in cat["apps"]}
        assert by_id["plain"]["fw"] == "stock", by_id["plain"]
        assert by_id["fancy"]["fw"] == "modified", by_id["fancy"]
        assert by_id["plain"]["name"] == "Name Of plain", by_id["plain"]
        assert by_id["fancy"]["size"] == 100, by_id["fancy"]
        assert by_id["fancy"]["fap"] == "faps/fancy.fap", by_id["fancy"]

        # catalog.txt must describe exactly the same apps as catalog.json, in the
        # same order — the device and the browser have to agree on the store.
        rows = [ln for ln in catalog_text(cat).splitlines() if ln and not ln.startswith("#")]
        assert len(rows) == len(cat["apps"]), rows
        for row, app in zip(rows, cat["apps"]):
            got = row.split("|")
            assert len(got) == 6, got
            assert got[0] == app["id"], got
            assert got[1] == app["name"], got
            assert got[2] == app["category"], got
            assert int(got[3]) == app["size"], got
            assert got[4] == app["fw"], got
            assert got[5] == app["fap"], got
        # A '|' in a curated field would silently shift every later column.
        bad = {"apps": [dict(cat["apps"][0], name="Pipe|Name")]}
        try:
            catalog_text(bad)
        except SystemExit:
            pass
        else:
            raise AssertionError("catalog_text accepted a '|' in a field")
        # Credit URL passes through when present, and is absent (not empty) when not.
        assert by_id["fancy"]["url"] == "https://example.invalid/app", by_id["fancy"]
        assert "url" not in by_id["plain"], by_id["plain"]

        # A listed app with no FAP must fail the build, not vanish from the catalog.
        os.remove(os.path.join(fapdir, "plain.fap"))
        try:
            build_catalog(apps_json, build, stock, apps_root)
        except SystemExit:
            pass
        else:
            raise AssertionError("missing FAP did not fail the build")

        # api_hashes must read the real table format.
        assert api_hashes("  {.hash = 0x0a1b2c3d, .name = \"x\"},\n") == {0x0a1b2c3d}

    print("selftest OK")


TEXT_HEADER = (
    "# T-Embed app catalog, flat form for the on-device WiFi store.\n"
    "# GENERATED by tools/make_catalog.py alongside catalog.json — do not edit.\n"
    "# The firmware has no JSON parser, and adding one to read a file we generate\n"
    "# ourselves would be backwards. This is parsed with strtok_r on-device, the\n"
    "# same way wlan_sd_update.c already parses files.txt.\n"
    "# id|name|category|size|fw|path\n"
)


def catalog_text(cat):
    """Flat catalog for the device, derived from the same dict as catalog.json.

    Both outputs come from one build_catalog() pass, so they cannot disagree
    about what is in the store.

    '|' is the separator, so it must not appear in a field. Only name/category
    are human-written (via apps.json); reject rather than emit a line the device
    would mis-split into the wrong columns.
    """
    lines = [TEXT_HEADER]
    for app in cat["apps"]:
        row = [
            app["id"],
            app["name"],
            app["category"],
            str(app["size"]),
            app["fw"],
            app["fap"],
        ]
        for field in row:
            if "|" in field or "\n" in field:
                raise SystemExit(
                    "catalog.txt: '|' or newline in field %r (app %r) — rename it in "
                    "store/apps.json" % (field, app["id"])
                )
        lines.append("|".join(row))
    return "\n".join(lines) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--apps", default=os.path.join(ROOT, "store", "apps.json"))
    p.add_argument("--build-dir", default=os.path.join(ROOT, "build_t_embed"))
    p.add_argument("--apps-root", default=os.path.join(ROOT, "applications_user"))
    p.add_argument("--stock-api", help="upstream firmware_api.c (default: git show origin/main:...)")
    p.add_argument("--out", default=os.path.join(ROOT, "store", "catalog.json"))
    p.add_argument("--out-text", default=os.path.join(ROOT, "store", "catalog.txt"))
    p.add_argument("--selftest", action="store_true")
    a = p.parse_args()

    if a.selftest:
        selftest()
        return 0

    stock = api_hashes(read_stock_api(a.stock_api))
    print("stock API table: %d symbols" % len(stock))

    cat = build_catalog(a.apps, a.build_dir, stock, a.apps_root)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(cat, f, indent=2)
        f.write("\n")
    print("wrote %s (%d apps)" % (a.out, len(cat["apps"])))

    # Newline-only: the device splits on '\n', so CRLF would leave a stray '\r'
    # on the last field of every line.
    with open(a.out_text, "w", encoding="utf-8", newline="\n") as f:
        f.write(catalog_text(cat))
    print("wrote %s (%d apps)" % (a.out_text, len(cat["apps"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
