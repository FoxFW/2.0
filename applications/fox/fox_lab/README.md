# FoxLAB

A one-button remote switch for the ESP32's FoxLAB WiFi portal.

## What it does

After the standard "is an ESP32 running Fox ESP32 Firmware connected"
check every Fox ESP32 app does (same UART probe/settings screen as
Fox Portal, FoxHub, etc.), FoxLAB shows a single screen:

```
      FoxLAB Launcher
    Connect PC to WiFi:
  FoxLAB  (Pass: 88888888)
       192.168.4.1
        [OK] Start
```

(the IP line is drawn bold/large - it's the actual address to browse
to once connected, not a fixed literal; it'll track wherever the
ESP32's AP IP ends up.)

Pressing OK sends `[LAB/START]` (or `[LAB/STOP]` if it's already
running) to the ESP32 and flips the button's label once the ESP32
acknowledges it. On (re)connect, FoxLAB asks the ESP32 with
`[LAB/STATUS]` so the button always reflects reality, including if the
portal was left running from a previous session.

This app is *only* the switch. Starting the portal stands up a WiFi AP
(`FoxLAB`) and a web server entirely on the ESP32 - closing FoxLAB (or
disconnecting the Flipper from the ESP32 altogether) does not stop it.
It keeps serving until FoxLAB sends `[LAB/STOP]` or the ESP32 loses
power. Deliberately **not** persisted: a power cycle always comes back
to "off, waiting for a command", it never auto-resumes on boot even if
the portal was left running (same as `fox_csi.cpp`'s web UI toggle).

If the connected ESP32 is an S2 board, FoxLAB is compiled out there
(see "Not supported on S2" below) - `[LAB/STATUS]` comes back as an
error instead of a status, and FoxLAB shows a short "not supported"
screen instead of the Launcher.

## ESP32 side

Implemented in `Fox_ESP32_FW/fox_lab.cpp` (`FoxLab::begin/loop/
handleCommand`), wired into `Fox_ESP32_FW.ino` alongside the other
subsystems. The page it serves is `Fox_ESP32_FW/foxlab_page.h` - the
provided `foxfw-lab.html` embedded verbatim as a PROGMEM string and
served at every path (`/` and 404s alike) via a plain `WebServer` on
port 80. See that file's header comment if the source page ever needs
regenerating.

**Known limitation:** `foxfw-lab.html` references a local
`assets/js/flipper-rpc.js` and a `cdnjs.cloudflare.com` script, plus
links to sibling pages (`index.html`, `wallpaper-painter.html`, ...)
that live on FOX_WEB, not on the ESP32. None of those are bundled here
- only the one HTML file the app was asked to push. The page's static
layout renders fine over FoxLAB; anything that depends on those other
files won't load unless the client also has real internet access.
(Replacing the page's WebSerial/USB transport with a WiFi path through
the ESP32 itself is tracked separately and is not part of this app.)

### Not supported on S2

`config.h`'s `FOX_HAS_LAB` gates the whole feature off on the
ESP32-S2 specifically - it doesn't have enough free RAM left over
(after WiFi + the rest of the firmware) to reliably run the AP + web
server + the ~100KB embedded page. On S2, `fox_lab.cpp` compiles to a
stub: `[LAB/STATUS]`, `[LAB/START]` and `[LAB/STOP]` all just reply
`[LAB/<CMD>/ERROR]NOLAB` instead of doing anything.

This is a single, easily-reversible switch: flip `FOX_HAS_LAB` back to
`1` for S2 in `config.h` (matching the `#if defined
(CONFIG_IDF_TARGET_ESP32S2)` pattern already used there for
`FOX_HAS_BLE`) if a future change frees up enough RAM for it to fit.
Every other supported chip (classic/C3/C5/C6/S3) keeps full
functionality regardless of this flag.

## Flipper side

Standard Fox ESP32 app skeleton (`app.h`/`main.c`/`esp_at.c`/
`fox_splash.c`/`connect_settings.c`/`message_view.c`, all following the
same shape as Fox Portal's), plus one new file: `launcher_view.c`, the
single-screen view described above. `message_view.c` also carries the
"FoxLAB Unsupported" screen shown when `[LAB/STATUS]` comes back with
`NOLAB` (S2 boards - see above).
