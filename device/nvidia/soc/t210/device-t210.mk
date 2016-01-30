# Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

ifeq ($(PLATFORM_IS_AFTER_KITKAT),1)
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
else
$(call inherit-product-if-exists, vendor/nvidia/tegra/core/nvidia-tegra-vendor.mk)
endif
$(call inherit-product-if-exists, vendor/nvidia/tegra/core/android/t210/full.mk)
$(call inherit-product-if-exists, vendor/nvidia/tegra/secureos/nvsi/nvsi.mk)

# Enable secure USB debugging in user release build
ifeq ($(TARGET_BUILD_TYPE),release)
ifeq ($(TARGET_BUILD_VARIANT),user)
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.adb.secure=1
endif
endif
