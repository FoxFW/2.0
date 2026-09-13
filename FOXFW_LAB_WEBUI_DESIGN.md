# FoxFW Lab — Browser WebUI for FOX_WEB (Design Doc, v2)

> **Superseded.** What actually shipped ("Option B") skipped the FOX_WEB/
> Web-Serial staging plan below entirely and went straight to §9's "v2,
> still" ESP32-hosted goal: the page (`foxlab_src/foxfw-lab.html`, compiled
> into `Fox_ESP32_FW/foxlab_page.h`) is served directly by the ESP32 over
> its own `FoxLAB` WiFi AP, reached at `http://192.168.4.1/` — no FOX_WEB
> hosting, no Web Serial, no USB at all. Both tabs described below still
> exist (FLIPPER: file browser/screen mirror/terminal via a real Flipper
> RPC session tunneled over WiFi through the ESP32; ESP32: the same WiFi/
> BLE/GPS/IR/SubGHz/AI command console), just reached over WiFi/WebSocket
> instead of Web Serial. See the READMEs in this repo and in `Fox_ESP32_FW`
> ("FoxLAB" sections) for the current, accurate description; treat
> everything below as historical design-process context, not current
> architecture.

Design only, no code written yet. Supersedes the v1 draft of this file,
which only covered the ESP32 command-console half of the ask. Revised
after reading the user's actual notes file, which describes something much
bigger: a FoxFW equivalent of `lab.flipper.net`, built to feel like the
desktop app [FlipperUI](https://github.com/fuckmaz/FlipperUI), with a
second tab for the ESP32 side, eventually also hosted directly off the
ESP32 itself. This version replaces §4-6 of the old draft with the full
two-tab scope and folds the old ESP32 content in as Part 2.

## 1. What this is

A new FOX_WEB page (`foxfw-lab.html`) with two tabs at the top of a left
sidebar: **FLIPPER** and **ESP32**. Whichever device is connected drives
which tab's options are active; the other tab stays visible but greyed out
until its device connects too. Both tabs talk to hardware directly from
the browser over Web Serial — no server, no app to install, matching how
`flasher.html`/`fox-esp32-flasher.html`/the wallpaper pages already work
today.

- **FLIPPER tab** — a browser reimplementation of what FlipperUI and
  `lab.flipper.net` do natively/on-device: file browsing, signal/app
  libraries, live screen mirroring, a CLI terminal, and a SubGHz
  viewer/trimmer. This is the larger, previously-missing half of this
  design.
- **ESP32 tab** — the WiFi/BLE/GPS/IR/SubGHz/AI command console already
  designed in the old v1 draft, largely unchanged (§7 below).

## 2. Research this revision is based on

No internet access existed for the v1 draft; it does now, so this version
is grounded in the actual references the user linked rather than guessed:

- **[FlipperUI](https://github.com/fuckmaz/FlipperUI)** (the explicit "this
  is basically exactly what I want" reference) — a Tauri v2 desktop app
  (React frontend, Rust backend doing Flipper RPC over USB serial or BLE).
  Feature list per its README: a connection manager (port detection,
  reconnect, battery/storage/latency readouts); a full file explorer for
  `/ext` and `/int` (upload/download/rename/delete/mkdir, breadcrumbs,
  cancelable transfers, drag-and-drop); "Libraries" — indexed, searchable,
  metadata-parsed views over SubGHz/IR/NFC/RFID/BadUSB files and installed
  `.fap` apps, with recursive scans and offline browsing after a scan;
  live screen mirroring with D-pad input, screenshots, and GIF recording;
  a serial CLI terminal; a dashboard tying device stats and library counts
  together; and a settings GUI for things normally only editable by hand-
  editing files on the SD card. Two facts worth designing around: "USB
  serial is the primary transport and supports the full feature set - BLE
  supports file and library workflows, but the CLI is USB-only," and only
  one app can hold the serial port at a time (close qFlipper first) - Web
  Serial has this same one-owner-at-a-time constraint natively, so it
  isn't a new problem to solve, just one to surface clearly in the UI.
  Licensed PolyForm Noncommercial - nothing here proposes copying any of
  its code (none was fetched, only the README), just matching its feature
  set and look with FOX_WEB's own implementation.
- **The actual screenshot** (`FlipperUI_v0.4.0_Dashboard.png`, viewed
  directly, not just described): dark near-black background, card-based
  panels with soft rounded corners, a narrow icon-only left sidebar (one
  icon per section, active one boxed in orange), a top status bar (app
  name, connection-type toggle, device-name pill with a green "connected"
  dot, latency, battery %, storage %, power, search), a hero card with the
  device's own icon/name/ID and a "Detailed view" button, a stat-card grid
  below (Battery/Storage/Settings-shortcuts), and a "Libraries" row of
  icon+count pairs (SUB-GHZ 180, INFRARED 16, NFC 42, RFID 2, BADUSB 543,
  APPS 362). The accent orange used throughout is close enough to FOX_WEB's
  own `--orange:#e8750a` that this look is a natural fit, not a fight - see
  §4.
- **[lab.flipper.net](https://lab.flipper.net)** — confirmed live and
  reachable; its actual page content wasn't extractable through this
  session's fetch tool (client-rendered), so its exact layout couldn't be
  inspected directly, but the user's own description of what to keep
  (Files, CLI, NFC tools, Paint, Pulse Plotter) and drop (the Apps menu)
  is specific enough to design from directly - see §6.
- **[Flipper-SubGHz-Viewer-Trimmer](https://siroxcw.github.io/Flipper-SubGHz-Viewer-Trimmer/)**
  — confirmed live: a fully client-side, no-backend tool. Load a `.sub`
  RAW file from local disk, it plots an RSSI/timing waveform you can zoom
  and scroll, select a start/end trim range, and export the trimmed
  result as a new `.sub` file. No device connection required at all - see
  §8.
- **[ESP32-Bit-Pirate](https://github.com/geo-tp/ESP32-Bit-Pirate)** (MIT
  licensed) — validates two design choices already leaning this way: it
  ships a browser-based Web Serial CLI terminal hosted on its own GitHub
  Pages site (exactly the pattern proposed for the ESP32 tab), *and*
  separately supports the ESP32 hosting its own web CLI over its own
  WiFi for cable-free phone/tablet access - confirming that's a normal,
  achievable pattern for this class of device, not something unusual to
  ask of Fox_ESP32_FW eventually (see §9, still v2/out of scope for now).

## 3. Two tabs, one shell

Left sidebar, top to bottom: a **FLIPPER**/**ESP32** tab switcher, then
that tab's section icons below it (Dashboard, Files, Libraries, Screen,
Terminal, SubGHz Viewer, Settings for Flipper; Dashboard, WiFi, BLE, GPS,
IR, SubGHz, AI, Settings for ESP32). Top bar: FoxFW Lab wordmark, a single
**Connect** button, then whichever status pills apply to the connected
device(s) (battery/storage for Flipper, WiFi-icon/connected-AP for ESP32).

**Connect behavior**: one Web Serial `requestPort()` prompt, like the
existing FOX_WEB pages already do. After the port opens, probe which
device answered (Flipper RPC handshake vs. Fox_ESP32_FW's own `HELLO`-
style line) and switch to that tab automatically. The *other* tab stays
visible with its icons present but disabled/greyed, with its own
independent Connect affordance, so a second `requestPort()` call can pair
it in too.

**Simultaneous Flipper + ESP32 (Flipper connected via USB, ESP32 reachable
through the Flipper's own GPIO UART passthrough) is explicitly *not*
designed here** - it depends on whether FoxHub's existing GPIO-UART relay
to the ESP32 can be driven by an arbitrary external client sending raw
ESP32 command lines through the Flipper's own CLI passthrough, which
wasn't confirmed this pass. Flagging as an open question for whoever
implements this, not assumed to work. Until confirmed, each tab gets its
own independent Web Serial connection to its own device's own USB port.

## 4. Visual design - reuse what FOX_WEB already has

Per `PROJECT_HANDOVER.md` §5.1, FOX_WEB already has two visually distinct
page "generations." The one used by `flasher.html`/`fap-compiler.html`/
`fox-esp32-flasher.html` (`--orange:#e8750a`, `--dark:#0d0d0d`, `.topbar`,
`.mc` method cards, dual progress bars) is already close to FlipperUI's
actual look - dark background, orange accent, card-based panels, a top
status bar. Build FoxFW Lab on that existing palette/component family
rather than inventing a third one: reuse `.topbar` for the top bar, `.mc`-
style cards for the dashboard grid, and add one new piece this family
doesn't have yet - a narrow icon-only left sidebar with an active-item
orange box highlight, matching the screenshot's layout.

## 5. FLIPPER tab - feature scope (new in this revision)

All of the below talks to the Flipper over the same Web Serial + protobuf
RPC pattern `wallpaper-painter.html`/`wallpaper-showcase.html` already use
successfully - reuse that `FlipperRPC` JS object (or, better, finally pull
it into the shared `assets/js/flipper-rpc.js` module those two pages'
own maintenance-risk note in `PROJECT_HANDOVER.md` §5.3 already flagged as
overdue, so FoxFW Lab doesn't become a *third* independent copy). Load
`.proto` files from `assets/proto/` with `keepCase: true` - this is the
single most important gotcha in this whole codebase per the handover doc,
and it'll bite here too if missed.

- **Dashboard**: connected device name/ID, firmware version, battery %,
  SD usage, `/int` usage, and library item counts (populated once the
  Libraries scan below has run). Modeled directly on the FlipperUI
  screenshot's layout.
- **File Explorer**: browse `/ext` and `/int`, upload/download/rename/
  delete/mkdir, breadcrumb navigation, progress reporting on transfers.
  RPC writes should default to `/ext` per FlipperUI's own noted gotcha
  that some firmware paths reject `/int` writes - test what FoxFW
  specifically allows before assuming parity.
- **Libraries**: recursive scan of SubGHz/IR/NFC/RFID/BadUSB files and
  installed `.fap` apps, with per-type counts and basic metadata (name,
  frequency/protocol for SubGHz, etc.), cached client-side so re-opening
  the tab doesn't require a full rescan every time.
- **Screen**: live 128x64 mirror over RPC, D-pad/button input relay,
  PNG screenshot export. GIF recording is a nice-to-have, not required
  for a first pass.
- **Terminal**: a CLI pane over the Flipper's serial CLI, streaming
  output with command history - same one-serial-port-at-a-time caveat
  as FlipperUI's, worth a visible "close qFlipper first" hint in the UI
  since it's a real, easy-to-hit gotcha.
- **Settings**: a GUI front-end for FoxFW-specific settings that
  currently require editing files by hand (matches FlipperUI's own
  "Flipper settings" shortcuts card in the screenshot - Desktop Keybinds,
  Mainmenu Apps, and FoxFW's own equivalents like `Fox.cfg`/menu theme).

## 6. FLIPPER tab - the lab.flipper.net-style landing page

A dedicated top-of-tab info page (not just the Dashboard above), matching
what the user described wanting kept from the real lab.flipper.net: an
overview of the connected Flipper plus Files, CLI, NFC tools, and Paint as
first-class linked sections (Pulse Plotter included per the user's "not
sure what it is but we might as well include it"). The one deliberate
change from lab.flipper.net's own page: where it offers an install button
for stock firmware, FoxFW Lab's version checks the connected device's
firmware version against FOX_WEB's own `directory.json` SDK index (already
published and used by the FAP Compiler per `PROJECT_HANDOVER.md` §2.7) and
either shows "up to date" or an "Update available" button that links to
FOX_WEB's own `flasher.html`, not a generic Apps menu.

## 7. ESP32 tab - feature scope (carried over from v1, unchanged)

A command palette for Fox_ESP32_FW's WiFi/BLE/GPS/IR/SubGHz/AI commands,
same design as before:

### Why a browser page, not (yet) an ESP32-hosted WebUI

GhostESP's ESP32 firmware hosts its own AP + HTTP server (see
`GHOSTESP_GAP_ANALYSIS.md` §2.3). Fox_ESP32_FW's `http_bridge.cpp` only
runs a `WebSocketsClient` - outbound to a relay, nothing served locally.
For this tab, direct Web Serial from the browser to the ESP32's own
USB-serial port (same API `fox-esp32-flasher.html` already uses) - no
Flipper needs to be attached, no ESP32 firmware changes needed. Adding a
local HTTP server to Fox_ESP32_FW itself is real firmware work with its
own flash-size cost and its own "unauthenticated OTA"-class review to do
properly - see §9 for why that's still the eventual goal, just not v1.

### Command/response framing

- Commands are newline-terminated plain text (`"WIFISCANAP\n"`).
- Responses are line-oriented; several subsystems use a `WHATEVERDONE`
  sentinel line to mark completion (`SCANDONE`, `SNIFFDONE`,
  `WARDRIVEDONE`, `TAGSCANDONE:<count>`, `ATTACKDONE`).
- Errors come back as `ERROR` or `ERROR:<REASON>`.

### Commands to expose (`wifi_recon.cpp`, `wifi_attack.cpp`, `ble_tags.cpp`,
`ble_attack.cpp`, `gps.cpp`, `ir.cpp`, `subghz.cpp`, `gemini.cpp`)

- **WiFi**: Scan APs/Stations/All, Wardrive, Ping/ARP scan, MAC track,
  port scan, channel set, packet count/sigmon, PCAP capture.
- **WiFi Attack**: Deauth (target or broadcast), Beacon spam, Probe flood,
  Rickroll, Bad-packet, CSA, Sleep, SAE, Quiet-time, Karma - gated behind
  a confirmation dialog and whatever `refuseIfDisabled()` checks
  firmware-side (exact setting name still needs confirming).
- **BLE**: Tag scan (FINDMY/SMARTTAG/TILE/FLOCK/META), Spoof AirTag,
  BLE spam.
- **GPS**: Fix/Sat/Lat/Lon/Alt/Date readout, Track, POI mark/start/end.
- **IR**: Send, Receive, TV-B-Gone.
- **SubGHz** (external CC1101-on-ESP32, independent of the Flipper's own
  radio): Init, set frequency, RX, TX.
- **AI**: Ask (Gemini relay), cooldown-gated.

### Still needs confirming before this gets built

- The exact attack-arm setting name/values.
- Whether PCAP capture responses are binary or line-text on the wire.
- Confirmation-dialog copy per command.

## 8. SubGHz Viewer/Trimmer - embed, don't rebuild

The linked tool is fully client-side and file-based - no device connection
needed at all, so the cleanest integration is to lift its load/plot/trim/
export logic directly into a panel under the FLIPPER tab's SubGHz library
section (own license/attribution to confirm before copying any code, but
the mechanism itself - Canvas-based waveform plot, drag-to-select trim
range, re-serialize a trimmed `.sub`- is straightforward enough to
reimplement if the license doesn't permit reuse). Two integration points
worth adding beyond the standalone tool: a "Trim" action directly on a
SubGHz file already listed in the File Explorer/Libraries view (skip the
manual load-file step), and a "save trimmed copy back to the Flipper"
option using the same RPC write path the File Explorer uses.

## 9. ESP32-hosted version - the stated "ultimate goal," still v2

The user's notes are explicit that browser-hosted (via FOX_WEB) and
ESP32-hosted (served from the device's own AP, reached at its own IP) are
both wanted, not either/or. ESP32-Bit-Pirate (§2) is a working precedent
that a WiFi-hosted web CLI is achievable on comparable hardware. Sequencing
recommendation: build and stabilize the FOX_WEB/Web-Serial version first
(§3-8) since it needs zero Fox_ESP32_FW firmware changes and reuses proven
code, then treat "give Fox_ESP32_FW its own HTTP server" as its own scoped
follow-up project - not a v1.1 add-on - given the security-review need
already flagged in `GHOSTESP_GAP_ANALYSIS.md` §2.3/`PROJECT_HANDOVER.md`
§4.4 around GhostESP's own unauthenticated-OTA lesson. Once that server
exists, the same frontend code built for the ESP32 tab here should be
reusable nearly as-is, served locally instead of talking out to a browser
tab over Web Serial - the command set and framing don't change, only the
transport does.

## 10. Suggested build order

Roughly biggest-bang-for-effort first, given the size of this ask:

1. Shared shell: sidebar, tab switcher, Connect flow, visual style (§3-4).
2. FLIPPER tab: Dashboard + Terminal + File Explorer (§5) - the three
   pieces with the most directly-reusable existing FOX_WEB RPC code.
3. ESP32 tab in full (§7) - already fully scoped from v1, independent of
   the Flipper-tab work.
4. FLIPPER tab: Libraries + SubGHz Viewer/Trimmer embed (§5, §8).
5. FLIPPER tab: Screen mirror + Settings GUI (§5) - the two most
   RPC-surface-heavy, least-precedented pieces in this codebase; save
   for last once the RPC plumbing from steps 2-4 is proven solid.
6. lab.flipper.net-style landing/update-check page (§6).
7. ESP32-hosted server, once scoped as its own project (§9).
