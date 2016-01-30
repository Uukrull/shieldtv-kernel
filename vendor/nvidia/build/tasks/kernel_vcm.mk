# Linux kernel and loadable kernel modules
#

ifneq ($(filter kernel,$(BUILD_BRAIN_MODULAR_COMPONENTS)),)

# Nothing to do in this case.

else ifeq ($(TARGET_KERNEL_VCM_BUILD),true)

ifneq ($(TOP),.)
$(error Kernel build assumes TOP == . i.e Android build has been started from TOP/Makefile )
endif

# Android build is started from the $TOP/Makefile, therefore $(CURDIR)
# gives the absolute path to the TOP.
KERNEL_PATH ?= $(CURDIR)/kernel

#kernel_version := $(strip $(shell head $(KERNEL_PATH)/Makefile | \
#	grep "SUBLEVEL =" | cut -d= -f2))

# Special handling for ARM64 kernel (diff arch/ and built-in bootloader)
TARGET_ARCH_KERNEL ?= $(TARGET_ARCH)

# Always use absolute path for NV_KERNEL_VCM_INTERMEDIATES_DIR
ifneq ($(filter /%, $(TARGET_OUT_INTERMEDIATES)),)
NV_KERNEL_VCM_INTERMEDIATES_DIR := $(TARGET_OUT_INTERMEDIATES)/KERNEL-VCM30
else
NV_KERNEL_VCM_INTERMEDIATES_DIR := $(CURDIR)/$(TARGET_OUT_INTERMEDIATES)/KERNEL-VCM30
endif

vcm_dotconfig := $(NV_KERNEL_VCM_INTERMEDIATES_DIR)/.config

ifeq ($(TARGET_ARCH_KERNEL),arm64)
BUILT_KERNEL_VCM_TARGET := $(NV_KERNEL_VCM_INTERMEDIATES_DIR)/arch/$(TARGET_ARCH_KERNEL)/boot/Image
else
BUILT_KERNEL_VCM_TARGET := $(NV_KERNEL_VCM_INTERMEDIATES_DIR)/arch/$(TARGET_ARCH_KERNEL)/boot/zImage
endif

TARGET_KERNEL_VCM_CONFIG ?= tegra_vcm30t124_android_defconfig

TARGET_KERNEL_VCM_DT_NAME := tegra124-ardbeg-p1859

# Always use absolute path for NV_KERNEL_VCM_MODULES_TARGET_DIR and
# NV_KERNEL_VCM_BIN_TARGET_DIR
ifneq ($(filter /%, $(TARGET_OUT)),)
NV_KERNEL_VCM_MODULES_TARGET_DIR := $(TARGET_OUT)/lib/modules/vcm
NV_KERNEL_VCM_BIN_TARGET_DIR     := $(TARGET_OUT)/bin/vcm
else
NV_KERNEL_VCM_MODULES_TARGET_DIR := $(CURDIR)/$(TARGET_OUT)/lib/modules/vcm
NV_KERNEL_VCM_BIN_TARGET_DIR     := $(CURDIR)/$(TARGET_OUT)/bin/vcm
endif



ifeq ($(wildcard $(KERNEL_PATH)/arch/$(TARGET_ARCH_KERNEL)/configs/$(TARGET_KERNEL_VCM_CONFIG)),)
    $(error Could not find kernel defconfig for board)
endif


KERNEL_VCM_DEFCONFIG_PATH := $(KERNEL_PATH)/arch/$(TARGET_ARCH_KERNEL)/configs/$(TARGET_KERNEL_VCM_CONFIG)

NV_KERNEL_VCM_BUILD_DIRECTORY_LIST :=  \
	$(NV_KERNEL_VCM_INTERMEDIATES_DIR) \
	$(NV_KERNEL_VCM_MODULES_TARGET_DIR) \
	$(NV_KERNEL_VCM_BIN_TARGET_DIR)

$(NV_KERNEL_VCM_BUILD_DIRECTORY_LIST):
	$(hide) mkdir -p $@

define dts-files-under
$(patsubst ./%,%,$(shell find $(1) -name "$(2)-*.dts"))
endef

define word-dash
$(word $(1),$(subst -,$(space),$(2)))
endef

# The target must provide a name for the DT file (sources located in arch/arm/boot/dts/*)
ifeq ($(TARGET_KERNEL_VCM_DT_NAME),)
    $(error Must provide a DT file name in TARGET_KERNEL_VCM_DT_NAME -- <kernel>/arch/arm/boot/dts/*)
else
    KERNEL_VCM_DTS_PATH := $(call dts-files-under,$(KERNEL_PATH)/arch/$(TARGET_ARCH_KERNEL)/boot/dts,$(call word-dash,1,$(TARGET_KERNEL_VCM_DT_NAME)))
    KERNEL_VCM_DT_NAME := $(subst .dts,,$(notdir $(KERNEL_VCM_DTS_PATH)))
    KERNEL_VCM_DT_NAME_DTB := $(subst .dts,.dtb,$(notdir $(KERNEL_VCM_DTS_PATH)))
    BUILT_KERNEL_VCM_DTB := $(addprefix $(NV_KERNEL_VCM_INTERMEDIATES_DIR)/arch/$(TARGET_ARCH_KERNEL)/boot/dts/,$(addsuffix .dtb,$(KERNEL_VCM_DT_NAME)))
    TARGET_BUILT_KERNEL_VCM_DTB := $(NV_KERNEL_VCM_INTERMEDIATES_DIR)/arch/$(TARGET_ARCH_KERNEL)/boot/dts/$(TARGET_KERNEL_VCM_DT_NAME).dtb
    VCM_INSTALLED_DTB_TARGET := $(addprefix $(OUT)/,$(addsuffix .dtb, $(KERNEL_VCM_DT_NAME)))
    VCM_DTS_PATH_EXIST := $(foreach dts_file,$(KERNEL_VCM_DTS_PATH),$(if $(wildcard $(dts_file)),,$(error DTS file not found -- $(dts_file))))
endif

define newline


endef



define kernel-vcm-make
$(KERNEL_EXTRA_ENV) $(MAKE) -C $(PRIVATE_SRC_PATH) \
    ARCH=$(TARGET_ARCH_KERNEL) \
    CROSS_COMPILE=$(PRIVATE_KERNEL_TOOLCHAIN) KCFLAGS=$(PRIVATE_KERNEL_CFLAGS) \
    O=$(NV_KERNEL_VCM_INTERMEDIATES_DIR) $(KERNEL_EXTRA_ARGS) \
    $(if $(SHOW_COMMANDS),V=1)
endef

$(vcm_dotconfig): $(KERNEL_VCM_DEFCONFIG_PATH) | $(NV_KERNEL_VCM_INTERMEDIATES_DIR)
	@echo "Kernel config vcm30t124 " $(TARGET_KERNEL_VCM_CONFIG)
	+$(show) $(kernel-vcm-make) $(TARGET_KERNEL_VCM_CONFIG)
ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
	@echo "TF SecureOS enabled kernel"
	$(hide) $(KERNEL_PATH)/scripts/config --file $@ \
	--enable TRUSTED_FOUNDATIONS \
	--enable TEGRA_USE_SECURE_KERNEL
endif
# Temporarily disable TLK secure OS related features
#ifeq ($(SECURE_OS_BUILD),tlk)
#	@echo "TLK SecureOS enabled kernel"
#	$(hide) $(KERNEL_PATH)/scripts/config --file $@ \
#	--enable TRUSTED_LITTLE_KERNEL \
#	--enable TEGRA_USE_SECURE_KERNEL \
#	--enable OTE_ENABLE_LOGGER
#endif
ifeq ($(NVIDIA_KERNEL_COVERAGE_ENABLED),1)
	@echo "Explicitly enabling coverage support in kernel config on user request"
	$(hide) $(KERNEL_PATH)/scripts/config --file $@ \
		--enable DEBUG_FS \
		--enable GCOV_KERNEL \
		--enable GCOV_TOOLCHAIN_IS_ANDROID \
		--disable GCOV_PROFILE_ALL
endif

$(TARGET_BUILT_KERNEL_VCM_DTB): $(vcm_dotconfig) $(BUILT_KERNEL_VCM_TARGET) FORCE
	$(info ==============Kernel DTS/DTB================)
	$(info KERNEL_VCM_DT_NAME_DTB = $(KERNEL_DT_NAME_DTB))
	$(info KERNEL_VCM_DTS_PATH = $(notdir $(KERNEL_DTS_PATH)))
	$(info BUILT_KERNEL_VCM_DTB = $(notdir $(BUILT_KERNEL_DTB)))
	$(info VCM_INSTALLED_DTB_TARGET = $(notdir $(VCM_INSTALLED_DTB_TARGET)))
	$(info ============================================)
	@echo "Device tree build" $(KERNEL_VCM_DT_NAME_DTB)
	+$(hide) $(kernel-vcm-make) $(KERNEL_VCM_DT_NAME_DTB)


kmodules-vcm-build_only: $(BUILT_KERNEL_VCM_TARGET) FORCE | $(NV_KERNEL_VCM_INTERMEDIATES_DIR)
	@echo "Kernel-vcm modules build"
	+$(hide) $(kernel-vcm-make) modules

# This will add all kernel modules we build for inclusion the system
# image - no blessing takes place.
kmodules-vcm: kmodules-vcm-build_only FORCE | $(NV_KERNEL_VCM_MODULES_TARGET_DIR) $(NV_COMPAT_KERNEL_MODULES_TARGET_DIR)
	@echo "Kernel-vcm modules install"
	for f in `find $(NV_KERNEL_VCM_INTERMEDIATES_DIR) -name "*.ko"` ; do cp -v "$$f" $(NV_KERNEL_VCM_MODULES_TARGET_DIR) ; done


$(BUILT_KERNEL_VCM_TARGET): $(vcm_dotconfig) $(TARGET_BUILT_KERNEL_VCM_DTB) FORCE | $(NV_KERNEL_VCM_INTERMEDIATES_DIR)
	@echo "Kernel build vcm"
	+$(hide) $(kernel-vcm-make) zImage
	+$(hide) $(BOOT_WRAPPER_CMD)

# We need to build kernel only for VCM30T124
kernel-vcm: kernel-vcm-build_only kmodules-vcm
	@echo "kernel-vcm-build_only called for kernel"

kernel-vcm-build_only: $(BUILT_KERNEL_VCM_TARGET)
	@echo "kernel VCM + modules built successfully! (Note, just build, no install done!)"

kernel-vcm-%: | $(NV_KERNEL_VCM_INTERMEDIATES_DIR)
	+$(hide) $(kernel-vcm-make) $*

dev: kernel-vcm
	@echo "kernel-vcm"

.PHONY: kernel-vcm kernel-vcm-%
# Set private variables for all builds.
ifeq ($(TARGET_ARCH_KERNEL),arm64)
kernel-vcm-% kmodules-vcm-build_only $(vcm_dotconfig) $(TARGET_BUILT_KERNEL_VCM_DTB) $(BUILT_KERNEL_VCM_TARGET): PRIVATE_KERNEL_CFLAGS := -mno-android
endif
endif
