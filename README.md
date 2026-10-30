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
