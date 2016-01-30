# Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.

# Common stuff for tablet products

# from device/google/atv/products/atv_generic.mk
# Put en_US first in the list, so make it default.
PRODUCT_LOCALES := en_US

# Include drawables for various densities.
PRODUCT_AAPT_CONFIG := normal large xlarge tvdpi hdpi xhdpi xxhdpi
PRODUCT_AAPT_PREF_CONFIG := xhdpi

# we have enough storage space to hold precise GC data
PRODUCT_TAGS += dalvik.gc.type-precise

PRODUCT_CHARACTERISTICS := tv

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config=mtp

DEVICE_PACKAGE_OVERLAYS := $(LOCAL_PATH)/../../common/overlay-common

$(call inherit-product, $(SRC_TARGET_DIR)/product/locales_full.mk)
$(call inherit-product, device/google/atv/products/atv_base.mk)

# Add base packages for non-GMS builds
ifneq ($(wildcard 3rdparty/google/gms-apps/tv),3rdparty/google/gms-apps/tv)
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_no_telephony.mk)
endif
