# SubRATT - Sub-GHz Randomized Attack

SubRATT is SubBrute's novelty sibling: instead of sequentially walking a
keyspace bit by bit (which only makes sense when the keyspace is small
enough to actually finish), SubRATT draws a fresh **random** candidate key
every transmission, forever, for protocols whose real keyspace (up to
2^64) could never be exhausted sequentially in a human lifetime. It is a
probability-driven novelty, not a guaranteed-eventual-success bruteforce -
the intro screen says so before anything runs.

## Protocols

| Protocol | Bits | Frequency | Notes |
|---|---|---|---|
| Honeywell Sec | 64 | 345MHz | US wireless security sensor frequency |
| Hollarm | 42 | 433MHz | |
| GangQi | 34 | 433MHz | |
| Magellan | 32 | 433MHz | |
| Intertechno_V3 | 32 | 433MHz | |
| Feron | 32 | 433MHz | |
| Doitrand | 37 | 433MHz | |
| Mastercode | 36 | 433MHz | |
| X10 | 32 (16 real) | 310MHz | See "X10's real keyspace" below |
| GateTX | 24 | 433MHz | Reused from SubBrute - included per user request even though technically bruteforce-able |
| Marantec24 | 24 | 868MHz | Reused from SubBrute |
| MegaCode | 24 | 315MHz | Reused from SubBrute |

### X10's real keyspace

X10's decoder validates that byte2 = ~byte3 and byte0 = ~byte1 of its
32-bit frame - only 16 of the nominal 32 bits carry real information.
SubRATT draws a 16-bit random value and expands it into the full 32-bit
on-air word with the required complement bytes filled in, rather than
wasting entropy drawing bits the decoder would reject anyway.

X10 is also the only protocol here upstream marks `SubGhzProtocolTypeDynamic`
despite being genuinely fixed-code (no rolling counter at all - see its
own decoder comments). SubRATT's own private vendored copy of X10 (see
`protocols/x10.c`) patches `.type` to `SubGhzProtocolTypeStatic`; this has
no effect on subghz_garage's or core firmware's own independent copies of
X10.

## The "leave it running" workflow

There is no way for a Flipper Zero to know a transmitted key actually
worked - these are one-way transmitters with no RF feedback channel, and
Flipper has no microphone to detect a physical reaction (a gate opening,
an alarm triggering). So SubRATT logs every key it transmits to the SD
card *as it runs* (`/ext/subghz/subratt/.session.rtk`), not just at the
end - flushed to disk every 25 keys. You can point a Flipper at a gate/
alarm/device, start an attack, walk away, and come back later: if it
worked at any point, the key that did it is already on the card even if
you never pressed anything.

When you stop an attack, you're asked how many keys were tested and
whether to save that log. Saving moves it into a permanent, named
`.rtk` file under `/ext/subghz/subratt/`; discarding deletes it.

**Open Saved Keys** on the start menu lets you browse those saved logs.
SubRATT reads the file's own header (frequency/preset/protocol/bit count),
counts how many keys it holds, and replays them one at a time,
sequentially and visibly (progress bar, not random) - so if one of them
is the real key, you can watch for it and see exactly which one it was.

## Notes for whoever builds this next

This app was hand-written (not generator-script-produced, unlike
SubBrute's most recent protocol expansion) directly against FoxFW2.0's
firmware headers, closely following SubBrute's own proven architecture
(private vendored protocol registry, SceneManager/ViewDispatcher,
FuriThread worker). It has **not been build-tested** in this environment
(no ARM/Flipper toolchain available here) - a first `./fbt` build pass is
the natural next step, and the usual suspects for a first-build fixup are:

- Exact Storage/Stream API names used for the live log
  (`storage_file_open`/`FSOM_OPEN_APPEND`, `stream_read_line`,
  `storage_common_rename`) - these are standard Flipper SDK calls but
  weren't directly exercised elsewhere in this codebase, unlike the
  FlipperFormat calls (all copied verbatim from SubBrute/subghz_garage,
  which are already proven working).
- `Widget` module element names (`widget_add_string_multiline_element`,
  `widget_add_button_element`) for the intro/stop-confirm screens.
- `Submenu` module for the flat protocol list on the start screen.
- Repeat count of 3 and 25-key log-flush interval are starting guesses,
  not measured - tune to taste once it's on real hardware.
