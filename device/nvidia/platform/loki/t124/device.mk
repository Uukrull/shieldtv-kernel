# NVIDIA Tegra5 "loki" development system
#
# Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.

## Common property overrides
PRODUCT_PROPERTY_OVERRIDES += ro.com.google.clientidbase=android-nvidia

$(call inherit-product-if-exists, vendor/nvidia/tegra/core/android/t124/full.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/core/nvidia-tegra-vendor.mk)
$(call inherit-product-if-exists, frameworks/base/data/videos/VideoPackage2.mk)
$(call inherit-product-if-exists, frameworks/base/data/sounds/AudioPackage3.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/secureos/nvsi/nvsi.mk)

include frameworks/native/build/phone-xhdpi-1024-dalvik-heap.mk
include $(LOCAL_PATH)/touchscreen/raydium/raydium.mk

PRODUCT_AAPT_CONFIG += mdpi hdpi xhdpi

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/bootloader/nvbootloader/odm-partner/loki
else
    NVFLASH_FILES_PATH := vendor/nvidia/tegra/odm/loki
endif


PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_B00_Hynix_4GB_H5TQ4G83AFR_TEC_792Mhz_r506_v2.cfg:bct_loki_b00.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_B00_Hynix_4GB_H5TQ4G83AFR_TEC_1056Mhz_r506_v7.cfg:bct_1056.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_Hynix_2GB_H5TC4G63AFR_RDA_792Mhz.cfg:bct.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_Hynix_2GB_H5TC4G63AFR_RDA_792Mhz.cfg:bct_loki.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_B00_SKU1_Hynix_4GB_H5TQ4G83AFR_TEC_204Mhz_r506_v1.cfg:bct_loki_b00_sku100.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2548_Hynix_4GB_H5TQ4G83AFR_TEC_204Mhz_r503_2gb_mode_v1_spi.cfg:bct_spi.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2549_Hynix_2GB_H5TC4G63AFR_RDA_792Mhz.cfg:bct_thor1_9.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2549_B00_Hynix_4GB_H5TQ4G83AFR_TEC_1056Mhz_r506_v7.cfg:bct_thor1_95.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2530_A00_Hynix_4GB_H5TQ4G83AFR_TEC_1056Mhz_r509_v6.cfg:bct_loki_ffd_sku0.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/E2530_A00_Hynix_4GB_H5TQ4G83AFR_TEC_1200Mhz_r509_v6.cfg:bct_loki_ffd_sku0_1200.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/P2530_C00_Hynix_2GB_H5TQ2G83FFR_TEC_1056MHz_r516_v3.cfg:bct_loki_ffd_sku100.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/P2530_C00_Hynix_2GB_H5TQ2G83FFR_TEC_1200MHz_r516_v3.cfg:bct_loki_ffd_2gb_sku0.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_foster.txt:NCT_foster.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_foster_a1.txt:NCT_foster_a1.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki.txt:NCT_loki.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_b00.txt:NCT_loki_b00.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_b00_sku100.txt:NCT_loki_b00_sku100.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_thor1_9.txt:NCT_thor1_9.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_thor1_95.txt:NCT_thor1_95.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku0.txt:NCT_loki_ffd_sku0.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku0_a1.txt:NCT_loki_ffd_sku0_a1.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku0_a3.txt:NCT_loki_ffd_sku0_a3.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku100.txt:NCT_loki_ffd_sku100.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku100_a1.txt:NCT_loki_ffd_sku100_a1.txt \
    $(NVFLASH_FILES_PATH)/nvflash/NCT_loki_ffd_sku100_a3.txt:NCT_loki_ffd_sku100_a3.txt \
    $(NVFLASH_FILES_PATH)/nvflash/eks_nokey.dat:eks.dat \
    $(NVFLASH_FILES_PATH)/partition_data/config/nvcamera.conf:system/etc/nvcamera.conf \
    $(NVFLASH_FILES_PATH)/nvflash/common_bct.cfg:common_bct.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/fuse_write.txt:fuse_write.txt \
    $(NVFLASH_FILES_PATH)/nvflash/bmp.bin:bmp.bin \
    $(NVFLASH_FILES_PATH)/nvflash/fuse_write.txt:fuse_write.txt \
    $(NVFLASH_FILES_PATH)/nvflash/charging.png:root/res/images/charger/charging.png \
    $(NVFLASH_FILES_PATH)/nvflash/charged.png:root/res/images/charger/charged.png \
    $(NVFLASH_FILES_PATH)/nvflash/fullycharged.png:root/res/images/charger/fullycharged.png \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_emmc_full.cfg:flash.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_emmc_full_mfgtest.cfg:mfg.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_emmc_full_mfgtest_signed.cfg:mfg_signed.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_emmc_full_signed.cfg:flash_signed.cfg \
    $(NVFLASH_FILES_PATH)/nvflash/nvbdktest_plan.txt:nvbdktest_plan.txt

PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_dtb_spi_sata_full.cfg:flash_spi_sata.cfg \

NVFLASH_CFG_BASE_FILE := $(NVFLASH_FILES_PATH)/nvflash/android_fastboot_nvtboot_dtb_emmc_full.cfg

NVFLASH_FILES_PATH :=

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml

PRODUCT_COPY_FILES += \
    $(call add-to-product-copy-files-if-exists,frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml)

PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/tegra-kbc.kl:system/usr/keylayout/tegra-kbc.kl \
  $(LOCAL_PATH)/../../../common/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
  $(LOCAL_PATH)/raydium_ts.idc:system/usr/idc/touch.idc \
  $(LOCAL_PATH)/sensor00fn11.idc:system/usr/idc/sensor00fn11.idc \
  $(LOCAL_PATH)/bt_vendor.conf:system/etc/bluetooth/bt_vendor.conf \
  $(LOCAL_PATH)/../../../common/wifi_loader.sh:system/bin/wifi_loader.sh \
  $(LOCAL_PATH)/../../../common/init.comms.rc:root/init.comms.rc \
  $(LOCAL_PATH)/../../../common/init.ussrd.rc:root/init.ussrd.rc \
  $(LOCAL_PATH)/../../../common/wpa_supplicant.sh:system/bin/wpa_supplicant.sh \
  $(LOCAL_PATH)/../../../common/ussr_setup.sh:system/bin/ussr_setup.sh \
  $(LOCAL_PATH)/ussrd.conf:system/etc/ussrd.conf \
  $(LOCAL_PATH)/../../../common/set_hwui_params.sh:system/bin/set_hwui_params.sh \
  $(LOCAL_PATH)/js_firmware.bin:system/etc/firmware/js_firmware.bin \
  $(LOCAL_PATH)/ct_firmware.bin:system/etc/firmware/ct_firmware.bin

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
  frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
  frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
  $(LOCAL_PATH)/media_profiles.xml:system/etc/media_profiles.xml \
  $(LOCAL_PATH)/media_codecs.xml:system/etc/media_codecs.xml
else
PRODUCT_COPY_FILES += \
  frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
  frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
  $(LOCAL_PATH)/media_profiles_noenhance.xml:system/etc/media_profiles.xml \
  $(LOCAL_PATH)/media_codecs_noenhance.xml:system/etc/media_codecs.xml \
  $(LOCAL_PATH)/audio_policy_noenhance.conf:system/etc/audio_policy.conf
endif
else
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/media_profiles.xml:system/etc/media_profiles.xml \
  $(LOCAL_PATH)/media_codecs_kk.xml:system/etc/media_codecs.xml \
  $(LOCAL_PATH)/legal/legal.html:system/etc/legal.html \
  $(LOCAL_PATH)/legal/legal_zh_tw.html:system/etc/legal_zh_tw.html \
  $(LOCAL_PATH)/legal/legal_zh_cn.html:system/etc/legal_zh_cn.html \
  $(LOCAL_PATH)/legal/tos.html:system/etc/tos.html \
  $(LOCAL_PATH)/legal/tos_zh_tw.html:system/etc/tos_zh_tw.html \
  $(LOCAL_PATH)/legal/tos_zh_cn.html:system/etc/tos_zh_cn.html \
  $(LOCAL_PATH)/legal/priv.html:system/etc/priv.html \
  $(LOCAL_PATH)/legal/priv_zh_tw.html:system/etc/priv_zh_tw.html \
  $(LOCAL_PATH)/legal/priv_zh_cn.html:system/etc/priv_zh_cn.html
else
PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/media_profiles_noenhance.xml:system/etc/media_profiles.xml \
  $(LOCAL_PATH)/media_codecs_noenhance_kk.xml:system/etc/media_codecs.xml \
  $(LOCAL_PATH)/audio_policy_kk.conf:system/etc/audio_policy.conf
endif
endif

ifeq ($(NO_ROOT_DEVICE),1)
  PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init_no_root_device.rc:root/init.rc
endif

# Face detection model
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/core/include/ft/model_frontalface.xml:system/etc/model_frontal.xml

# Icera modem integration
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/icera-modules.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../../common/icera/firmware/nvidia-e1729-loki/fw-cpy-nvidia-e1729-loki-do-prod.mk)
PRODUCT_COPY_FILES += \
        $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/init.icera.rc:root/init.icera.rc) \
        $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/icera/ril/icera-util/ril_atc.usb.config:system/etc/ril_atc.config)

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../../common/cluster:system/bin/cluster \
    $(LOCAL_PATH)/../../../common/cluster_get.sh:system/bin/cluster_get.sh \
    $(LOCAL_PATH)/../../../common/cluster_set.sh:system/bin/cluster_set.sh \
    $(LOCAL_PATH)/../../../common/dcc:system/bin/dcc \
    $(LOCAL_PATH)/../../../common/hotplug:system/bin/hotplug \
    $(LOCAL_PATH)/../../../common/mount_debugfs.sh:system/bin/mount_debugfs.sh

PRODUCT_COPY_FILES += \
    device/nvidia/platform/loki/nvcms/device.cfg:system/lib/nvcms/device.cfg

PRODUCT_COPY_FILES += \
	device/nvidia/common/bdaddr:system/etc/bluetooth/bdaddr \
	device/nvidia/platform/loki/t124/nvaudio_fx.xml:system/etc/nvaudio_fx.xml

PRODUCT_COPY_FILES += \
	device/nvidia/platform/loki/t124/enctune.conf:system/etc/enctune.conf

# nvcpud specific cpu frequencies config
PRODUCT_COPY_FILES += \
        device/nvidia/platform/loki/t124/nvcpud.conf:system/etc/nvcpud.conf

# pbc config
PRODUCT_COPY_FILES += \
        device/nvidia/platform/loki/t124/pbc.conf:system/etc/pbc.conf

PRODUCT_PACKAGES += \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
PRODUCT_COPY_FILES += \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm43241/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-vsdb-wapi-wl11d-sr-srvsdb-opt1.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm43241/fw_bcmdhd.bin \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm43241/wlan/bcm943241ipaagb_p304.txt:system/etc/nvram_43241.txt \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm43241/bluetooth/AB113_BCM43241B0_0012_Azurewave_AW-AH691_TEST.HCD:system/etc/firmware/bcm43241.hcd \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm4335/bluetooth/BCM4335B0_002.001.006.0037.0046_ORC.hcd:system/etc/firmware/bcm4335.hcd \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm4335/wlan/bcm94335wbfgn3_r04_hwoob.txt:system/etc/nvram_4335.txt \
   vendor/nvidia/tegra/3rdparty/bcmbinaries/bcm4335/wlan/sdio-ag-pool-p2p-pno-pktfilter-keepalive-aoe-ccx-sr-vsdb-proptxstatus-lpc-wl11u-autoabn.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm4335/fw_bcmdhd.bin
else
PRODUCT_COPY_FILES += \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm43241/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-vsdb-wapi-wl11d-sr-srvsdb-opt1.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm43241/fw_bcmdhd.bin \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm43241/wlan/bcm943241ipaagb_p304.txt:system/etc/nvram_43241.txt \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm43241/bluetooth/AB113_BCM43241B0_0012_Azurewave_AW-AH691_TEST.HCD:system/etc/firmware/bcm43241.hcd \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm4335/bluetooth/BCM4335B0_002.001.006.0037.0046_ORC.hcd:system/etc/firmware/bcm4335.hcd \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm4335/wlan/bcm94335wbfgn3_r04_hwoob.txt:system/etc/nvram_4335.txt \
   vendor/nvidia/tegra/prebuilt/loki/3rdparty/bcmbinaries/bcm4335/wlan/sdio-ag-pool-p2p-pno-pktfilter-keepalive-aoe-ccx-sr-vsdb-proptxstatus-lpc-wl11u-autoabn.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm4335/fw_bcmdhd.bin
endif

# Nvidia Miracast
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../../common/miracast/com.nvidia.miracast.xml:system/etc/permissions/com.nvidia.miracast.xml

# Copy tnspec
PRODUCT_COPY_FILES += $(LOCAL_PATH)/tnspec.json:tnspec.json

# NvBlit JNI library
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/graphics-partner/android/frameworks/Graphics/com.nvidia.graphics.xml:system/etc/permissions/com.nvidia.graphics.xml

#enable Widevine drm
PRODUCT_PROPERTY_OVERRIDES += drm.service.enabled=true
PRODUCT_PACKAGES += \
    com.google.widevine.software.drm.xml \
    com.google.widevine.software.drm \
    libdrmwvmplugin \
    libwvm \
    libWVStreamControlAPI_L1 \
    libwvdrm_L1

#needed by google GMS lib:libpatts_engine_jni_api.so
PRODUCT_PACKAGES += \
    libwebrtc_audio_coding

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
    librs_jni

PRODUCT_PACKAGES += \
	lights.loki \
	audio.primary.tegra \
	audio.a2dp.default \
	audio.usb.default \
	libaudiopolicymanager \
	libaudience_voicefx \
	audio.nvwc.tegra \
	pbc.loki \
	setup_fs \
	drmserver \
	Gallery2 \
	gpload \
	ctload \
	c2debugger \
	libdrmframework_jni \
	e2fsck \
	InputViewer \
	NVSS \
	charger \
	charger_res_images

PRODUCT_PACKAGES += \
	tos.img

# TegraOTA
PRODUCT_PACKAGES += \
	TegraOTA

# SHIELD sleep menu option
PRODUCT_PROPERTY_OVERRIDES += fw.sleep_in_power_menu=true

# HDCP SRM Support
PRODUCT_PACKAGES += \
		hdcp1x.srm \
		hdcp2x.srm \
		hdcp2xtest.srm

PRODUCT_PACKAGES += \
                overlaymon


# we have enough storage space to hold precise GC data
PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_CHARACTERISTICS := shield

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

# Enable secure USB debugging in user release build
ifeq ($(TARGET_BUILD_TYPE),release)
ifeq ($(TARGET_BUILD_VARIANT),user)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.adb.secure=1
endif
endif

#ControllerMapper
PRODUCT_PACKAGES += \
    ControllerMapper
