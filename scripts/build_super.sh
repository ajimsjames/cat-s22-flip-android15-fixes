#!/usr/bin/env bash
# Cat S22 Flip — Canonical super.img builder (NON-A/B logical partitions: "system" and "vendor")
# NEVER use system_a / vendor_a on Cat S22 Flip! First-stage init expects "system" and "vendor".
set -e

WORK_DIR="/home/aju/Android/VoltageOS_Work/Infinityx_a16_work"
LPMAKE="/home/aju/Android/voltageos/out/host/linux-x86/bin/lpmake"
SYS_IMG="$WORK_DIR/working/system_mod.img"
VEN_IMG="$WORK_DIR/base_images/vendor.img"
OUT_IMG="${1:-$WORK_DIR/builds/Infinity-X_CatS22_super_v5.img}"

e2fsck -fy "$SYS_IMG" || true

"$LPMAKE" \
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

ls -lh "$OUT_IMG"
