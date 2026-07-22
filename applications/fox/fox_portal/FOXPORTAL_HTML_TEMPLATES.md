# Fox Portal's start.html / finish.html - how they work

Short answer to "where do these live before compile, and can I edit
them": they're never compiled into the ESP32 firmware at all. They're
plain HTML files that live on the **Flipper's own SD card**, at
`/ext/apps_data/fox_portal/start.html` and
`/ext/apps_data/fox_portal/finish.html`. Open them with qFlipper's
file browser, Flipper Mobile App, or by pulling the SD card into a
computer, edit them like any other HTML file, save, done - no firmware
rebuild, no recompile, nothing ESP32-side to touch.

## How they actually reach the ESP32

Every time you press **Start** in the Fox Portal app, it:

1. Reads `start.html` and `finish.html` fresh off the SD card (so
   whatever's saved there right now is always what gets used - no
   stale cached copy from a previous run).
2. Flattens each one to a single line (real newlines become spaces -
   HTML doesn't care about whitespace, and the transfer to the ESP32
   is one line at a time over serial) and sends it with a
   `WIFIFOXPORTAL:SETPAGE:START:`/`SETPAGE:THANKS:` command.
3. The ESP32 saves what it receives to its own flash (LittleFS) and
   serves it to anyone who joins the portal's WiFi network.

If either file is empty or missing, the ESP32 falls back to a plain
generated page built from whatever Title/Intro/Note text and field
names are currently configured (see the main README's FoxPortal
section) - so an empty SD card folder still works, just with a plain
default look instead of your own design.

## Two included starter templates

This delivery includes a working `start.html` and `finish.html` in
`apps_data/fox_portal/` (same folder shape as the SD card) as a
starting point, styled to match the ESP32's own generated default
page (dark background, orange accent) - copy both into
`/ext/apps_data/fox_portal/` on your SD card and edit freely.

They use two example fields, `name` and `phone` - matching each
`<input name="...">` in `start.html` to a `{{FIELD:...}}` token in
`finish.html`, which gets replaced with whatever the visitor actually
typed into that field when the thank-you page renders.

**If you rename, add, or remove fields:** the `<input name="...">`
attributes in your `start.html` need to match whatever field names are
currently configured in the app's own **Edit Input Names** screen -
those are what's used for the CSV log's column headers and for
matching a submitted form value to a field. `{{FIELD:name}}` tokens in
`finish.html` follow the same names. Mismatched names don't crash
anything (a submission just logs an empty cell for whatever didn't
match), but won't capture what you're expecting.

## Size limit

Each file is capped at 1024 bytes once flattened to one line (a
DNS/serial-transfer safety limit, not an HTML feature limit) - the two
included templates are 953 and 699 bytes, leaving some room to adjust
copy/styling before you'd hit that ceiling. If you need more room than
that, trim the CSS rather than the copy - it's the bigger chunk of
both files.
