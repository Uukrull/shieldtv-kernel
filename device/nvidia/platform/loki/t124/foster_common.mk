# NVIDIA Tegra4 "loki" development system
#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
#

PRODUCT_LOCALES := en_US in_ID ca_ES cs_CZ da_DK de_DE en_GB es_ES es_US tl_PH fr_FR hr_HR it_IT lv_LV lt_LT hu_HU nl_NL nb_NO pl_PL pt_BR pt_PT ro_RO sk_SK sl_SI fi_FI sv_SE vi_VN tr_TR el_GR bg_BG ru_RU sr_RS uk_UA iw_IL ar_EG fa_IR th_TH ko_KR zh_CN zh_TW ja_JP

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/power.loki.rc:system/etc/power.foster.rc \
    $(LOCAL_PATH)/init.foster.rc:root/init.foster.rc \
    $(LOCAL_PATH)/../../../common/init.none.rc:root/init.none.rc \
    $(LOCAL_PATH)/fstab.foster:root/fstab.foster \
    $(LOCAL_PATH)/init.t124.rc:root/init.t124.rc \
    $(LOCAL_PATH)/../../../common/init.nv_dev_board.usb.rc:root/init.nv_dev_board.usb.rc

PRODUCT_COPY_FILES += \
    device/nvidia/platform/loki/t124/nvaudio_conf_foster.xml:system/etc/nvaudio_conf.xml

PRODUCT_PROPERTY_OVERRIDES += ro.radio.noril=true

PRODUCT_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../overlays/wifi

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/audio_policy_foster.conf:system/etc/audio_policy.conf
endif

PRODUCT_PACKAGES += \
	sensors.foster \
	power.foster

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_PACKAGES += \
	Stats
endif

$(call inherit-product, $(LOCAL_PATH)/../foster_common.mk)

