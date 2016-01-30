include $(BUILD_SYSTEM)/multilib.mk

ifdef LOCAL_IS_HOST_MODULE
ifndef LOCAL_MODULE_HOST_ARCH
ifndef my_module_multilib
#ifneq ($(LOCAL_MODULE_CLASS),EXECUTABLES)
ifneq ($(findstring $(LOCAL_MODULE_CLASS),STATIC_LIBRARIES SHARED_LIBRARIES),)
    ifeq ($(HOST_PREFER_32_BIT),true)
        LOCAL_MULTILIB := 32
    else
    # By default we only build host module for the first arch.
        LOCAL_MULTILIB := first
    endif # HOST_PREFER_32_BIT
endif # EXECUTABLES STATIC_LIBRARIES SHARED_LIBRARIES
endif
endif
endif

include $(NVIDIA_BASE)
include $(BUILD_PREBUILT)
include $(NVIDIA_POST)
