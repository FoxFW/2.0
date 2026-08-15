#!/usr/bin/env bash
# fbt_build_menu.sh - interactive FoxFW build menu (Linux/macOS)

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd -P)"
cd "$SCRIPT_DIR" || exit 1

LOG_FILE="fbt_build_output.tmp.log"
DATA_FILE="fbt_build_size.txt"

clean_workspace() {
    rm -rf build dist .sconsign.dblite
}

# retries once if the log shows a permission/denied error
run_with_retry() {
    local desc="$1"
    shift
    while true; do
        echo "$desc"
        set -o pipefail
        "$SCRIPT_DIR/fbt" "$@" 2>&1 | tee "$LOG_FILE" | awk '
            {
                print;
                fflush();
                l = tolower($0);
                if (l ~ /permission|denied/) flag = 1;
                if (flag && l ~ /error/) restart = 1;
            }
            END { exit !restart }'
        awk_status=$?
        set +o pipefail
        if [ "$awk_status" -eq 0 ]; then
            echo
            echo "Permission/Denied + Error detected! Retrying command..."
            echo
            continue
        fi
        break
    done
}

size_report() {
    python3 - "$LOG_FILE" "$DATA_FILE" <<'PYEOF'
import os
import re
import sys
import time

log_path, data_path = sys.argv[1], sys.argv[2]

try:
    with open(log_path) as f:
        lines = f.read().splitlines()
except FileNotFoundError:
    print("Build log not found - skipping firmware size report.")
    sys.exit(0)

needed = ("text", "rodata", "data", "bss")
heading_idxs = [i for i, l in enumerate(lines) if l.strip() == "Firmware size"]
if not heading_idxs:
    print("Could not find a 'Firmware size' section in the build log - skipping report.")
    sys.exit(0)

section_re = re.compile(r"^\.(text|rodata|data|bss)\s+(\d+)\s+\(")
free_flash_re = re.compile(r"^\.free_flash\s+(\d+)\s+\(")


def get_block(idx):
    sizes = {}
    has_free_flash = False
    trailing = 0
    for j in range(idx + 1, min(idx + 61, len(lines))):
        line = lines[j].strip()
        m = section_re.match(line)
        if m:
            sizes[m.group(1)] = int(m.group(2))
            continue
        if free_flash_re.match(line):
            has_free_flash = True
            continue
        if len(sizes) >= len(needed):
            trailing += 1
            if trailing >= 3 or has_free_flash:
                break
    return sizes, has_free_flash


# prefer a block with the .free_flash marker over the first complete block found
sizes = None
for idx in heading_idxs:
    blk_sizes, has_ff = get_block(idx)
    if len(blk_sizes) >= len(needed) and has_ff:
        sizes = blk_sizes
        break
if sizes is None:
    for idx in heading_idxs:
        blk_sizes, _ = get_block(idx)
        if len(blk_sizes) >= len(needed):
            sizes = blk_sizes
            break

if not sizes or not set(needed).issubset(sizes):
    print("Could not find a complete firmware size block in the build log - skipping report.")
    sys.exit(0)

FLASH_BASE = 0x08000000
RADIO_ADDRESS = 0x080D7000
CEILING = RADIO_ADDRESS - FLASH_BASE

new_text, new_rodata, new_data, new_bss = (sizes[k] for k in needed)
new_flash_used = new_text + new_rodata + new_data
new_free = CEILING - new_flash_used

prev = {}
if os.path.exists(data_path):
    with open(data_path) as f:
        for line in f:
            line = line.strip()
            if "=" in line:
                k, v = line.split("=", 1)
                try:
                    prev[k] = int(v)
                except ValueError:
                    pass


def fmt(n):
    return f"{n:,} bytes ({n / 1024:.2f} Kb)"


def fmt_diff(old, new):
    d = new - old
    sign = "+" if d > 0 else ""
    return f"{sign}{d:,} bytes ({sign}{d / 1024:.2f} Kb)"


print()
print("=" * 90)
print("INTERNAL FLASH SIZE REPORT (bytes, with Kb shown alongside)")
print(" (core internal firmware only - excludes external applications, etc.)")
print("=" * 90)

have_prev = set(needed).issubset(prev.keys())
if have_prev:
    prev_flash_used = prev["text"] + prev["rodata"] + prev["data"]
    prev_free = CEILING - prev_flash_used
    rows = [
        (".text", prev["text"], new_text),
        (".rodata", prev["rodata"], new_rodata),
        (".data", prev["data"], new_data),
        (".bss (RAM)", prev["bss"], new_bss),
    ]
    print(f"{'Metric':<12}{'Current Build':<30}{'Difference':<30}{'Last Build':<30}")
    print("-" * 90)
    for name, old, new in rows:
        print(f"{name:<12}{fmt(new):<30}{fmt_diff(old, new):<30}{fmt(old):<30}")
    print("=" * 90)
    print(f"{'FREE SPACE':<12}{fmt(new_free):<30}{fmt_diff(prev_free, new_free):<30}{fmt(prev_free):<30}")
else:
    print("No previous build data found - this run is the new baseline.")
    print()
    print(f"{'Metric':<12}{'Value':<30}")
    print("-" * 90)
    for name, val in ((".text", new_text), (".rodata", new_rodata), (".data", new_data), (".bss (RAM)", new_bss)):
        print(f"{name:<12}{fmt(val):<30}")
    print("=" * 90)
    print(f"{'FREE SPACE':<12}{fmt(new_free):<30}")
print("=" * 90)
print()

timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
with open(data_path, "w") as f:
    f.write(f"Last Build: {timestamp}\n")
    f.write(f"text={new_text}\n")
    f.write(f"rodata={new_rodata}\n")
    f.write(f"data={new_data}\n")
    f.write(f"bss={new_bss}\n")
    f.write(f"FlashUsed={new_flash_used}\n")
    f.write(f"FreeSpace={new_free}\n")
PYEOF
}

pause_return() {
    echo
    read -r -p "Press ENTER to return to the Main Menu... " _
}

show_menu() {
    cat <<'EOF'

===================================================
 FBT SHORTCUT MENU
===================================================

 [1] Clean Workspace + Generate TGZ & DFU [updater_package]
 [2] Clean Workspace + Flash Live USB [flash_usb_full]
 [3] Native Fast Compilation Pass Only [fbt]
 [4] Min-Package (DFU ONLY) [fbt updater_minpackage]
 [5] Run fbt -c
 [6] Delete Build, Dist, & scons.dblite
 [7] Delete Build, Dist, & scons.dblite + run fbt -c

 [8] Exit FBT Shortcut Menu

===================================================

EOF
}

while true; do
    show_menu
    read -rsn1 -p "Enter Selection: " choice
    echo
    echo

    case "$choice" in
        2)
            echo "==================================================="
            echo "Cleaning workspace and launching live USB flash..."
            echo "==================================================="
            clean_workspace
            "$SCRIPT_DIR/fbt" -c
            run_with_retry "Running live USB flash..." flash_usb_full COMPACT=1 DEBUG=0 FORCE=1
            size_report
            pause_return
            ;;
        1)
            echo "==================================================="
            echo "Cleaning workspace and generating updater package..."
            echo "==================================================="
            clean_workspace
            "$SCRIPT_DIR/fbt" -c
            run_with_retry "Running updater package..." updater_package COMPACT=1 DEBUG=0 FORCE=1
            size_report
            pause_return
            ;;
        5)
            echo "==================================================="
            echo "Running fbt clean (-c)..."
            echo "==================================================="
            "$SCRIPT_DIR/fbt" -c
            pause_return
            ;;
        6)
            echo "==================================================="
            echo "Deleting build, dist, & scons.dblite..."
            echo "==================================================="
            clean_workspace
            echo "Done."
            pause_return
            ;;
        7)
            echo "==================================================="
            echo "Deleting files and running fbt clean..."
            echo "==================================================="
            clean_workspace
            "$SCRIPT_DIR/fbt" -c
            pause_return
            ;;
        3)
            echo "==================================================="
            echo "Launching native fast compilation pass only..."
            echo "==================================================="
            set -o pipefail
            "$SCRIPT_DIR/fbt" 2>&1 | tee "$LOG_FILE"
            set +o pipefail
            size_report
            pause_return
            ;;
        4)
            echo "==================================================="
            echo "Running min-package (DFU only)..."
            echo "==================================================="
            set -o pipefail
            "$SCRIPT_DIR/fbt" updater_minpackage 2>&1 | tee "$LOG_FILE"
            set +o pipefail
            size_report
            pause_return
            ;;
        8)
            echo "Exiting..."
            rm -f "$LOG_FILE"
            exit 0
            ;;
        *)
            ;;
    esac
done
