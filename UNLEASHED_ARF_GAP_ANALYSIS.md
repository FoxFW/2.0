# Unleashed Firmware & ARF Firmware — Feature Gap Analysis

## Checkpoint — last reviewed up to here

| Repo | Branch | Commit reviewed through | Date | Local checkout |
|---|---|---|---|---|
| Unleashed (`DarkFlippers/unleashed-firmware`) | `dev` | `92c4fdd9cab1754e8c1e16d7ad1fc236c28b3e83` ("bump apps tag") | 2026-09-04 | `D:\FZ\FIRMWARE\unleashed` |
| ARF (`D4C1-Labs/Flipper-ARF`) | `main` | `cc413f2f084531d1b21dfe13f20b05b4470b99de` ("Update protocol_items.c") | 2026-09-21 | `D:\FZ\FIRMWARE\ARF` |

Every item below was assessed and given a decision (ported, declined, or
tracked as a pending task) as of this checkpoint. Next time this
assessment is redone: `git log <checkpoint-commit>..HEAD` (or re-pull and
diff) against each repo's local checkout above to see only what's new
since this pass, rather than re-reviewing the full history again. Update
this table (and re-run the two "recent commits" reports, tasks #347/#348's
pattern) once the next pass completes.

**Standing methodology rule (added 2026-09-22, applies to every future
pass, not just this one):** always check
[ARF's GitHub Releases page](https://github.com/D4C1-Labs/Flipper-ARF/releases)
in addition to `git log`/commit review, every time this assessment is
redone. Commits and Releases are separate signals on this repo — ARF's
Releases page has historically run well behind its commit stream (see the
2026-09-21 note below: still "Dev Build dev-f7b04f13" from Jul 12, 2026 as
of that pass), so it usually adds nothing, but it costs one page load to
check and the day it does have something new is exactly the day skipping
it would matter. Record the Releases-page result in each pass's summary
note (as done below) even when it's "nothing new," so the next pass can
see the check was actually done rather than assuming it.

Research doc, no code changes — per explicit instruction, nothing here has
been ported. Unleashed's history was read live from GitHub
(`DarkFlippers/unleashed-firmware`, `dev` branch, commits/PRs/CHANGELOG
covering roughly Jun 6 - Sep 4, 2026). ARF's history was read the same way
against `D4C1-Labs/Flipper-ARF` (confirmed as the right repo by matching
its last commit hash/date/message against our own local ARF checkout's
`git log` and `git remote -v`) — at the time, its `main` branch had had
zero commits since Jul 25, 2026, so "recent" for ARF really meant Jun 1 -
Jul 25.
Excluded from both: bug fixes, renames, CI/docs/typo changes, and routine
upstream-merge noise. Items FoxFW already covers (often more extensively)
are also left out — see `PROJECT_HANDOVER.md` for what FoxFW already has.

**2026-09-16 re-pass complete.** The "zero commits since Jul 25" line
above was wrong (see the original note this replaces, still visible in
this file's git history) - ARF's `main` had gained 26 new commits by
Sep 16 (`56701a8..dc54f40`), reviewed via `git log`/`git show` against
the freshly re-pulled local checkout (the user updated it and access was
granted this pass). Most are protocol-specific bugfixes or joke commits,
excluded per this doc's own criteria (e.g. `e928e1b5` "My name is barney,
I'm an alcoholic", `057c5563` "BMW?", `15f26142` "I'm not a state i'm a
monster"). Two are real and added as items 9-10 in Section 2 below.

The Discord report that prompted this re-pass is **confirmed accurate**:
`2f88aff9` "Trying multi-region support" (Jul 26) added
`SubGhzProtocolFlag_315` to `fiat_spa.c`, `kia_v5.c`, `kia_v6.c`,
`kia_v7.c`, `mazda_v0.c`, and `mazda_siemens.c` (all `433`-only before),
and to `mitsubishi_v0.c` (`868`-only before) - confirmed by diffing the
actual `.flag` definitions in both repos' `lib/subghz/protocols/` trees
directly, not by trusting the commit message or release notes. See item 9
for the FoxFW-side gap this leaves open.

One near-miss ruled out: `mazda_infinity.c` (touched by `e928e1b5`) exists
in ARF's tree and is referenced in its TX-key catalog, but its protocol
struct registration is commented out in `protocol_items.c` - not actually
shipped/active in ARF yet, so not counted as a gap.

Also folded in here: the separate API-level research task (why Unleashed
moved from 88.2 to 88.4, where FoxFW still sits). Sourced from Unleashed
PR #1073's commit message (Aug 20, 2026): upstream official firmware added
`fap_exclude_libs=["gcc"]` manifest support and exported the AEABI
soft-float helper routines through `api_symbols.csv`, so an app doing
double-precision float math no longer needs to statically link libgcc's
soft-float routines into its own `.fap` — a real per-app size win. Worth
remembering if FoxFW ever bumps past 88.2: TPMS's pack()/unpack() math and
any other float-heavy app code could drop that linkage. Unleashed's `dev`
tip has since moved further to 88.6 via routine merges with no distinct
version-bump commit found by message search (GitHub's commit search only
indexes messages, not diffs).

## 1. Unleashed Firmware

1. **NFC resident-RAM cut via per-protocol plugins (~54%).** Moved NFC
   protocol scenes out of the main app image into loadable per-protocol
   plugins — the same architectural pattern FoxFW already uses for SubGHz
   Garage's protocol groups. FoxFW's NFC support is largely stock this
   session, so this is the single most relevant architectural idea in the
   window if NFC RAM/flash ever becomes a constraint.
   [PR #1073](https://github.com/DarkFlippers/unleashed-firmware/pull/1073)
   (commit `2b4411f`, Aug 20)

2. **Native MIFARE Plus SL3 support.** MIFARE Plus promoted from
   detection-only to a first-class protocol: AES-auth read, dictionary
   attack with a per-UID key cache, full SL3 emulation with
   shadow-writeback, write/update-to-card, and an "Add Manually" flow for
   18 Plus variants.
   [PR #1032](https://github.com/DarkFlippers/unleashed-firmware/pull/1032)
   (commit `45c3762`, Jul 7; refined by #1014, #1016, #1035)

3. **NFC Magic 2.0.** Magic Ultralight/NTAG (USCUID-UL) write/clone/wipe
   with automatic transport selection (direct CUID/ATS vs. backdoor),
   PWD-AUTH for protected tags, resumable per-page Partial Write, and
   family-first detection across UL11/UL21, NTAG213/215/216, UL-C, UL-5.
   [CHANGELOG at build tag 27jul2026](https://raw.githubusercontent.com/DarkFlippers/unleashed-firmware/28a95c9/CHANGELOG.md)

4. **RPC Network + GPS services.** New RPC service classes exposing
   network and GPS to external tooling — directly relevant to FoxFW Lab's
   own CLI/RPC-driven WebUI. Could plug in as a new panel (real GPS
   location, HTTP-style bridging) without needing the ESP32 companion.
   [PR #1013](https://github.com/DarkFlippers/unleashed-firmware/pull/1013)
   (Jun 28); [PR #1047](https://github.com/DarkFlippers/unleashed-firmware/pull/1047)
   (Jul 28)

5. **JS Runner externalized to a `.fap`.** Moved out of the firmware image
   into `apps/assets/js_app.fap` to free internal flash/RAM; the `js` CLI
   command became a CLI plugin. Same externalization strategy FoxFW
   already used for TPMS/Frequency Analyzer/Modulation Analyzer — worth a
   quick check for any FoxFW stock app that could still get the same
   treatment.
   [Commit `1a402a8`](https://github.com/DarkFlippers/unleashed-firmware/commit/1a402a8)
   "sorry no ram for js scripts anymore" (Jul 30)

6. **NightStand Clock**, a new default Clock app: overnight display with
   Up/Down brightness control, a red LED nightlight, a stopwatch, and a
   locale-aware 12h/24h daily alarm.
   [Commit `be05e0c`](https://github.com/DarkFlippers/unleashed-firmware/commit/be05e0c)
   (Jul 25)

7. **Pluggable main-menu style system + new "Grid" style.** Unleashed
   generalized its menu styling into a pluggable system and shipped Grid
   as the first style built on it — worth a look alongside FoxFW's own
   Fox Theme/Carousel/Classic system, either as a 4th style or as
   inspiration for making the style system itself more pluggable.
   [PR #1119](https://github.com/DarkFlippers/unleashed-firmware/pull/1119)
   (Sep 3); [PR #1126](https://github.com/DarkFlippers/unleashed-firmware/pull/1126)
   (Grid, Sep 4)

8. **LFRFID: Hitag Micro chip support (8265/8210/H5.5)**, read/write.
   [PR #1002](https://github.com/DarkFlippers/unleashed-firmware/pull/1002)
   (Jun 9)

9. **LFRFID: Wipe T5577** — reset a T5577 to blank with read-back
   verification.
   [PR #1003](https://github.com/DarkFlippers/unleashed-firmware/pull/1003)
   (Jun 9)

10. **NFC: auto-save recovered MIFARE Classic keys to the user
    dictionary**, so a cracked key persists for future reads instead of
    being one-shot.
    [PR #1118](https://github.com/DarkFlippers/unleashed-firmware/pull/1118)
    (Sep 3)

11. **GUI: apps can cover their own startup, and the Loader shows a
    loading animation while a large `.fap` is read from SD** — relevant
    context for FoxFW's own loading-wheel/Installing-screen work.
    [PR #1125](https://github.com/DarkFlippers/unleashed-firmware/pull/1125)
    (Sep 4); [PR #1101](https://github.com/DarkFlippers/unleashed-firmware/pull/1101)
    (Aug 21)

12. **New GUI primitives**: a public `canvas_get_buffer` API (could
    simplify future screenshot/frame-capture work) and a reusable
    date/time input widget.
    [PR #4399](https://github.com/DarkFlippers/unleashed-firmware/pull/4399),
    [PR #4261](https://github.com/DarkFlippers/unleashed-firmware/pull/4261)
    (both Jun 30, upstream OFW)

13. **New SubGHz protocols**: Telcoma/Cardin EDGE
    ([PR #1001](https://github.com/DarkFlippers/unleashed-firmware/pull/1001)),
    Elplast (PR #4309), Cardin S449 + FSK12Kdev modulation (PR #4328),
    Superrollo GW60 roller-shutter KeeLoq HCS361
    ([PR #1068](https://github.com/DarkFlippers/unleashed-firmware/pull/1068),
    Aug 26). Pure protocol-coverage work rather than architecture — worth
    a quick registry diff against FoxFW's Automotive/Garage lists, lowest
    priority of this section.

14. **NFC: Bambu Lab filament-spool parser** (type/color/code/temps),
    ported from an external open-source project. Fun, low-effort parity
    item.
    [PR #1012](https://github.com/DarkFlippers/unleashed-firmware/pull/1012)
    (Jun 16)

15. **Infrared: save universal-remote buttons** into a profile — minor UX
    addition to the universal IR remote.
    Commit dated Sep 1, message "infrared allow saving universal remote
    buttons" (see `dev` branch history).

## 2. ARF Firmware

Repo: [`D4C1-Labs/Flipper-ARF`](https://github.com/D4C1-Labs/Flipper-ARF)
("Flipper-ARF — Automotive Research Firmware," described in its own
README as "based on Unleashed Firmware but heavily modified... focuses
exclusively on automotive research and experimentation"). `main` branch,
last commit `56701a8` on Jul 25, 2026 — matches our local ARF checkout
exactly, so this is confirmed as the same fork we already track.

1. **RollJam attack app** — a standalone SubGHz app implementing the
   classic jam-and-capture-two-codes rolling-code attack, shipped
   alongside a rewritten Garage Door Remote (ProtoPirate-based). This is a
   distinct technique from anything in FoxFW's Automotive/RF
   Jammer/ProtoPirate suite: jam while recording two consecutive rolling
   codes, then replay the one the receiver never saw, rather than blind
   jamming or simple capture-replay.
   [Commit `3a63e14`](https://github.com/D4C1-Labs/Flipper-ARF/commit/3a63e14)
   (Jun 25)

2. **SubGHz modulation hopping** — automatically cycles RX through a
   table of modulation profiles to catch signals sent on non-default
   modulations, with a script to add new hopping tables. Could improve
   unknown-signal capture odds across Automotive/Garage/Read generally.
   [Commit `a3698f9`](https://github.com/D4C1-Labs/Flipper-ARF/commit/a3698f9),
   `99ac826` (both Jun 13)

3. **Full-d-pad live Emulate on the Receiver screen, no save required** —
   maps all four d-pad directions to different repeat/emulate behaviors
   directly off a decoded-but-unsaved signal, including "keep protocol
   encoders running while held" and "repeat car-emulate TX while held."
   FoxFW already has its own hold-OK-to-repeat-TX feature, so this reads
   as a possible enhancement (4-direction mapping, no save step) rather
   than a flat gap.
   [Commit `833c9ad`](https://github.com/D4C1-Labs/Flipper-ARF/commit/833c9ad)
   (Jul 24); also `f1422cb` (Jul 14), `b34a73b`/`069a42d` (Jul 12)

4. **Saved-signal editor + duplicate-capture detection** — a scene to edit
   a saved SubGHz signal's metadata directly, plus a warning when a new
   capture duplicates one already saved. FoxFW deliberately kept Garage's
   saved-file management lean (browse + Details + Emulate only, no
   editor) as a considered design choice (see task history) — flagging
   this so it's a conscious decision to revisit or not, not an oversight.
   [Commit `e606c5b`](https://github.com/D4C1-Labs/Flipper-ARF/commit/e606c5b)
   "Add saved signal editor and duplicate detection" (Jul 12)

5. **FlipperGB — Game Boy emulator app.** Added Jul 3, stability pass Jul
   15. A fun, high-visibility capability FoxFW has no equivalent of.
   [Commit `0d598b8`](https://github.com/D4C1-Labs/Flipper-ARF/commit/0d598b8)
   (Jul 3)

6. **FlipperDoom — Doom port.** Added Jul 2 "for fun," exit-freeze fix the
   next day. Same category as FlipperGB.
   [Commit `a0c53e3`](https://github.com/D4C1-Labs/Flipper-ARF/commit/a0c53e3)
   (Jul 2)

7. **Native Android companion app pairing built into the firmware.**
   ARF is building an "Official Flipper Mobile APP" connection layer
   directly into the firmware (still unfinished as of its last commit:
   "the connection with the mobile app isn't working... I'll take care of
   that tomorrow"), backed by its own
   [Android companion repo](https://github.com/D4C1-Labs/arf-android-companion).
   A different strategic approach from FoxFW Lab's browser-based
   CLI/RPC WebUI — worth being aware of as an alternative architecture,
   not necessarily something to copy outright.
   [Commit `94dcc82`](https://github.com/D4C1-Labs/Flipper-ARF/commit/94dcc82)
   (Jun 13)

8. **Protocol name allowlist ("Proto Filter")** — a Receiver Config option
   restricting RX decode to a chosen list of protocols by name, persisted
   in settings, to cut RAM use and improve capture odds for a known
   target. Lowest priority here: FoxFW's 11-group RX/TX plugin system
   with on/off filtering already achieves a similar goal at group
   granularity, so this is a refinement (per-protocol vs. per-group)
   rather than a clean gap.
   [Commit `f4fe2d4`](https://github.com/D4C1-Labs/Flipper-ARF/commit/f4fe2d4)
   (Jul 15)

9. **Multi-region frequency flags for 7 shared automotive protocols.**
   `fiat_spa.c`, `kia_v5.c`, `kia_v6.c`, `kia_v7.c`, `mazda_v0.c`, and
   `mazda_siemens.c` gained `SubGhzProtocolFlag_315` (FoxFW currently
   flags all six `433`-only). `mitsubishi_v0.c` gained both `_315` and
   `_433` (FoxFW currently flags it `868`-only - a bigger gap than the
   Discord report implied, since FoxFW was missing `433` too, not just
   `315`). Low-risk to port: these are categorization/RX-filter flags
   (which frequency bands a protocol is discoverable/enabled under), not
   new decode logic - FoxFW already fully implements all seven protocols.
   Confirmed 2026-09-16 by diffing `.flag` definitions directly.
   [Commit `2f88aff9`](https://github.com/D4C1-Labs/Flipper-ARF/commit/2f88aff9)
   "Trying multi-region support" (Jul 26)

10. **Weather-Editor** - a new app
    (`applications/system/Weather-Editor-EN/`) for decoding/editing
    wireless weather-station transmissions (temperature/humidity
    sensors), with its own radio-device-loader helper mirroring the
    pattern FoxFW's own apps use for external CC1101 support. FoxFW has no
    weather-station protocol support at all currently (confirmed - no
    file or string matching "weather" anywhere in `applications/`). A
    real, distinct capability gap; not yet assessed for code quality or
    effort to port.
    [Commit `6fed312c`](https://github.com/D4C1-Labs/Flipper-ARF/commit/6fed312c)
    "Added Weather-Editor" (Sep 11)

11. **Renault V1 "Seed Bruteforce" - recover a 4-byte Hitag2 SEED from one
    captured signal and derive future valid codes.** New standalone cipher
    engine `lib/subghz/protocols/hitag2_seed.c`/`.h` (a decoupled,
    symbol-safe copy of the classic byte-array Hitag2 cipher already used by
    `renault_v1.c`'s key-recovery path) backs a new manual scene,
    `applications/main/subghz/scenes/subghz_scene_seed_bf.c` (368 lines),
    reachable from a saved Renault V1 signal's menu: brute-forces the
    frame's 4-byte SEED (~0x40000 candidates, worker-threaded so the GUI
    stays responsive) and, on success, writes the recovered SEED back into
    the `.sub` file so the signal can be re-encoded forward into a valid
    next code. The actual entry point
    (`subghz_protocol_renault_v1_run_seed_bf_ex()`) lives in `renault_v1.c`
    itself, which grew to 1669 lines to accommodate this (ours: 1298). A
    genuine, well-scoped gap - our own `renault_v1.c` already has the
    HITAG2 cipher from our own earlier port, but nothing that brute-forces
    a captured frame's SEED or derives a next code from it. Confirmed
    absent: zero matches for `seed_bf` or `hitag2_seed` anywhere in our
    tree.
    [Commit `3852873`](https://github.com/D4C1-Labs/Flipper-ARF/commit/3852873974956acc50a89b535801cf7950f2c4f8)
    "Momentum leviosa" (Sep 16, introduces it), refined by `81e05e7` /
    `d767562` / `3d2cb6e` (Sep 16)

12. **New protocol: General Motors (GM/Chevrolet/GMC/Buick/Cadillac) car
    remote.** `lib/subghz/protocols/gm.c`/`.h`, OOK/PPM at 315MHz,
    decoder+encoder, reverse-engineered from the public rtl_433 project
    (cited/credited in the header) - button and serial are plaintext, a
    24-bit rolling counter is plaintext, and a 24-bit field is documented
    as "encrypted, cipher NOT public" (decode/replay works, forward
    prediction doesn't). Confirmed absent: no `gm.c`/`gm.h`, no
    `SUBGHZ_PROTOCOL_GM` anywhere in our `protocol_items.c`. A clean,
    low-risk protocol-coverage gap - no crypto to port, decode/replay only.
    [Commit `3852873`](https://github.com/D4C1-Labs/Flipper-ARF/commit/3852873974956acc50a89b535801cf7950f2c4f8)
    "Momentum leviosa" (Sep 16)

    **Note added 2026-09-21:** this one may already be moot - FoxFW's own
    `protocol_items.c` now lists `&subghz_protocol_gm,` as active (last
    entry in the registry array), so either this was already ported since
    the 2026-09-16 pass or FoxFW independently added GM support. Not
    re-verified line-by-line against ARF's `gm.c` this pass (out of scope
    for this window - see item 21 below for what this pass actually
    covered); flagging only so the next pass doesn't re-log it as an open
    gap without checking first.

13. **Fiat V1/V2 custom-button (D-pad) emulation - a real gap, not just a
    bugfix despite the commit message.** Our own custom-button
    infrastructure (`subghz_custom_btn_*`) is already wired into ~30 other
    protocols (Ford, PSA, VAG, Chrysler, Kia V0, etc. - confirmed by
    repo-wide grep) but into neither `fiat_v1.c` nor `fiat_v2.c` (zero
    references in both, confirmed). ARF's Fiat V1 now maps all four D-pad
    directions to real button codes (Up=Unlock 0x8, Down=Lock 0x4,
    Left=Trunk 0x2, Right=Close 0x1, OK=byte-identical replay of the
    capture), gated on whether the HITAG2 key was actually recovered
    (`key_loaded`) so a keyless capture still replays safely instead of
    producing a frame the receiver rejects, and advances the rolling
    counter per direction press the same way PSA already does in our own
    tree. Fiat V2 gained the same capability from scratch
    (`fiat_v2_patch_button()`, new `custom_btn_i.h` include). Worth a look
    independent of anything else here - it's a capability neighboring
    protocols already have, just missing from these two.
    [Commit `bd7a69a3`](https://github.com/D4C1-Labs/Flipper-ARF/commit/bd7a69a39863731769b6b9b5d38f7ef31823f325)
    "update kia v3/v4 should resolve bugs on TX" (Sep 13 - commit message
    is about Kia but the actual diff touched is Fiat V1/V2)

14. **Momentum-style "Control Center" quick-settings panel on the lock
    screen.** A real UI subsystem port from Momentum Firmware (hence
    "Momentum leviosa"): rewrites `desktop_view_lock_menu.c` (+272 lines)
    and `.h`, extends `desktop_scene_lock_menu.c` and
    `power_settings_scene_power_off.c`, and adds 3 new 16x16 icons
    (Bluetooth/Lock/Settings) plus 2 more 10x10 icons for SubGhz/
    ProtoPirate added in the following two commits - suggesting the panel
    is being extended toward app-specific quick-launch tiles, not just
    system toggles. FoxFW has no equivalent (confirmed: no
    `ControlCenter`/`CC_Bluetooth`/`CC_Lock_16x16` anywhere in
    `applications/services/desktop/`). This is the biggest lift of
    anything in this list - a genuine new UI subsystem across several
    files, not a drop-in - worth a deliberate look at the actual UX before
    committing to it, not a quick port.
    [Commit `3852873`](https://github.com/D4C1-Labs/Flipper-ARF/commit/3852873974956acc50a89b535801cf7950f2c4f8)
    "Momentum leviosa" (Sep 16), extended by `81e05e7` / `d767562` (Sep 16)

    **Decision (2026-09-17):** keeping an eye on this one only for now -
    no immediate action. We'll revisit and decide if it comes up again
    later, rather than committing to the port now. For reference, a
    pixel-accurate mockup of the lock-screen panel was rendered directly
    from ARF's `desktop_view_lock_menu.c` draw logic (exact toggle-grid/
    brightness-volume-bar geometry and the real icon assets) so we have a
    visual on hand of what this would look like if we do port it later.

15. **`flipper-sub-dup` - a standalone Sub-GHz duplicate-file finder/
    cleanup app.** A separate, cleanly-structured `.fap` (own unit tests,
    `make test`/`make linter` via `cppcheck`) that scans the Sub-GHz
    saved-signal folder, groups duplicates, and lets you review/delete
    them, browsed by groups rather than caught at capture time. Different
    approach from our own item 82 (a per-capture "Update instead of Save"
    check against Protocol+Serial/Key at the moment of saving) - theirs is
    a standalone cleanup tool for signals already on disk (including ones
    saved before either fix existed), ours is prevention at save time. Not
    a gap so much as a complementary idea: a "find and clean up existing
    duplicates" utility we don't have.
    [Commit `b08857a2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/b08857a220867030ade353355bb752a06f95f544)
    "UNTESTED protopirate to subghz" (Sep 13 - same commit, unrelated to
    its message)

16. **`protopirate_to_subghz` - a converter app that translates a
    ProtoPirate capture into the main app's Sub-GHz format.** Maps
    ProtoPirate's own protocol names (`Fiat V1`, `Kia V3`/`Kia V4` ->
    `KIA/HYU V3/V4`, `Porsche Touareg` -> `Porsche AG`, etc.) onto the main
    app's naming so a signal captured in their ProtoPirate fork can be
    dropped into the standard Saved-signals folder and read correctly,
    rather than staying siloed in ProtoPirate's own storage. We
    deliberately keep ProtoPirate_FoxEdition stock (per this project's own
    standing policy) and don't have anything bridging its captures into
    `subghz`'s own folder today - a real, distinct capability, low-risk
    since it's pure file/metadata translation, no protocol logic of its
    own.
    [Commit `b08857a2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/b08857a220867030ade353355bb752a06f95f544)
    (Sep 13)

17. **"All Protocols" bulk ON/OFF row atop the protocol list - minor,
    likely not worth porting.** Adds one global 3-state (ON/OFF/Mixed)
    master row above ARF's flat per-protocol list, bulk-toggling every
    registered protocol at once. FoxFW already solves the same underlying
    need at a coarser, arguably more useful grain - each of our 11
    protocol-family groups already has its own master toggle (confirmed:
    `subghz_scene_protocol_list.c`'s own header already describes "Each
    group has its own master toggle"). Overlaps with item 8 above (Proto
    Filter), already logged as low priority; flagging the "one-shot
    disable everything" convenience specifically in case it's still wanted
    on top of group-level toggles, but it isn't a capability gap.
    [Commit `ec5d5170`](https://github.com/D4C1-Labs/Flipper-ARF/commit/ec5d51709727eb8350cad17d339d77d9932a0f5d)
    "vibecoding a master switch" (Sep 13)

18. **Architecture note, not a gap: ARF's Hitag2Hell bitslice attack is a
    main-app helper, ours is a standalone app.**
    `applications/main/subghz/helpers/subghz_hitag2_hell.c` (604 lines,
    bitsliced, `-O3`-pragma'd for M4 speed) implements the same class of
    attack our own `fox_hitag2_hell` app does, adapted to the Fiat V1 BCM
    cipher - but lives inside their main SubGhz app rather than as a
    separate `.fap`. Pre-dates this checkpoint's window (found incidentally
    while reading `hitag2_seed.h`'s own header comment, which explicitly
    calls it out to explain a naming choice) - not evaluated for whether
    its coverage/approach exceeds ours, would need a real side-by-side of
    the bitslice kernels themselves, not just file presence. Noted here so
    it isn't mistaken for a new capability if it comes up again; the
    interesting question if we ever revisit this is integration depth
    (helper vs. standalone app), not whether the capability exists.

19. **PORTED 2026-09-22.** ~~SubGHz receiver AM/FM modulation gate — filters decoders by the
    capture's actual modulation, closing a cross-protocol false-positive
    class.~~ New `subghz_txrx_receiver_apply_modulation_filter()`
    (`applications/main/subghz/helpers/subghz_txrx.c`/`.h`) reads the
    CC1101 `MDMCFG2` register straight out of the active preset's raw
    register-pair blob, classifies it as `SubGhzProtocolFlag_AM` (ASK/OOK)
    or `_FM` (2-FSK), and applies it via
    `subghz_receiver_set_modulation_filter()` at the single RX choke point
    (`subghz_txrx_rx()`), so AM-only decoders (e.g. Fiat V2) can no longer
    fire on an FM capture (e.g. KIA V6) and vice versa - the documented
    motivating bug. Falls back to "gate disabled" (feed everything,
    today's behavior) if the preset's register blob doesn't carry
    `MDMCFG2`. Confirmed absent in our tree: zero matches for
    `modulation_filter`/`apply_modulation` in our `subghz_txrx.c`/`.h` or
    `lib/subghz/receiver.c`.

    **Caveat that made this more than a drop-in, resolved 2026-09-22:** the
    gate is only as correct as every protocol's own `.flag` declaration of
    which modulations it actually supports. ARF got this wrong on first
    ship - `psa.c`/`psa2.c` only declared `_FM`, so real AM-modulated PSA
    captures would have been silently dropped by their own new gate -
    caught and hotfixed 3 days later (`SubGhzProtocolFlag_AM` added to
    both).

    **Flag audit (2026-09-22): done, before the gate went live, not after.**
    Diffed the `.flag` line of all 97 shared protocol files in
    `lib/subghz/protocols/` (not just the ones ARF's own commits happened
    to touch) against what each protocol's own decoder function names
    imply about its actual demodulation. Found exactly three real
    mismatches, all fixed before the gate was wired in:
    - `psa.c`, `psa2.c` - same bug ARF hit: declared `_FM` only, missing
      `_AM`, even though both decode AM/OOK (`psa_am_*` functions). Fixed
      by adding `SubGhzProtocolFlag_AM` to both `.flag` lines.
    - `renault_v1.c` - a mismatch ARF's own item 9 audit didn't catch:
      declared **both** `_AM` and `_FM` (the extra `_FM` was stray),
      which would have made the gate a silent no-op for it either way.
      Confirmed via `git log` that this exact stray `_FM` is the same flag
      ARF itself removed from `renault_v1.c` in commit `976a03f2` (see
      item 23 below) - so this was independently re-derived here, then
      cross-checked against ARF's own fix and found to match. Fixed by
      removing the stray `_FM`, keeping `_AM`.

    A protocol declaring *both* AM and FM flags is a quieter failure mode
    than declaring the wrong single flag: the gate becomes a complete
    no-op for that protocol (always passes, whichever modulation is
    active) rather than the loud "protocol stops decoding entirely" a
    single wrong flag causes. Worth remembering for any future
    `.flag`-audit pass: check for *extra* flags, not just missing ones.

    Ported: `SubGhzReceiver` gained a `modulation_filter` field and a
    `subghz_receiver_set_modulation_filter()` setter (`lib/subghz/
    receiver.c`/`.h`); `subghz_receiver_decode()`'s existing filter check
    gained a middle clause so a protocol's declared AM/FM flags are
    checked only when the gate is active, decoder registration and
    FoxFW's own pre-existing `protocol_enabled_callback` mechanism (which
    ARF's baseline doesn't have) are otherwise untouched.
    `subghz_txrx_receiver_apply_modulation_filter()` reads the CC1101
    `MDMCFG2` register straight out of the active preset's raw
    register-pair blob and classifies AM vs FM, called once per RX start
    in `subghz_txrx_rx()`. **Scoped to the internal SubGHz app only**
    (`applications/main/subghz/` + the shared `lib/subghz/receiver.c/.h`)
    per explicit instruction - `applications/fox/subghz_garage/` and every
    other Fox app are untouched.
    [Commit `da08b612`](https://github.com/D4C1-Labs/Flipper-ARF/commit/da08b6120ec0f63083b3e06b2e9dc14de3f1b032)
    "Test update and fixes" (Sep 17/18), PSA hotfix in
    [`32b36f62`](https://github.com/D4C1-Labs/Flipper-ARF/commit/32b36f623e566bff631ce7307be4f632eafc849b)
    "psa fixes" (Sep 20)

20. **PORTED 2026-09-22.** ~~Ford V1 custom-button mapping bug - we
    currently ship the exact wrong mapping ARF just fixed.~~ Confirmed by
    direct read of our own `lib/subghz/protocols/ford_v1.c`: our
    custom-button `OK` case transmitted the hardcoded Trunk code (`0x04`)
    instead of replaying the originally-captured button, and there was no
    `RIGHT` case at all (fell through to default). ARF's fix corrects the
    mapping to
    Up=Unlock(`0x2`)/Right=Trunk(`0x4`)/Down=Panic(`0x8`)/Left=Lock(`0x1`),
    with `OK` now replaying the captured button byte-for-byte instead of
    hardcoding Trunk. This was not a "consider adopting" item - it was a
    live bug already in our shipped code (shared ancestry with ARF's
    pre-fix state), independently reproduced by reading our own file, not
    inferred from theirs. Ported as-is; trivial, low-risk, single-file
    fix.
    [Commit `da08b612`](https://github.com/D4C1-Labs/Flipper-ARF/commit/da08b6120ec0f63083b3e06b2e9dc14de3f1b032)
    "Test update and fixes" (Sep 17/18)

21. **PORTED 2026-09-22 (decode-only).** ~~Toyota: new decode-only
    "Variant C" (2006 Prius / Corolla Verso, 433.92MHz) - and separately,
    Toyota isn't wired into our build at all yet.~~ Added a third
    KeeLoq-style variant to our own `lib/subghz/protocols/toyota.c`
    alongside the existing Corolla (A) and Tundra (B) variants - its own
    preamble/framing constants, its own button set (Lock/Unlock/Trunk/
    Panic), decode-only (no encoder), plus `&subghz_protocol_toyota` added
    to `protocol_items.c`'s registry (it was `#include`d but never
    actually registered - a pure pre-existing oversight, now fixed, so
    Toyota decode is live in our build for the first time). We did **not**
    port ARF's three flash-saving `protocol_items.c` comment-outs
    (`porsche_cayenne`, `fiat_spa`, `mazda_siemens`) - that was ARF
    managing their own flash margin, not a decision we need to mirror; if
    Toyota's added footprint becomes a real concern, size it against our
    own live flash-budget work (`GARAGE_APP_SIZE_REDUCTION_FINDINGS.md`,
    `CC1101_EXTERNAL_RAM_THRESHOLD_STATUS.md`) rather than reflexively
    dropping the same three protocols ARF did.

    **Behavior change worth flagging explicitly:** Variant A and Variant C
    share *identical* preamble timing (400/800us TE) and are only
    distinguishable by what follows the preamble - an immediate LS/SL data
    pair for A, vs. an XL sync gap (1050-1500us) for C. ARF's own tested
    design routes **all** 433MHz-threshold traffic to Variant C and leaves
    Variant A's state machine as unreachable-but-compiled dead code (kept
    only to avoid an unused-function warning), on the empirical claim that
    real-world Corolla-family fobs actually match C's framing, not A's. We
    adopted this exactly as ARF ships it (mirroring their own
    real-hardware-tested judgment - otherwise Variant C would be entirely
    unreachable and the whole port would have zero effect), but this is a
    genuine dispatch-routing change from our prior behavior, not a pure
    addition: any capture that previously would have matched Variant A's
    decoder now matches Variant C's instead. Worth keeping in mind if a
    Toyota capture ever decodes differently than before this pass.
    [Commit `976a03f2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/976a03f299fcfad7b4464102b3352de1be825883)
    "Secure rollback..." (Sep 20), follow-ups
    [`c056637a`](https://github.com/D4C1-Labs/Flipper-ARF/commit/c056637af17f53fd739f24096ad52327e8f945f0)
    "Toyota C version" and
    [`cc413f2f`](https://github.com/D4C1-Labs/Flipper-ARF/commit/cc413f2f084531d1b21dfe13f20b05b4470b99de)
    "Update protocol_items.c" (Sep 20-21, both 1-line `protocol_items.c`
    touches, not independently deep-dived)

22. **REVIEWED 2026-09-22; VALIDITY-GATE FIX AND ADAPTIVE AM DEMODULATOR
    BOTH PORTED 2026-09-22.** ~~PSA / PSA2 decoder rewrite - large, not yet
    reviewed in depth.~~ Read line-by-line
    against our own `psa.c` (via `git show 976a03f2 -- psa.c`, 451 changed
    lines: 343 insertions, 108 deletions). `psa2.c` turns out to be
    untouched by this commit entirely - the rewrite is `psa.c`-only; the
    `_AM` flag fix applied to `psa2.c` in item 19's audit came from our own
    independent sweep, not from this commit. Two real things happened
    here, both specific to PSA's **AM** decode path:

    - **New adaptive AM (OOK) demodulator**, explicitly "ported from
      psa.js" per ARF's own comments (a reference JS implementation they
      work from, not in our tree). PSA's AM preamble is a Manchester
      square wave whose half-bit period varies by model (~65-125us
      observed), so the old fixed-tolerance timing could desync on some
      fobs. The rewrite adds: a raw bit buffer (`am_bits[96]`) that
      records the demodulated stream instead of committing to a frame
      position immediately; a "varied bytes" sanity filter
      (`psa_am_varied`, requires >= 4 distinct bytes among the 8 key1
      bytes) to reject repetitive noise/preamble false-positives; and a
      **frame realignment scan** (`psa_am_complete`) that tries bit
      offsets 0 through min(8, buffered-len-80), keeps only offsets whose
      key1_high carries the correct sync nibble (`0xA`) and passes the
      varied-bytes filter, then picks whichever candidate has the most
      leading 1-bits (the preamble's all-ones tail) as the best alignment
      - rather than assuming the frame always starts at a fixed
      precomputed bit position.
    - **The actual bug worth caring about: the old validity gate rejected
      ~15 of every 16 genuine captures.** The old code only fired the
      decode callback when `(validation_field & 0xF) == 0xA` (or the
      frame decrypted). `validation_field` comes from key2 - the rolling
      code - so its low nibble is effectively random across presses; it
      only happened to equal `0xA` by chance about 1 time in 16. Every
      other genuine, perfectly good capture was silently dropped before
      the user ever saw it. The fix checks `(key1_high >> 16) & 0xF ==
      0xA` instead - a nibble that's part of PSA's *fixed* framing
      structure, not the rolling code, so it matches reliably on every
      valid frame regardless of the counter. ARF's own inline comment
      states this diagnosis explicitly: "The old gate compared the key2
      low nibble (validation_field), which is part of the rolling code
      and therefore random -- it wrongly rejected most Lock/Close
      captures." A secondary, related tightening: the short-pulse
      tolerance check in `feed()` changed from a dynamic `tolerance >
      PSA_TOLERANCE_49` to a hardcoded `tolerance > 40` plus a new
      absolute ceiling `duration > 180`.

    **CONFIRMED then FIXED 2026-09-22: we had the exact same bug in shipped
    code, now corrected.** Checked our own `psa.c` for the same pattern
    (not left as a guess): `grep -n "validation_field.*0xf"` found our
    decode-callback gate using `(instance->validation_field & 0xf) != 0xa`
    / `== 0xa` at **three** separate call sites (the two streaming-decode
    completion paths in `PSADecoderState2`/`PSADecoderState4` plus the
    `!should_process` fallback path), all gating on `validation_field` -
    which our own code also derives from the rolling `key2`/decode-data
    stream, same as ARF's pre-fix version. Our PSA decode was very likely
    silently rejecting the large majority of genuine captures, for the
    identical reason ARF fixed.

    **Ported 2026-09-22.** The uncertainty flagged below (whether the
    nibble-check fix is safely separable from the adaptive-AM-demod
    rewrite) was resolved by tracing `key1_high`'s assignments end to end
    in our own file: it's populated identically to ARF's baseline
    (Manchester-decoded during the existing state machine, every one of
    its assignment sites matching ARF's own file structurally 1:1, the
    *only* exception being ARF's new `psa_am_frame_at()` scan function,
    which our tree doesn't have at all), and it's already read two lines
    below every one of our three gate sites
    (`instance->generic.data = ((uint64_t)instance->key1_high << 32) |
    ...`) - so it's populated and in scope regardless of the demod path.
    All three sites changed from checking `(instance->validation_field &
    0xf)` to `((instance->key1_high >> 16) & 0xF)` against the same `0xA`
    constant, preserving each site's existing comparison direction (`!=`
    to reject in the two streaming-completion paths, `==` to proceed in
    the fallback path). Verified: brace count 324/324, paren count
    1201/1201 before and after edit; `psa2.c` deliberately left untouched
    (see below); `applications/system/ProtoPirate/protocols/psa.c`
    deliberately left untouched per standing policy - it has the
    identical-looking pattern but is explicitly out of scope.

    **`psa2.c` checked and deliberately left alone - not the same bug.**
    `psa2.c` uses an identically-shaped `(inst->validation_field & 0xF) ==
    PSA_VALID_NIBBLE` check at its own three call sites, but critically,
    ARF's own current `psa2.c` (post-`976a03f2`) *still uses this exact
    pattern, unchanged* - the same people who diagnosed and fixed `psa.c`'s
    bug left `psa2.c`'s look-alike code alone, which is strong evidence
    `psa2.c` is a different frame layout where this nibble genuinely is a
    fixed marker rather than rolling-code-derived. Not touched, on the same
    "don't blindly copy because the code looks similar" principle applied
    throughout this whole gap-analysis exercise.

    **Adaptive AM demodulator - PORTED 2026-09-22.** This is the other half
    of ARF's rewrite: a new `psa_am_frame_at()`-based scan that buffers raw
    demodulated bits and tries multiple bit-offsets to find the best frame
    alignment, rather than assuming a fixed start position. Ported into our
    own `psa.c` as six changes: six new `PSA_AM_*` defines (preamble
    min/max, glitch-decay threshold, resync-bit count), four new struct
    fields (`pre_glitch`, `am_await_high`, `am_bits[96]`, `am_bits_len`),
    the missing `SubGhzProtocolFlag_315` on the `.flag` line (ARF gained
    multi-region support here too, same pattern as item 9), `reset()`
    extended to zero the new fields, and four new helper functions
    (`psa_add_am_bit`, `psa_am_frame_at`, `psa_am_varied`,
    `psa_am_complete`) inserted ahead of `feed()`, which itself had its
    `PSADecoderState3`/`PSADecoderState4` handling replaced with ARF's
    buffer-then-realign approach. **One adaptation, not a verbatim copy:**
    our own AM/OOK path already inverts every decoded Manchester bit
    (`decoded_bit = !decoded_bit`), a FoxFW-specific convention absent from
    ARF's tree - copying `psa_add_am_bit()` as-is would have silently
    flipped every AM key bit. Caught by diffing our own existing AM path
    before writing the new function, not after; `bit = bit ? 0 : 1;` added
    at the top of our `psa_add_am_bit()` to compensate, everything
    downstream of it left matching ARF exactly.

    **Verified two ways, since no real AM-modulated PSA capture was on hand
    to test the new logic against directly** (unlike the XOR-decrypt
    pipeline below `psa_am_complete()`, which a prior pass already
    validated against a real `Sf.sub` capture with an exact match - this
    new code sits upstream of that, in the demodulation path itself).
    Structurally: brace count 346/346 and paren count 1281/1281 before and
    after the patch (up from 324/324, 1201/1201 pre-patch, consistent with
    the added code, not a sign of a dropped/duplicated brace), zero `//` or
    `/*` comments anywhere in the file post-patch, and every new function's
    definition-plus-call-site count matches what the new code actually
    calls (`psa_add_am_bit`: 2, `psa_am_frame_at`: 3, `psa_am_varied`: 2,
    `psa_am_complete`: 4). Behaviorally: built a standalone gcc test harness
    that extracts the four new functions **verbatim** from the patched file
    (re-read from the shipped source immediately before compiling, to
    guarantee byte-for-byte match, not from a remembered copy) with mocked
    `psa_setup_byte_buffer`/`psa_direct_xor_decrypt` calls so the test
    isolates the new realignment logic specifically. 27 assertions, all
    passed, covering bit-inversion correctness, `am_bits_len`/decode-buffer
    accumulation, the `KEY1_BITS` latch, frame extraction at offset 0, the
    varied-bytes accept/reject filter, and - the actual point of the
    rewrite - `psa_am_complete()` correctly finding and recovering the
    right key (`key1_high`/`key1_low`/`key2_low`) after 3 bits of injected
    leading noise, plus correctly rejecting every offset when none carries
    a valid sync nibble. This confirms the realignment scan behaves as
    designed against synthetic misalignment; it doesn't replace a real
    multi-model hardware test (ARF's own comment cites ~65-125us half-bit
    variance across fob models) for confirming real-world capture-rate
    improvement - flagged the same way item 21's Toyota dispatch-routing
    change was, as a behavior worth double-checking the first time a real
    AM PSA capture is on hand post-flash.
    [Commit `976a03f2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/976a03f299fcfad7b4464102b3352de1be825883)
    "Secure rollback..." (Sep 20)

23. **INDIVIDUALLY REVIEWED 2026-09-22 - all 20 files, each diffed against
    our own current code (not assumed to match ARF's diff context).**
    Confirmed hypothesis from the prior pass: every file carries the same
    two recurring patterns already seen in `ford_v1.c` (item 20) and
    `fiat_v1.c`/`fiat_v2.c` (item 13) - **ROLLING_CNT** (advance the
    counter by `furi_hal_subghz_get_rolling_counter_mult()`, default 1, on
    each OK/D-pad press instead of replaying a stale captured counter,
    then persist the re-encoded frame back to the `.sub` file so the UI
    shows the increment) and **BUGFIX_UI** (re-derive the displayed
    button/CRC in `get_string` from the *current* D-pad selection rather
    than the originally-captured button, since `get_string` runs live as
    the user moves the D-pad, before any encoder deserialize). No new
    decode capability in any of these 20 files - this really was cleanup,
    not new coverage, confirming the prior pass's guess. Per-file
    disposition below; the common thread across every "declined" file is
    the same judgment call: adapt the *fix* to our own file's actual
    architecture, never blindly copy ARF's hunk text when the surrounding
    code has diverged, and skip rather than guess when what ARF's diff
    assumes already exists isn't actually there.

    **Ported (16 of 20), each adapted to our own file's actual code, not
    copied verbatim:** `chrysler.c`, `fiat_v1.c`, `ford_v0.c`,
    `honda_static.c`, `honda_v1.c`, `honda_v2.c`, `kia_v0.c`, `kia_v1.c`,
    `kia_v2.c`, `kia_v3_v4.c`, `kia_v6.c`, `kia_v7.c`, `mazda_v0.c`,
    `renault_v0.c`, `subaru.c`, `suzuki.c`. Several needed real adaptation
    rather than a drop-in: our `ford_v0.c` encoder already exceeded ARF's
    fix and uses a 3-way button scheme, not ARF's 5-way, so only the
    decoder-side deserialize (which was missing the remap+CRC-rebuild) was
    touched, using our own 3-way codes; `kia_v0.c` is a completely
    different, simpler decoder architecture than ARF's (shares none of
    ARF's `KiaV0Fields`/`instance->type` design), so only a minimal,
    safe `get_string`-only fix was applied using our own existing calls;
    `mazda_v0.c`'s encoder button-code mapping doesn't match ARF's
    Up=Lock/Down=Boot/Right=Remote/Left-OK scheme at all, so the fix
    mirrors our own encoder's actual switch instead; `subaru.c`/
    `suzuki.c` already had a *more* advanced encoder pattern than ARF's
    (a global button/counter-override layer ARF doesn't have), so only
    the missing `get_string` display fix was added, mirroring our own
    encoder's exact existing pattern; `kia_v1.c`/`kia_v2.c`/`kia_v3_v4.c`
    each had zero or only partial pre-existing custom-button
    infrastructure despite ARF's diff assuming it already existed, so each
    got the missing plumbing built fresh, matching whatever numbering
    convention that file's own existing button-name lookup already used
    (never inventing a new one); `kia_v6.c` needed the same treatment and
    turned out fully portable once checked - unlike `kia_v5.c` below,
    every symbol ARF's diff referenced as pre-existing (`kia_v6_encoder_
    build_upload`, the `stored_part*`/`data_part3` AES-frame fields)
    really does already exist in our tree at the same baseline.

    **Partially ported - `renault_v1.c` (AM/FM flag fixed under item 19;
    ROLLING_CNT/BUGFIX_UI on its Hitag2 paths declined).** The flag half
    is done (see item 19's audit). The rest of this file's delta sits on
    top of ARF's `seed_tx`/`renault_v1_encoder_reencode_seed`/
    `HITAG2_SEED_RECOVERED_YES` Hitag2 forward-re-encoding feature - which
    doesn't exist in our tree under those names, because **our own
    `renault_v1.c` has a substantially different, independently-built
    Hitag2 implementation** (`hitag2_encoder_next_frame` for a
    full-recovered-key path, `hitag2_encrypt_frame` for a seed-only path,
    both already persisting Key/Key_2/Recovered/Seed unconditionally -
    more thorough than ARF's baseline in some ways). Confirmed both of our
    re-encode paths currently feed the counter through **unmodified** -
    neither advances it at all, not even by a hardcoded `+1` - so we do
    have the staleness gap ARF's fix targets. Declined to patch this
    unattended: correctly advancing a counter that feeds a real Hitag2
    authentication/hop-code calculation, on a protocol tied to a car's
    rolling-code acceptance window, is a materially different risk profile
    than the checksum-based protocols ported this pass - a mistake here
    risks permanently desyncing a real captured remote from its car
    (rolling-code systems typically tolerate a drift *window*, not
    unlimited drift), not just "doesn't open." No hardware available this
    pass to verify against. Recommend a dedicated, hardware-tested follow
    up rather than folding it into an unattended cleanup pass.

    **Same-day follow-up (2026-09-22): a second, independent renault_v1.c
    problem found while researching the counter-staleness gap above - not
    part of ARF's delta at all.** Reading `renault_v1.c` end to end turned
    up two distinct Hitag2-family cipher implementations already living in
    our tree: the classic byte-array LFSR+filter stream cipher (used for
    RFID auth and the SEED-recovery path, filter/feedback constants
    confirmed correct against proxmark3's public reference
    `armsrc/hitag2.c` via direct web lookup), and a separate
    manufacturer-specific "BCM" authenticator wrapper that reuses those
    same primitives with a different key/IV-injection schedule. Our own
    `fiat_v1.c` implements this BCM wrapper correctly - confirmed its
    constants match the classic reference. Our own `renault_v1.c`,
    however, has its **own, independently-built** BCM implementation
    (`hitag2_authenticator`/`hitag2_f20`/`hitag2_lfsr`) using **different**
    filter/feedback constants and a different LFSR construction than both
    the public reference and our own proven `fiat_v1.c` - and ARF's own
    `renault_v1.c` doesn't have this problem, because ARF explicitly
    reuses `fiat_v1`'s cipher functions for Renault outright (confirmed by
    reading ARF's `renault_v1_verify_hitag2_key()`, which calls
    `subghz_protocol_fiat_v1_get_known_keys()`/`_compute_auth()` directly,
    next to ARF's own comment stating Renault V1 "REUSES the Fiat V1
    hitag2 cipher (do not duplicate it)"). Strong evidence our BCM-model
    Renault auth has been computing wrong values since it was first built,
    independent of anything in this pass's ARF diff.

    **Written, then APPLIED 2026-09-22 on the maintainer's explicit
    request, still not hardware-confirmed.** Three fixes drafted in full
    in `claude/RENAULT_V1_STAGED_FIXES.md`: (1) swap `renault_v1.c`'s BCM
    call site to `fiat_v1.c`'s proven `subghz_protocol_fiat_v1_compute_auth()`
    instead of its own `hitag2_authenticator()`, plus a new
    `hitag2_find_known_key()` helper that tries `fiat_v1`'s known-key table
    automatically when no manual key is set; (2) the same rolling-counter
    advance pattern used elsewhere this pass (`ford_v0.c`/`honda_v1.c`/
    `fiat_v2.c`'s `mult`-aware `(cnt+mult)&MASK`), for both the
    full-key-recovered re-encode path and, lower-confidence, the seed-model
    re-encode path (its `cnt`/seed bit-interleaving in `hitag2_build_iv` is
    intricate enough to want a second look before trusting it against a
    real car); (3) D-pad button remap (`subghz_custom_btn_*`, matching
    `hitag2_get_button_name`'s existing Lock/Unlock/Trunk/Panic codes) -
    `renault_v1.c` had zero `custom_btn` references before this, so this is
    new capability, not a fix.

    Before applying, the maintainer supplied two real `.sub` captures (a
    2017 Renault Captur, Lock and Unlock) as a possible real-hardware
    pre-check. Both turned out to be RAW/undecoded captures with no `Key:`
    field, so a direct crypto test wasn't possible - instead, the actual
    shipped `feed()`/`reset()` state machines from both `renault_v0.c` and
    `renault_v1.c` were extracted verbatim into standalone simulators and
    run against every pulse in both files. Clean negative both ways:
    Renault V1's header preamble never occurs once in ~19,000/~22,500
    pulses, and Renault V0's more lenient sync never gets past 15 of the
    ~81 bits it needs. Likely explanation: both files were captured with
    the OOK preset and show a smeared, non-clustering pulse-width spread
    consistent with an OOK receiver misreading an FSK-modulated signal -
    not usable as a pre-check either way. With no way to validate against
    real hardware, the maintainer chose to apply anyway
    ("Apply your Renault fix anyway and then I will compile"); all three
    fixes above went in exactly as staged, via the same
    precondition-checked patch-script discipline used throughout this
    project (brace count 188/188, paren count 934/934, zero comments,
    every changed symbol's occurrence count verified after). This is a
    one-way, hardware-facing change to what gets authenticated against, or
    transmitted to, a real car's rolling-code receiver, applied without a
    real Renault Hitag2 capture to confirm it against - still worth a
    deliberate first real-world test once flashed, per the maintainer's own
    original caution when this was staged.

    **Declined entirely (2 of 20) - missing prerequisite infrastructure ARF's
    diff assumes but our tree never received:**
    - `kia_v5.c` - ARF's diff for this file calls
      `kia_v5_custom_to_btn()`/`kia_v5_reencrypt_and_upload()` and reads
      `instance->reencrypt_mode` as pre-existing context; none of the
      three exist anywhere in our `kia_v5.c` (confirmed: zero matches).
      Our version still has the older "pure replay" path ARF's own diff
      shows being *removed*, meaning our tree predates the ARF commit that
      originally introduced AES re-encryption-on-button-change for this
      protocol. Porting just this commit's small delta isn't possible
      without first backporting that entire larger, untested prerequisite
      feature - out of scope for a lightly-touched-file review.
    - `mazda_siemens.c` - a different, more subtle blocker. Our encoder's
      `subghz_protocol_encoder_mazda_siemens_get_upload()` builds the
      transmitted frame by decomposing `instance->generic.data` directly
      into raw bytes and incrementing a byte-level counter in place - it
      never reads `instance->generic.btn` at all. Setting `.btn` from a
      D-pad remap (as ARF's fix does) would have **zero effect on what
      actually transmits** - exactly the kind of "UI shows one thing, air
      gets another" bug this whole pass exists to prevent, not reproduce.
      Our own counter-increment already avoids exact-replay (just not
      mult-aware or button-selectable). Fixing this properly means
      modifying the proven `get_upload()` byte-encoding path itself, not
      adding an independent pre-processing step - a bigger, riskier change
      than anything else in this pass; declined unattended, flagged for a
      dedicated hardware-tested look.
    - `fiat_v2.c` (moved here from item 13, which had assumed it needed
      only the same fix as `fiat_v1.c`) - turns out to be a much larger
      gap than `fiat_v1.c`. Our `fiat_v2.c` encoder is a **pure
      byte-for-byte replay** of the "Raw" field (~35-line deserialize: read
      generic, validate, read Raw, `subghz_custom_btn_set_max(0)`
      [explicitly disabled], build upload, done) - no Hitag2 key recovery,
      no re-encoding, no Serial/Btn/Cnt override reads at all. ARF's
      version has all of that (`instance->hitag2_key_valid`,
      `instance->uid`, `fiat_v2_hop()`, a car-emulate-scene Cnt-override
      mechanism) - a whole missing feature layer, not a small delta. Our
      own `subghz_custom_btn_set_max(0)` already reflects a deliberate,
      correct choice to keep the D-pad off until (if ever) that foundation
      gets built. Same class of gap as `kia_v5.c`; not a same-day
      unattended port.

    [Commit `976a03f2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/976a03f299fcfad7b4464102b3352de1be825883)
    "Secure rollback..." (Sep 20)

24. **ProtoPirate: car-model preset picker, rebuilt as a loadable config
    plugin - UX/architecture, not a capability gap.** New
    `applications/system/ProtoPirate/scenes/plugins/
    protopirate_config_plugin.{c,h}` (a `flipper_application`-style
    loadable plugin, versioned `PROTOPIRATE_CONFIG_PLUGIN_API_VERSION 2`)
    plus a new `helpers/protopirate_models.{c,h}` and a small
    `keystore/models.txt` database let the receiver-config screen offer
    named presets (generic AM650/FM476 combos at 315/433, plus two real
    vehicles: Ford Falcon BA-FGX, Ford Territory SX-SYII) instead of
    manual frequency/modulation entry, alongside a new self-contained
    `variable_item_list.{c,h}` UI widget (ProtoPirate no longer depends on
    firmware's shared one). Our own ProtoPirate (confirmed present, kept
    stock per this project's standing policy) is on the older architecture
    - no config plugin, no `variable_item_list.c`. Nice-to-have UX polish
    with thin actual car-model coverage (2 named vehicles) rather than a
    functional gap; low priority.
    [Commit `976a03f2`](https://github.com/D4C1-Labs/Flipper-ARF/commit/976a03f299fcfad7b4464102b3352de1be825883)
    "Secure rollback..." (Sep 20)

**2026-09-21 re-pass complete.** Coverage now extends through `cc413f2f08`
("Update protocol_items.c", author timestamp Sep 20 23:42 -03:00 = Sep 21
02:42 UTC) - confirmed as the true current `main` HEAD via `git fetch`
against the real GitHub remote (done 2026-09-21; nothing landed after this
commit at fetch time). GitHub Releases were also checked as requested: the
most recent entry on
[the Releases page](https://github.com/D4C1-Labs/Flipper-ARF/releases) is
still "Dev Build dev-f7b04f13" from Jul 12, 2026 - well before this
window and every prior checkpoint. ARF's Releases page runs well behind
its commit stream and isn't a distinct signal beyond what `git log`
already shows; nothing new there.

Six commits since the prior checkpoint were reviewed (`da08b612`,
`976a03f2`, `d77ceb39`, `32b36f62`, `c056637a`, `cc413f2f`) and are covered
by items 19-24 above; `d77ceb39` ("Minor fixes..", `protocol_items.c`
only, 6 lines) was excluded as routine per this doc's own criteria. Also
checked: the `experiments` branch (`origin/experiments`, tip `6ca56247`
"more test updates, don't flash") is cleanly in sync with its own remote
and its own tip commit warns not to flash it - left out of scope,
consistent with this doc's main-branch-only methodology throughout.

Next re-pass: `git log cc413f2f08..HEAD` against the local ARF checkout
(and the equivalent for Unleashed's checkout, still at the Sep 4
checkpoint above).

**Correction made during this 2026-09-17 pass:** the previous checkpoint
table's ARF commit, `dc54f40f76a9ed0dd306f7e36ebc4ff97804b9e7`, turned out
to be a LOCAL merge commit created by that session's own `git pull` on the
local checkout (confirmed: 404s on GitHub; matches the "Merge branch
'main' of ..." shape of this session's own post-pull HEAD) - not a real
upstream commit. Recorded here so a future pass doesn't waste time looking
it up on GitHub directly; compare local-checkout state by content/date
instead of assuming a locally-recorded "checkpoint commit" resolves
upstream. Also corrected item 9's citation date: commit `2f88aff9`
("Trying multi-region support") is dated **2026-09-13** by its actual
author timestamp, not "Jul 26" as this doc previously said in two places -
the finding itself (the 7-protocol frequency-flag gap) is unaffected, only
the date label was wrong.

**2026-09-22 pass complete: items 19-21 ported, item 22 explained (not
ported, but its underlying bug confirmed present in our own code), item
23's 20 files individually resolved (16 ported, 1 partial, 3 declined with
reasons).** This pass didn't advance the checkpoint commit (still
`cc413f2f08` from the 2026-09-21 pass, above) - it went deep on work
already queued from that checkpoint rather than reviewing new upstream
history. GitHub Releases re-checked per the new standing rule above:
still "Dev Build dev-f7b04f13" (Jul 12, 2026), same as the 2026-09-21
pass - confirmed by fetching the Releases page directly today, nothing
new. ProtoPirate: untouched this pass, confirmed via `git status`/`git
diff` against `applications/system/ProtoPirate/` showing no changes -
standing policy holds.

The one new finding this pass surfaced that isn't really about ARF at all:
item 22's PSA validity-gate bug is now **confirmed present in our own
`psa.c`**, independent of whether we ever port ARF's rewrite - see item 22
for detail. Flagging it again here because it's the single highest-impact
item to come out of this pass and is easy to miss buried under "not
ported."

**2026-09-22 same-day follow-up: item 22's validity-gate fix ported, on
request.** The maintainer asked for "the psa fix" after reading this pass's
summary. Ported the confirmed bug fix (see item 22 above for full detail):
all three `validation_field`-nibble gate sites in `lib/subghz/protocols/
psa.c` now check `key1_high`'s fixed sync nibble instead. `psa2.c` checked
and deliberately left alone (ARF's own current code still uses that
pattern there - not the same bug). The adaptive AM demodulator half of the
same ARF rewrite remains **not** ported - see item 22's own "still not
ported, deliberately" note for why. Not yet compiled/flashed, same as
everything else from this window.

Next re-pass: continue from `cc413f2f08` per the note above. Item 22's
validity-gate fix is now done; the adaptive AM demod half is still open
if ever wanted (needs real multi-model hardware captures to validate, not
a same-day unattended job). Remaining priorities: the three declined
item-23 files (`kia_v5.c`, `mazda_siemens.c`, `fiat_v2.c`) and
`renault_v1.c`'s remaining Hitag2 counter gap if hardware becomes
available to test against.

**Local-checkout cleanup during this 2026-09-21 pass:** before this diff
could be trusted, the local ARF checkout had to be desnarled first, same
class of issue as the correction above. GitHub Desktop showed local `main`
"ahead 6" of `origin/main` - not manual edits (the maintainer doesn't edit
this checkout; confirmed by `git reflog` showing nothing but
`pull`/`checkout` since the original clone) but a side effect of upstream
rewriting `main`'s history at some point after Jul 25: three of the
"ahead" commits were `d4rks1d33`'s old Jul 23-25 work
(`8117e422`/`833c9ad2`/`56701a81`, the same commits already retired from
this doc's checkpoint table back on 2026-09-16) that upstream had since
dropped from the public history, and the other three were the local
checkout's own `git pull` merge commits - created automatically each time
a fast-forward wasn't possible against the rewritten history, and
resurrecting the old commits' content into the local tree each time
instead of dropping it. Resolved by fetching, then setting local `main` to
`origin/main` exactly - confirmed after: `git diff --cached origin/main`
empty, `git rev-list --left-right --count origin/main...main` = `0 0`, and
the four directories the stale commits had reintroduced
(`applications/system/ble_jammer`, `ble_scanner`, `karr_poc_flipper`,
`lib/ble_central` - all long since removed from ARF's real upstream
history) confirmed gone. Nothing was pushed to ARF's remote at any point.
Recorded here in case any earlier log or summary in this project
references those six local commit hashes - they no longer exist in the
local checkout and were never part of ARF's real upstream history to
begin with.

**2026-09-22 later same-day follow-up: item 22's adaptive AM demodulator
now also ported (previously left open above), and a second renault_v1.c
issue found and staged (see item 23).** Prompted by the standing
instruction to sweep the SD card's `.sub` captures for additional
real-hardware validation material and implement everything found. PSA's
adaptive AM demod - the one piece of item 22 explicitly left as a
"distinct, optional follow-up" above - is now ported and verified
(structurally and via a new synthetic-noise unit test; see item 22's
updated entry for full detail). Separately, deeper reading of
`renault_v1.c` while re-examining its rolling-counter call sites (already
flagged as a gap under item 23) turned up a second, independent problem:
its own BCM-cipher constants don't match either the public Hitag2
reference or our own proven `fiat_v1.c` implementation of the same
cipher, while ARF's own `renault_v1.c` avoids this entirely by reusing
`fiat_v1`'s cipher outright. Fix written and staged alongside the
counter-advance and D-pad fixes in `claude/RENAULT_V1_STAGED_FIXES.md`.
Also used the SD card's real captures opportunistically to validate two
already-shipped decoders unrelated to any ARF gap: KIA/HYU V0's CRC8 (4/4
real captures matched) and V2's nibble-CRC (2/2 matched) - both confirmed
correct as shipped, no code changes needed, recorded in
`claude/COMMIT_LOG_NEXT.md` item 29 rather than here since neither is an
ARF-gap item.

**Later the same day: all three staged renault_v1.c fixes APPLIED**, on
the maintainer's explicit request, after two real Renault Captur `.sub`
captures they supplied turned out to be RAW/undecoded and couldn't serve
as a real-hardware pre-check (see item 23's own updated entry above for
the full detail on both the capture investigation and the apply itself).
Next re-pass: continue from `cc413f2f08`; remaining priorities are the
three still-declined item-23 files (`kia_v5.c`, `mazda_siemens.c`,
`fiat_v2.c` was already closed in item 29) and a real-hardware test of
`renault_v1.c`'s now-applied fixes once a genuine decoded Renault capture
or a Renault Hitag2 test setup is available.
