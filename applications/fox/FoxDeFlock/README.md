# FoxDeFlock

Flock Safety / ALPR camera detector for FoxFW2.0, using Fox_ESP32_FW as the
WiFi/BLE radio backend.

## Attribution

Based on [FlipDeFlock](https://github.com/ReconGrunt/FlipDeFlock) by
ReconGrunt, licensed GPL-3.0-or-later. This port reuses FlipDeFlock's
detection logic verbatim:

- `helpers/flock_db.c/h` — OUI/SSID confidence scoring, device classification.
- `helpers/flock_ble.c/h` — Flock external-battery BLE advert decoding.
- `helpers/oui_vendor.c/h` — vendor OUI lookup table (not yet wired into the
  UI in this first cut; kept for a future "show vendor" detail field).

Everything else — the app shell, scan/parse layer, and UI — is written
against Fox_ESP32_FW's own command set and is not a line-for-line port of
FlipDeFlock's Marauder-based `esp_link.c`/`esp_parser.c`/`marauder_scan.c` or
its Scene Manager app structure. See `foxdeflock_scan.c` for why: Fox ESP32
FW's `WIFISNIFF:PROBE`/`WIFISNIFF:BEACON`/`BLETAGSCAN:FLOCK` commands return
structured `key:value` lines, one MAC per line, so there's no free-text
scraping to reimplement.

The `_FoxEdition` naming convention used elsewhere in this repo isn't used
here — FlipDeFlock's own `TRADEMARK.md` asks for a different name (not a
suffixed variant) on modified redistributions, hence "FoxDeFlock".

## Requirements

A Flipper running FoxFW2.0, wired to an ESP32 running Fox_ESP32_FW
(UART, 115200 baud, standard Fox pinout). The app prompts for this on
launch and does nothing destructive if the ESP32 isn't present — it just
won't see any hits.

## What it detects

Rotates through three Fox ESP32 FW scan modes every ~8.5s: WiFi probe
requests, WiFi beacons, and `BLETAGSCAN:FLOCK`. Applies FlipDeFlock's
precision rule unchanged: a bare OUI-prefix match is never shown as a
detection on its own (those prefixes are shared silicon-vendor ranges and
would false-positive on ordinary hardware); it only counts once corroborated
by a Flock-shaped SSID, or when the SSID alone is enough.

## Known v1 limitations

Deliberately out of scope for this first cut (open to follow-up):

- **No Raven-vs-Falcon GATT disambiguation.** Fox ESP32 FW's
  `BLETAGSCAN:FLOCK` doesn't currently surface BLE service UUIDs, so
  `raven_gatt` is hardcoded `false` — BLE hits report as a generic Flock
  battery unit, never asserted as either model.
- **No GPS/mapping, no "guardian" continuous-monitoring mode, no
  deflock.me QR handoff, no plugin host (flasher/QR), no
  signature-database sync (`sig_db.c`).** FlipDeFlock's `watchscore.c`
  (BLE anti-stalking / deauth-flood / evil-twin fusion) is also not
  carried over. All of this pulls in significant additional Fox ESP32 FW
  surface area and UI; left for a later pass if wanted.
- **Marauder-firmware-flashing UI was intentionally dropped**, not
  ported — FoxDeFlock only ever talks to Fox_ESP32_FW.
