# Inputs:
#   LOCAL_MODULE
#   LOCAL_2ND_ARCH_VAR_PREFIX
#   LOCAL_NVIDIA_EXPORTS
#
# This makefile saves and restores LOCAL_MODULE, any other variable is the
# responsiblity of the caller

NVIDIA_CHECK_MODULE := $(LOCAL_MODULE)
NVIDIA_2ND_ARCH_VAR_PREFIX := $(LOCAL_2ND_ARCH_VAR_PREFIX)

include $(CLEAR_VARS)

LOCAL_MODULE := $(NVIDIA_CHECK_MODULE)_apicheck
ifneq ($(NVIDIA_2ND_ARCH_VAR_PREFIX),)
LOCAL_MODULE := $(NVIDIA_CHECK_MODULE)_apicheck$(TARGET_2ND_ARCH_MODULE_SUFFIX)
endif

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(call local-intermediates-dir,,$(NVIDIA_2ND_ARCH_VAR_PREFIX))/CHECK

ifneq ($(NVIDIA_2ND_ARCH_VAR_PREFIX),)
LOCAL_MULTILIB := 32
else
LOCAL_MULTILIB := first
endif

GEN := $(local-generated-sources-dir)/check.c
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_NVIDIA_EXPORTS)
$(GEN): PRIVATE_CUSTOM_TOOL = python $(NVIDIA_GETEXPORTS) -apicheck none none none $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_NVIDIA_EXPORTS) $(NVIDIA_GETEXPORTS)
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SHARED_LIBRARIES := $(NVIDIA_CHECK_MODULE)
include $(BUILD_EXECUTABLE)

# restore some of the variables for potential further use in caller
LOCAL_MODULE := $(NVIDIA_CHECK_MODULE)

# Clear used variables
NVIDIA_CHECK_MODULE :=
NVIDIA_2ND_ARCH_VAR_PREFIX :=
