<div align="center">
  <h1>🦊 FoxFW v2.0</h1>
  <p><em>A custom Flipper Zero firmware built for power users.</em></p>
  <p>
    <a href="https://foxfw.github.io/2.0/">📖 Full User Guide</a>
    &nbsp;·&nbsp;
    <a href="mailto:foxcustomfirmware@gmail.com">📧 Support</a>
  </p>
</div>

---

## What is FoxFW?

FoxFW is a fully custom Flipper Zero firmware that extends the official release
with deeper security tools, a smarter file browser, enhanced Sub-GHz analysis,
and a polished system-wide experience. It is designed around three goals:
**security**, **usability**, and **deeper RF tooling**.

---

## ✨ Highlights

| Feature | Description |
|---|---|
| 🔐 **PIN Lock System** | Configurable attempt limits, auto-lock timer, and a Honeypot / Fake-Wipe defence mode |
| 🔌 **Disconnect on Lock** | Individually toggle BLE, GPIO, and USB disconnection when the screen locks |
| 📁 **Fox File Browser** | SD card browser with Favorites, firmware install, and app launch — replaces the stock Archive |
| 📡 **Modulation Analyzer** | FoxFW-exclusive: find what modulation an unknown device uses, then open the Receiver pre-configured for it |
| 🌊 **Waterfall Display** | Scrolling signal history alongside the live RSSI view — great for spotting intermittent bursts |
| ✂️ **RAW Waveform Editor** | Trim Sub-GHz RAW captures in-app without leaving the device |
| 🔘 **Protocol Filters** | Enable/disable individual Sub-GHz protocols — fewer active protocols = faster, more accurate decoding |
| 🔤 **Bounce-Scroll Text** | System-wide: labels too long to fit bounce left and right rather than truncating — works in every menu |
| 🖼️ **Custom Wallpaper** | 128×64 XBM image support; a default is auto-generated from firmware if your wallpaper.xbm is missing |
| 🧙 **First-Boot Wizard** | Guided setup for device name and PIN on every fresh install |

---

## 📖 Full User Guide

The complete guide covers every feature in detail — first-boot setup, all Fox
Settings options, the Fox File Browser, the Sub-GHz toolset, firmware updates,
and lockout recovery.

**[→ Open the Full User Guide](https://foxcustomfirmware.github.io/FoxFW/)**

*(The guide is an HTML file hosted on GitHub Pages. You can also find it in
this repository as `FoxFW_Help.html` and open it locally in any browser.)*

---

## 🚀 Installing FoxFW

### Via qFlipper (recommended)

1. Connect your Flipper to your computer via USB.
2. Open **qFlipper** (v1.3.3 or later).
3. Click **Install from file** and select the FoxFW `.tgz` update package.
4. qFlipper flashes the firmware and copies resources automatically.
5. The Flipper reboots. On first boot the Fox Setup Wizard runs.

### Via SD card (Fox File Browser)

1. Copy the `.fuf` firmware update file to your SD card.
2. Open **Fox File Browser** from the desktop (Down button or any configured shortcut).
3. Navigate to the `.fuf` file, press OK, and select **Install**.
4. The updater launches and the device restarts to apply the update.

> **After an update:** The Fox Setup Wizard runs once on the first boot.
> Your PIN, name, settings, and saved files are all preserved.

---

## 🔑 Desktop Quick Reference

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

## 🔒 Locked Out?

If you have exhausted your PIN attempts and cannot access the device, email us:

**📧 foxcustomfirmware@gmail.com**

Include your device serial number and a description of how you became locked out.

---

## 📁 Repository Structure

```
FoxFW_Help.html     ← Complete user guide (open in any browser)
README.md           ← This file
```

---

<div align="center">
  <sub>FoxFW v2.0 &nbsp;·&nbsp; foxcustomfirmware@gmail.com</sub>
</div>
