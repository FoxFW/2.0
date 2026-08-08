<div align="center">
  <h1> FoxFW v2.0</h1>
  <p><em>A custom Flipper Zero firmware built for automotive research and advanced ESP32 manipulation.</em></p>
  <p>
    <a href="https://foxfw.github.io/fox-web/">Full User Guide</a>
    &nbsp;·&nbsp;
    <a href="https://foxfw.github.io/fox-web/screenshots/screenshots.html">SCREENSHOTS!</a>
    &nbsp;·&nbsp;
    <a href="https://foxfw.github.io/fox-web/fap-compiler.html">FAP Compiler</a>
    &nbsp;·&nbsp;
    <a href="mailto:foxcustomfirmware@gmail.com">Support</a>
    &nbsp;·&nbsp;
    <a href="https://www.github.com/SamMichaelINC/FoxFW">View Legacy v1.0</a>
  </p>
</div>

---

## What is FoxFW?

FoxFW is a fully custom Flipper Zero firmware that extends the official release
with deeper security tools, a smarter file browser, enhanced Sub-GHz analysis,
a full suite of ESP32 companion apps, and a polished system-wide experience.
It is designed around three goals: **security**, **usability**, and **deeper
RF tooling**. I've used a fresh push for this version because you dont need to
know that it took me 126 tries (and 56 versions) to get the Web Flasher to work.

---

## Highlights

| Feature | Description |
|---|---|
| **Sub-GHz** | Completely overhauled Sub-GHz app with all new features like Modulation Analyzer, RF Jammer and Garage Door Remote all included! |
| **Fox ESP32 App Suite** | A full suite of companion apps that turn a wired ESP32 into a WiFi/BLE/HTTP extension of your Flipper — see below |
| **PIN Lock System** | Configurable attempt limits, auto-lock timer, and an On Exceed lock action |
| **Disconnect on Lock** | Individually toggle BLE, GPIO, and USB disconnection when the screen locks |
| **Fox File Browser** | SD card browser with Favorites, firmware install, and app launch — replaces the stock Archive |
| **Modulation Analyzer** | FoxFW-exclusive: find what modulation an unknown device uses, then open the Receiver pre-configured for it |
| **Waterfall Display** | Scrolling signal history alongside the live RSSI view — great for spotting intermittent bursts |
| **RAW Waveform Editor** | Trim Sub-GHz RAW captures in-app without leaving the device |
| **Protocol Filters** | Enable/disable individual Sub-GHz protocols — fewer active protocols = faster, more accurate decoding |
| **Bounce-Scroll Text** | System-wide: labels too long to fit bounce left and right rather than truncating — works in every menu |
| **Custom Wallpaper** | Multiple 128×64 XBM wallpapers stored in `/ext/wallpapers/`, switchable from Fox Settings; a default is auto-generated if none exist |
| **First-Boot Wizard** | Guided setup for device name and PIN on every fresh install |

---

## Fox ESP32 App Suite

Wire a cheap ESP32 dev board to your Flipper's GPIO header (four jumper
wires — no special adapter) and these apps turn it into a WiFi, Bluetooth,
and internet extension of your Flipper. All of it runs on top of a separate
companion firmware, **Fox ESP32 Firmware**, flashed once to the ESP32 itself.

| App | What it does |
|---|---|
| **Fox ESP32 Commander** | The main control hub — WiFi recon and attacks, BLE scanning and tag detection, an HTTP/WebSocket bridge, the FoxScript engine, and a raw Terminal |
| **Fox ESP32 Detector** | Diagnostic tool — scans every GPIO pin pair and baud rate to identify a connected ESP32 and confirm your wiring |
| **Fox ESP32 Flasher** | Flash Fox ESP32 Firmware onto a connected board directly from the Flipper — no PC, no browser, no files to download |
| **Fox Update Downloader** | Checks GitHub for newer FoxFW and Fox ESP32 Firmware releases, downloads them over the ESP32 bridge, and hands off to install |
| **Fox File Downloader** | App Catalog browsing/installs, GitHub repo installs, and resumable URL/file downloads — all over the ESP32 bridge |
| **Fox ESP32 Terminal** | A focused live-terminal companion — streaming console, quick command send, and per-session logs saved to your SD card |
| **Fox Chat** | Post to and read from a Discord channel through the ESP32's internet connection |
| **Fox Portal** | A configurable, consent-based lead-capture WiFi captive portal with QR join and auto-detected field logging |
| **Fox Chameleon** | BLE bridge between the Flipper and a Chameleon Ultra, relayed over the ESP32's BLE stack |

> **Legal notice:** WiFi/BLE reconnaissance and attack commands are gated
> behind a single Attacks toggle intended for authorized security testing and
> research only. Know your local telecommunications regulations before
> transmitting anything.

Source code, releases, and full board wiring for the ESP32 side live in a
separate repository: **[github.com/FoxFW/Fox_ESP32_FW](https://github.com/FoxFW/Fox_ESP32_FW)**.
The complete walkthrough for every app above — menus, commands, and board
compatibility — is in `FoxESP32_Help.html`, also linked from the
[Full User Guide](https://foxfw.github.io/fox-web/).

---

## Automotive & RF Research

FoxFW is primarily intended for **automotive security research**, including key fob
analysis, signal capture, tyre-pressure sensor monitoring, and remote-access
system investigation. The Sub-GHz app ships with protocol decoders and encoders
for the following manufacturers and systems — all within the 315 MHz, 433 MHz,
and 868 MHz bands commonly used in automotive applications.

> **Legal notice:** This functionality is intended for research on systems you
> own or have explicit authorisation to test. Always comply with your local
> radio and telecommunications regulations.

### Automotive Protocols

| Manufacturer | Protocol | Frequency | Mod | Encoder | Decoder | CRC |
|---|---|---|---|:---:|:---:|:---:|
| VAG (VW / Audi / Skoda / Seat) | VAG GROUP | 433 MHz | AM | ✓ | ✓ | — |
| Porsche | Porsche AG | 433 / 868 MHz | AM | ✓ | ✓ | — |
| PSA (Peugeot / Citroën / DS) | PSA GROUP | 433 MHz | AM/FM | ✓ | ✓ | ✓ |
| Ford | Ford V0 | 315 / 433 MHz | AM | ✓ | ✓ | ✓ |
| Ford | Ford V1 | 315 / 433 MHz | FM | ✓ | ✓ | ✓ |
| Fiat | Fiat SpA | 433 MHz | AM | ✓ | ✓ | ✓ |
| Fiat | Marelli / Delphi | 433 MHz | AM | — | ✓ | ✓ |
| Renault (older models) | Marelli | 433 MHz | AM | — | ✓ | — |
| Mazda | Siemens 5WK49365D | 315 / 433 MHz | AM/FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V0 | 433 MHz | FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V1 | 315 / 433 MHz | AM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V2 | 315 / 433 MHz | AM/FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V3 / V4 | 315 / 433 MHz | AM/FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V5 | 433 MHz | FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA/HYU V6 | 433 MHz | FM | ✓ | ✓ | ✓ |
| Kia / Hyundai | KIA V7 | 433 MHz | FM | ✓ | ✓ | ✓ |
| Subaru | Subaru | 433 MHz | AM | ✓ | ✓ | — |
| Suzuki | Suzuki | 433 MHz | FM | ✓ | ✓ | ✓ |
| Mitsubishi | Mitsubishi V0 | 868 MHz | FM | ✓ | ✓ | — |
| Honda | Honda Type A / B | 433 MHz | FM (custom) | ✓ | ✓ | — |
| Honda | Honda Static | 433 MHz | AM | ✓ | ✓ | — |
| Chrysler / Dodge / Jeep | FOBIK GQ43VT | 315 / 433 MHz | AM | ✓ | ✓ | — |
| Starline | Star Line | 433 MHz | AM | ✓ | ✓ | — |
| Scher-Khan | Scher-Khan | 433 MHz | FM | ✓ | ✓ | — |
| Scher-Khan | Magic Code PRO1 / PRO2 | 433 MHz | FM | ✓ | ✓ | ✓ |
| Sheriff | Sheriff CFM ZX750 / ZX930 | 433 MHz | AM | ✓ | ✓ | — |

### Gate & Access Control Protocols

| Protocol | Frequency | Mod | Encoder | Decoder | CRC |
|---|---|---|:---:|:---:|:---:|
| Keeloq | 315 / 433 / 868 MHz | AM | ✓ | ✓ | — |
| Nice FLO | 433 MHz | AM | ✓ | ✓ | — |
| Nice FloR-S | 433 MHz | AM | ✓ | ✓ | ✓ |
| CAME | 315 / 433 MHz | AM | ✓ | ✓ | — |
| CAME TWEE | 433 MHz | AM | ✓ | ✓ | — |
| CAME Atomo | 433 MHz | AM | ✓ | ✓ | — |
| Faac SLH | 433 / 868 MHz | AM | ✓ | ✓ | — |
| Holtek | 433 MHz | AM | ✓ | ✓ | — |
| Holtek HT12x | 433 MHz | AM | ✓ | ✓ | — |
| Somfy Telis | 433 MHz | AM | ✓ | ✓ | ✓ |
| Somfy Keytis | 433 MHz | AM | ✓ | ✓ | ✓ |
| Alutech AT-4N | 433 MHz | AM | ✓ | ✓ | ✓ |
| Keyfinder | 433 MHz | AM | ✓ | ✓ | — |
| KingGates Stylo4k | 433 MHz | AM | ✓ | ✓ | — |
| Beninca ARC | 433 MHz | AM | ✓ | ✓ | — |
| Hormann HSM | 433 / 868 MHz | AM | ✓ | ✓ | — |
| Marantec | 433 MHz | AM | ✓ | ✓ | ✓ |
| Marantec24 | 433 MHz | AM | ✓ | ✓ | ✓ |

### General RF Protocols

| Protocol | Frequency | Mod | Encoder | Decoder | CRC |
|---|---|---|:---:|:---:|:---:|
| Princeton | 315 / 433 MHz | AM | ✓ | ✓ | — |
| Linear | 315 MHz | AM | ✓ | ✓ | — |
| LinearDelta3 | 315 MHz | AM | ✓ | ✓ | — |
| GateTX | 433 MHz | AM | ✓ | ✓ | — |
| Security+ 1.0 | 315 MHz | AM | ✓ | ✓ | — |
| Security+ 2.0 | 315 MHz | AM | ✓ | ✓ | — |
| Chamberlain Code | 315 MHz | AM | ✓ | ✓ | — |
| MegaCode | 315 MHz | AM | ✓ | ✓ | — |
| Mastercode | 433 MHz | AM | ✓ | ✓ | — |
| Dickert MAHS | 433 MHz | AM | ✓ | ✓ | — |
| SMC5326 | 433 MHz | AM | ✓ | ✓ | — |
| Phoenix V2 | 433 MHz | AM | ✓ | ✓ | — |
| Doitrand | 433 MHz | AM | ✓ | ✓ | — |
| Hay21 | 433 MHz | AM | ✓ | ✓ | — |
| Revers RB2 | 433 MHz | AM | ✓ | ✓ | — |
| Roger | 433 MHz | AM | ✓ | ✓ | — |

<details>
<summary>Column guide</summary>

| Column | Meaning |
|---|---|
| **Mod** | Radio modulation — AM = OOK/ASK, FM = FSK |
| **Encoder** | FoxFW can transmit / replay signals in this protocol |
| **Decoder** | FoxFW can receive and decode signals in this protocol |
| **CRC** | Protocol includes a checksum — decoded frames are validated |

</details>

---

## Full User Guide

The complete guide covers every feature in detail — first-boot setup, all Fox
Settings options, the Fox File Browser, the Sub-GHz toolset, the Fox ESP32
App Suite, firmware updates, and lockout recovery.

**[→ Open the Full User Guide](https://foxfw.github.io/fox-web/)**

*(The guide is an HTML file hosted on GitHub Pages. You can also find it in
this repository as `FoxFW_Help.html`, and the ESP32-specific guide as
`FoxESP32_Help.html` — open either locally in any browser.)*

---

## Run Any Flipper App — FAP Compiler

Want to run a third-party Flipper app that isn't bundled with FoxFW? Paste
its GitHub URL into the **[FAP Compiler](https://foxfw.github.io/fox-web/fap-compiler.html)**
and it builds a `.fap` against FoxFW v2.0 for you — no local toolchain
install, no GitHub account or login needed.

---

## Installing FoxFW

### Via Web Installer (recommended)

Install FoxFW directly from your browser — no software required.

**[→ Open the FoxFW Web Installer](https://foxfw.github.io/fox-web/flasher.html)** `Recommended`

1. Open the link above in **Chrome** or **Edge** on desktop.
2. Connect your Flipper Zero via USB.
3. Click **Connect Flipper** and select your device.
4. Click **Install Now** — the installer downloads and transfers the firmware automatically.
5. Your Flipper reboots and runs the Fox Setup Wizard.

> Supports **Standard Update** (Flipper on, normal mode) AND **DFU Recovery** (Flipper unresponsive / screen off).

---

### Via qFlipper

1. Connect your Flipper to your computer via USB.
2. Open **qFlipper** (v1.3.3 or later).
3. Click **Install from file** and select the FoxFW `.tgz` update package.
4. qFlipper flashes the firmware and copies resources automatically.
5. The Flipper reboots. On first boot the Fox Setup Wizard runs.

### Via SD card (Fox File Browser)

1. Copy the firmware update files (the folder with `update.fuf`) to your SD card.
2. Open **Fox File Browser** from the desktop (Down button or any configured shortcut).
3. Navigate to the `.fuf` file, press OK, and select **Install**.
4. The updater launches and the device restarts to apply the update.

### Via Fox Update Downloader (already have an ESP32 wired up?)

If you already have an ESP32 running Fox ESP32 Firmware connected to your
Flipper's GPIO header, **Fox Update Downloader** can check GitHub and install
a new FoxFW release entirely on-device — no PC or SD card swap needed.

1. Open **Fox Update Downloader** and select **Fox Custom Firmware**.
2. If an update is available, press **Download**, then **Install** once it finishes.
3. The device restarts automatically to apply it. Not ready yet? Press **Later** — the download stays on your SD card and a **Restart** button installs it whenever you come back.

> **After an update:** The Fox Setup Wizard runs once on the first boot.
> Your PIN, name, settings, and saved files are all preserved.

---

## Desktop Quick Reference

| Button | Action |
|---|---|
| **↑ Up (short)** | Lock Menu (lock / power off / reboot) |
| **↑ Up (hold)** | Lock screen immediately |
| **↓ Down (short)** | Fox File Browser |
| **↓ Down (hold)** | Toggle clock freeze |
| **← Left (short/hold)** | Configurable Favorite (default: FFB) |
| **→ Right (short/hold)** | Configurable Favorite (default: FFB) |
| **OK (hold)** | Configurable Favorite (default: FFB) |
| **OK (short)** | Open App Menu |

Configure the Favorite slots in **App Menu → Settings → Fox Settings → Favorite Apps**.

---

## Locked Out?

If you have exhausted your PIN attempts and cannot access the device, email us:

**foxcustomfirmware@gmail.com**

Include a description of what happened and how you became locked out.

---

## Repository Structure

```
FoxFW_Help.html      ← Complete user guide (open in any browser)
FoxESP32_Help.html   ← Fox ESP32 App Suite guide (open in any browser)
README.md            ← This file
```

---

<div align="center">
  <sub>FoxFW v2.0 &nbsp;·&nbsp; foxcustomfirmware@gmail.com<br>
Michael Joyce, also known as <b>AussieMike86</b><br>
Adam Alioa, also known as <b>z4men</b>
  </sub>
</div>
