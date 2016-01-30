# NVIDIA Tegra6 "T132ref" development system
#
# Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.

$(call inherit-product, device/nvidia/soc/t210/device-t210.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, device/nvidia/platform/t210/device.mk)
$(call inherit-product, device/nvidia/product/tablet/device.mk)

PRODUCT_NAME := t210ref
PRODUCT_DEVICE := t210
PRODUCT_MODEL := t210ref
PRODUCT_MANUFACTURER := NVIDIA

## Values of PRODUCT_NAME and PRODUCT_DEVICE are mangeled before it can be
## used because of call to inherits, store their values to
## use later in this file below
_product_name := $(strip $(PRODUCT_NAME))
_product_device := $(strip $(PRODUCT_DEVICE))

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../common/init.cal_fct.rc:root/init.cal.rc \
    $(LOCAL_PATH)/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
    device/nvidia/platform/t210/ussrd.conf:system/etc/ussrd.conf

# Common for all ers
$(call inherit-product, $(LOCAL_PATH)/ers_common.mk)

## Icera modem integration: data + voice capable
################################################
## Modem firmware:
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/icera-modules.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/firmware/nvidia-e1729-loki/fw-cpy-nvidia-e1729-loki-do-prod.mk)
$(call inherit-product-if-exists, $(LOCAL_PATH)/../../common/icera/firmware/nvidia-e1729-loki/fw-cpy-nvidia-e1729-loki-voice-prod.mk)
PRODUCT_COPY_FILES += \
        $(call add-to-product-copy-files-if-exists, $(LOCAL_PATH)/init.icera.rc:root/init.icera.rc) \
        $(call add-to-product-copy-files-if-exists, vendor/nvidia/tegra/icera/ril/icera-util/ril_atc.usb.config:system/etc/ril_atc.config)
## Telephony capability. This includes rild, Mms & Dialer packages
## Note: for data-only, this must be removed and replaced by direct embed of rild (and Mms if required) packages.
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony.mk)
## Use phone overlay to fully handle voice capability
## Note: for data-only, this must be removed and replaced with overlay-tablet (or overlay-tablet-do allowing sms)
DEVICE_PACKAGE_OVERLAYS += $(LOCAL_PATH)/../../product/phone/overlay-phone

PRODUCT_PACKAGES += \
    rp3_image

## Factory Reset Protection to be disabled in RP2 Partition
PRODUCT_COPY_FILES += device/nvidia/tegraflash/t210/rp2_binaries/rp2_disable_frp.bin:rp2.bin

## common apps for all skus
$(call inherit-product-if-exists, vendor/nvidia/$(_product_device)/skus/t210ref_variants_common.mk)

## nvidia apps for this sku
$(call inherit-product-if-exists, vendor/nvidia/$(_product_device)/skus/$(_product_name).mk)

## sensor conponents for all skus
$(call inherit-product, vendor/nvidia/tegra/core/android/sensor/invensense.mk)

## Clean local variables
_product_name :=
_product_device :=

