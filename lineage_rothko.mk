#
# Copyright (C) 2025 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from device makefile.
$(call inherit-product, device/xiaomi/rothko/device.mk)

# Stock MediaTek IMS and its platform-side compatibility closure.
$(call inherit-product, $(LOCAL_PATH)/ims.mk)

# Inherit some common LineageOS stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Product overlays are highest-priority first; retain stock geometry over common defaults.
PRODUCT_PACKAGE_OVERLAYS := $(LOCAL_PATH)/overlay $(PRODUCT_PACKAGE_OVERLAYS)

PRODUCT_NAME := lineage_rothko
PRODUCT_DEVICE := rothko
PRODUCT_MANUFACTURER := Xiaomi
PRODUCT_BRAND := Redmi
PRODUCT_MODEL := 2407FRK8EC

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi

PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="rothko-user 16 BP2A.250605.031.A3 OS3.0.307.0.WNNCNXM release-keys" \
    BuildFingerprint=Redmi/rothko/rothko:16/BP2A.250605.031.A3/OS3.0.307.0.WNNCNXM:user/release-keys

# The source A16 Bluetooth audio HAL and the stock MTK provider use different
# session libraries. Keep only the device-specific stock-compatible module.
PRODUCT_PACKAGES := $(filter-out audio.bluetooth.default,$(PRODUCT_PACKAGES))
