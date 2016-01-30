include vendor/nvidia/build/detectversion.mk
PRODUCT_MAKEFILES := \
  $(LOCAL_DIR)/loki.mk \
  $(LOCAL_DIR)/loki_p.mk \
  $(LOCAL_DIR)/loki_p_lte.mk \
  $(LOCAL_DIR)/loki_b.mk \
  $(LOCAL_DIR)/thor_195.mk \
  $(LOCAL_DIR)/foster.mk \
  $(LOCAL_DIR)/lokibl.mk
