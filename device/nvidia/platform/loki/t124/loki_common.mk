# NVIDIA Tegra4 "loki" development system
#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
#

## These are default init.rc and settings files for all Loki skus

PRODUCT_LOCALES := en_US in_ID ca_ES cs_CZ da_DK de_DE en_GB es_ES es_US tl_PH fr_FR hr_HR it_IT lv_LV lt_LT hu_HU nl_NL nb_NO pl_PL pt_BR pt_PT ro_RO sk_SK sl_SI fi_FI sv_SE vi_VN tr_TR el_GR bg_BG ru_RU sr_RS uk_UA iw_IL ar_EG fa_IR th_TH ko_KR zh_CN zh_TW ja_JP

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/power.loki.rc:system/etc/power.loki.rc \
    $(LOCAL_PATH)/init.loki.rc:root/init.loki.rc \
    $(LOCAL_PATH)/init.icera.rc:root/init.icera.rc \
    $(LOCAL_PATH)/../../../common/init.none.rc:root/init.none.rc \
    $(LOCAL_PATH)/fstab.loki:root/fstab.loki \
    $(LOCAL_PATH)/init.t124.rc:root/init.t124.rc \
    $(LOCAL_PATH)/../../../common/init.nv_dev_board.usb.rc:root/init.nv_dev_board.usb.rc \
    $(LOCAL_PATH)/raydium_ts.idc:system/usr/idc/raydium_ts.idc \
    $(LOCAL_PATH)/../../../common/set_light_sensor_perm.sh:system/bin/set_light_sensor_perm.sh \
    $(LOCAL_PATH)/gps.conf:system/etc/gps.conf

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \

PRODUCT_COPY_FILES += \
    device/nvidia/platform/loki/t124/nvaudio_conf.xml:system/etc/nvaudio_conf.xml

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_policy.conf:system/etc/audio_policy.conf
endif
else
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/audio_policy_kk.conf:system/etc/audio_policy.conf
endif

PRODUCT_PACKAGES += \
	sensors.loki \
	power.loki

# apps
PRODUCT_PACKAGES += \
    TegraZone

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_PACKAGES += \
	Stats
endif

$(call inherit-product, $(LOCAL_PATH)/../loki_common.mk)

