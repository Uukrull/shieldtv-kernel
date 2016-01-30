LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(BUILD_SYSTEM)/multilib.mk

ifndef my_module_multilib
# libraries default to building for both architectures
my_module_multilib := both
endif

include $(NVIDIA_BASE)
include $(NVIDIA_WARNINGS)
include $(NVIDIA_COVERAGE)

LOCAL_LDFLAGS += -Wl,--build-id=sha1

# try guessing the .export file if not given
ifeq ($(LOCAL_NVIDIA_EXPORTS),)
LOCAL_NVIDIA_EXPORTS := $(strip $(wildcard $(LOCAL_PATH)/$(LOCAL_MODULE)_*.export) $(wildcard $(LOCAL_PATH)/$(LOCAL_MODULE).export))
else
LOCAL_NVIDIA_EXPORTS := $(addprefix $(LOCAL_PATH)/,$(LOCAL_NVIDIA_EXPORTS))
endif

# if .export files are given, add linker script to linker options
ifneq ($(LOCAL_NVIDIA_EXPORTS),)
GEN := $(generated_sources_dir)/$(LOCAL_MODULE).script

$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_NVIDIA_EXPORTS)
$(GEN): PRIVATE_CUSTOM_TOOL = python $(NVIDIA_GETEXPORTS) -script none none none $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_NVIDIA_EXPORTS) $(NVIDIA_GETEXPORTS)
	$(transform-generated-source)

# This needs to be LOCAL_ADDITIONAL_DEPENDENCIES instead of LOCAL_GENERATED_SOURCES
# in case you don't have any non-generated sources (as in the static_and_shared_library case)
# The only thing that has an a order dependency on generated sources is normal objects,
# which wouldn't exist if you don't have any non-generated sources.
LOCAL_ADDITIONAL_DEPENDENCIES += $(GEN)

LOCAL_LDFLAGS += -Wl,--version-script=$(GEN)
endif

include $(BUILD_SHARED_LIBRARY)
include $(NVIDIA_POST)

# rule for building the apicheck executable
ifneq ($(LOCAL_NVIDIA_EXPORTS),)
ifeq ($(NVIDIA_APICHECK),1)

NVIDIA_CHECK_MODULE_LINK := $(LOCAL_BUILT_MODULE)

include $(BUILD_SYSTEM)/multilib.mk

ifndef my_module_multilib
# libraries default to building for both architectures
my_module_multilib := both
endif

LOCAL_2ND_ARCH_VAR_PREFIX :=
include $(BUILD_SYSTEM)/module_arch_supported.mk
my_module_primary_arch_supported := $(my_module_arch_supported)

# Do both checks before including apicheck.mk, since it will clear all of the
# inputs to module_arch_supported.mk
ifdef TARGET_2ND_ARCH
LOCAL_2ND_ARCH_VAR_PREFIX := $(TARGET_2ND_ARCH_VAR_PREFIX)
include $(BUILD_SYSTEM)/module_arch_supported.mk

ifeq ($(my_module_arch_supported),true)
include $(NVIDIA_BUILD_ROOT)/apicheck.mk
endif
endif

LOCAL_2ND_ARCH_VAR_PREFIX :=
ifeq ($(my_module_primary_arch_supported),true)
include $(NVIDIA_BUILD_ROOT)/apicheck.mk
endif

# restore some of the variables for potential further use in caller
LOCAL_BUILT_MODULE := $(NVIDIA_CHECK_MODULE_LINK)
# Clear used variables
NVIDIA_CHECK_MODULE_LINK :=
my_module_arch_supported :=
my_module_primary_arch_supported :=

endif
endif
