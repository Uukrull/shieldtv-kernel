# Use Nvidia optimized renderscript driver
include device/nvidia/soc/t210/BoardConfigCommon.mk
#Do not un comment this is done in BoardConfigCommon.mk
#OVERRIDE_RS_DRIVER := libnvRSDriver.so

TARGET_SYSTEM_PROP    := device/nvidia/soc/t210/system.prop
TARGET_SYSTEM_PROP    += device/nvidia/platform/t210/system.prop

ifeq ($(TARGET_PRODUCT),foster_e_hdd)
TARGET_RECOVERY_FSTAB := device/nvidia/platform/t210/fstab.foster_e_hdd
else
TARGET_RECOVERY_FSTAB := device/nvidia/platform/t210/fstab.t210ref
endif

# TARGET_KERNEL_DT_NAME := tegra210-grenada
TARGET_KERNEL_DT_NAME := tegra210-

BOARD_SUPPORT_NVOICE := true

BOARD_SUPPORT_NVAUDIOFX :=true

BOARD_USERDATAIMAGE_PARTITION_SIZE  := 12228902400
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2147483648

# OTA
TARGET_RECOVERY_UPDATER_LIBS += libnvrecoveryupdater
TARGET_RECOVERY_UPDATER_EXTRA_LIBS += libfs_mgr
TARGET_RECOVERY_UI_LIB := librecovery_ui
TARGET_RELEASETOOLS_EXTENSIONS := device/nvidia/common

BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR ?= device/nvidia/platform/t210/bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true

# powerhal
BOARD_USES_POWERHAL := true

# Wifi related defines
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION      := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
WIFI_DRIVER_FW_PATH_STA     := "/data/misc/wifi/firmware/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_AP      := "/data/misc/wifi/firmware/fw_bcmdhd_apsta.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/data/misc/wifi/firmware/fw_bcmdhd_p2p.bin"
WIFI_DRIVER_FW_PATH_PARAM   := "/data/misc/wifi/firmware/firmware_path"
WIFI_DRIVER_MODULE_ARG      := "iface_name=wlan0"
WIFI_DRIVER_MODULE_NAME     := "bcmdhd"

# Default HDMI mirror mode
# Crop (default) picks closest mode, crops to screen resolution
# Scale picks closest mode, scales to screen resolution (aspect preserved)
# Center picks a mode greater than or equal to the panel size and centers;
#     if no suitable mode is available, reverts to scale
BOARD_HDMI_MIRROR_MODE := Scale

# NVDPS can be enabled when display is set to continuous mode.
BOARD_HAS_NVDPS := true

BOARD_SUPPORT_SIMULATION := true
SIM_KERNEL_DT_NAME := tegra210-grenada

# Double buffered display surfaces reduce memory usage, but will decrease performance.
# The default is to triple buffer the display surfaces.
# BOARD_DISABLE_TRIPLE_BUFFERED_DISPLAY_SURFACES := true

# Use CMU-style config with Nvcms
NVCMS_CMU_USE_CONFIG := false

# Dalvik option
DALVIK_ENABLE_DYNAMIC_GC := true

# Using the NCT partition
TARGET_USE_NCT := true

#Display static images for charging
BOARD_CHARGER_STATIC_IMAGE := true

#Use tegra health HAL library
BOARD_HAL_STATIC_LIBRARIES := libhealthd.tegra

# Enable Paragon filesystem solution.
BOARD_SUPPORT_PARAGON_FUSE_UFSD := true

# Icera modem definitions
-include device/nvidia/common/icera/BoardConfigIcera.mk

# Raydium touch definitions
include device/nvidia/drivers/touchscreen/raydium/BoardConfigRaydium.mk

# sepolicy
# try to detect AOSP master-based policy vs small KitKat policy
ifeq ($(wildcard external/sepolicy/lmkd.te),)
# KitKat based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/t210/sepolicy/
else
# AOSP master based board specific sepolicy
BOARD_SEPOLICY_DIRS += device/nvidia/platform/t210/sepolicy_aosp/
endif

# Per-application sizes for shader cache
MAX_EGL_CACHE_SIZE := 128450560
MAX_EGL_CACHE_ENTRY_SIZE := 262144

# GPU/EMC boosting for hwcomposer yuv packing
HWC_YUV_PACKING_GPU_FREQ_MIN := 614400
HWC_YUV_PACKING_GPU_FREQ_MAX := 998400
HWC_YUV_PACKING_GPU_FREQ_PRIORITY := 15
HWC_YUV_PACKING_EMC_FREQ_MIN := 106560

# GPU/EMC floor for glcomposer composition
HWC_GLCOMPOSER_GPU_FREQ_MIN := 153600
HWC_GLCOMPOSER_GPU_FREQ_MAX := 998400
HWC_GLCOMPOSER_GPU_FREQ_PRIORITY := 15
HWC_GLCOMPOSER_EMC_FREQ_MIN := 4080