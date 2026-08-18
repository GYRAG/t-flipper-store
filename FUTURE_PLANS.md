# Future Plans — T-Embed Port

Forward-looking plan for the store, FAP management, and firmware integration.
Companion docs: **HANDOFF.md** (how to build/flash + what exists), **APPSTORE_PLAN.md**
(store architecture + the RPC protocol facts), **BUGS.md** (open crashes).
Backup repo: private **github.com/GYRAG/t-flipper** (branch → its `main`).

---

## 0. "Have we detached from the original port too much?" — the honest answer

**No — the fork is a compatible superset, not a divergence.** Here's the accounting of
every firmware change we made vs. the original creator's (Sor3nt) port:

| Change | Nature | Breaks stock? |
|---|---|---|
| API export audit (939→1372 symbols) | **Additive** — only adds table entries | No. Stock apps still work; only *new* apps that use the extra symbols need our fw. |
| `number_input` linear keypad | Behavior fix for the encoder | No — strictly better on this hardware |
| Thread pin (`LoaderApplications`→internal RAM) | Perf tweak | No |
| FAP icon cache (`/ext/.fap_icon_cache`) | Perf, additive file | No |
| WiFi RAM fix | **Came FROM upstream** (we merged `origin/main`) | No — we're *closer* to upstream now |

So the only real divergence is the **larger API table**, and it's purely additive. This is
why the store's **compat tag** works: `stock` apps run on both firmwares, `modified` apps
need ours. **Nothing we did prevents merging upstream updates** (we already did once,
conflict-free except one `.gitignore` line).

**To stay shareable and close to upstream:**
1. Periodically `git merge origin/main` (we're a superset, merges stay clean).
2. **Offer the API export audit upstream as a PR** — it benefits every port user and would
   erase the `stock`/`modified` split entirely if accepted.
3. Keep our additions in clearly-named files (`fap_icon_cache.*`, `compile_animation.py`,
   `coloranim/`, `store/`) so they're easy to review/port.
4. Credit the original creator prominently when sharing (this is their port + our layer).

**Bottom line:** share it. It's the original port + a compatible, additive "experience layer."

---

## 1. The vision — smooth "it just works" (the Steve Jobs bar)

Every feature below is judged against one question: **could a beginner who has never
touched a Flipper do this without reading anything?** Concretely that means:

- **One obvious path.** Flash firmware and install apps from **one website**, one button each.
- **No jargon on screen.** No "RPC", "FAP", "qFlipper bridge", "download mode" in the UI —
  translate to "Connect", "Install", "hold the button while plugging in."
- **No dead ends.** Every failure state has a plain next step ("Your device is asleep —
  press the button"), never a raw error.
- **It looks like the product.** Flipper pixel theme everywhere, consistent, playful.
- **Zero setup to *use*.** A freshly flashed device already has good apps on it (§4).

---

## 2. Store polish (the web app — `store/`)

Current: works locally, installs one app at a time over USB/Web-Serial. Next:

1. **Host it (GitHub Pages).** Turns the localhost demo into a real URL anyone can open.
   Highest-value next step. Pages serves `store/` static; `.fap`s hosted alongside.
2. **Unify flashing + installing.** We already have `interface.html` (firmware flasher) and
   `store/index.html` (app installer). Merge into **one site**: "1. Flash firmware  2. Get apps."
   Same Web-Serial tech, same theme. A beginner does both in one place.
3. **Catalog CI.** GitHub Action builds a curated app list for Xtensa with `buildFap.sh`,
   auto-writes `catalog.json` — set the `stock`/`modified` tag automatically from each FAP's
   undefined-symbol diff (stock 939 table vs full 1372). This is what lets the catalog grow.
4. **UX polish:** install progress %, "Install all", "Update available" (compare installed
   vs catalog by size/hash), clearer connect flow, remember the port, mobile-friendly.
5. **Robustness:** the `mkdir` should walk nested paths; verify the write (read back md5 via
   the RPC `md5sum` field); retry on transient errors.

---

## 3. Firmware-integrated store + on-device FAP manager (what you asked for)

The web store is the "from a computer" path. You also want it **built into the firmware,
available from the moment you install** — so the device is self-sufficient. Two parts:

### 3a. On-device FAP/app manager (no computer needed)
- Extend the existing **Apps browser** (`components/loader/loader_applications.c`, already
  has our icon cache) into a real manager: **launch, delete (long-press → confirm →
  `storage_remove`), rename, organize by category**. This is firmware work but small.
- Ship it as a first-class menu entry so it's obvious.

### 3b. On-device store over WiFi (the cordless dream)
- A built-in **"App Store" app** that uses the board's **native WiFi** to fetch `catalog.json`
  + `.fap`s from the hosted catalog (§2) and install them straight to `/ext/apps/`. No cable.
- Browsing on 128×64 is the UX challenge — keep it a simple scrolling list with the icon,
  name, size, and a one-click "Get". Reuse the color path (coloranim/DOOM) if a richer UI helps.
- Needs: on-device HTTPS GET (the WiFi apps already do HTTP), JSON parse, and the *local*
  storage write (trivial on-device — no RPC needed, it's the same device).

### 3c. "From the install" — a batteries-included flash
When someone flashes our firmware, they should land on a device that's **already useful**:
- The firmware build/SD image **preinstalls a curated set of working FAPs** (§4).
- The on-device manager (3a) and store (3b) are built in.
- First-boot shows a friendly "welcome / here's what you can do" (a coloranim or a short guide).

---

## 4. Bundle more FAP apps (a full device out of the box)

A fresh flash should feel full, not empty. Curate a **verified-working** set into the
default SD image / catalog. Candidates (you verify each *runs*, not just builds):
- **Ours:** resistors, net_calculator, wikiflip, coloranim (+ tetris & fortune once fixed).
- **Port's known-good** built-in apps (games, tools already in `applications/`).
- **New ports** from the encoder-friendly shortlist (clock, unit converter, TOTP, pomodoro,
  metronome-if-audio, and the BLE unknown-tracker detector as the signature app).
Keep the **compat tag** on each. The build pipeline (§2.3) produces them all.

---

## 5. Roadmap (suggested order)

1. **Host the store on GitHub Pages** (+ unify with the flasher). Small, unlocks real use.
2. **Catalog CI** — auto-build + auto-tag a growing app list.
3. **On-device FAP manager** (3a: delete/organize) — firmware, small, high daily value.
4. **Preinstalled-apps flash** (§4 + §3c) — makes a fresh device feel complete.
5. **On-device WiFi store** (3b) — the cordless finale.
6. **Fix tetris + fortune** (enable coredump, decode — see BUGS.md) — do alongside whenever.
7. **Upstream PR of the API audit** — collapses the stock/modified split, gives back.

---

## 6. Known constraints / gotchas to carry forward

- **Windows build/flash recipe** and all the env gotchas: see HANDOFF.md §2.
- **RPC-over-USB protocol facts** (DTR, single command_id, reply-only-on-final): APPSTORE_PLAN.md.
- **Panic backtraces go to UART0 pins, not USB** — crash debugging needs the coredump partition
  enabled (BUGS.md). Do this before hunting the tetris/fortune crashes.
- **`text_input`/`byte_input`** still have the encoder grid problem `number_input` had.
- **PSRAM task stacks** are slower than internal RAM (WiFi tradeoff) — pin latency-sensitive
  threads if something feels sluggish (we did this for the app browser).
