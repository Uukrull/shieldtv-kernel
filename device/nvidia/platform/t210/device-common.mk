# NVIDIA Tegra development system
#
# Copyright (c) 2013-2015 NVIDIA Corporation.  All rights reserved.
#
# Common 32/64-bit userspace options

include device/nvidia/common/dalvik/tablet-10in-hdpi-2048-dalvik-heap.mk
include $(LOCAL_PATH)/../../drivers/comms/comms.mk
include $(TEGRA_TOP)/core/android/services/utils.mk
-include $(TEGRA_TOP)/bct/t210/bct.mk

PRODUCT_AAPT_CONFIG += mdpi hdpi xhdpi

DEVICE_ROOT := device/nvidia

NVFLASH_FILES_PATH := $(DEVICE_ROOT)/tegraflash/t210

PRODUCT_COPY_FILES += \
    $(NVFLASH_FILES_PATH)/eks_nokey.dat:eks.dat \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sdmmc.xml:flash_t210_android_sdmmc.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sata.xml:flash_t210_android_sata.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sdmmc_fb.xml:flash_t210_android_sdmmc_fb.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sata_fb.xml:flash_t210_android_sata_fb.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sdmmc_diag.xml:flash_t210_android_sdmmc_diag.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sata_diag.xml:flash_t210_android_sata_diag.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sdmmc_fb_diag.xml:flash_t210_android_sdmmc_fb_diag.xml \
    $(NVFLASH_FILES_PATH)/flash_t210_android_sata_fb_diag.xml:flash_t210_android_sata_fb_diag.xml \
    $(NVFLASH_FILES_PATH)/bmp.blob:bmp.blob

NVFLASH_FILES_PATH :=

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.hdmi.cec.xml:system/etc/permissions/android.hardware.hdmi.cec.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \

# OPENGL AEP permission file
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:system/etc/permissions/android.hardware.opengles.aep.xml

PRODUCT_COPY_FILES += \
    $(call add-to-product-copy-files-if-exists,frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml) \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/js_firmware.bin:system/etc/firmware/js_firmware.bin) \
    $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/ct_firmware.bin:system/etc/firmware/ct_firmware.bin) \
    $(call add-to-product-copy-files-if-exists, device/nvidia/tegraflash/t210/rp4_binaries/rp4.bin:rp4.bin)

PRODUCT_COPY_FILES += \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.e2190.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.e2220.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.loki_e_lte.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.loki_e_wifi.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.loki_e_base.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.foster_e.rc \
  $(LOCAL_PATH)/ueventd.t210ref.rc:root/ueventd.foster_e_hdd.rc \
  $(LOCAL_PATH)/tegra-kbc.kl:system/usr/keylayout/tegra-kbc.kl \
  device/nvidia/platform/loki/Vendor_0955_Product_7205.kl:system/usr/keylayout/Vendor_0955_Product_7205.kl \
  device/nvidia/platform/loki/Vendor_0955_Product_7210.kl:system/usr/keylayout/Vendor_0955_Product_7210.kl \
  $(LOCAL_PATH)/../../common/dhcpcd.conf:system/etc/dhcpcd/dhcpcd.conf \
  $(LOCAL_PATH)/raydium_ts.idc:system/usr/idc/touch.idc \
  $(LOCAL_PATH)/sensor00fn11.idc:system/usr/idc/sensor00fn11.idc \
  $(LOCAL_PATH)/../../common/ussr_setup.sh:system/bin/ussr_setup.sh \
  $(LOCAL_PATH)/../../common/set_hwui_params.sh:system/bin/set_hwui_params.sh \
  $(LOCAL_PATH)/bt_vendor.conf:system/etc/bluetooth/bt_vendor.conf \
  $(LOCAL_PATH)/update_js_touch_fw.sh:system/bin/update_js_touch_fw.sh

PRODUCT_COPY_FILES += \
  $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/multimedia/audio/effects/android/libnvoicefx/audio_effects.conf:system/vendor/etc/audio_effects.conf)

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
  frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
  frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
  $(LOCAL_PATH)/media_codecs.xml:system/etc/media_codecs.xml
else
PRODUCT_COPY_FILES += \
  frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
  frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
  $(LOCAL_PATH)/media_codecs_noenhance.xml:system/etc/media_codecs.xml
endif
else
ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/media_codecs_kk.xml:system/etc/media_codecs.xml
else
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/media_codecs_noenhance_kk.xml:system/etc/media_codecs.xml
endif
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init.e2190.rc:root/init.e2190.rc \
    $(LOCAL_PATH)/init.e2220.rc:root/init.e2220.rc \
    $(LOCAL_PATH)/init.t210.rc:root/init.t210.rc \
    $(LOCAL_PATH)/init.dualwifi.rc:root/init.dualwifi.rc \
    $(LOCAL_PATH)/init.loki_e_lte.rc:root/init.loki_e_lte.rc \
    $(LOCAL_PATH)/init.loki_e_wifi.rc:root/init.loki_e_wifi.rc \
    $(LOCAL_PATH)/init.loki_e_base.rc:root/init.loki_e_base.rc \
    $(LOCAL_PATH)/init.foster_e.rc:root/init.foster_e.rc \
    $(LOCAL_PATH)/init.foster_e_hdd.rc:root/init.foster_e_hdd.rc \
    $(LOCAL_PATH)/init.loki_e_common.rc:root/init.loki_e_common.rc \
    $(LOCAL_PATH)/init.foster_e_common.rc:root/init.foster_e_common.rc \
    $(LOCAL_PATH)/init.loki_foster_e_common.rc:root/init.loki_foster_e_common.rc \
    $(DEVICE_ROOT)/common/init.tegra.rc:root/init.tegra.rc \
    $(DEVICE_ROOT)/common/init.tegra_emmc.rc:root/init.tegra_emmc.rc \
    $(DEVICE_ROOT)/common/init.ray_touch.rc:root/init.ray_touch.rc \
    $(DEVICE_ROOT)/common/init.tegra_sata.rc:root/init.tegra_sata.rc \
    $(DEVICE_ROOT)/soc/t210/init.t210_common.rc:root/init.t210_common.rc \
    $(DEVICE_ROOT)/common/init.ussrd.rc:root/init.ussrd.rc \
    $(LOCAL_PATH)/fstab.t210ref:root/fstab.e2190 \
    $(LOCAL_PATH)/fstab.t210ref:root/fstab.e2220 \
    $(LOCAL_PATH)/fstab.loki_e:root/fstab.loki_e_lte \
    $(LOCAL_PATH)/fstab.loki_e:root/fstab.loki_e_base \
    $(LOCAL_PATH)/fstab.loki_e:root/fstab.loki_e_wifi \
    $(LOCAL_PATH)/fstab.foster_e:root/fstab.foster_e \
    $(LOCAL_PATH)/fstab.foster_e_hdd:root/fstab.foster_e_hdd \
    $(DEVICE_ROOT)/common/init.nv_dev_board.usb.rc:root/init.nv_dev_board.usb.rc \
    $(DEVICE_ROOT)/common/init.tlk.rc:root/init.tlk.rc \
    $(DEVICE_ROOT)/common/init.hdcp.rc:root/init.hdcp.rc

# System power mode configuration file
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/power.loki_e_common.rc:system/etc/power.loki_e_lte.rc \
    $(LOCAL_PATH)/power.loki_e_common.rc:system/etc/power.loki_e_wifi.rc \
    $(LOCAL_PATH)/power.loki_e_common.rc:system/etc/power.loki_e_base.rc \
    $(LOCAL_PATH)/power.foster_e_common.rc:system/etc/power.foster_e.rc \
    $(LOCAL_PATH)/power.foster_e_common.rc:system/etc/power.foster_e_hdd.rc

# Face detection model
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/core/include/ft/model_frontalface.xml:system/etc/model_frontal.xml

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/cluster:system/bin/cluster \
    $(LOCAL_PATH)/../../common/cluster_get.sh:system/bin/cluster_get.sh \
    $(LOCAL_PATH)/../../common/cluster_set.sh:system/bin/cluster_set.sh \
    $(LOCAL_PATH)/../../common/dcc:system/bin/dcc \
    $(LOCAL_PATH)/../../common/hotplug:system/bin/hotplug \
    $(LOCAL_PATH)/../../common/mount_debugfs.sh:system/bin/mount_debugfs.sh

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/enctune.conf:system/etc/enctune.conf

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
    BCMBINARIES_PATH := vendor/nvidia/tegra/3rdparty/bcmbinaries
else
    BCMBINARIES_PATH := vendor/nvidia/tegra/prebuilt/t210/3rdparty/bcmbinaries
endif

PRODUCT_COPY_FILES += \
   $(BCMBINARIES_PATH)/bcm43241/bluetooth/AB113_BCM43241B0_0012_Azurewave_AW-AH691_TEST.HCD:system/etc/firmware/bcm43241.hcd \
   $(BCMBINARIES_PATH)/bcm43241/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-vsdb-wapi-wl11d-sr-srvsdb-opt1.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm43241/fw_bcmdhd.bin \
   $(BCMBINARIES_PATH)/bcm43241/wlan/bcm943241ipaagb_p100_hwoob.txt:system/etc/nvram_43241.txt \
   $(BCMBINARIES_PATH)/bcm43341/bluetooth/BCM43341A0_001.001.030.0015.0000_Generic_UART_37_4MHz_wlbga_iTR_Pluto_Evaluation_for_NVidia.hcd:system/etc/firmware/BCM43341A0_001.001.030.0015.0000.hcd \
   $(BCMBINARIES_PATH)/bcm43341/bluetooth/BCM43341B0_002.001.014.0008.0011.hcd:system/etc/firmware/BCM43341B0_002.001.014.0008.0011.hcd \
   $(BCMBINARIES_PATH)/bcm43341/wlan/sdio-ag-pno-pktfilter-keepalive-aoe-idsup-idauth-wme.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm43341/fw_bcmdhd.bin \
   $(BCMBINARIES_PATH)/bcm43341/wlan/sdio-ag-pno-p2p-proptxstatus-dmatxrc-rxov-pktfilter-keepalive-aoe-sr-wapi-wl11d.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm43341/fw_bcmdhd_a0.bin \
   $(BCMBINARIES_PATH)/bcm43341/wlan/bcm943341wbfgn_2_hwoob.txt:system/etc/nvram_rev2.txt \
   $(BCMBINARIES_PATH)/bcm43341/wlan/nvram_43341_rev3.txt:system/etc/nvram_rev3.txt \
   $(BCMBINARIES_PATH)/bcm43341/wlan/bcm943341wbfgn_4_hwoob.txt:system/etc/nvram_rev4.txt \
   $(BCMBINARIES_PATH)/bcm4335/bluetooth/BCM4335B0_002.001.006.0037.0046_ORC.hcd:system/etc/firmware/bcm4335.hcd \
   $(BCMBINARIES_PATH)/bcm4335/wlan/bcm94335wbfgn3_r04_hwoob.txt:system/etc/nvram_4335.txt \
   $(BCMBINARIES_PATH)/bcm4335/wlan/sdio-ag-pool-p2p-pno-pktfilter-keepalive-aoe-ccx-sr-vsdb-proptxstatus-lpc-wl11u-autoabn.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm4335/fw_bcmdhd.bin \
   $(BCMBINARIES_PATH)/bcm4350/wlan/bcm94350wlagbe_KA_hwoob.txt:system/etc/nvram_4350.txt \
   $(BCMBINARIES_PATH)/bcm4350/wlan/sdio-ag-p2p-pno-aoe-pktfilter-keepalive-sr-mchan-proptxstatus-ampduhostreorder-lpc-wl11u-txbf-pktctx-dmatxrc.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm4350/fw_bcmdhd.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_4354.txt:system/etc/nvram_4354.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_loki_e_4354.txt:system/etc/nvram_loki_e_4354.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_foster_e_4354.txt:system/etc/nvram_foster_e_4354.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_darcy_a00.txt:system/etc/nvram_darcy_a00.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_loki_e_antenna_tuned_4354.txt:system/etc/nvram_loki_e_antenna_tuned_4354.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_foster_e_antenna_tuned_4354.txt:system/etc/nvram_foster_e_antenna_tuned_4354.txt \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_4354.bin:system/etc/nvram_4354.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_loki_e_4354.bin:system/etc/nvram_loki_e_4354.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_foster_e_4354.bin:system/etc/nvram_foster_e_4354.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_loki_e_antenna_tuned_4354.bin:system/etc/nvram_loki_e_antenna_tuned_4354.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/nvram_foster_e_antenna_tuned_4354.bin:system/etc/nvram_foster_e_antenna_tuned_4354.bin \
   $(BCMBINARIES_PATH)/bcm4354a1/wlan/sdio-ag-p2p-pno-aoe-pktfilter-keepalive-sr-mchan-proptxstatus-ampduhostreorder-lpc-wl11u-txbf-pktctx-okc-tdls-ccx-ve-mfp-ltecxgpio.bin:$(TARGET_COPY_OUT_VENDOR)/firmware/bcm4354/fw_bcmdhd.bin \
   $(BCMBINARIES_PATH)/bcm4354/bluetooth/BCM4354_003.001.012.0163.0000_Nvidia_NV54_TEST_ONLY.hcd:system/etc/firmware/bcm4350.hcd

# Nvidia Miracast
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/miracast/com.nvidia.miracast.xml:system/etc/permissions/com.nvidia.miracast.xml

# NvBlit JNI library
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/graphics-partner/android/frameworks/Graphics/com.nvidia.graphics.xml:system/etc/permissions/com.nvidia.graphics.xml

PRODUCT_COPY_FILES += \
    $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/tnspec_data/t210/tnspec.json:tnspec.json)

T210_PREBUILT_BOOTLOADER_PATH := vendor/nvidia/tegra/bootloader/prebuilt/t210/signed/Loki/prod-loki/

ifneq ($(wildcard $(T210_PREBUILT_BOOTLOADER_PATH)*),)
PRODUCT_COPY_FILES += \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/cboot.bin.signed:cboot.bin.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/nvtboot.bin.signed:nvtboot.bin.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/nvtboot_cpu.bin.signed:nvtboot_cpu.bin.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/tos.img.signed:tos.img.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/warmboot.bin.signed:warmboot.bin.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/flash_t210_android_sdmmc_fb.xml:flash_t210_android_sdmmc_fb.xml.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e/bct_p2530_e01.bct:bct_p2530_e01.bct \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e_hdd/flash_t210_android_sata_fb.xml:flash_t210_android_sata_fb.xml.signed \
    $(T210_PREBUILT_BOOTLOADER_PATH)/foster_e_hdd/bct_p2530_sata_e01.bct:bct_p2530_sata_e01.bct
endif

# OTA version definition.  Depends on environment variable NV_OTA_VERSION
# being set prior to building.
ifneq ($(NV_OTA_VERSION),)
    PRODUCT_PROPERTY_OVERRIDES += \
        ro.build.version.ota = $(NV_OTA_VERSION)
endif
