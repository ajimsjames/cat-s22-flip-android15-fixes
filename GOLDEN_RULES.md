# Cat S22 Flip — Persistent Build & Flash Golden Rules (DO NOT VIOLATE)

> [!CAUTION]
> Always check every rule in this file before modifying `system_mod.img`, running `lpmake`, editing SELinux policies, or flashing the Cat S22 Flip.

---

## 1. `lpmake` Logical Partition Names: ALWAYS `system` and `vendor` (NEVER `system_a` / `vendor_a`)
- The Cat S22 Flip is a **non-A/B** dynamic partition device (`super` = `3221225472` bytes).
- First-stage `init` mounts logical partitions named **`system`** and **`vendor`**.
- Using `--partition system_a` or `--partition vendor_a` causes first-stage `init` to fail to find `/system` and immediately reboot to the bootloader!
- **Always use [`/home/aju/Android/VoltageOS_Work/Infinityx_a16_work/scripts/build_super.sh`](file:///home/aju/Android/VoltageOS_Work/Infinityx_a16_work/scripts/build_super.sh)**:
  ```bash
  /home/aju/Android/voltageos/out/host/linux-x86/bin/lpmake \
    --metadata-size 65536 \
    --super-name super \
    --metadata-slots 2 \
    --device super:3221225472 \
    --group main:3217031168 \
    --partition system:readonly:$(stat -c%s "$SYS_IMG"):main \
    --image system="$SYS_IMG" \
    --partition vendor:readonly:$(stat -c%s "$VEN_IMG"):main \
    --image vendor="$VEN_IMG" \
    --sparse \
    --output "$OUT_IMG"
  ```

---

## 2. Flashing `super.img`: ALWAYS Use `fastbootd` (NEVER Bootloader Mode)
- Stock bootloader (`fastboot`) cannot flash logical/sparse `super` properly on the Cat S22 Flip.
- Always boot into userspace **`fastbootd`** first:
  - From Android: `adb reboot fastboot`
  - From Bootloader: `fastboot reboot fastboot`

---

## 3. `debugfs` Extended Attributes (`security.selinux`): ALWAYS Include Trailing NUL (`\x00`) & Correct Contexts
- `debugfs write` creates new inodes **without** `security.selinux` extended attributes. Any file without `security.selinux` is labeled `unlabeled` by the kernel and blocked by SELinux during early boot.
- When setting `security.selinux` with `ea_set -f`, the file **MUST** end with a NUL byte (`printf "u:object_r:...:s0\x00" > /tmp/ctx`).
- Mandatory SELinux contexts for critical system files:
  - `/system/etc/cgroups.json` → `u:object_r:cgroup_desc_file:s0\x00` (**NOT** `system_file`! If `system_file`, early `init` cannot read `cgroups.json` and `/dev/memcg` fails to mount, breaking `lmkd`).
  - `/system/etc/selinux/plat_sepolicy.cil` → `u:object_r:sepolicy_file:s0\x00`
  - `/system/build.prop` → `u:object_r:system_file:s0\x00` (mode `0100600`)
  - Custom daemons in `/system/bin/` (`outerscreen_service`, `cat_watchdog`, `cat_ptt_service`, `cat_hall_keylight`, `cat_boot_display`, `cat_perf.sh`) → `u:object_r:phhsu_exec:s0\x00` (with `seclabel u:r:phhsu_daemon:s0` in `cat_hardware.rc`).
  - `/system/bin/phh-prop-handler.sh` → `u:object_r:system_file:s0\x00`

---

## 4. SELinux CIL Verification Before Packing `super.img`
- Always test `/system/etc/selinux/plat_sepolicy.cil` against the device's `30.0.cil` and `vendor_sepolicy.cil` using `secilc` with Android `init`'s exact flags before packing `super.img`:
  ```bash
  /home/aju/Android/voltageos/out/host/linux-x86/bin/secilc -c 30 -M true -G -m -N \
    plat_sepolicy.cil 30.0.cil system_ext_sepolicy.cil product_sepolicy.cil \
    vendor_sepolicy.cil plat_pub_versioned.cil -o /tmp/policy.out -f /dev/null
  ```

---

## 5. 32-Bit ARM Dynamic Binaries Only (`armv7a-linux-androideabi21-clang`)
- The Cat S22 Flip runs a 64-bit kernel (`aarch64` Linux `4.9.227`) with **32-bit ARM (`armeabi-v7a`) userspace** (` treble_a64`).
- Never compile 64-bit (`aarch64`) binaries for `/system/bin` (no `/system/bin/linker64` exists).
- Never compile `-static` with NDK r30 `libc.a` (crashes with `SIGABRT` on kernel 4.9 due to `MADV_WIPEONFORK` in `pthread_create`).
- Never call `signal(SIGCHLD, SIG_IGN)` in any daemon that uses `system()` or `popen()`/`pclose()`, because `waitpid()` will fail with `ECHILD` and return `-1`.

---

## 6. Android 15/16 Edge-to-Edge UI Inset Rule for System Settings Activities (`targetSdkVersion 35`)
- Android 15/16 enforces edge-to-edge drawing by default on `Theme.DeviceDefault.Settings`.
- Any `Activity` / `PreferenceFragment` targeting API 35 must set `view.setFitsSystemWindows(true)` in `onViewCreated()` so the top `ActionBar` title does not overlap the first preference item.
