# Cat S22 Flip (Android 15 / VoltageOS) Complete Fixes & Porting Guide

This repository contains the complete collection of reverse-engineered fixes, native daemons, CIL SELinux policies, telephony patches, and prebuilts developed to run **Android 15 (VoltageOS GSI / Custom ROM)** smoothly on the **Cat S22 Flip** (`armeabi-v7a`, Qualcomm Snapdragon 215 / QM215).

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
├── ims_volte/              # Qualcomm VoLTE / HD Voice Calling
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
├── selinux/                # SELinux Enforcing rules & boot init definitions
│   ├── cat_hardware.rc         # Android Init script launching daemons & setting permissions
│   └── custom_cat_s22.cil      # Custom CIL rules enabling priv_app <-> rild binder IPC
│
└── scripts/                # Fastboot, debugfs, and lpmake repacking automation
    ├── inject_smart_keypad.sh  # Injects smart keypad daemon into system.img & builds super.img
    └── inject_faceunlock.sh    # Injects 32-bit Face++ libraries into system.img
```

---

## 🛠️ Issue-by-Issue Technical Breakdown

### 1. Physical Keypad Keybounce / Double-Typing Fix
* **The Problem:** The Snapdragon 215 matrix keypad driver suffers from contact chatter/bounce, causing letters and numbers to type multiple times per physical click. When using a standard uinput virtual keyboard on bus `0x0019`, Android's `EventHub` merged the raw physical keypad and the virtual keypad into a single device, doubling the keystrokes.
* **The Solution:**
  1. An input configuration file [`soc:matrix_keypad.idc`](keypad/soc:matrix_keypad.idc) sets `keyboard.layout = none`, muting the raw driver events in `InputReader`.
  2. [`keypad_debounce_smart.c`](keypad/keypad_debounce_smart.c) exclusively grabs `/dev/input/event1` (`EVIOCGRAB`) and creates a clean virtual keyboard device named `Cat_Keypad_Clean` on USB bus `0x0003` (`18d1:0001`).
  3. Every key event is filtered with a strict **120ms non-blocking debounce window**.
  4. **Smart Speaker Key Navigation:**
     - **Short press (< 500ms):** Emits `KEY_TAB` (`KEYCODE_TAB`) allowing instant keyboard navigation across bottom bars, action menus, and UI tabs in modern apps (such as Instagram, Chrome, and Settings) without touching the touchscreen.
     - **Long press (≥ 500ms):** Emits `KEY_MENU` (`KEYCODE_MENU`) for quick context actions.

### 2. Secondary Outer Display & Backlight Control
* **The Problem:** Modern AOSP/GSIs do not natively understand dual-panel flip phone configurations; closing the flip turned off the main screen while leaving the outer display unpowered.
* **The Solution:**
  1. [`outerscreen_service.c`](outerscreen/outerscreen_service.c) monitors the physical hall sensor switch (`/dev/input/event3`, `EVIOCGSW`).
  2. Directly manages the outer LCD backlight sysfs node:
     `/sys/class/leds/lcd-backlight2/brightness`
  3. Provides a lightweight IPC control socket (`/dev/shm_outerscreen_cmd`) supporting `FORCE_ON`, `FORCE_OFF`, and `TOGGLE` commands.

### 3. VoLTE & IMS Telephony Calling
* **The Problem:** Cat S22 Flip running Android 15 GSI lacked carrier IMS bearer negotiation, preventing outgoing/incoming 4G VoLTE calls.
* **The Solution:**
  1. Injected Qualcomm's proprietary IMS service [`ims.apk`](ims_volte/ims/ims.apk) (`org.codeaurora.ims`) into `/system/priv-app/ims/`.
  2. Resolved a critical `UnsatisfiedLinkError` caused by missing `libimsmedia_jni.so` by disassembling `ims.apk` with `baksmali`, neutralizing the unused load call, and repacking with `smali`.
  3. Added [`android.hardware.telephony.ims.xml`](ims_volte/android.hardware.telephony.ims.xml) to `/system/etc/permissions/` to register IMS telephony capabilities with Android's `TelephonyRegistry`.

### 4. Megvii Face++ Biometric Face Unlock (32-Bit ARM Fix)
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

### 5. SELinux Enforcing Compatibility
* **The Problem:** Running Android in `Enforcing` mode blocked binder IPC between Qualcomm's `rild` radio daemon and privileged telephony apps (`priv_app`).
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
