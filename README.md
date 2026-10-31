# Cat S22 Flip — Infinity-X 3.12 (Android 16) & Android 15 Complete Hardware Port & Fixes

This repository contains the complete collection of reverse-engineered hardware daemons, native Settings integration (`CatParts`), CIL SELinux policies, Qualcomm IMS/VoLTE patches, 32-bit FaceUnlock prebuilts, keypad debouncing/backlight controllers, and low-RAM/CPU optimizations for running **Infinity-X 3.12 (Android 16)** and **VoltageOS (Android 15)** on the **Cat S22 Flip** (`armeabi-v7a` 32-bit binder/userspace on 64-bit `aarch64` Linux `4.9.227`, Qualcomm Snapdragon 215 / QM215).

---

## 📁 Repository Structure

```text
├── catparts/               # Native Android Settings Integration ("Cat S22 Flip Settings")
│   ├── AndroidManifest.xml     # IA_SETTINGS homepage injector + Boot/Torch/Outer broadcast receiver
│   ├── CatParts.apk            # Precompiled 21 KB system app (zero background RAM)
│   ├── build_catparts.sh       # Standalone SDK build script (aapt2 + javac + d8 + apksigner)
│   ├── res/                    # Preferences XML, string arrays, and vector settings icon
│   └── src/com/cat/parts/
│       ├── CatPartsActivity.java # PreferenceFragment controller syncing to /dev/shm_outerscreen_cmd
│       └── BootReceiver.java     # Restores prefs on boot + handles TOGGLE_TORCH & TOGGLE_OUTER
│
├── outerscreen/            # Secondary 128x128 Outer OLED Display Suite (/dev/graphics/fb2)
│   ├── outerscreen_service.c   # Pip-Boy 3000 HUD (Clock, BAT/CPU/RAM gauges, Battery Life Est, Call HUD)
│   ├── outerscreen_service     # Precompiled 32-bit ARM dynamic binary
│   ├── cat_boot_display.c      # Early-boot live kernel dmesg logger ("CAT S22 Flip Booting")
│   ├── cat_boot_display        # Precompiled 32-bit ARM dynamic binary
│   ├── cat_watchdog.c          # Auto-recovery watchdog daemon using direct /proc/<pid>/comm scan
│   └── cat_watchdog            # Precompiled 32-bit ARM dynamic binary
│
├── ptt_service/            # Programmable PTT Side Button Daemon (Keycodes 523 & 525)
│   ├── cat_ptt_service.c       # Short press, Long hold, and In-Call PTT action controller
│   └── cat_ptt_service         # Precompiled 32-bit ARM dynamic binary
│
├── keypad/                 # Physical T9 Keypad Debouncer & Backlight Controller
│   ├── cat_hall_keylight.c     # Hall sensor + keypress listener with configurable backlight timeout
│   ├── cat_hall_keylight       # Precompiled 32-bit ARM dynamic binary
│   ├── keypad_debounce_smart.c # 120ms anti-chatter uinput filter + Smart Speaker key (Tab/Menu)
│   ├── keypad_debounce_clean   # Precompiled 32-bit ARM binary
│   ├── soc:matrix_keypad.idc   # Mutes duplicate raw matrix keypad events in InputReader
│   └── soc:matrix_keypad.kl    # Physical keypad layout mapping
│
├── system_init/            # Android 16 Init RC, Build Properties & Performance Scripts
│   ├── cat_hardware.rc         # Init service definitions (u:r:phhsu_daemon:s0) & sysfs permissions
│   ├── cat_perf.sh             # LMKD minfree, zRAM, SurfaceFlinger & GMS background service trimmer
│   ├── phh-prop-handler.sh     # Patched Infinity-X prop handler fixing resetprop_phh -n CPU loop
│   └── build.prop              # System properties with baked-in LMKD minfree & blur disables
│
├── selinux/                # SELinux CIL Policy Rules
│   ├── custom_cat_s22.cil      # Verified secilc CIL rules for phhsu_daemon, CatParts, IMS, RILD, HALs
│   └── cat_hardware.rc         # Copy of init RC with SELinux transitions
│
├── ims_volte/              # Qualcomm VoLTE / HD Voice Calling & VINTF Matrix
│   ├── compatibility_matrix.device.xml # VINTF matrix (sepolicy 202504 + IImsRadio 1.0-7 + IQcrilHook)
│   ├── ims/ims.apk             # Patched Qualcomm IMS APK
│   └── android.hardware.telephony.ims.xml
│
├── faceunlock/             # 32-Bit ARM Megvii Face++ Biometric Unlock Libraries
│   └── faceunlock_arm/         # libFaceDetectCA.so, libmegface.so, libMegviiUnlock.so, JNI
│
├── scripts/                # Build & Image Repacking Automation
│   ├── build_super.sh          # Canonical non-A/B lpmake script (--partition system & vendor)
│   ├── inject_smart_keypad.sh
│   └── inject_faceunlock.sh
│
├── GOLDEN_RULES.md         # Mandatory build, debugfs xattr, SELinux & fastbootd rules
└── CHANGELOG_INFINITYX_A16.md # Detailed version-by-version build changelog (super_v0 -> super_v5)
```

---

## 🌟 Key Highlights in the Infinity-X 3.12 (Android 16) Port

### 1. Native "Cat S22 Flip Settings" (`CatParts.apk`) inside Android Settings
Injected directly into the main Android **Settings** homepage via `com.android.settings.action.IA_SETTINGS` (`com.android.settings.category.ia.homepage`) with **zero background RAM footprint** (`21 KB` APK):
- **PTT Side Button (`0x20d` / `525` & `0x20b` / `523`):**
  - **Short Press (`< 2.0s`):** Toggle Flashlight/Torch (via native `CameraManager.setTorchMode()`), Toggle Outer OLED Display ON/OFF, Launch Camera, Voice Assistant, Mute/Unmute Volume, or Disabled.
  - **Long Hold (`≥ 2.0s`):** Directly linked to `com.cat.parts.TOGGLE_OUTER` so toggling the outer display via PTT long-hold stays 100% synchronized with the **Outer Display Enabled** switch in Settings.
  - **In-Call Action (`0.5s – 2.0s`):** Answer on Speakerphone / Hold to Hang Up, Answer on Earpiece, Toggle Mic Mute, or Disabled.
- **Outer 128x128 OLED Display (`/dev/graphics/fb2`):**
  - **Outer Display Enabled:** Master ON/OFF switch (keeps `outerscreen_service` and `cat_watchdog` cleanly paused with `0` backlight when disabled).
  - **Outer Display Bottom Footer:**
    1. **Estimated Battery Life & Power Info** *(Default)* — Calculates real-time remaining battery hours/minutes (or charge time to full) and displays remaining `mAh` and live `mA` current draw on the bottom two lines (`> EST: ~14h 30m LEFT` / `> 85% (1700mAh) -54mA`).
    2. **Live System Logcat Tail (2 lines)**
    3. **Pip-Boy Vault-Tec Status Line**
    4. **Clean / Empty Footer**
- **Physical T9 Keypad Backlight:**
  - **Keypad Backlight Enabled:** Master ON/OFF toggle.
  - **Keypad Backlight Timeout:** Always On While Flip Open, or Auto-Off after `5s`, `10s`, `30s`, or `60s` of inactivity (wakes immediately on any physical keypress while the flip is open).

---

### 2. Fixed `28–35%` Idle CPU Drain -> Down to `2–5%` Idle CPU
1. **Fixed `phh-prop-handler.sh` Infinite Trigger Loop:**
   - In Infinity-X 3.12, `/system/bin/phh-prop-handler.sh` handled `persist.sys.phh.traffic_indicator_fallback` by calling `resetprop_phh` without `-n`, which re-triggered `init`'s `on property:persist.sys.phh.traffic_indicator_fallback=*` rule 10 times per second forever (`45,000+` process forks/hour). Changing to `resetprop_phh -n` eliminated the loop completely.
2. **Fixed `TelephonyRegistry.dump` `EPIPE (Broken pipe)` Spam:**
   - Removed `| head -n 30` from `dumpsys telephony.registry` inside `outerscreen_service.c` so `system_server` no longer throws Java `IOException: EPIPE` stacktraces every second.
3. **Trimmed Unneeded Google Background Services in `cat_perf.sh`:**
   - Automatically disables `com.google.android.partnersetup`, Nearby Share (`DiscoveryService`, `LifeCycleService`, `ReceiveSurfaceService`, `SendSurfaceService`), `WearableService`, `OfflineBeaconService_Persistent`, `ClearcutDebugDumpService`, and `Tron.CollectionService` on boot.

---

### 3. Fixed Early-Boot `cgroups.json` SELinux Context & Legacy `lmkd` (`minfree`)
- Linux `4.9.227` on the Cat S22 Flip does not support `/proc/pressure/memory` (PSI).
- Fixed the SELinux extended attribute on `/system/etc/cgroups.json` to `u:object_r:cgroup_desc_file:s0` so `first_stage_init` mounts `/dev/memcg` and `/dev/blkio` cleanly, and baked `ro.lmk.use_minfree_levels=true` and `ro.lmk.use_psi=false` directly into `/system/build.prop`.

---

### 4. Fixed Outer Screen 5-Second Restart Loop (`SIGCHLD` + `/proc` Scanner)
- Removed `signal(SIGCHLD, SIG_IGN)` from `cat_watchdog.c`, `outerscreen_service.c`, and `cat_ptt_service.c` (ignoring `SIGCHLD` caused `waitpid()` inside Bionic's `system()` and `pclose()` to fail with `ECHILD` and return `-1`, making `cat_watchdog` think `outerscreen_service` had died every 7 seconds).
- Replaced `system("pidof outerscreen_service")` in `cat_watchdog.c` with a zero-fork `/proc/<pid>/comm` scanner (`is_outerscreen_running()`).

---

## 🚀 Building & Flashing (`super.img`)

> [!IMPORTANT]
> Always read [`GOLDEN_RULES.md`](GOLDEN_RULES.md) before modifying `system.img` or packing `super.img`.
> - The Cat S22 Flip is a **non-A/B** device: always use `--partition system` and `--partition vendor` (never `system_a`/`vendor_a`).
> - Always flash `super` from userspace **`fastbootd`** (`adb reboot fastboot`), never from bootloader mode.

```bash
# 1. Pack super.img using the canonical builder script
./scripts/build_super.sh /path/to/output_super.img

# 2. Flash via fastbootd
adb reboot fastboot
fastboot flash super /path/to/output_super.img
fastboot reboot
```
---

## 🎨 Developer Guide: Creating UI Themes for the Cat S22 Flip Outer OLED Display

The Cat S22 Flip features a secondary external **128×128 pixel** monochromatic/color OLED panel driven over SPI via the secondary Qualcomm MDSS framebuffer device: `/dev/graphics/fb2`.

This guide documents the architecture, framebuffer specifications, color constraints, font engine, and step-by-step instructions for adding custom **Watchfaces** and **Call HUDs**.

---

### 1. Hardware & Framebuffer Specifications

| Property | Value | Notes |
| :--- | :--- | :--- |
| **Device Node** | `/dev/graphics/fb2` | Direct Linux Framebuffer |
| **Resolution** | **128 × 128** pixels | 1:1 Square Aspect Ratio |
| **Color Format** | `ARGB8888` (32-bit Little Endian) | `0xAARRGGBB` |
| **Total Buffer Size** | 65,536 Bytes (`128 * 128 * 4`) | Memory-mapped via `mmap()` |
| **Backlight Sysfs** | `/sys/class/leds/lcd-backlight2/brightness` | `0` (Off) to `255` (Max) |
| **SPI Power Lock** | `/sys/power/wake_lock` | Write `"outerscreen_wakelock\n"` to prevent DOZE SPI sleep |

> [!CAUTION]
> **Alpha Channel Rule (0xFF Alpha Mandatory):**
> The Qualcomm MDSS display controller interprets `0x00` in the high byte as full transparency / inverted white background. Every 32-bit pixel written to `/dev/graphics/fb2` **MUST** have its alpha byte set to `0xFF` (e.g. `0xFF000000 | (r << 16) | (g << 8) | b`). Failing to do so causes full-screen white inversion.

---

### 2. Palette & Color Definitions

Always use 32-bit ARGB constants:

```c
#define COLOR_BLACK         0xFF000000
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_PIP_GREEN     0xFF10B981   /* Vault-Tec Phosphor Green */
#define COLOR_PIP_LIGHT     0xFFA7F3D0   /* Mint Green Glow */
#define COLOR_PIP_DARK      0xFF064E3B   /* Dim Segment Green */
#define COLOR_AMBER         0xFFF59E0B   /* Amber CRT Warning */
#define COLOR_AMBER_BRIGHT  0xFFFBBF24   /* High-Intensity Amber */
#define COLOR_RED           0xFFEF4444   /* Danger / Alert Red */
#define COLOR_CYAN          0xFF06B6D4   /* Cyberpunk / VoLTE Cyan */
#define COLOR_MAGENTA       0xFFEC4899   /* 80s Synthwave Magenta */
#define COLOR_PURPLE        0xFFA855F7   /* Deep Violet */
#define COLOR_YELLOW        0xFFEAB308   /* CAT Heavy Equipment Yellow */
#define COLOR_GRAY_LIGHT    0xFF94A3B8   /* Casio LCD Gray */
```

---

### 3. Core Drawing Primitives

The rendering engine operates directly in RAM on the mmap'd pointer `uint32_t *fb`:

#### Drawing a Single Pixel:
```c
static void draw_pixel(uint32_t *fb, int x, int y, uint32_t color) {
    if (x >= 0 && x < 128 && y >= 0 && y < 128) {
        fb[y * 128 + x] = color;
    }
}
```

#### Drawing Solid Rectangles:
```c
static void fill_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int cy = y; cy < y + h && cy < 128; cy++) {
        if (cy < 0) continue;
        for (int cx = x; cx < x + w && cx < 128; cx++) {
            if (cx >= 0) fb[cy * 128 + cx] = color;
        }
    }
}
```

#### Outlined Rectangles:
```c
static void draw_rect(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int cx = x; cx < x + w; cx++) { draw_pixel(fb, cx, y, color); draw_pixel(fb, cx, y + h - 1, color); }
    for (int cy = y; cy < y + h; cy++) { draw_pixel(fb, x, cy, color); draw_pixel(fb, x + w - 1, cy, color); }
}
```

---

### 4. Typography & Font Engine

The outer display uses an embedded **5×7 ASCII bitmapped font** table:
- **`scale = 1`**: 5×7 pixels per glyph, 6px pitch (ideal for telemetry, labels, and footer text; fits 21 characters across the 128px screen).
- **`scale = 2`**: 10×14 pixels per glyph, 12px pitch (ideal for caller names, stardates, subheadings).
- **`scale = 3`**: 15×21 pixels per glyph, 18px pitch (ideal for main digital clock and call duration timer).

#### Centered Text Routine:
```c
static void draw_string_centered(uint32_t *fb, int y, const char *str, uint32_t color, int scale) {
    int len = strlen(str);
    int w = len * 6 * scale - scale;
    int x = (128 - w) / 2;
    if (x < 0) x = 0;
    draw_string(fb, x, y, str, color, scale);
}
```

---

### 5. Step-by-Step: Adding a New Watchface Theme

1. **Choose a Unique Identifier**:
   E.g., `my_theme`
2. **Add Preference to `catparts`**:
   In `catparts/res/values/strings.xml`:
   ```xml
   <!-- Under outer_theme_entries -->
   <item>23. My Cool Custom Theme</item>

   <!-- Under outer_theme_values -->
   <item>my_theme</item>
   ```
3. **Implement Rendering in `outerscreen/outerscreen_service.c`**:
   Inside `render_watchface()`:
   ```c
   else if (strcmp(theme, "my_theme") == 0) {
       draw_status_bar(fb, "MY THEME", COLOR_WHITE, bat);
       draw_string_centered(fb, 26, time_str, COLOR_CYAN, 3);
       draw_string_centered(fb, 56, date_str, COLOR_PIP_LIGHT, 1);
       char info[32];
       snprintf(info, sizeof(info), "CPU:%d%%  RAM:%d%%", cpu, ram);
       draw_string_centered(fb, 76, info, COLOR_AMBER, 1);
       fill_rect(fb, 14, 96, (bat * 100) / 100, 6, COLOR_PIP_GREEN);
       draw_rect(fb, 14, 96, 100, 6, COLOR_GRAY);
       draw_string_centered(fb, 112, "BATTERY STATUS", COLOR_WHITE, 1);
   }
   ```

---

### 6. Step-by-Step: Adding a New Call UI HUD

1. **Choose a Call Identifier**:
   E.g., `my_call`
2. **Add Preference to `catparts`**:
   In `catparts/res/values/strings.xml` under `outer_call_entries` and `outer_call_values`.
3. **Implement in `render_call_hud()`**:
   ```c
   else if (strcmp(theme, "my_call") == 0) {
       fill_rect(fb, 0, 0, 128, 16, COLOR_CYAN);
       draw_string_centered(fb, 4, call_state == 1 ? "INCOMING CALL" : "CALL ACTIVE", COLOR_BLACK, 1);
       draw_string_centered(fb, 24, disp_name, COLOR_WHITE, 2);
       if (disp_num[0]) draw_string_centered(fb, 44, disp_num, COLOR_CYAN, 1);
       if (call_state == 2) draw_string_centered(fb, 60, dur_str, COLOR_YELLOW, 2);
       fill_rect(fb, 6, 92, 116, 16, COLOR_PIP_GREEN);
       draw_string_centered(fb, 96, call_state == 1 ? "PTT: ANSWER" : "PTT: END", COLOR_BLACK, 1);
   }
   ```
4. **Compile as 32-bit ARM Dynamic Binary**:
   ```bash
   armv7a-linux-androideabi21-clang -O2 -Wall -Wextra \
     outerscreen/outerscreen_service.c -o outerscreen/outerscreen_service
   ```

---

### 7. Telephony & Contact Name Lookup Architecture

`outerscreen_service` monitors phone state via `dumpsys telephony.registry`:
- `mCallState=1` / `mRingingCallState=1`: Incoming Ringing
- `mCallState=2` / `mForegroundCallState=1`: Active In-Call
- `mCallIncomingNumber`: Phone number string

When a call is detected, the daemon automatically queries Android's Contacts Provider:
```bash
content query --uri content://com.android.contacts/phone_lookup/<phone_number> --projection display_name
```
If a matched contact exists, `disp_name` is populated with the contact's name (e.g. `JOHN DOE`). Otherwise, the raw phone number is displayed.

---
