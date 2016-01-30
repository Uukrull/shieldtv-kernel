# NVIDIA Tegra4 "loki" development system
#
# Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## All essential packages
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
$(call inherit-product, device/nvidia/platform/loki/t124/device.mk)
$(call inherit-product, device/nvidia/product/tv/device.mk)

## Thse are default settings, it gets changed as per sku manifest properties
PRODUCT_NAME := foster
PRODUCT_DEVICE := loki
PRODUCT_MODEL := loki
PRODUCT_MANUFACTURER := NVIDIA
PRODUCT_BRAND := nvidia

## Fury LBH  feature
-include vendor/nvidia/fury/tools/lbh/fury.mk

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/ueventd.loki.rc:root/ueventd.foster.rc

## factory script
ifeq ($(wildcard vendor/nvidia/tegra/apps/diagsuite),vendor/nvidia/tegra/apps/diagsuite)
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/apps/diagsuite/bin/release/flag_scripts_foster/flag_for_foster.txt:flag_for_foster.txt
endif



## Values of PRODUCT_NAME and PRODUCT_DEVICE are mangeled before it can be
## used because of call to inherits, store their values to use later in this
## file below
_product_name := $(strip $(PRODUCT_NAME))
_product_device := $(strip $(PRODUCT_DEVICE))

## common for Foster SKUs
include $(LOCAL_PATH)/foster_common.mk

## common for mp images
$(call inherit-product-if-exists, vendor/nvidia/loki/diag/mp_common.mk)

## common apps for all skus
$(call inherit-product-if-exists, vendor/nvidia/$(_product_device)/skus/foster_common.mk)

## nvidia apps for this sku
$(call inherit-product-if-exists, vendor/nvidia/$(_product_device)/skus/$(_product_name).mk)

## 3rd-party apps for this sku
$(call inherit-product-if-exists, 3rdparty/applications/prebuilt/common/$(_product_name).mk)

## Clean local variables
_product_name :=
_product_device :=
