# Copyright (c) 2013-2015, NVIDIA CORPORATION.  All rights reserved.
# Build definitions common to all NVIDIA boards.

# If during build configuration setup i.e. during choosecombo or lunch or
# using $TOP/buildspec.mk TARGET_PRODUCT is set to one of Nvidia boards then
# REFERENCE_DEVICE is the same as TARGET_DEVICE. For boards derived from 
# NVIDIA boards, REFERENCE_DEVICE should be set to the NVIDIA
# reference device name in BoardConfig.mk or in the shell environment.

REFERENCE_DEVICE ?= $(TARGET_DEVICE)
TARGET_USES_PYTHON_IN_VENDOR := true

TARGET_RELEASETOOLS_EXTENSIONS := device/nvidia/common

ifeq ($(SECURE_OS_BUILD),tlk)
	# enable secure HDCP for secure OS build
	BOARD_VENDOR_HDCP_ENABLED ?= true
	BOARD_ENABLE_SECURE_HDCP ?= 1
	BOARD_VENDOR_HDCP_PATH ?= vendor/nvidia/tegra/tests-partner/hdcp
endif

# common sepolicy
# try to detect AOSP master-based policy vs small KitKat policy
ifeq ($(PLATFORM_IS_AFTER_KITKAT),)
# KitKat based board specific sepolicy
BOARD_SEPOLICY_DIRS := device/nvidia/common/sepolicy/
BOARD_SEPOLICY_UNION := \
	te_macros
BOARD_SEPOLICY_UNION += \
	app.te \
	comms.te \
	domain.te \
	file_contexts \
	file.te \
	genfs_contexts \
	healthd.te \
	netd.te \
	property_contexts \
	property.te \
	untrusted_app.te \
	usb.te \
	ussrd.te \
	ussr_setup.te \
	vold.te \

else
# AOSP master based board specific sepolicy
BOARD_SEPOLICY_DIRS := device/nvidia/common/sepolicy_aosp/
BOARD_SEPOLICY_UNION := \
	te_macros
BOARD_SEPOLICY_UNION += \
	app.te \
	bluetooth.te \
	bootanim.te \
	ctload.te \
	cvc.te \
	cyupdate.te \
	device.te \
	diag.te \
	domain.te \
	drmserver.te \
	file_contexts \
	file.te \
	genfs_contexts \
	gpload.te \
	healthd.te \
	hostapd.te \
	installd.te \
	init.te \
	mediaserver.te \
	netd.te \
	pbc.te \
	platform_app.te \
	property_contexts \
	property.te \
	qvs.te \
	recovery.te \
	service_contexts \
	set_hwui.te \
	setup_fs.te \
	shell.te \
	surfaceflinger.te \
	system_app.te \
	system_server.te \
	tee.te \
	ueventd.te \
	uncrypt.te \
	untrusted_app.te \
	update_js_touch_fw.te \
	usb.te \
	ussrd.te \
	ussr_setup.te \
	vold.te \
	wifi_loader.te \
	wpa.te \
	zygote.te \

endif
