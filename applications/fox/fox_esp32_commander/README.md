# Fox ESP32 Commander

The Fox-styled companion app for Fox ESP32 Firmware's broader
WiFi/BLE/HTTP/scripting toolkit - the "remade Marauder app" project.
Round 3: the project is now feature-complete for this app - round 1's
boot sequence and shell, round 2's full menu tree (WiFi, Bluetooth,
Scripts, Settings), and round 3's four previously-deferred screens
(custom-SSID beacon spam, targeted deauth, a manual pin/baud override
screen, and a general HTTP client). See "What's next" below for what's
left outside this app entirely (RFID/Sub-GHz/IR, gated on firmware-side
driver work).

## What this app does

1. **Splash screen.** `fox_64x64.png` (see the placeholder note below)
   shows centered on screen for 2 seconds, then dissolves to blank over
   2/3 of a second, then moves on. Flipper's display is 1-bit monochrome
   with no alpha blending, so there's no true "fade" available - this
   dissolves the icon in small randomly-ordered blocks instead, which
   reads as a fade on the real screen. See `fox_splash.c/.h`.
2. **ESP32 + Fox ESP32 Firmware detection.** After the splash, probes
   both standard pin pairs (13/14 USART, 15/16 LPUART) at 115200 baud,
   sending `info` and checking for the exact `Fox ESP32 Firmware`
   self-identification line back - not just a bare AT/OK, so this
   specifically confirms the right firmware is on the other end, the
   same probe `fox_esp32_detector` uses. Immediately after a successful
   match, one more quick round trip - `CAPS`, expecting `HASBLE:1` or
   `HASBLE:0` - decides whether the Bluetooth and Tag Detection menus
   get shown at all (see "BLE capability detection" below).
3. **Retry screen.** If neither pin pair answers correctly: a blocking
   screen reading "Fox ESP32 Firmware required on ESP32 connected via
   GPIO." with two buttons - **Retry** (re-runs the same two-pin-pair
   auto-probe) and **Settings** (opens Connect Settings, below, for a
   manual override). Kept to 3 short lines on purpose - a longer first
   draft overlapped the Retry button on real hardware; see "Fixed,
   found via a real fbt build" for that history.
4. **Connect Settings** (new this round) - reached only from the Retry
   screen's **Settings** button. An arrow-adjust screen mirroring
   `fox_chameleon`'s own Pins/Baud/Start pattern: Up/Down moves between
   the **Pins** row (13/14 USART or 15/16 LPUART), the **Baud** row
   (115200 today - the row is built to cycle if a second option is ever
   added, but there's only the one for now), and a **Retry** row;
   Left/Right adjusts Pins/Baud; OK on Retry re-probes with exactly the
   selected pair, instead of looping over both pairs the way the
   automatic Retry button does. Success lands on the main menu; failure
   returns to the Retry screen so the user can try a different
   combination or give up.
5. **Main menu: WiFi / Bluetooth / Tag Detection / Scripts / Settings /
   Terminal.** Bluetooth and Tag Detection are only shown on a board
   that actually has BLE - see "BLE capability detection" below. FoxChat
   and FoxPortal moved out to their own standalone apps (`fox_foxchat`,
   `fox_portal`) rather than living as menus here - see those apps' own
   READMEs.

### BLE capability detection

ESP32-S2 boards have no Bluetooth radio at all, so `FOX_HAS_BLE` is
compiled out on that target (see the firmware's own `config.h`) and
every `BLE*`/`BLESPAM:*` command on that build just replies
`ERROR:Incompatible ESP32-S2 Module has no BLE`. Rather than showing
Bluetooth and Tag Detection on every board and letting the user find
that out the hard way after picking an action, this app asks up front:
right after the `info` self-ID check succeeds, it sends `CAPS` and
expects back exactly `HASBLE:1` or `HASBLE:0` (see
`fox_esp32_firmware.ino`'s handler). A confirmed `HASBLE:0` hides both
menus from the main menu entirely; anything else - `HASBLE:1`, no
reply at all within 500ms, or an older pre-CAPS firmware build that
doesn't recognize the command - defaults to showing them (fail open),
so a slow/garbled round trip can never hide a perfectly working menu.
The result is cached in `app->has_ble` for the whole session; it's
re-checked on every fresh detection (app launch, Retry, or a Connect
Settings re-probe), not just once.

### WiFi menu

- **Connect** - scans (`WIFISCANAP`) and merges with saved networks
  (`[WIFI/LIST]`) into one list, saved networks first, shown as long
  rounded Fox-styled buttons (same visual language as `fox_chameleon`'s
  Chameleon-scan-result list) with SSID plus signal/security/saved
  status. Selecting a **saved** network reconnects immediately
  (`[WIFI/CONNECT]{"ssid":"..."}`, the firmware supplies the stored
  password). Selecting an **unsaved** network opens a password entry
  screen, then connects and auto-saves for next time
  (`[WIFI/CONNECT]` + `[WIFI/SAVE]`) - open networks work too, just
  submit an empty password.
- **My IP** - `[WIFI/IP]`, result shown in Terminal.
- **HTTP Request** (new this round) - a small GET/POST submenu.
  **GET** is one text entry (the URL), sends `[GET]<url>`. **POST** is
  two sequential text entries (URL, then body), sends
  `[POST/HTTP]{"url":"...","payload":"..."}`. Both show the raw
  response in Terminal. Deliberately scoped to GET/POST only - no
  PUT/DELETE/custom headers - Terminal already reaches every
  `[BRACKET]` command by hand for anything more exotic than this.
- **Recon** - Scan APs, Scan Stations, Select AP (opens the network
  list in "target" mode - only in-range APs, feeds `WIFISELECT:AP:<n>`),
  Signal Monitor (`WIFISIGMON`, 10s), Packet Count (`WIFIPACKETCOUNT`,
  5s), **Wardrive** (new this round - `WIFIWARDRIVE`, appends each
  result to a dated CSV log on the SD card at
  `/ext/apps_data/fox_esp32/wardrive/wardrive_<YYYY-MM-DD>.csv`, columns
  `bssid,ssid,latitude,longitude,rssi,channel,encryption_type,timestamp`
  - one file per calendar day, appended to across runs), **Packet
  Capture** (new this round - `WIFIPCAP`, a 15s raw capture hopping
  channels 1-11, reassembled into a real `.pcap` file at
  `/ext/apps_data/fox_esp32/captures/capture_<timestamp>.pcap` - opens
  directly in Wireshark).
- **Attacks** - Select AP (same target-select flow as Recon), **Select
  Station** (new this round - a second rounded-box list, this one over
  `WIFISCANSTA` results: MAC + RSSI, no ssid/secure/saved concept),
  **Deauth (Broadcast)** (the original untargeted `WIFIATTACK:DEAUTH`),
  **Deauth (Targeted)** (new this round - `WIFIATTACK:DEAUTH:<mac>`
  against whichever station Select Station picked; if none has been
  picked yet, logs a message asking for that first rather than sending
  a malformed command), **Beacon Spam (Random)** (renamed from plain
  "Beacon Spam" - `WIFIATTACK:BEACON:RANDOM:8`), **Beacon Spam
  (Custom)** (new this round - one text entry for a comma-separated
  SSID list, sends `WIFIATTACK:BEACON:<list>` verbatim), and **Probe
  Flood**. All gated firmware-side by the Attacks setting (see Settings
  below) - `ERROR:DISABLED` comes back unchanged and is shown as-is if
  attacks are off.

### Bluetooth menu

BLE spam only - iOS / Windows / Samsung / Android / All
(`BLESPAM:*`). The BLE bridge to a Chameleon Ultra
(`BLEINIT`/`BLESCAN`/`BLECONN`/etc.) is `fox_chameleon`'s job, not this
app's - see `app.h`'s top comment. Only shown on a board that actually
has BLE - see "BLE capability detection" above.

### Tag Detection menu (new this round)

Three items, each a bounded BLE scan reported straight to Terminal
(same log-and-done shape as Recon's Signal Monitor/Packet Count - there
is nothing to select afterward, so there's no dedicated results
screen):

- **AirTag / Find My** - `BLETAGSCAN:FINDMY`, matches Apple's Find My
  "offline finding" broadcast. This is what an AirTag sends, but so
  does every other Find My network accessory (Chipolo, Pebblebee,
  etc.) - the protocol itself can't tell them apart, only "Apple Find
  My network" from anything else. Includes a decoded battery-status
  hint (full/medium/low/verylow) where available.
- **Samsung SmartTag** - `BLETAGSCAN:SMARTTAG`.
- **Tile** - `BLETAGSCAN:TILE`.

This is a snapshot, not a stalking detector - a result means "a tag of
this type is broadcasting nearby right now", not "this tag has been
following you". See the firmware's own README (`ble_tags.cpp` section)
for the full reasoning and the exact signature each one matches. Not
available on an ESP32-S2 board (no BLE radio) - see "BLE capability
detection" below for how this app knows to hide this menu (and
Bluetooth) entirely on that hardware, rather than showing them and
letting each action fail with `ERROR:Incompatible ESP32-S2 Module has
no BLE`.

### Scripts menu

A dynamic list of saved scripts (`SCRIPTLIST`) plus a trailing **New
Script** item. Selecting an existing script opens a small actions menu
- **Run** (`SCRIPTRUN`, output streamed to Terminal), **Show**
(`SCRIPTSHOW`), **Delete** (`SCRIPTDEL`) - rather than running
immediately, so Delete isn't one accidental OK press away. **New
Script** is two sequential text-entry screens (name, then source),
ending in `SCRIPTSAVE:<name>:<source>`.

Real on-device typing for a whole FoxScript one-liner is genuinely slow
on Flipper's keyboard - the text input buffer this screen uses is sized
well under the firmware's 2048-byte `SCRIPT_SOURCE_MAX`, on purpose.
Scripts longer than that still fully work, they just need to arrive via
Terminal's `SCRIPTSAVE:` line instead of this screen.

### Settings

A single arrow-adjustable row - **Attacks: ON/OFF** - same rounded-row
visual pattern as `fox_chameleon`'s Settings screen, minus the "Start"
action row that one needed (that screen was configuring a connection
that didn't exist yet; this one only exists after the ESP32 link is
already up). Toggling applies immediately over serial
(`SETTINGS:ATTACKS:ON/OFF`) - no separate save step. The screen also
queries the firmware's real persisted value (`SETTINGS`) every time
it's entered, so it never drifts from what's actually stored in the
firmware's NVS.

This is the main-menu Settings screen - not to be confused with
**Connect Settings** (item 4 above), the separate pin/baud override
screen reachable only from the not-detected Retry screen.

### Terminal

Unchanged since round 1 - raw command access, and also where almost
every action above reports its result. One consistent place to look,
and it doubles as a running session log, rather than each action
needing its own bespoke result screen.

## Menu architecture (for anyone extending this further)

One `Submenu` widget (`app->submenu`) is reused across every list-style
menu (Main, Wifi, WifiRecon, WifiAttacks, WifiHttp, Chat, Bluetooth,
Scripts, ScriptActions) - each context resets/repopulates it before switching to
the shared Menu view, the same "one Submenu, many logical menus" shape
`fox_chameleon`'s own Connection submenu already established in this
project. `app->menu_context` tracks which logical menu is showing;
`app_menu_item_callback()` (in `main.c`, declared in `app.h`) is the one
`SubmenuItemCallback` every `render_*_menu()` passes to
`submenu_add_item()` - it dispatches on `menu_context` to the right
module's `*_select()` function. `app->menu_return_context` tracks which
menu Back should land on from a non-menu view (Terminal, the network
list, the station list, text input) - captured centrally in
`app_render_log()` and `app_show_text_input()`, right before switching
away from a menu. `menu_parent_context()` maps every sub-context back to
its parent for Back-from-menu: `WifiRecon`/`WifiAttacks`/`WifiHttp` all
return to `Wifi`; `ScriptActions` returns to `Scripts`; everything else
returns to `Main`.

Two screens sit outside that Submenu system entirely, since they're each
reached from exactly one place rather than being a list of selectable
items: **Connect Settings** (`connect_settings.c/.h`) is a three-row
arrow-adjust `View`, reachable only from the not-detected Retry screen's
"Settings" button, mirroring `fox_chameleon`'s Pins/Baud/Start pattern.
The main-menu **Settings** screen (`settings_view.c/.h`) is the same
arrow-adjust pattern with just one row.

Per-feature logic lives in its own module, mirroring the firmware's own
`.c`/`.h`-per-feature convention: `wifi_menu.c/.h` (Connect, My IP,
Recon, Attacks - including both rounded-list screens, network and
station), `http_menu.c/.h` (the GET/POST HTTP client, kept separate
from `wifi_menu.c` since that file was already the largest in the app),
`foxchat_menu.c/.h` (Post Message/Read Messages/Bot Settings),
`ble_menu.c/.h`, `scripts_menu.c/.h`, `settings_view.c/.h`,
`connect_settings.c/.h`. `main.c` owns the app lifecycle, the Terminal
view, the main menu, the pin/baud option tables (with accessor functions
`app_pin_option_count()`/`app_pin_option_label()`/
`app_baud_option_count()`/`app_baud_option_value()` for
`connect_settings.c` to display/cycle through them without owning a
duplicate copy), and the shared helpers every module calls (`app_log`,
`app_render_log`, `app_expect_line`, `app_switch_to_menu`,
`app_show_text_input`, `app_menu_item_callback`,
`app_probe_uart_selected` - all declared in `app.h` since C has no
"friend module" concept short of a shared header).

## The fox_64x64.png placeholder - needs your real file

You pasted/attached a fox logo in chat, and I can see it there, but it
isn't reaching me as an actual file - only files that land in my
uploads folder are accessible to my tools, and this one didn't show up
there. `images/fox_64x64.png` in this zip is a plain placeholder
(a bordered box, two triangle "ears", and the text "FOX") just so the
app actually builds and the splash mechanism is demonstrable - not a
guess at your real artwork.

To get your actual logo in: attach `fox_64x64.png` as a proper file
upload (not pasted inline) and I'll drop it straight into
`images/fox_64x64.png` - no other code changes needed, `fox_splash`
just draws whatever `Icon` it's handed. It needs to be a 1-bit (pure
black/white, no greyscale/anti-aliasing) 64x64 PNG for Flipper's icon
compiler - if the source file isn't already 1-bit I'll convert it, but
sharp black/white source art will look cleaner than a converted photo
or gradient-heavy image.

## Making the splash a theme across your Fox apps

Per your flag: `fox_splash.c` and `fox_splash.h` are written to be
copy-pasted into any Fox app with no changes to those two files
themselves. To reuse it in another app:

1. Copy `fox_splash.c`/`fox_splash.h` into that app's source folder.
2. Drop that app's own 64x64 1-bit PNG into its `images/` folder (e.g.
   `images/fox_64x64.png` - same filename is fine across apps, or
   different, doesn't matter).
3. Add `fap_icon_assets="images"` to that app's `application.fam` (if
   it doesn't already have it).
4. `#include "<appid>_icons.h"` - fbt names the generated header after
   that app's own `appid` from `application.fam`, e.g.
   `fox_chameleon_icons.h` for `fox_chameleon`, **not** a generic
   `"icons.h"` (that was a real bug in this app's first round - see
   "Fixed, found via a real fbt build" below - fixed to
   `fox_esp32_commander_icons.h`, matching this app's own appid). Gets
   you the `Icon` symbol - `I_fox_64x64` if you keep the same filename -
   to pass into `fox_splash_alloc()`.
5. Call `fox_splash_alloc()` in `app_alloc()`, add its view to the
   `ViewDispatcher`, switch to it, call `fox_splash_start()` - see this
   app's `main.c` (`app_alloc()`, `fox_splash_done_cb`,
   `custom_event_callback`) for the full reference wiring, including
   the thread-safety note below.

## Setup

Same as `fox_chameleon` and `fox_esp32_detector`: open in the Flipper
Zero Q app or build with ufbt, flash to the Flipper, wire an ESP32
running Fox ESP32 Firmware to pins 13/14 or 15/16.

## A note on the splash-then-probe ordering (thread safety)

`fox_splash`'s timer runs on FreeRTOS's shared timer service thread,
not this app's own thread. Its `done_cb` therefore does nothing but
post a custom event via `view_dispatcher_send_custom_event()` - a fast,
thread-safe handoff - rather than running the actual (blocking, up to
~3s worst case) UART probe directly from the timer callback, which
would block a thread shared with other system timers. The real probe
only ever runs from `custom_event_callback()`, which the
`ViewDispatcher` runs on this app's own thread - the same thread every
other blocking action in this app (WiFi scans/connects, recon, attacks,
BLE spam, scripts, HTTP requests) runs from too, since all of them are
triggered from a `Submenu`/`View` input callback, never a timer
callback.

## What's next

- RFID/Sub-GHz/IR menus - the firmware side now has real driver code
  (PN532/CC1101/IRremoteESP8266, see the firmware's own README), but
  this app doesn't have dedicated screens for them yet; reachable via
  Terminal in the meantime.
- Whatever real-hardware bugs show up once this round's additions get
  their first real fbt build and a run on actual hardware - see "Known
  rough edges" below for what's genuinely untested.

## Known rough edges going in

- **Tag Detection, Wardrive, and Packet Capture are all newly written
  and not yet run on real hardware.** Tag Detection just parses/logs
  `TAG:` lines (low risk, same shape as every other scan-and-log
  action already in this app). Wardrive's CSV writer and Packet
  Capture's pcap reassembly (`run_wardrive()`/`run_pcap_capture()` in
  `wifi_menu.c`) are more involved - real file I/O, a hand-rolled line
  parser, and (for pcap) hex-decoding a chunked binary stream back into
  a standard file format - worth a real test of each against actual
  WiFi activity before trusting the exported files. If a `.pcap` won't
  open in Wireshark, checking the frame lengths declared in each
  `PCAPPKT:` line against how many `PCAPDATA:` bytes actually arrived
  is the first thing to check.
- **Everything added this round is newly written and not yet compiled
  or run on real hardware** - the station list, Connect Settings, and
  the HTTP client screens and custom-SSID beacon spam are all first
  builds. Round 2's WiFi/Bluetooth/Scripts/Settings menu tree had
  its own first real fbt build in progress as of this round (the two
  bugs it turned up - see below - are fixed); round 1's
  splash/detect/Retry/Terminal is the most battle-tested part of this
  app.
- **`AP:`/`STA:` line parsing uses `sscanf` with a fixed field order** -
  matches `wifi_recon.cpp`'s print functions exactly as of this round,
  but if either print format ever changes, these parsers need to change
  with them (silently parsing zero results is the likely failure mode,
  not a crash).
- **Nothing sent to the firmware is JSON-escaped** - not SSIDs/passwords
  in `[WIFI/CONNECT]`/`[WIFI/SAVE]`, and now also not URLs/bodies in
  `[POST/HTTP]`. Matches the firmware's own hand-rolled JSON extractor,
  which doesn't handle escape sequences either (see the firmware's
  README). A value containing a literal `"` or `\` will break the
  request. Real limitation, not expected to come up often for typical
  URLs/SSIDs, not fixed this round.
- **Custom-SSID beacon spam and targeted deauth do no client-side
  validation** beyond "field isn't empty" - a malformed SSID list or an
  unreachable-looking MAC is sent to the firmware as typed, which will
  report its own error back to Terminal rather than this screen
  catching it earlier.
- **The Connect Settings Baud row only ever offers 115200** - the row
  is built generically (cycles through a `baud_options[]` table the
  same way Pins cycles through `pin_options[]`) so a second baud is a
  one-line addition to that table in `main.c` if ever needed, but
  Left/Right on that row is currently a no-op.
- **Both rounded-box list screens (network and station) re-scan every
  time they're opened** - no caching between visits. Simple and always
  fresh, at the cost of a few seconds each time either screen opens.
- Carried over from round 1, still true: `fox_splash`'s block-dissolve
  fade and the FuriTimer + custom-event handoff are both written against
  documented Flipper patterns but hadn't been run on real hardware as of
  that round.

### Fixed, found via a real fbt build

- `main.c` originally had `#include "icons.h"`, which fails with
  `fatal error: icons.h: No such file or directory` - external apps
  using `fap_icon_assets` get a header named after their own `appid`
  instead. Fixed to `#include "fox_esp32_commander_icons.h"`.
- `fox_splash.h` hit an `error: unterminated comment` from a build-tool
  file-write issue in this project's own environment (not a real code
  problem - the intended file content was always well-formed) that
  silently truncated the file on disk mid-comment. Rewritten and
  independently re-verified from the delivered zip.
- The not-detected message text originally ran long enough on real
  hardware to overlap the Retry button docked at the bottom of the
  widget (`widget_add_string_multiline_element` doesn't reserve space
  for a button added after it). Shortened to the current 3-line
  message, with a code comment flagging why it has to stay short.

### Caught and fixed before ever reaching a build (self-review)

- Every `submenu_add_item()` call across the menu modules originally
  passed `NULL, NULL` as a placeholder callback+context - since
  `Submenu` requires a real non-NULL callback to fire selection events
  at all, this would have made every menu item in the app silently do
  nothing on OK. Fixed by introducing the shared
  `app_menu_item_callback()` dispatcher and wiring every
  `submenu_add_item()` call through it.
- `app_show_text_input()` originally called
  `text_input_set_result_callback()` a second time with a placeholder
  `NULL`, which would have silently broken the one real callback wired
  in `app_alloc()` the first time any text-input screen was used.
  Removed the redundant call.
- `wifi_menu.c` called `atoi()` without `#include <stdlib.h>`.
