# NVIDIA Tegra7 "foster-e-hdd" development system
#
# Copyright (c) 2014, NVIDIA Corporation.  All rights reserved.
#
# AndroidProducts.mk is included before BoardConfig.mk, variable essential at
# start of build and used in here should always be intialized in this file

## This is the file that is common for mp and diag images, for a single sku.

## Common for all foster_e skus
$(call inherit-product, $(LOCAL_PATH)/foster_e_common.mk)

PRODUCT_PROPERTY_OVERRIDES += ro.radio.noril=true

## factory script
ifeq ($(wildcard vendor/nvidia/tegra/apps/diagsuite),vendor/nvidia/tegra/apps/diagsuite)
PRODUCT_COPY_FILES += \
    vendor/nvidia/tegra/apps/diagsuite/bin/release/flags/flag_for_foster_e_hdd.txt:flag_for_foster_e_hdd.txt
endif

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/../../t210/init.foster_e_hdd.rc:root/init.foster_e_hdd.rc \
    $(LOCAL_PATH)/../../t210/fstab.foster_e_hdd:root/fstab.foster_e_hdd

$(call inherit-product, $(LOCAL_PATH)/../foster_common.mk)

