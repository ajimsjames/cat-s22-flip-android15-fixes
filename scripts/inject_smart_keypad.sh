#!/bin/bash
set -e

IMG="/home/aju/Android/VoltageOS_Work/unpacked/system.img"
BIN="/home/aju/Android/Cat/keypad_debounce_clean"

echo "Injecting updated keypad_debounce_clean into system.img..."
debugfs -w "$IMG" << CMD
cd /system/bin
rm keypad_debounce_clean
write ${BIN} keypad_debounce_clean
sif keypad_debounce_clean mode 0100755
sif keypad_debounce_clean uid 0
sif keypad_debounce_clean gid 0
ea_set keypad_debounce_clean security.selinux "u:object_r:system_file:s0\0"
q
CMD

echo "Checking filesystem..."
e2fsck -fy "$IMG"

echo "Repacking super.img..."
lpmake \
  --metadata-size 65536 \
  --super-name super \
  --metadata-slots 2 \
  --device super:9126805504 \
  --group qti_dynamic_partitions:9126805504 \
  --partition system:readonly:2621440000:qti_dynamic_partitions \
  --image system=/home/aju/Android/VoltageOS_Work/unpacked/system.img \
  --partition vendor:readonly:419389440:qti_dynamic_partitions \
  --image vendor=/home/aju/Android/VoltageOS_Work/unpacked/vendor.img \
  --sparse \
  --output /home/aju/Android/VoltageOS_Work/VoltageOS_CatS22_custom_super.img

echo "Done!"
