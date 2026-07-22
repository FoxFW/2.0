# Fox ESP32 Detector

A small, single-purpose diagnostic tool: it tells you whether anything
that speaks AT commands is connected to the Flipper's GPIO header, and
if so, exactly which pins and baud rate it's on and what firmware it
identifies as. Nothing else - no menu, no configuration, no persistent
state.

It exists because building [Fox Chameleon](../fox_chameleon) involved
several rounds of "is it the wiring, the baud rate, the firmware, or the
pin pair?" that this tool answers in one pass instead of by elimination
across separate test builds.

## What it does

Press **Scan**. A live progress screen appears immediately - a
text-based bar (`################----`), a `combo/total` counter, and
the specific pins/baud currently being tried, updated before every
single attempt (18 combinations: 2 pin pairs times 9 baud rates).

Getting that progress screen to actually appear live, rather than
freezing for the whole scan, took a real architecture change, not a
tweak. Flipper's GUI does redraw on its own thread whenever content
changes - that part was always true - but calling `widget_reset()` and
friends directly from inside a button's own callback while that
callback is still running a long blocking loop doesn't reliably surface
those changes on screen. The scanning itself now runs on its own
`FuriThread`, which never touches `Widget`/`Gui` directly; it only
writes to a few shared fields (behind a mutex) and then calls
`view_dispatcher_send_custom_event()`. The main thread's registered
custom event callback - which, per Flipper's own documentation, always
runs "on the thread that invoked `view_dispatcher_run`" - is what
actually redraws. That guarantee is what makes this safe, not an
assumption.

For each baud, up to four things are tried in order, stopping at the
first that gets a response:

1. **GPS (NMEA), passive.** No command sent at all - GPS modules speak
   NMEA sentences (`$GPGGA,...`) continuously and unprompted, so this
   tier just listens briefly and checks whether a line starts with `$`.
2. **ESP-AT.** Plain `AT`, watching for `OK`. If it answers, a follow-up
   `AT+GMR` grabs its version banner.
3. **Known non-AT firmware signatures.** Currently just Bruce: `info` is
   a real, documented Bruce serial command (confirmed against
   [BruceDevices/firmware's own wiki](https://github.com/BruceDevices/firmware/wiki/Serial)),
   sent purely to elicit a response, and the reply is checked for the
   word "bruce". This table (`signatures[]` in `main.c`) is meant to
   grow.
4. **Generic fallback.** If nothing above matched, one last try sending
   `?` (a common "show help" convention), requiring the *same* response
   twice in a row before it counts - see "Known limitations" for why.

Whatever text comes back in tiers 3 or 4 is also checked against
`known_tags[]`, a small, deliberately separate table mapping raw
substrings to clean names (`{"bruce", "Bruce firmware"}` today). The
idea is that as real hardware responses get confirmed - e.g. if a
Marauder build turns out to identify itself with a short tag like
"MRDR" - adding `{"MRDR", "Marauder"}` there is enough to have it
reported cleanly everywhere that tag shows up, tier 3 or tier 4, probed
for by name or not, without touching the detection logic itself.

If truly nothing answers on any of the 18 combinations across all four
tiers, it says so plainly rather than leaving you looking at a specific
"no ESP32 detected" message wondering whether it's the pins, the baud,
or the wiring. A worst-case full scan (nothing connected at all) takes
on the order of 30 seconds.

## Known limitations

- **"Unidentified" results require the probe to repeat identically
  before being reported at all.** A single capture can't tell a real
  command-response apart from a device that's continuously outputting
  something on its own (a boot log, a crash loop) getting caught
  mid-transmission, or a baud mismatch scrambling readable bytes into
  other readable-looking garbage rather than obvious noise. Tier 3 now
  sends `?` twice and only reports a result if both captures match
  byte-for-byte - real firmware replying to a command is repeatable;
  ambient or misread traffic isn't. If a scan reports different
  "Unidentified" text (or even a different baud) on repeated runs, that
  inconsistency is itself informative: it means nothing is actually
  listening and replying at any tested baud, and what's being seen is
  something transmitting on its own, most likely at a baud outside the
  9 tested here. The most direct way to settle that is to bypass the
  Flipper entirely and connect the module straight to a computer over
  USB with a serial terminal, where far more baud rates can be tried
  quickly and the raw text is visible without any capture window at
  all.
- **Whether the screen actually redraws live during the scan hasn't
  been confirmed on real hardware.** `run_scan()` runs the whole scan on
  the same thread that's handling the button press, updating the screen
  by changing widget content between attempts (`app_render_progress()`
  below) rather than on a separate worker thread. The expectation is
  that Flipper's GUI service redraws the active screen asynchronously
  whenever content changes, regardless of what the app's own thread is
  doing at that moment - the same assumption Fox Chameleon's SD-dump
  progress display relies on - but neither app has had this specifically
  confirmed working on a physical Flipper yet. The progress bar and
  combo counter (`Scanning 6/18`, `[######----------]`) exist
  specifically so this is easy to check: if they visibly count up while
  scanning, the assumption holds. If the screen instead sits frozen for
  up to ~25 seconds and then jumps straight to a final result, it
  doesn't, and the fix would be moving the scan onto its own thread the
  way `fox_chameleon`'s `esp_at.c` already does for its own UART work -
  a real change, not just a tweak, so worth confirming which case this
  is before assuming either way.
- **15/16 (LPUART)'s bus-enum name.** Same caveat as in Fox Chameleon:
  `FuriHalBusLPUART1` follows the naming pattern of the confirmed-working
  `FuriHalBusUSART1` but wasn't independently verified against a fetched
  SDK reference. 13/14 is unaffected either way.
- **Untested on hardware.** Written the same way as the rest of this
  project - against documented and, where possible, source-verified
  APIs - but not yet compiled or run. Build with `ufbt` and treat the
  first run as a bring-up session.
- **Bruce's exact "info" output isn't confirmed, only that the command
  itself is real.** `info` is genuinely documented in Bruce's own wiki
  as a valid serial command; what it actually prints back was not found
  in that documentation and so wasn't independently verified the way
  the command's existence was. Matching on "bruce" appearing anywhere in
  the response is a reasonable bet - these firmwares tend to
  self-identify prominently - but if a real Bruce module gets reported
  as "Unidentified" instead of "Bruce" during testing, this is the
  assumption to revisit first, most likely by adjusting `match` for the
  Bruce entry in `signatures[]` once the real output text is known.
- **Marauder has no entry in `signatures[]` at all.** Its documented
  serial commands (`scanap`, `sniffdeauth`, `channel`, ...) are all
  WiFi-attack actions - none of them double as a safe, side-effect-free
  "identify yourself" the way Bruce's `info` does, so no command+match
  pair could be written with the same confidence as Bruce's entry. A
  Marauder module will still be reported by the generic fallback tier
  (labeled "Unidentified", with its raw response shown) rather than
  missed entirely, but adding a real `signatures[]` entry for it - once
  a suitable command and its actual output are confirmed against real
  hardware - would make that identification precise instead of generic.
- **The generic "`?`" fallback probe is a convention guess, not a
  confirmed protocol.** Plenty of simple serial CLIs treat `?` as a
  help request, but this wasn't verified against any specific firmware
  - it's a reasonable single extra attempt before concluding a baud is
  silent, not a guaranteed way to elicit a response.

## Detecting hardware beyond serial-speaking modules

Everything above works because it targets modules that speak a text
protocol over UART. That covers a lot of real GPIO addons (any
ESP32/ESP8266 running AT-style or CLI-style firmware, GPS breakouts),
but "detect everything plugged into GPIO" spans several genuinely
different hardware layers, each needing its own, separate detection
approach rather than an extension of this one:

- **Any ESP32 chip, regardless of what firmware (if any) is flashed.**
  Espressif's ROM bootloader - baked into the silicon, present even on
  a blank chip - has its own always-available serial protocol (SLIP
  framing, a specific sync sequence), which is what tools like
  [flipperzero-esp-flasher](https://github.com/0xchocolate/flipperzero-esp-flasher)
  use to talk to a chip before any application firmware is even
  running. This is a real, promising path to genuinely firmware-agnostic
  ESP32 detection, but it needs two more wires than this app currently
  asks for (GPIO0 and EN/RST, to force the chip into bootloader mode) and
  a chunk of the esptool sync protocol implemented - a real feature,
  not a small addition.
- **I2C/SPI devices** (many camera and sensor modules, including
  ESP32-CAM-style boards like
  [Flipper-Zero-Camera-Suite](https://github.com/CodyTolene/Flipper-Zero-Camera-Suite),
  which - worth noting - runs custom firmware over UART for its actual
  camera protocol, not ESP-AT, so it wouldn't be reachable by this app's
  probes even if serial-only detection were extended). Flipper does
  expose an I2C bus on GPIO, reachable through `furi_hal_i2c`, and a
  bus scan (try every address 0x00-0x7F, see what ACKs) is a real,
  different detection tier that could be added - but it's a separate
  bus entirely from UART, with its own code path from the ground up.
- **Modules with no protocol at all** - a "gaming module" that's just
  buttons or a joystick wired to GPIO pins has nothing to detect in a
  protocol sense. Reading raw pin states could tell you *a* pin is
  being pulled high or low, but not what's attached or what it means,
  since there's no standard to interpret it against.
- **Flipper's own built-in IR hardware** isn't a GPIO module at all -
  it's dedicated hardware on the Flipper itself, already known to be
  present, nothing to detect.

None of this is implemented here. It's scoped out deliberately rather
than silently skipped, so the gap is visible rather than discovered
later.

## Part of the Fox apps suite

Built as a standalone companion to `fox_chameleon`, not a dependency of
it - useful on its own for bringing up any ESP-AT project, or for
answering "what's actually plugged into GPIO right now" in general.
