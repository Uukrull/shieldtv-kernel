# NVIDIA Tegra4 "loki" development system
#
# Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

$(warning Product "$(TARGET_PRODUCT)" is being deprecated. Instead use - loki_p, loki_b or loki_p_lte)

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product, device/nvidia/platform/loki/t124/device.mk)
$(call inherit-product, device/nvidia/product/tablet/device.mk)

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := loki
PRODUCT_DEVICE := loki
PRODUCT_MODEL := loki
PRODUCT_MANUFACTURER := NVIDIA
PRODUCT_BRAND := nvidia

## Fury LBH  feature
-include vendor/nvidia/fury/tools/lbh/fury.mk

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/ueventd.loki.rc:root/ueventd.loki.rc

## common for Loki SKUs
include $(LOCAL_PATH)/loki_common.mk

## common apps for all skus
$(call inherit-product-if-exists, vendor/nvidia/loki/skus/loki_common.mk)

