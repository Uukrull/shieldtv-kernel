# NVIDIA Tegra6 "T132" development system
#
# Copyright (c) 2013-2015 NVIDIA Corporation.  All rights reserved.
#
# 32-bit specific product settings

$(call inherit-product-if-exists, frameworks/base/data/videos/VideoPackage2.mk)
$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackage3.mk)
$(call inherit-product, device/nvidia/platform/t210/device-common.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/t210/nvflash.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/touch/raydium.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/multimedia/base.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/multimedia/firmware.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/camera/full.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/services/nvcpl.mk)
$(call inherit-product, vendor/nvidia/tegra/core/android/services/edid.mk)

#enable Widevine drm
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true
PRODUCT_PACKAGES += \
    com.google.widevine.software.drm.xml \
    com.google.widevine.software.drm \
    libdrmwvmplugin \
    libwvm \
    libWVStreamControlAPI_L1 \
    libwvdrm_L1

ifeq ($(TARGET_PRODUCT),loki_e_lte)
PRODUCT_COPY_FILES += \
   $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gpsconfig-t210ref.xml:system/etc/gps/gpsconfig.xml)
else
PRODUCT_COPY_FILES += \
   $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/3rdparty/broadcom/gps/bin/gpsconfig-wf-t210ref.xml:system/etc/gps/gpsconfig.xml)
endif

PRODUCT_COPY_FILES += \
   device/nvidia/platform/loki/t210/pbc.conf:system/etc/pbc.conf

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)

PRODUCT_PACKAGES += \
    bpmp.bin \
    tegra_xusb_firmware \
    tegra21x_xusb_firmware

PRODUCT_PACKAGES += \
        tos.img \
        icera_host_test \
        setup_fs \
        e2fsck \
        make_ext4fs \
        hdmi_cec.tegra \
        lights.tegra \
        pbc.tegra \
        power.tegra \
        power.loki_e \
        power.loki_e_lte \
        power.loki_e_wifi \
        power.foster_e \
        power.foster_e_hdd \
        libnvglsi \
        libnvwsi \
        sensors.tegra

# HDCP SRM Support
PRODUCT_PACKAGES += \
		hdcp1x.srm \
		hdcp2x.srm \
		hdcp2xtest.srm
#dual wifi
PRODUCT_PACKAGES += \
    libnvwifi-service \
    wpa_supplicant_2
PRODUCT_COPY_FILES += \
   device/nvidia/drivers/comms/brcm_wpa_xlan.conf:system/etc/firmware/brcm_wpa_xlan.conf

#enable Widevine drm
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true
PRODUCT_PACKAGES += \
    liboemcrypto \
    libdrmdecrypt

PRODUCT_RUNTIMES := runtime_libart_default
else
# Live Wallpapers
PRODUCT_PACKAGES += \
    LiveWallpapers \
    LiveWallpapersPicker \
    HoloSpiralWallpaper \
    MagicSmokeWallpapers \
    NoiseField \
    Galaxy4 \
    VisualizationWallpapers \
    PhaseBeam \
    icera_host_test \
    librs_jni

PRODUCT_PACKAGES += \
	sensors.tegra \
	lights.tegra \
	audio.primary.tegra \
	audio.a2dp.default \
	audio.usb.default \
	libaudiopolicymanager \
	libnvoicefx \
	audio.nvwc.tegra \
	audio.nvrc.tegra \
	pbc.tegra \
	power.tegra \
        power.loki_e \
        power.loki_e_lte \
        power.foster_e \
        power.foster_e_hdd \
	setup_fs \
	drmserver \
	Gallery2 \
	libdrmframework_jni \
	e2fsck \
	charger \
	charger_res_images

PRODUCT_PACKAGES += \
	tos.img

# HDCP SRM Support
PRODUCT_PACKAGES += \
		hdcp1x.srm \
		hdcp2x.srm \
		hdcp2xtest.srm
endif

PRODUCT_PACKAGES += \
	gpload \
	ctload \
	c2debugger

# Application for sending feedback to NVIDIA
PRODUCT_PACKAGES += \
	nvidiafeedback

#TegraOTA
PRODUCT_PACKAGES += \
    TegraOTA

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_PACKAGES += \
	Stats
endif

# LED service
PRODUCT_PACKAGES += \
	NvShieldService \
	NvGamepadMonitorService

# Paragon filesystem solution binaries
PRODUCT_PACKAGES += \
    mount.ufsd \
    chkufsd \
    mkexfat \
    chkexfat \
    mkhfs \
    chkhfs \
    mkntfs \
    chkntfs


