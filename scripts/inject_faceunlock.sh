#!/bin/bash
set -e

IMG="/home/aju/Android/VoltageOS_Work/unpacked/system.img"
ARM_DIR="/home/aju/Android/VoltageOS_Work/faceunlock_arm"

echo "Injecting faceunlock libraries into /system/lib/ and /system/priv-app/ParanoidSense/lib/arm/..."

# In /system/lib/
debugfs -w "$IMG" << CMD
cd /system/lib
write ${ARM_DIR}/libFaceDetectCA.so libFaceDetectCA.so
sif libFaceDetectCA.so mode 0100644
sif libFaceDetectCA.so uid 0
sif libFaceDetectCA.so gid 0
ea_set libFaceDetectCA.so security.selinux "u:object_r:system_lib_file:s0\0"

write ${ARM_DIR}/libmegface.so libmegface.so
sif libmegface.so mode 0100644
sif libmegface.so uid 0
sif libmegface.so gid 0
ea_set libmegface.so security.selinux "u:object_r:system_lib_file:s0\0"

write ${ARM_DIR}/libMegviiUnlock.so libMegviiUnlock.so
sif libMegviiUnlock.so mode 0100644
sif libMegviiUnlock.so uid 0
sif libMegviiUnlock.so gid 0
ea_set libMegviiUnlock.so security.selinux "u:object_r:system_lib_file:s0\0"

write ${ARM_DIR}/libMegviiUnlock-jni-1.2.so libMegviiUnlock-jni-1.2.so
sif libMegviiUnlock-jni-1.2.so mode 0100644
sif libMegviiUnlock-jni-1.2.so uid 0
sif libMegviiUnlock-jni-1.2.so gid 0
ea_set libMegviiUnlock-jni-1.2.so security.selinux "u:object_r:system_lib_file:s0\0"
q
CMD

# In /system/priv-app/ParanoidSense/lib/arm/
debugfs -w "$IMG" << CMD
cd /system/priv-app/ParanoidSense
mkdir lib
sif lib mode 040755
sif lib uid 0
sif lib gid 0
ea_set lib security.selinux "u:object_r:system_file:s0\0"
cd lib
mkdir arm
sif arm mode 040755
sif arm uid 0
sif arm gid 0
ea_set arm security.selinux "u:object_r:system_file:s0\0"
cd arm

write ${ARM_DIR}/libFaceDetectCA.so libFaceDetectCA.so
sif libFaceDetectCA.so mode 0100644
sif libFaceDetectCA.so uid 0
sif libFaceDetectCA.so gid 0
ea_set libFaceDetectCA.so security.selinux "u:object_r:system_file:s0\0"

write ${ARM_DIR}/libmegface.so libmegface.so
sif libmegface.so mode 0100644
sif libmegface.so uid 0
sif libmegface.so gid 0
ea_set libmegface.so security.selinux "u:object_r:system_file:s0\0"

write ${ARM_DIR}/libMegviiUnlock.so libMegviiUnlock.so
sif libMegviiUnlock.so mode 0100644
sif libMegviiUnlock.so uid 0
sif libMegviiUnlock.so gid 0
ea_set libMegviiUnlock.so security.selinux "u:object_r:system_file:s0\0"

write ${ARM_DIR}/libMegviiUnlock-jni-1.2.so libMegviiUnlock-jni-1.2.so
sif libMegviiUnlock-jni-1.2.so mode 0100644
sif libMegviiUnlock-jni-1.2.so uid 0
sif libMegviiUnlock-jni-1.2.so gid 0
ea_set libMegviiUnlock-jni-1.2.so security.selinux "u:object_r:system_file:s0\0"
q
CMD

echo "Done injecting libraries!"
