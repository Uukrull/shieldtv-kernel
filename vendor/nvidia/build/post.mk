# This file needs to be included after the core Android build (base_rules.mk)

# Get a list of all possible registered targets
NVIDIA_TARGETS := $(LOCAL_MODULE)
ifdef LOCAL_IS_HOST_MODULE
NVIDIA_TARGETS += $(LOCAL_MODULE)$(HOST_2ND_ARCH_MODULE_SUFFIX)
else
ifdef TARGET_2ND_ARCH
NVIDIA_TARGETS += $(LOCAL_MODULE)$(TARGET_2ND_ARCH_MODULE_SUFFIX)
endif
endif

# Prune out all targets that weren't defined
NVIDIA_TARGETS := $(filter $(NVIDIA_TARGETS),$(ALL_MODULES))

# Do nothing until all callers have been updated

# Clean local variables
NVIDIA_TARGETS :=
