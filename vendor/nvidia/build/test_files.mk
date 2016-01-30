# Only support test files in /data
ifneq ($(LOCAL_MODULE_PATH),)
$(error $(LOCAL_MODULE_MAKEFILE): $(LOCAL_MODULE): Use LOCAL_MODULE_RELATIVE_PATH instead of LOCAL_MODULE_PATH)
endif
ifneq ($(findstring ..,$(LOCAL_MODULE_RELATIVE_PATH) $(LOCAL_SRC_FILES)),)
$(error $(LOCAL_MODULE_MAKEFILE): $(LOCAL_MODULE): Do not use '..' - only install test files to /data/... or the host
endif

ifeq ($(LOCAL_IS_HOST_MODULE),true)
  LOCAL_MODULE_PATH := $(HOST_OUT)/usr/$(LOCAL_MODULE_RELATIVE_PATH)
  ifneq ($(filter nvidia-tests-automation,$(MAKECMDGOALS)),)
  LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests/host/$(LOCAL_MODULE_RELATIVE_PATH)
  endif
else
  LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/$(LOCAL_MODULE_RELATIVE_PATH)
  ifneq ($(filter nvidia-tests-automation,$(MAKECMDGOALS)),)
  LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests/$(TARGET_COPY_OUT_DATA)/$(LOCAL_MODULE_RELATIVE_PATH)
  endif
endif

# Prepend $(LOCAL_PATH) to src
# Prepend $(LOCAL_MODULE_PATH) to dest
# If dest is empty, use filename of src
LOCAL_SRC_FILES := $(foreach f,$(LOCAL_SRC_FILES), \
    $(eval _src := $(call word-colon,1,$(f))) \
    $(eval _dst := $(call word-colon,2,$(f))) \
    $(eval _out := $(LOCAL_PATH)/$(_src):$(LOCAL_MODULE_PATH)/$(or $(_dst), $(notdir $(_src)))) \
    $(_out))

# Expand directories, copy-many-files only handles files
LOCAL_SRC_FILES := $(foreach f,$(LOCAL_SRC_FILES), \
    $(eval _src := $(call word-colon,1,$(f))) \
    $(eval _dst := $(call word-colon,2,$(f))) \
    $(eval _srcs := $(shell find $(_src) -type f $(LOCAL_NVIDIA_FIND_FILTER))) \
    $(eval _out := $(foreach fs,$(_srcs), \
        $(fs):$(_dst)$(patsubst $(_src)%,%,$(fs)))) \
    $(_out))

installed_files := $(call copy-many-files,$(LOCAL_SRC_FILES))

LOCAL_ADDITIONAL_DEPENDENCIES += $(installed_files)

LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_IS_HOST_MODULE :=

include $(BUILD_SYSTEM)/multilib.mk
include $(NVIDIA_BASE)

LOCAL_SRC_FILES :=
LOCAL_MODULE_PATH :=
LOCAL_MODULE_RELATIVE_PATH :=
LOCAL_PROPRIETARY_MODULE := false

include $(BUILD_PHONY_PACKAGE)

$(cleantarget) : PRIVATE_CLEAN_FILES += $(installed_files)

include $(NVIDIA_POST)
