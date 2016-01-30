# NVIDIA Tegra7 "loki_e" development system
#
# Copyright (c) 2014, NVIDIA Corporation.  All rights reserved.

## This is the file that is common for all Loki_e skus(loki_e_base, loki_e_lte).

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    $(LOCAL_PATH)/media_profiles_loki_e.xml:system/etc/media_profiles.xml \
    $(LOCAL_PATH)/nvcamera_loki_e.conf:system/etc/nvcamera.conf

PRODUCT_PROPERTY_OVERRIDES += \
    ro.sf.lcd_density=320

SHIELDTECH_FEATURE_NVLAUNCHER := false

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
    device/nvidia/platform/loki/t210/audio_policy_loki.conf:system/etc/audio_policy.conf \
    $(LOCAL_PATH)/../legal/legal.html:system/etc/legal.html \
    $(LOCAL_PATH)/../legal/legal_zh_tw.html:system/etc/legal_zh_tw.html \
    $(LOCAL_PATH)/../legal/legal_zh_cn.html:system/etc/legal_zh_cn.html \
    $(LOCAL_PATH)/../legal/tos.html:system/etc/tos.html \
    $(LOCAL_PATH)/../legal/tos_zh_tw.html:system/etc/tos_zh_tw.html \
    $(LOCAL_PATH)/../legal/tos_zh_cn.html:system/etc/tos_zh_cn.html \
    $(LOCAL_PATH)/../legal/priv.html:system/etc/priv.html \
    $(LOCAL_PATH)/../legal/priv_zh_tw.html:system/etc/priv_zh_tw.html \
    $(LOCAL_PATH)/../legal/priv_zh_cn.html:system/etc/priv_zh_cn.html
else
PRODUCT_COPY_FILES += \
    device/nvidia/platform/t210/audio_policy_noenhance.conf:system/etc/audio_policy.conf
endif

PRODUCT_COPY_FILES += \
    device/nvidia/platform/t210/nvaudio_conf.xml:system/etc/nvaudio_conf.xml \
    device/nvidia/platform/t210/nvaudio_fx.xml:system/etc/nvaudio_fx.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../../common/init.cal.rc:root/init.cal.rc

PRODUCT_PACKAGES += \
    rp3_image

# Enable dex-preoptimization to speed up first boot sequence
ifeq ($(HOST_OS),linux)
  ifeq ($(TARGET_BUILD_VARIANT),user)
    ifeq ($(WITH_DEXPREOPT),)
      WITH_DEXPREOPT := true
    endif
  endif
endif

PRODUCT_COPY_FILES += device/nvidia/tegraflash/t210/rp2_binaries/rp2_enable_frp.bin:rp2.bin
