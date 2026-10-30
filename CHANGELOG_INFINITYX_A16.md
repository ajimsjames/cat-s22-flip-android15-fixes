# Cat S22 Flip — Infinity-X 3.12 (Android 16) Build & Backup Changelog

All files for the Infinity-X 3.12 port are organized under `/home/aju/Android/VoltageOS_Work/Infinityx_a16_work/`:
- `base_images/` — Untouched stock/vanilla images (`Infinity-X-3.12_system_vanilla.img`, `boot_stock_enforcing.img`)
- `working/` — Active ext4 `system_mod.img` used for `debugfs` injections
- `builds/` — Latest ready-to-flash `super` and `boot` images
- `backups/` — Timestamped backups of every `super` and `boot` build prior to new modifications
- `scripts/` — Build & injection automation scripts

---

## Changelog

### `super_v0_vanilla` (`2026-10-30 10:53`)
- **Backup File:** `backups/super_v0_vanilla_20261030_1053.img`
- **Changes:**
  - Unmodified `Infinity-X-3.12_GSI_treble_a64-ab-VANILLA-Official-20260802.img.xz` (`2538561536` bytes) + stock `vendor.img` (`419389440` bytes) packed into `super.img`.

---

### `super_v1` (`2026-10-30 11:21` — superseded by `v2`)
- **Changes:**
  - Unshared ext4 `shared_blocks` on `system.img`.
  - Removed unused VNDK APEXes (`v28`, `v29`, `v31`, `v32`, `v33`, `v34`), kept `com.android.vndk.v30.apex`.
  - Removed all `.ogg` ringtones, notifications, alarms, and UI sounds from `/system/product/media/audio`.
  - Removed non-flip GSI bloat (`ColumbusService`, `GameSpace`, `LMOFreeformSidebar`).
  - Kept `FaceUnlock` (`/system/system_ext/priv-app/FaceUnlock`) and injected 32-bit ARM native libraries (`libFaceDetectCA.so`, `libmegface.so`, `libMegviiUnlock.so`, `libMegviiUnlock-jni-1.2.so`) into `/system/lib/`, `/system/system_ext/lib/`, and `FaceUnlock/lib/arm/`.
  - Injected **LiteGApps Core Android 16.0 ARM** (`GmsCore.apk`, `Phonesky.apk`, `GoogleServicesFramework.apk`, `GoogleCalendarSyncAdapter.apk`, `GoogleContactsSyncAdapter.apk`, `PartnerSetupPrebuilt.apk`, `GoogleExtShared.apk`, `com.google.android.dialer.support.jar`, and 41 permission/sysconfig XMLs).
  - Injected **Qualcomm IMS / VoLTE** (`/system/priv-app/ims/ims.apk`, `android.hardware.telephony.ims.xml`, and VINTF `compatibility_matrix.device.xml` with `202504` sepolicy + `IImsRadio` 1.0–7 + `IQcrilHook` 1.0).
  - Appended custom SELinux CIL rules (`custom_cat_s22.cil`) to `/system/etc/selinux/plat_sepolicy.cil`.
  - Injected keypad keylayout & IDC (`soc:matrix_keypad.idc`, `soc:matrix_keypad.kl`).

---

### `boot_v1_permissive` (`2026-10-30 11:36`)
- **Active Build:** `builds/boot_v1_permissive.img`
- **Backup File:** `backups/boot_v1_permissive_20261030_1136.img`
- **Changes:**
  - Unpacked `base_images/boot_stock_enforcing.img` (`32 MB`).
  - Added `buildvariant=userdebug androidboot.selinux=permissive enforcing=0` to kernel cmdline.
  - Added full 32 MB AVB hash footer (`avbtool add_hash_footer --partition_size 33554432`).

---

### `super_v2` (`2026-10-30 11:35` — backed up)
- **Backup File:** `backups/Infinity-X_CatS22_super_v2_20261030_1135.img`
- **Changes:**
  - Recompiled all Cat S22 Flip hardware daemons (`outerscreen_service`, `cat_boot_display`, `cat_watchdog`, `cat_ptt_service`, `cat_hall_keylight`) as **32-bit ARM (`armv7a-linux-androideabi21-clang`) dynamically linked** executables (fixing the 64-bit `aarch64` mismatch and NDK r30 static `libc.a` `MADV_WIPEONFORK` crash on Linux 4.9.227).
  - Updated `cat_hardware.rc` to use `seclabel u:r:phhsu_daemon:s0` and labeled binaries in `/system/bin/` as `u:object_r:phhsu_exec:s0` so Android 16 `init` can transition without `avc: denied { transition }`.
  - Updated `/system/etc/cgroups.json` with `/dev/memcg` definition.

---

### `super_v3` (`2026-10-30 11:55` — backed up)
- **Backup File:** `backups/Infinity-X_CatS22_super_v3_20261030_1155.img`
- **Changes:**
  - Fixed SELinux extended attribute on `/system/etc/cgroups.json` from `u:object_r:system_file:s0` to **`u:object_r:cgroup_desc_file:s0`** (matching `/system/etc/selinux/plat_file_contexts`), fixing the early-init SELinux read denial that prevented `CgroupSetup()` from mounting `/dev/memcg`, `/dev/cpuset`, `/dev/cpuctl`, `/dev/blkio`, and `/sys/fs/cgroup`.
  - Added fallback `mkdir /dev/memcg 0700 root system` + `mount cgroup none /dev/memcg memory` in `cat_hardware.rc` `on early-init`.
  - Enabled Infinity-X low-RAM/kernel-4.9 tuning properties (`persist.sys.phh.lmk_tweaks=true`, `persist.sys.phh.axion_props=true`) in `/system/bin/cat_perf.sh`.
  - Added comprehensive SELinux CIL rules in `/system/etc/selinux/plat_sepolicy.cil` (verified with `secilc -c 30`) for `phhsu_daemon` (`block_suspend`, `fsetid`, `sys_rawio`), Qualcomm `ims`, `hal_imsrtp`, `cnd`, `netmgrd`, `rild`, `hal_gnss_qti`, `location`, `hal_audio_default`, `hal_sensors_default`, `wcnss_service`, `thermal-engine`, and `init`.

---

### `super_v4` (`2026-10-30 12:10` — backed up)
- **Backup File:** `backups/Infinity-X_CatS22_super_v4_20261030_1210.img`
- **Changes:**
  - **Fixed Outer Screen 5-Second Restart Loop (`cat_watchdog` + `outerscreen_service`):**
    - Removed `signal(SIGCHLD, SIG_IGN)` from `cat_watchdog.c`, `outerscreen_service.c`, and `cat_ptt_service.c` (with `SIGCHLD` ignored, `waitpid()` inside `system()` and `pclose()` fails with `ECHILD` and returns `-1`, which caused `cat_watchdog` to think `outerscreen_service` had died every loop and kill/restart it, resetting the in-call timer).
    - Replaced `system("pidof outerscreen_service")` in `cat_watchdog.c` with a direct `/proc/<pid>/comm` scanner (`is_outerscreen_running()`), eliminating shell fork overhead every 2 seconds.
    - Fixed `mDataConnectionState=1` false positive in `cat_ptt_service.c` (`get_call_state()`) by checking `mRingingCallState=1` and `mForegroundCallState=1`.
  - **Baked LMKD `minfree` & SurfaceFlinger Smoothness Directly into `/system/build.prop`:**
    - Added `ro.lmk.use_minfree_levels=true`, `ro.lmk.use_psi=false`, `ro.lmk.low=1001`, `ro.lmk.medium=800`, `ro.lmk.critical=0`, `ro.sys.fw.bg_apps_limit=12`, and `ro.config.max_starting_bg=4` directly to `/system/build.prop` so `lmkd` starts in legacy `minfree` mode from the very first second of every boot without waiting for post-boot scripts.
    - Disabled expensive SurfaceFlinger background blurs (`ro.surface_flinger.supports_background_blur=0`, `ro.sf.blurs_are_expensive=1`, `persist.sys.sf.disable_blurs=1`) and enabled latch-unsignaled/backpressure bypass + `0.5x` animation scales and `persist.sys.phh.scroll_boost=true`.

---

### `super_v5` (`2026-10-30 13:46`)
- **Active Build:** `builds/Infinity-X_CatS22_super_v5.img`
- **Backup File:** `backups/Infinity-X_CatS22_super_v5_20261030_1346.img`
- **Changes:**
  - **Fixed `28–35%` Idle CPU Drain (`phh-prop-handler.sh` Infinite Loop):**
    - Fixed a bug in Infinity-X's `/system/bin/phh-prop-handler.sh` (`persist.sys.phh.traffic_indicator_fallback`) where `resetprop_phh` without `-n` continuously re-triggered `init`'s `on property:persist.sys.phh.traffic_indicator_fallback=*` handler 10 times per second forever (`45,000+` spawns/hour). Changed to `resetprop_phh -n`.
  - **Optimized `outerscreen_service` Idle CPU & Fixed `/proc/stat` Calculation:**
    - Removed `| head -n 30` from `dumpsys telephony.registry` inside `outerscreen_service.c` so `system_server` (`TelephonyRegistry.dump`) stops throwing `EPIPE (Broken pipe)` Java exceptions every second.
    - Reduced idle telephony polling to once every 2s (1s during calls) and logcat tail polling to once every 10s when idle.
    - Updated `get_cpu_usage()` in `outerscreen_service.c` to include all 8 `/proc/stat` CPU fields (`user`, `nice`, `sys`, `idle`, `iowait`, `irq`, `softirq`, `steal`) for accurate CPU% gauge readings.
  - **Trimmed Unneeded Google Background Services in `/system/bin/cat_perf.sh`:**
    - Added automatic `pm disable` on boot for `com.google.android.partnersetup`, Nearby Share (`DiscoveryService`, `LifeCycleService`, `ReceiveSurfaceService`, `SendSurfaceService`), `WearableService`, `OfflineBeaconService_Persistent`, `ClearcutDebugDumpService`, and `Tron.CollectionService`.
  - **Added Native `CatParts` ("Cat S22 Flip Settings") inside the Android Settings App (`/system/app/CatParts/CatParts.apk`):**
    - Ultra-lightweight (`17 KB`, zero background RAM) `IA_SETTINGS` preference controller that injects **Cat S22 Flip Settings** directly onto the main Android **Settings** homepage.
    - Configures PTT Short Press, PTT Long Hold, PTT In-Call Action, Outer Display Enable/Disable, Outer Display Brightness, Outer Display Footer Mode (Live Logcat / Pip-Boy Status / Off), and Physical T9 Keypad Backlight (Enable/Disable toggle, 5 Brightness levels `8%–100%`, and Inactivity Auto-Off Timeout `Always On / 5s / 10s / 30s / 60s` with instant wake on keypress) in real time via `/dev/shm_outerscreen_cmd`.
    - Added permissive SELinux domains (`system_app`, `platform_app`, `untrusted_app_30`) and explicit `allow appdomain` CIL rules in `/system/etc/selinux/plat_sepolicy.cil` for `/dev/shm_outerscreen_cmd` (`tmpfs`) and LED sysfs nodes (`sysfs_kb_light`, `sysfs_leds`).
