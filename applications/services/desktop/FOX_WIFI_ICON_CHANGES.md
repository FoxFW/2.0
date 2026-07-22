# Status bar WiFi icon - what changed (round 5)

One thing to drop into your firmware fork this round:

1. **`desktop_src/desktop.c`** and **`desktop_src/desktop_i.h`** - drop
   both back into your firmware fork's desktop app source, replacing
   what you have. Neither icon PNG changed this round - no need to
   re-copy `assets_icons_StatusBar/`.

## What changed since round 4

Round 4 fixed how the icon looks. This round fixes a real bug in how
its *state* gets decided.

**The bug:** the icon has only ever reflected a one-byte flag file
(`/ext/apps_data/fox_esp32/wifi_status.txt`) written by
`fox_esp32_commander`'s WiFi menu whenever the user explicitly
connects/disconnects/forgets a network. That file never expires and
nothing else ever touched it - so if the ESP32 got reflashed, or
unplugged from the GPIO header entirely, while no Fox app happened to
be open to notice and write "0", the icon just kept showing whatever
it said last, indefinitely. Reported exactly this: reflashed the
ESP32, closed every Fox app, and the status bar kept saying connected.

**The fix - `desktop_wifi_recheck_thread()`.** A new background thread
(started once in `desktop_alloc()`, runs for the life of the service)
that, roughly once a minute, tries to confirm the ESP32's real WiFi
state over UART and rewrites the same flag file the icon already
polls. Key details:

- **Never fights a running Fox app for the UART.**
  `furi_hal_serial_control_acquire()` returns `NULL` if something else
  already owns the port - the thread treats that as "can't check right
  now" and just backs off, trying again next minute. In practice this
  means the probe is a no-op almost the entire time any Fox app is
  actually open (which already keeps the file fresh itself); it only
  meaningfully does anything while the user is sitting on the idle
  desktop with nothing running - exactly the situation the flag file
  had no other way to self-correct in.
- **Uses `[WIFI/STATUS]`, not anything Fox-specific.** This is the
  shared FlipperHTTP-compatible bracket-command grammar this project's
  own ESP32 firmware also implements. Sending this instead of, say, a
  self-ID challenge means an ESP32 running a *different* compatible
  firmware (FlipperHTTP itself, for instance) that also has its own
  WiFi connection still reads as "connected" here too - the icon
  represents "does whatever's on the GPIO header right now have WiFi,"
  not "is this project's own firmware specifically flashed."
- **Checks AP-association, not full internet reachability.**
  `[WIFI/STATUS]` answers "is the ESP32 connected to a network," not
  "does that network actually have working internet" - a captive
  portal or a dead WAN link would still read as connected. A stricter
  check would need the ESP32 to actually fetch a URL (the
  generate_204-style trick Android/Chrome use for their own
  connectivity checks) - deliberately not done here, since that's a
  real DNS+TCP+HTTP round trip over the air, multiple seconds worst
  case, for what's meant to be a cheap once-a-minute background poll.
  Happy to add a heavier "true internet" mode later if it turns out to
  matter in practice - flagging the tradeoff here rather than picking
  silently.
- **Fails safe toward "disconnected."** If neither pin pair (13/14,
  then 15/16) gives back a real `true`/`false` within
  `FOX_ESP32_WIFI_PROBE_TIMEOUT_MS` (500ms each), the thread writes
  "0" rather than leaving the old value in place. This is the actual
  fix for the reported bug - unplugged/reflashed/unresponsive now
  self-corrects within about a minute instead of staying stuck forever.
- **Dedicated thread, not another `FuriTimer`.** A UART probe can
  legitimately take up to ~500ms per pin pair (1s worst case trying
  both). Every `FuriTimer` in the system shares one timer-service
  thread, so parking that thread for up to a second once a minute would
  have stalled every other timer's callback system-wide for that whole
  window. A dedicated thread that just sleeps between checks avoids
  that entirely and costs nothing while idle.

**Everything else is unchanged** - the 2-second flag-file poll that
actually redraws the icon, the self-correcting
`gui_view_port_send_to_front()` positioning fix from round 3, and the
thin-line disconnected glyph from round 4.

## Worth a quick check after flashing

Reflash the ESP32 (or just unplug it from GPIO) while sitting on the
idle desktop with no Fox app open, and confirm the status bar icon
flips to disconnected within about a minute on its own, with no app
needing to be opened to force it.
