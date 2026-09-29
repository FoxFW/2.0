# Clemsa MV-12 remote — Flipper Zero app

A working clone of a **Clemsa MV-12 MASTERcode** garage remote (433.92 MHz, fixed
code) as a Flipper Zero FAP. Two door buttons, and an on-device editor for the 8
trinary DIP switches — so it adapts to any MV-12 without recompiling.

There is a **[Cardputer + CC1101 version](https://github.com/josedamianm/clemsa-mv12-cardputer)**
of the same thing, which also carries the tooling and the
[full investigation history](https://github.com/josedamianm/clemsa-mv12-cardputer/blob/main/HISTORY.md).

<!-- screenshot placeholder: add a photo of the Flipper running the app -->

## Why the built-in Sub-GHz app can't do this

The Flipper decodes Clemsa MasterCode perfectly and still won't open the door —
neither native emulation nor RAW replay, on any preset, frequency or power. That
has been an open complaint since 2023 in
[issue #3182](https://github.com/flipperdevices/flipperzero-firmware/issues/3182),
across many users and several remote models.

The MV-12 uses **"carrier over carrier"**, like a 38 kHz IR remote. Inside every
"carrier on" period it chops the 433.92 MHz carrier at a **~65 µs chip rate** — a
~7.7 kHz subcarrier. The receiver requires it. A solid gated carrier, which is
what both MasterCode emulation and RAW replay emit, is ignored.

Slicer-based receivers smooth the chopping away, so your capture looks like a
clean textbook 36-bit frame, your decode is correct, your bits verify — and the
second carrier was filtered out before it ever reached the `.sub`. Capturing at
AM650, which *is* wide enough to see it, produces what looks like noise and gets
discarded.

This app synthesises the waveform instead of replaying a capture, so the
subcarrier is present.

## The protocol

```
chip      = 65 us                       (subcarrier half-period)
bit slot  = 48 chips = 3120 us
bit '1'   = 16 subcarrier cycles, then 16 chips silence   (burst 2015 us)
bit '0'   =  8 subcarrier cycles, then 32 chips silence   (burst  975 us)
frame     = 36 bits MSB first, then ~16 ms silence
payload   = [35:20] preamble 0xB7E0 | [19:4] serial | [3:2] button | [1:0] 00
```

**The serial is not a factory secret.** It is the DIP switch bank, two bits per
switch (`up`=`11`, `middle`=`10`, `down`=`00`), switch 1 in the least significant
pair.

Transmitted through `furi_hal_subghz_start_async_tx` as a strictly
level-alternating `LevelDuration` stream — the same shape a RAW `.sub` uses. The
async TX timer has 1 µs resolution, so 65 µs chips are unremarkable for the radio.

## Build

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
python3 -m pip install --upgrade ufbt
ufbt              # build -> dist/clemsa_mv12.fap
ufbt launch       # build, upload over USB and run
```

Or copy `dist/clemsa_mv12.fap` to `SD/apps/Sub-GHz/`.

### Match the SDK to your firmware's API version

If the Apps list shows a **`?` icon and the filename** instead of the icon and
`Clemsa MV-12`, the FAP's API version doesn't match your firmware. Unleashed is
lenient enough to still *run* a mismatched FAP, which makes this easy to miss.

Read what your device actually wants (Flipper connected, qFlipper closed):

```sh
python3 -c "
import serial,glob,time
s=serial.Serial(glob.glob('/dev/cu.usbmodemflip*')[0],timeout=2); time.sleep(0.7)
s.write(b'device_info\r\n'); time.sleep(2.5)
print([l for l in s.read(s.in_waiting).decode().splitlines() if 'api_' in l or 'firmware_version' in l])"
```

Then match `firmware_api_major.minor` to the SDK's `api_symbols.csv` version.
Vendor indexes only carry the *current* release, so for anything older grab the
SDK zip from the release assets:

```sh
curl -sL -o sdk.zip <sdk asset url from the firmware's GitHub release>
ufbt update --hw-target=f7 --local=sdk.zip     # --hw-target is required with --local
head -2 ~/.ufbt/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv
```

Confirm the result without flashing anything:

```sh
arm-none-eabi-readelf -x .fapmeta dist/clemsa_mv12.fap
```

`48444752` = magic, then manifest version, then `api_version` as little-endian
`minor,major`, target id, stack size, the 32-byte name, a `has_icon` byte that
must be `01`, and the compressed icon.

Developed against **Unleashed unlshd-089 (API 87.8)**. Note that
`furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async)` no longer exists on
Unleashed and newer official firmware — the enum was dropped from the HAL and this
app uses `furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs)`
instead. That is the one line to change if you build against older official
firmware.

## Using it

| Screen | Keys |
|---|---|
| **Main** | `←`/`→` pick a door · `OK` transmit · `↑` settings · `Back` exit |
| **Settings** | `↑`/`↓` move · `←`/`→` change · `OK` on *DIP switches* opens the editor · `Back` saves |
| **DIP editor** | `←`/`→` pick a switch · `↑`/`↓` set it · `OK`/`Back` returns |

Settings persist to `/ext/apps_data/clemsa_mv12/settings.bin`.

| Setting | Notes |
|---|---|
| DIP switches | Graphical 8-switch editor laid out like the remote (`1` up, `F` middle, `0` down). Shows the resulting serial and payload live. |
| Chip period | 55–80 µs. Two remotes measured 65 and 60–65. The PIC's oscillator is untrimmed, so **sweep this first if a copy doesn't open.** |
| Repeats | Frames per press (default 10) |
| Frequency | 433.05–434.79 MHz in 10 kHz steps |

If a press shows `TX BLOCKED`, your firmware's region settings are refusing to
transmit at that frequency — that's a firmware setting, not an app problem.

## Adapting it to your remote

Read the trinary switches off your remote — `up` = `1`, `middle` = `F`,
`down` = `0`, switch 1 first — and enter them in the DIP editor. That is the whole
configuration. The shipped default is a neutral placeholder (all switches down).

If the door doesn't respond, sweep **chip period** around 60–68 µs.

## Credits

- [**@jesusvallejo**](https://github.com/jesusvallejo) — identified the
  two-carrier structure and published a working ESPHome implementation at
  [ESPHOME_GARAGE](https://github.com/jesusvallejo/ESPHOME_GARAGE).
- [**@skotopes**](https://github.com/skotopes) — independently found the
  carrier-over-carrier structure on physical remotes in 2024.
- Everyone in [flipperzero-firmware#3182](https://github.com/flipperdevices/flipperzero-firmware/issues/3182).

## Legal

Only use this on a door you own or are authorised to operate. Fixed-code remotes
offer no meaningful security; if this concerns you, the fix is a rolling-code
receiver, not obscurity.

## License

MIT — see [LICENSE](LICENSE).
