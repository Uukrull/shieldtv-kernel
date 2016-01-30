$(call inherit-product, $(LOCAL_PATH)/firmware.mk)
$(call inherit-product, $(LOCAL_PATH)/../multimedia/base.mk)
$(call inherit-product, $(LOCAL_PATH)/../multimedia/firmware.mk)
$(call inherit-product, $(LOCAL_PATH)/../multimedia/nvsi.mk)
ifndef PLATFORM_IS_AFTER_KITKAT
$(call inherit-product, $(LOCAL_PATH)/../multimedia/wfd.mk)
endif
$(call inherit-product, $(LOCAL_PATH)/../multimedia/widevine.mk)
$(call inherit-product, $(LOCAL_PATH)/../multimedia/playready.mk)
$(call inherit-product, $(LOCAL_PATH)/../multimedia/tests.mk)
