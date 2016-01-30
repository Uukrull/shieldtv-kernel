###############################################################################
#
# Copyright (c) 2012-2014 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#
###############################################################################
# This file is included from $TOP/build/core/Makefile
# It has those variables available which are set from above Makefile
#

#
# Override OTA update package target (run with -n)
# Used for developer OTA packages which legitimately need to go back and forth
#
$(INTERNAL_OTA_PACKAGE_TARGET): $(BUILT_TARGET_FILES_PACKAGE) $(DISTTOOLS)
	@echo "Package Dev OTA: $@"
	$(hide) $(TOP)/build/tools/releasetools/ota_from_target_files -n -v \
	   --block \
	   -p $(HOST_OUT) \
	   -k $(KEY_CERT_PAIR) \
	   $(BUILT_TARGET_FILES_PACKAGE) $@

#
# Override properties in build.prop
#
# *** Use of TARGET_DEVICE here is intentional ***
ifneq ($(filter shieldtablet ardbeg loki t210, $(TARGET_DEVICE)),)
ifeq ($(NV_SKU_MANIFEST),)
# *** Use of TARGET_DEVICE here is intentional ***
# SKU manifest containing properties and values to change
NV_SKU_MANIFEST := vendor/nvidia/$(TARGET_DEVICE)/skus/sku-properties.xml
endif
# Tool which changes the value of properties in build.prop
NV_PROP_MANGLE_TOOL := vendor/nvidia/build/tasks/process_build_props.py
ifneq ($(wildcard $(NV_SKU_MANIFEST)),)
# List of TARGET_PRODUCTs for which we will make changes in build.prop
_skus := \
	wx_na_wf \
	wx_na_do \
	wx_un_mo \
	wx_un_do \
	wx_diag \
	loki_b \
	loki_p \
	loki_p_lte \
	foster \
	thor_195 \
	fosterdiag \
	lokidiag_b \
	lokidiag_p \
	lokidiag_p_lte \
	loki_e_wifi \
	foster_e \
	foster_e_hdd \
	loki_e_wifi_diag \
	foster_e_diag \
	foster_e_hdd_diag
ifneq ($(filter $(_skus), $(TARGET_PRODUCT)),)

droidcore: update-build-properties
.PHONY: update-build-properties

update-build-properties: $(INSTALLED_BUILD_PROP_TARGET) \
	                 $(NV_PROP_MANGLE_TOOL) \
			 $(NV_SKU_MANIFEST)
	@echo $@ - Changing properties for $(TARGET_PRODUCT)
	$(hide) $(filter %.py,$^) \
		-s $(TARGET_PRODUCT) \
		-m $(NV_SKU_MANIFEST) \
		-b $(filter %.prop,$^)
endif
# Clear local variable
_skus :=
endif
endif

# Override factory bundle target so that we can copy an APK inside it
# PRODUCT_FACTORY_BUNDLE_MODULES could not be used for target binaries
# Also PRODUCT_COPY_FILES could not be used for prebuilt apk
# *** Use of TARGET_DEVICE here is intentional ***
ifeq ($(TARGET_DEVICE),ardbeg)
ifneq ($(wildcard vendor/nvidia/tegra/apps/mfgtest),)
# Let the defaualt target depend on factory_bundle target
droidcore: factory_bundle
factory_bundle_dir := $(PRODUCT_OUT)/factory_bundle
$(eval $(call copy-one-file,$(PRODUCT_OUT)/tst.apk,$(factory_bundle_dir)/tst.apk))
nv_factory_copied_files := $(factory_bundle_dir)/tst.apk
$(eval $(call copy-one-file,$(PRODUCT_OUT)/tdc.apk,$(factory_bundle_dir)/tdc.apk))
nv_factory_copied_files += $(factory_bundle_dir)/tdc.apk
$(eval $(call copy-one-file,$(PRODUCT_OUT)/tmc.apk,$(factory_bundle_dir)/tmc.apk))
nv_factory_copied_files += $(factory_bundle_dir)/tmc.apk
$(eval $(call copy-one-file,$(PRODUCT_OUT)/pcba_testcases.xml,$(factory_bundle_dir)/pcba_testcases.xml))
nv_factory_copied_files += $(factory_bundle_dir)/pcba_testcases.xml
$(eval $(call copy-one-file,$(PRODUCT_OUT)/postassembly_testcases.xml,$(factory_bundle_dir)/postassembly_testcases.xml))
nv_factory_copied_files += $(factory_bundle_dir)/postassembly_testcases.xml
$(eval $(call copy-one-file,$(PRODUCT_OUT)/preassembly_testcases.xml,$(factory_bundle_dir)/preassembly_testcases.xml))
nv_factory_copied_files += $(factory_bundle_dir)/preassembly_testcases.xml
$(eval $(call copy-one-file,$(PRODUCT_OUT)/audio_testcases.xml,$(factory_bundle_dir)/audio_testcases.xml))
nv_factory_copied_files += $(factory_bundle_dir)/audio_testcases.xml
$(eval $(call copy-one-file,$(PRODUCT_OUT)/usbhostumsread,$(factory_bundle_dir)/usbhostumsread))
nv_factory_copied_files += $(factory_bundle_dir)/usbhostumsread

$(INSTALLED_FACTORY_BUNDLE_TARGET): $(nv_factory_copied_files)
endif
endif
