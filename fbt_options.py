from pathlib import Path
import os
import posixpath

# For more details on these options, run 'fbt -h'

FIRMWARE_ORIGIN = "FoxFW"

# Default hardware target
TARGET_HW = 7

# Optimization flags
## Optimize for size
COMPACT = 1
## Optimize for debugging experience
DEBUG = 0

# Suffix to add to files when building distribution
# If OS environment has DIST_SUFFIX set, it will be used instead
DIST_SUFFIX = "local"

if not os.environ.get("DIST_SUFFIX"):
    def git(*args):
        import subprocess

        return (
            subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL)
            .decode()
            .strip()
        )

    # firmware.ver (repo root) is the single source of truth for the release
    # version - bump it there and every build re-tags HEAD to match, no
    # manual "git tag" needed. Line 1 is the firmware name, line 2 is the
    # version - same format scripts/version.py's GitVersion reads, so there's
    # only ever one file to keep in sync.
    version_file = Path("firmware.ver")
    version_tag = ""
    if version_file.exists():
        lines = [line.strip() for line in version_file.read_text().splitlines() if line.strip()]
        if len(lines) >= 2:
            version_tag = lines[1]
        elif len(lines) == 1:
            version_tag = lines[0]

    if version_tag:
        try:
            git("tag", "-f", version_tag)
        except Exception:
            pass
        DIST_SUFFIX = f"foxfw-{version_tag}"
    else:
        try:
            # If HEAD is exactly on a release tag, e.g. v2.0.1 -> foxfw-v2.0.1
            tag_name = git("describe", "--tags", "--abbrev=0", "--exact-match")
            DIST_SUFFIX = f"foxfw-{tag_name}"
        except Exception:
            # Otherwise this is a dev build: foxfw-(branch)-(commit)
            try:
                branch_name = git("rev-parse", "--abbrev-ref", "HEAD")
                commit_sha = git("rev-parse", "HEAD")[:8]
                DIST_SUFFIX = f"foxfw-{branch_name}-{commit_sha}"
            except Exception:
                DIST_SUFFIX = "local"
    DIST_SUFFIX = DIST_SUFFIX.replace("/", "-")
    # scripts/version.py reads DIST_SUFFIX straight from the OS environment
    # (see FORWARDED_ENV_VARIABLES in scripts/fbt/util.py), so this has to be
    # a real env var, not just this file's Python variable, for VERSION to
    # actually pick up the computed value below.
    os.environ["DIST_SUFFIX"] = DIST_SUFFIX
# FIRMWARE_ORIGIN (below) is what qFlipper shows as "FIRMWARE" - it stays "FoxFW"
# regardless of DIST_SUFFIX. DIST_SUFFIX only feeds VERSION, used by the
# in-app update checker and dist/output file naming.

# Coprocessor firmware
COPRO_OB_DATA = "scripts/ob.data"

# Must match lib/stm32wb_copro version
COPRO_CUBE_VERSION = "1.20.0"

COPRO_CUBE_DIR = "lib/stm32wb_copro"

# Default radio stack
COPRO_STACK_BIN = "stm32wb5x_BLE_Stack_light_fw.bin"
# Firmware also supports "ble_full", but it might not fit into debug builds
COPRO_STACK_TYPE = "ble_light"

# Leave 0 to let scripts automatically calculate it
COPRO_STACK_ADDR = "0x0"

# If you override COPRO_CUBE_DIR on commandline, override this as well
COPRO_STACK_BIN_DIR = posixpath.join(COPRO_CUBE_DIR, "firmware")

# Supported toolchain versions
# Also specify in scripts/ufbt/SConstruct
FBT_TOOLCHAIN_VERSIONS = (" 12.3.", " 13.2.")

OPENOCD_OPTS = [
    "-f",
    "interface/stlink.cfg",
    "-c",
    "transport select hla_swd",
    "-f",
    "${FBT_DEBUG_DIR}/stm32wbx.cfg",
    "-c",
    "stm32wbx.cpu configure -rtos auto",
]

SVD_FILE = "${FBT_DEBUG_DIR}/STM32WB55_CM4.svd"

# Look for blackmagic probe on serial ports and local network
BLACKMAGIC = "auto"

# Application to start on boot
LOADER_AUTOSTART = ""

FIRMWARE_APPS = {
    "default": [
        # Svc
        "basic_services",
        # Apps
        "main_apps",
        "system_apps",
        # Settings
        "settings_apps",
    ],
    "unit_tests": [
        "basic_services",
        "updater_app",
        "radio_device_cc1101_ext",
        "unit_tests",
        "infrared",
        "archive",
    ],
}

FIRMWARE_APP_SET = "default"

custom_options_fn = "fbt_options_local.py"

if Path(custom_options_fn).exists():
    exec(compile(Path(custom_options_fn).read_text(), custom_options_fn, "exec"))
