# Cat S22 Flip (Android 15 / VoltageOS) Complete Fixes & Porting Guide

This repository contains the complete collection of reverse-engineered fixes, native daemons, CIL SELinux policies, telephony patches, prebuilts, hardware mappings, and performance optimizations developed to run **Android 15 (VoltageOS GSI / Custom ROM)** smoothly on the **Cat S22 Flip** (`armeabi-v7a`, Qualcomm Snapdragon 215 / QM215).

---

## 📁 Repository Structure

```text
├── keypad/                 # Physical matrix keypad clean debouncing & smart navigation
│   ├── keypad_debounce_smart.c # Native C daemon: 120ms debounce + Tab/Menu gestures
│   ├── keypad_debounce_clean   # Precompiled static ARM binary
│   ├── soc:matrix_keypad.idc   # Input device configuration (mutes raw driver duplicate events)
│   └── soc:matrix_keypad.kl    # Android key layout file
│
├── outerscreen/            # Secondary outer OLED display service
│   ├── outerscreen_service.c   # Hall sensor listener & backlight daemon
│   └── outerscreen_service     # Precompiled ARM binary
│
├── ims_volte/              # Qualcomm VoLTE / HD Voice Calling & VINTF Compatibility
│   ├── compatibility_matrix.device.xml # VINTF matrix allowing Treble framework to bind vendor radio HALs
│   ├── ims/ims.apk             # Qualcomm IMS Service (patched without crashing native libraries)
│   └── android.hardware.telephony.ims.xml # Telephony IMS framework feature descriptor
│
├── faceunlock/             # Megvii Face++ Biometric Face Unlock for 32-bit ARM
│   └── faceunlock_arm/         # 32-bit (armeabi-v7a) JNI & Face++ shared libraries
│       ├── libFaceDetectCA.so
│       ├── libmegface.so
│       ├── libMegviiUnlock.so
│       └── libMegviiUnlock-jni-1.2.so
│
├── catsystemmanager/       # Privileged System Manager ("Cat Kernel Control")
│   ├── CatSystemManager/CatSystemManager.apk # Compiled system application
│   ├── privapp-permissions-cat-manager.xml  # Privileged permission whitelist
│   └── app_source/                          # Complete Kotlin source code
│
├── performance/            # ART AOT & Profile-Guided Optimization scripts
│   └── speed_optimize.sh       # Compiles core UI (SystemUI, Launcher, Settings, Dialer, PhonePe)
│
├── system_init/            # Boot Init services, hardware permissions & boot scripts
│   ├── cat_hardware.rc         # Android Init script launching daemons & setting permissions
│   └── cat_perf.sh             # Boot-time performance tuning & End Call button activator
│
├── selinux/                # SELinux Enforcing rules & boot init definitions
│   ├── cat_hardware.rc         # Android Init script launching daemons & setting permissions
│   └── custom_cat_s22.cil      # Custom CIL rules enabling priv_app <-> rild binder IPC
│
└── scripts/                # Fastboot, debugfs, and lpmake repacking automation
    ├── inject_smart_keypad.sh  # Injects smart keypad daemon into system.img & builds super.img
    └── inject_faceunlock.sh    # Injects 32-bit Face++ libraries into system.img
```

---

## 🛠️ Complete Technical Issue-by-Issue Breakdown

### 1. Red End Call Button (`☎ / ⏻`) Hardware Wiring & Behavior Fix
* **The Problem:** Pressing the Red End Call button during an active phone call did not hang up or end the call.
* **The Root Cause:**
  - On the Cat S22 Flip hardware, the Red End Call key is **not part of the keypad matrix**. It is hardwired directly to the Qualcomm PMIC power-on chip (`qpnp_pon` on `/dev/input/event0`) generating standard `KEY_POWER` events.
  - On stock AOSP / GSI ROMs, the Android framework defaults the accessibility setting `incall_power_button_behavior` to `1` (turn off display) or `null`, so pressing Power during a call merely locked the screen.
* **The Solution:**
  - Injected `settings put secure incall_power_button_behavior 2` into `/system/bin/cat_perf.sh` which executes automatically on boot (`on property:sys.boot_completed=1`).
  - **In-Call:** Pressing the Red button immediately terminates / hangs up the active call via Android's `PhoneWindowManager` and `Telecom` service.
  - **Out-of-Call:** Pressing the Red button functions as normal **Screen Lock / Screen Off / Wake / Power**.

---

### 2. Physical Keypad Keybounce & Smart Navigation Gestures
* **The Problem:** The Snapdragon 215 matrix keypad driver suffers from contact chatter/bounce, causing letters and numbers to type multiple times per physical click. When using a standard uinput virtual keyboard on bus `0x0019`, Android's `EventHub` merged the raw physical keypad and the virtual keypad into a single device, doubling the keystrokes. Furthermore, flip phone users cannot navigate non-Dpad UI buttons (like bottom navigation bars in Instagram or web apps) without reaching for the touchscreen.
* **The Solution:**
  1. An input configuration file [`soc:matrix_keypad.idc`](keypad/soc:matrix_keypad.idc) sets `keyboard.layout = none`, muting the raw driver events in `InputReader`.
  2. [`keypad_debounce_smart.c`](keypad/keypad_debounce_smart.c) exclusively grabs `/dev/input/event1` (`EVIOCGRAB`) and creates a clean virtual keyboard device named `Cat_Keypad_Clean` on USB bus `0x0003` (`18d1:0001`).
  3. Every key event is filtered with a strict **120ms non-blocking debounce window**.
  4. **Smart Speaker Key Navigation:**
     - **Short press (< 500ms):** Emits `KEY_TAB` (`KEYCODE_TAB`) allowing instant keyboard navigation across bottom bars, action menus, and UI tabs in modern apps without touching the touchscreen. Once focused, pressing **Center OK** (`ENTER`) selects it!
     - **Long press (≥ 500ms):** Emits `KEY_MENU` (`KEYCODE_MENU`) for quick context actions.

---

### 3. VoLTE, IMS Calling & VINTF Treble Compatibility Matrix Fix
* **The Problem:** 
  1. Generic System Images (GSIs) do not declare Qualcomm's proprietary radio HALs in the framework's VINTF compatibility matrix (`/system/etc/vintf/compatibility_matrix.device.xml`). As a result, Android Treble refuses to link the Android 15 telephony framework to the vendor's `rild` and `IImsRadio` HIDL services.
  2. Qualcomm's stock `ims.apk` crashed on boot with `java.lang.UnsatisfiedLinkError: library "libimsmedia_jni.so" not found` because Android 15 lacks that legacy library.
  3. SELinux `Enforcing` mode blocked binder IPC between `priv_app` and `rild`.
* **The Complete Solution:**
  1. **VINTF Compatibility Matrix Injected:** Injected [`compatibility_matrix.device.xml`](ims_volte/compatibility_matrix.device.xml) into `/system/etc/vintf/` declaring:
     ```xml
     <hal format="hidl" optional="true">
         <name>vendor.qti.hardware.radio.ims</name>
         <version>1.0-7</version>
         <interface>
             <name>IImsRadio</name>
             <instance>imsradio0</instance>
             <instance>imsradio1</instance>
         </interface>
     </hal>
     <hal format="hidl" optional="true">
         <name>vendor.qti.hardware.radio.qcrilhook</name>
         <version>1.0</version>
         <interface>
             <name>IQcrilHook</name>
             <instance>oemhook0</instance>
             <instance>oemhook1</instance>
         </interface>
     </hal>
     ```
  2. **Patched `ims.apk`:** Disassembled `ims.apk` with `baksmali`, commented out the static call `System.loadLibrary("imsmedia_jni")` inside `ImsMedia.smali`, re-assembled with `smali`, and re-signed using the platform testkey.
  3. **IMS Feature Descriptor:** Added [`android.hardware.telephony.ims.xml`](ims_volte/android.hardware.telephony.ims.xml) to `/system/etc/permissions/` so Android's `TelephonyRegistry` knows the hardware supports IMS.
  4. **SELinux CIL Rules:** Injected [`custom_cat_s22.cil`](selinux/custom_cat_s22.cil) into `/system/etc/selinux/plat_sepolicy.cil` allowing binder transfer between `priv_app` and `rild`.
* **Result:** Full 4G VoLTE Calling with the native `HD` status bar icon and bearer connectivity on Jio/Airtel/T-Mobile.

---

### 4. Hall Sensor Flip Sleep/Wake & Secondary Outer Display Control
* **The Problem:** 
  1. Generic System Images do not recognize the Cat S22 Flip's hardware lid switch. Closing the flip left the main inner screen powered on and drained the battery, while opening the flip did not wake the device.
  2. The Cat S22 Flip's kernel driver (`soc:hall` / `hall_sensor` on `input4`) emits hardware interrupts (`hall near` / `hall far`), but does not expose standard `EV_SW` `SW_LID` events to the generic Android EventHub.
  3. The secondary outer OLED display (`/dev/graphics/fb2`) was left unpowered when the flip closed.
* **The Solution:**
  1. [`outerscreen_service.c`](outerscreen/outerscreen_service.c) features an integrated real-time kernel event listener monitoring the hall sensor state transitions:
     - **Flip Closed (`hall near`):** Immediately issues `KEYCODE_SLEEP` to turn off the inner screen, powers on the outer display backlight (`/sys/class/leds/lcd-backlight2/brightness`), and renders the standalone digital clock.
     - **Flip Opened (`hall far`):** Immediately issues `KEYCODE_WAKEUP` to wake the inner display up instantly and turns off the outer OLED backlight.
  2. Provides an IPC control socket (`/dev/shm_outerscreen_cmd`) supporting `FORCE_ON`, `FORCE_OFF`, `BRIGHTNESS=<val>`, and `TOGGLE` commands from userland and [CatSystemManager](catsystemmanager/).

---

### 5. Megvii Face++ Biometric Face Unlock (32-Bit ARM Fix)
* **The Problem:** VoltageOS shipped with ParanoidSense (`co.aospa.sense`), but its Face++ biometric SDK libraries were built only for 64-bit (`arm64-v8a`), causing a fatal crash during enrollment on the 32-bit Cat S22 Flip:
  ```text
  java.lang.UnsatisfiedLinkError: dlopen failed: library "libMegviiUnlock-jni-1.2.so" not found
  ```
* **The Solution:**
  1. Extracted and validated official 32-bit (`armeabi-v7a`) ELF libraries matching all required JNI signatures (`Java_com_megvii_facepp_sdk_jni_LiteApi_*`).
  2. Injected into `/system/lib/` and `/system/priv-app/ParanoidSense/lib/arm/`:
     - `libFaceDetectCA.so`
     - `libmegface.so`
     - `libMegviiUnlock.so`
     - `libMegviiUnlock-jni-1.2.so`

---

### 6. SELinux Enforcing Compatibility & File Contexts
* **The Problem:** Running Android in `Enforcing` mode blocked binder IPC between Qualcomm's `rild` radio daemon and privileged telephony apps (`priv_app`). Furthermore, debugfs file injections into `system.img` omit extended attributes by default, causing init `execv()` permission denials.
* **The Solution:**
  - Injected custom CIL rules [`custom_cat_s22.cil`](selinux/custom_cat_s22.cil) into `/system/etc/selinux/plat_sepolicy.cil`:
    ```cil
    (allow priv_app rild (binder (call transfer)))
    (allow rild priv_app (binder (call transfer)))
    (allow priv_app rild (fd (use)))
    (allow priv_app vendor_radio_prop (file (read getattr open map)))
    (typepermissive priv_app)
    (typepermissive su)
    ```
  - Applied explicit SELinux xattrs on all injected binaries and libraries via debugfs (`ea_set <path> security.selinux "u:object_r:system_file:s0\0"` or `system_lib_file`).

---

### 7. System Responsiveness & AOT Compilation (2GB RAM Optimization)
* **The Problem:** On Snapdragon 215 with 2GB RAM, standard JIT compilation causes frame drops in launcher, dialer, and UI transitions. Attempting full AOT (`-m speed`) on large modern apps (such as PhonePe) exhausts available RAM during `dex2oat` and causes compiler crashes.
* **The Solution:**
  - Applied targeted compilation using [`speed_optimize.sh`](performance/speed_optimize.sh):
    - Full AOT (`-m speed`) on critical UI packages: `com.android.systemui`, `com.android.launcher3`, `com.android.settings`, `com.android.dialer`.
    - Profile-Guided compilation (`-m speed-profile`) on heavy applications like `com.phonepe.app` to prevent memory exhaustion while guaranteeing instantaneous app launching.

---

### 8. Google Play Services / Sandboxed GMS / App Store Architecture
* **The Problem:** Attempting to install Google Services Framework (GSF) on VoltageOS threw:
  `INSTALL_FAILED_SESSION_INVALID: GSF installation is not allowed`
* **The Solution / Finding:**
  - VoltageOS includes GrapheneOS's `GmsCompat` baked into the framework. `GmsCompat` internally stubs and redirects GSF and GMS calls in userland sandbox mode without needing root or system-priv GApps packages.
  - Apps and alternative app stores (such as Aurora Store v4.7.5) operate cleanly in this environment.

---

## 🚀 How to Build and Flash

### 1. Compile the Keypad Debounce Daemon
Using the Android NDK or AOSP Clang toolchain:
```bash
$CLANG \
  --target=armv7a-linux-androideabi28 \
  -nostdlib -static -O2 \
  -ffreestanding -fno-builtin \
  -o keypad/keypad_debounce_clean \
  keypad/keypad_debounce_smart.c
```

### 2. Repack the Super Dynamic Partition Image
Using `lpmake`:
```bash
lpmake \
  --metadata-size 65536 \
  --super-name super \
  --metadata-slots 2 \
  --device super:9126805504 \
  --group qti_dynamic_partitions:9126805504 \
  --partition system:readonly:2621440000:qti_dynamic_partitions \
  --image system=unpacked/system.img \
  --partition vendor:readonly:419389440:qti_dynamic_partitions \
  --image vendor=unpacked/vendor.img \
  --sparse \
  --output VoltageOS_CatS22_custom_super.img
```

### 3. Flash to Device via Fastbootd
```bash
adb reboot fastboot
fastboot flash super VoltageOS_CatS22_custom_super.img
fastboot reboot
```

---

## 📄 License
All custom daemons and scripts are released under the **Apache 2.0 License**. Qualcomm, Megvii, and Android components belong to their respective copyright holders.
