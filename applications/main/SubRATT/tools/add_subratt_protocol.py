#!/usr/bin/env python3
"""
add_subratt_protocol.py - Vendors additional fixed-code SubGhz protocols
into SubRatt's private registry/menu system, following exactly the same
pattern used for the app's original 8 protocols (CAME, Nice FLO,
Chamberlain, Linear, Ansonic, SMC5326, Holtek HT12X, Princeton - see
COMMIT_LOG item 23/29 and applications/main/subratt/protocols/came.c as
the reference template).

WHY THIS EXISTS
----------------
SubRatt is an external .fap that cannot link against core firmware's
SubGhz protocol structs (none of the individual protocol symbols are
exported in targets/f7/api_symbols.csv). So, like
applications/fox/subghz_garage before it, SubRatt vendors private copies
of just the protocol logic it needs, in its own protocols/ folder, with
its own SubGhzProtocolRegistry (protocols/protocol_items.c/.h) pointing
at that private copy instead of core's registry.

Adding a protocol to SubRatt therefore always touches the same five
places:
  1. protocols/<name>.c + .h    - vendored copy of the protocol logic
                                   (copied from subghz_garage's own
                                   already-adapted copy, NOT from
                                   lib/subghz/protocols/'s originals -
                                   byte sizes differ, confirming Garage's
                                   copies are adapted, not verbatim)
  2. protocols/protocol_items.h - #include the new header
  3. protocols/protocol_items.c - add &subghz_protocol_<name> to the
                                   private SubGhzProtocolRegistry
  4. subratt_protocols.h       - SubRattFileProtocol enum slot (only if
                                   not already reserved), one
                                   SubRattAttacks slot per frequency
                                   variant, one SubRattBrand slot
  5. subratt_protocols.c       - the SubRattProtocol struct instance(s),
                                   the name/freq-name/registry/file-type
                                   table entries, and the two-level
                                   brand-group menu wiring

This script does all five mechanically, for a data-driven list of
protocols (see PROTOCOLS below), so the next protocol addition is a
config entry instead of an evening of manual C editing.

IMPORTANT CAVEAT: this brute-forces by exhaustively transmitting every
value in a protocol's *fixed* keyspace. It only makes sense for
static/fixed-code protocols with a small enough min_count_bit_for_found -
never for rolling-code protocols (KeeLoq, Nice FloR-S, Somfy, FAAC, etc.)
which will reject any out-of-sequence code. Check
subghz_protocol_<name>_const.min_count_bit_for_found in the source file
before adding an entry: 24 bits (16.7M combos) is already a long brute
force; don't add anything past that without warning the user.

USAGE
-----
    python3 add_subratt_protocol.py \
        --garage-protocols <path to subghz_garage/protocols> \
        --subratt-root    <path to applications/main/subratt>

Idempotent: re-running with the same config is a no-op for any anchor
that's already been patched (it prints "already present, skipping" and
moves on) - safe to re-run after adding a new entry to PROTOCOLS.

This performs pure textual insertion at well-known anchor points; it does
NOT parse C. Always read the diff before committing/building.
"""
import argparse
import shutil
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Protocol configuration
#
# Each entry describes one protocol source file (protocols/<key>.c/.h in
# subghz_garage) and how it plugs into SubRatt's menu. `variants` holds
# one dict per frequency SubRatt should offer as a distinct menu entry -
# most of these protocols only have one sensible frequency, unlike CAME's
# six.
# ---------------------------------------------------------------------------

PROTOCOLS = [
    {
        "key": "gate_tx",
        "brand_enum": "SubRattBrandGateTX",
        "brand_name": "GateTX",
        "file_enum": "GateTXFileProtocol",  # already reserved in the enum
        "file_enum_is_new": False,
        "file_string": "GateTX",  # already present in subratt_protocol_file_types[]
        "file_string_is_new": False,
        "type_label": "24bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackGateTX24bit433",
                "name": "GateTX 24bit 433MHz",
                "freq": 433920000,
                "freq_name": "433MHz",
                "bits": 24,
                "te": 0,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
    {
        "key": "marantec24",
        "brand_enum": "SubRattBrandMarantec24",
        "brand_name": "Marantec24",
        "file_enum": "Marantec24FileProtocol",  # already reserved
        "file_enum_is_new": False,
        "file_string": "Marantec24",  # already present in subratt_protocol_file_types[]
        "file_string_is_new": False,
        "type_label": "24bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackMarantec2424bit868",
                "name": "Marantec24 24bit 868MHz",
                # NOT 433MHz - marantec24.c declares SubGhzProtocolFlag_868,
                # not _433. Confirmed against the .c file, not assumed.
                "freq": 868350000,
                "freq_name": "868MHz",
                "bits": 24,
                "te": 0,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
    {
        "key": "legrand",
        "brand_enum": "SubRattBrandLegrand",
        "brand_name": "Legrand",
        "file_enum": "LegrandFileProtocol",  # already reserved
        "file_enum_is_new": False,
        "file_string": "Legrand",  # already present in subratt_protocol_file_types[]
        "file_string_is_new": False,
        "type_label": "18bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackLegrand18bit433",
                "name": "Legrand 18bit 433MHz",
                "freq": 433920000,
                "freq_name": "433MHz",
                "bits": 18,
                # Legrand's encoder deserialize explicitly requires a
                # non-zero "TE" field in the generated file (unlike the
                # other 5 protocols here, which hardcode their timing
                # internally) - confirmed via legrand.c's
                # subghz_protocol_encoder_legrand_deserialize().
                "te": 375,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
    {
        "key": "clemsa",
        "brand_enum": "SubRattBrandClemsa",
        "brand_name": "Clemsa",
        "file_enum": "ClemsaFileProtocol",  # already reserved
        "file_enum_is_new": False,
        "file_string": "Clemsa",  # already present in subratt_protocol_file_types[]
        "file_string_is_new": False,
        "type_label": "18bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackClemsa18bit433",
                "name": "Clemsa 18bit 433MHz",
                "freq": 433920000,
                "freq_name": "433MHz",
                "bits": 18,
                "te": 0,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
    {
        "key": "bett",
        "brand_enum": "SubRattBrandBett",
        "brand_name": "BETT",
        "file_enum": "BETTFileProtocol",  # already reserved
        "file_enum_is_new": False,
        "file_string": "BETT",  # already present in subratt_protocol_file_types[]
        "file_string_is_new": False,
        "type_label": "18bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackBett18bit433",
                "name": "BETT 18bit 433MHz",
                "freq": 433920000,
                "freq_name": "433MHz",
                "bits": 18,
                "te": 0,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
    {
        "key": "megacode",
        "brand_enum": "SubRattBrandMegaCode",
        "brand_name": "MegaCode",
        "file_enum": "MegaCodeFileProtocol",  # NEW - not previously reserved
        "file_enum_is_new": True,
        "file_string": "MegaCode",  # NEW - not previously in subratt_protocol_file_types[]
        "file_string_is_new": True,
        "type_label": "24bit",
        "variants": [
            {
                "attack_enum": "SubRattAttackMegaCode24bit315",
                "name": "MegaCode 24bit 315MHz",
                # NOT 433MHz - megacode.c declares SubGhzProtocolFlag_315,
                # not _433.
                "freq": 315000000,
                "freq_name": "315MHz",
                "bits": 24,
                "te": 0,
                "repeat": 3,
                "preset": "FuriHalSubGhzPresetOok650Async",
            },
        ],
    },
]


def _fill_var_idents(protocols: list) -> None:
    """Derive each variant's C identifier suffix (subratt_protocol_<this>)
    from <key>_<bits>bit_<freq>, matching the existing naming convention
    (e.g. subratt_protocol_came_12bit_303, subratt_protocol_unilarm_24bit_330),
    unless a variant already specifies "var_ident" explicitly."""
    for protocol in protocols:
        for v in protocol["variants"]:
            if "var_ident" in v:
                continue
            freq_suffix = v["freq_name"].replace("MHz", "").replace(".", "_")
            v["var_ident"] = f'{protocol["key"]}_{v["bits"]}bit_{freq_suffix}'


def anchored_insert(text: str, anchor: str, insertion: str, *, before: bool, label: str) -> str:
    """Insert `insertion` immediately before/after the unique `anchor`
    substring. Raises if anchor isn't found exactly once (drift guard);
    if `insertion` is already present, no-ops so re-runs stay idempotent."""
    if insertion.strip() and insertion.strip() in text:
        print(f"  [skip] {label}: already present")
        return text
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"anchor for {label!r} matched {count} times (expected 1): {anchor[:80]!r}")
    if before:
        return text.replace(anchor, insertion + anchor, 1)
    else:
        return text.replace(anchor, anchor + insertion, 1)


def vendor_files(protocol: dict, garage_dir: Path, subratt_protocols_dir: Path) -> None:
    key = protocol["key"]
    for ext in (".c", ".h"):
        src = garage_dir / f"{key}{ext}"
        dst = subratt_protocols_dir / f"{key}{ext}"
        if not src.exists():
            raise FileNotFoundError(f"missing source file: {src}")
        if dst.exists():
            print(f"  [skip] {dst.name}: already vendored")
            continue
        shutil.copyfile(src, dst)
        print(f"  [copy] {src.name} -> {dst}")


def patch_protocol_items_h(protocol: dict, path: Path) -> None:
    text = path.read_text()
    key = protocol["key"]
    anchor = '\nextern const SubGhzProtocolRegistry subratt_subghz_protocol_registry;'
    insertion = f'#include "{key}.h"\n'
    text = anchored_insert(text, anchor, insertion, before=True, label=f"protocol_items.h include {key}")
    path.write_text(text)


def patch_protocol_items_c(protocol: dict, path: Path) -> None:
    text = path.read_text()
    key = protocol["key"]
    anchor = "};\n\nconst SubGhzProtocolRegistry subratt_subghz_protocol_registry"
    insertion = f"    &subghz_protocol_{key},\n"
    text = anchored_insert(
        text, anchor, insertion, before=True, label=f"protocol_items.c registry entry {key}")
    path.write_text(text)


def patch_subratt_protocols_h(protocol: dict, path: Path) -> None:
    text = path.read_text()

    if protocol["file_enum_is_new"]:
        anchor = "    UnknownFileProtocol,\n    TotalFileProtocol,\n} SubRattFileProtocol;"
        insertion = f'    {protocol["file_enum"]},\n'
        text = anchored_insert(
            text, anchor, insertion, before=True,
            label=f"SubRattFileProtocol slot {protocol['file_enum']}")

    attacks_anchor = "    SubRattAttackLoadFile,\n    SubRattAttackTotalCount,\n} SubRattAttacks;"
    attacks_insertion = "".join(f'    {v["attack_enum"]},\n' for v in protocol["variants"])
    text = anchored_insert(
        text, attacks_anchor, attacks_insertion, before=True,
        label=f"SubRattAttacks slots for {protocol['key']}")

    brand_anchor = "    SubRattBrandLoadFile,\n    SubRattBrandCount,\n} SubRattBrand;"
    brand_insertion = f'    {protocol["brand_enum"]},\n'
    text = anchored_insert(
        text, brand_anchor, brand_insertion, before=True,
        label=f"SubRattBrand slot {protocol['brand_enum']}")

    path.write_text(text)


def patch_subratt_protocols_c(protocol: dict, path: Path) -> None:
    text = path.read_text()
    key = protocol["key"]

    # 1. SubRattProtocol struct instance(s) - inserted right before the
    #    "BF existing dump" placeholder block, i.e. as the last "real"
    #    protocol(s) before the load-from-file fallback.
    struct_anchor = "/**\n * BF existing dump\n */"
    struct_blocks = []
    for v in protocol["variants"]:
        struct_blocks.append(
            f'/**\n * {v["name"]}\n */\n'
            f'const SubRattProtocol subratt_protocol_{v["var_ident"]} = {{\n'
            f'    .frequency = {v["freq"]},\n'
            f'    .bits = {v["bits"]},\n'
            f'    .te = {v["te"]},\n'
            f'    .repeat = {v["repeat"]},\n'
            f'    .preset = {v["preset"]},\n'
            f'    .file = {protocol["file_enum"]}}};\n\n'
        )
    text = anchored_insert(
        text, struct_anchor, "".join(struct_blocks), before=True,
        label=f"SubRattProtocol struct(s) for {key}")

    # 2. subratt_protocol_names[]
    names_anchor = '    [SubRattAttackLoadFile] = "BF existing dump",\n    [SubRattAttackTotalCount] = "Total Count",\n};'
    names_insertion = "".join(
        f'    [{v["attack_enum"]}] = "{v["name"]}",\n' for v in protocol["variants"])
    text = anchored_insert(
        text, names_anchor, names_insertion, before=True,
        label=f"subratt_protocol_names[] entries for {key}")

    # 3. subratt_protocol_registry[]
    registry_anchor = "    [SubRattAttackLoadFile] = &subratt_protocol_load_file};"
    registry_lines = []
    for v in protocol["variants"]:
        registry_lines.append(
            f'    [{v["attack_enum"]}] = &subratt_protocol_{v["var_ident"]},\n')
    text = anchored_insert(
        text, registry_anchor, "".join(registry_lines), before=True,
        label=f"subratt_protocol_registry[] entries for {key}")

    # 4. Brand grouping arrays - inserted right before the brand_groups[]
    #    table itself, alongside the other per-brand attacks[]/types[]
    #    arrays.
    groups_anchor = "static const SubRattBrandGroup subratt_brand_groups[] = {"
    attacks_array_name = f"{key}_{protocol['type_label']}_attacks"
    types_array_name = f"{key}_types"
    group_block = (
        f"static const SubRattAttacks {attacks_array_name}[] = {{\n"
        + "".join(f'    {v["attack_enum"]},\n' for v in protocol["variants"])
        + "};\n"
        f"static const SubRattTypeGroup {types_array_name}[] = {{\n"
        f'    {{.name = "{protocol["type_label"]}", .attacks = {attacks_array_name}, .attack_count = {len(protocol["variants"])}}},\n'
        f"}};\n\n"
    )
    text = anchored_insert(
        text, groups_anchor, group_block, before=True,
        label=f"brand grouping arrays for {key}")

    # 5. subratt_brand_groups[] entry
    brand_groups_anchor = '    [SubRattBrandLoadFile] = {"BF existing dump", load_file_types, 1},\n};'
    brand_entry = f'    [{protocol["brand_enum"]}] = {{"{protocol["brand_name"]}", {types_array_name}, 1}},\n'
    text = anchored_insert(
        text, brand_groups_anchor, brand_entry, before=True,
        label=f"subratt_brand_groups[] entry for {key}")

    # 6. subratt_protocol_freq_names[] - note this table's LoadFile line
    #    is NOT followed by a TotalCount entry (unlike names[] above), so
    #    it needs a different anchor to stay unique.
    freq_anchor = '    [SubRattAttackLoadFile] = "BF existing dump",\n};\n\n// --- End brand grouping data ---'
    freq_insertion = "".join(
        f'    [{v["attack_enum"]}] = "{v["freq_name"]}",\n' for v in protocol["variants"])
    text = anchored_insert(
        text, freq_anchor, freq_insertion, before=True,
        label=f"subratt_protocol_freq_names[] entries for {key}")

    # 7. subratt_protocol_file_types[] - only if this protocol needed a
    #    brand-new SubRattFileProtocol enum slot (most of these 6 reuse
    #    slots the enum already reserved, with the string already wired).
    if protocol["file_string_is_new"]:
        file_types_anchor = '    [UnknownFileProtocol] = "Unknown"};'
        file_types_insertion = f'    [{protocol["file_enum"]}] = "{protocol["file_string"]}",\n'
        text = anchored_insert(
            text, file_types_anchor, file_types_insertion, before=True,
            label=f"subratt_protocol_file_types[] entry for {key}")

    path.write_text(text)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--garage-protocols", required=True, type=Path,
                     help="path to applications/fox/subghz_garage/protocols")
    ap.add_argument("--subratt-root", required=True, type=Path,
                     help="path to applications/main/subratt")
    ap.add_argument("--only", nargs="*", default=None,
                     help="restrict to these protocol keys (default: all in PROTOCOLS)")
    args = ap.parse_args()

    subratt_protocols_dir = args.subratt_root / "protocols"
    protocol_items_h = subratt_protocols_dir / "protocol_items.h"
    protocol_items_c = subratt_protocols_dir / "protocol_items.c"
    subratt_protocols_h = args.subratt_root / "subratt_protocols.h"
    subratt_protocols_c = args.subratt_root / "subratt_protocols.c"

    for p in (subratt_protocols_dir, protocol_items_h, protocol_items_c,
              subratt_protocols_h, subratt_protocols_c):
        if not p.exists():
            print(f"error: expected path not found: {p}", file=sys.stderr)
            return 1

    protocols = PROTOCOLS
    _fill_var_idents(protocols)
    if args.only:
        wanted = set(args.only)
        protocols = [p for p in protocols if p["key"] in wanted]
        missing = wanted - {p["key"] for p in protocols}
        if missing:
            print(f"error: unknown protocol key(s): {sorted(missing)}", file=sys.stderr)
            return 1

    for protocol in protocols:
        print(f"=== {protocol['key']} ===")
        vendor_files(protocol, args.garage_protocols, subratt_protocols_dir)
        patch_protocol_items_h(protocol, protocol_items_h)
        patch_protocol_items_c(protocol, protocol_items_c)
        patch_subratt_protocols_h(protocol, subratt_protocols_h)
        patch_subratt_protocols_c(protocol, subratt_protocols_c)

    print("\nDone. Review the diffs, then build with fbt before committing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
