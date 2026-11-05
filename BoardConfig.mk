#
# Copyright (C) 2025 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/xiaomi/rothko

# A/B
AB_OTA_UPDATER := true

AB_OTA_PARTITIONS += \
    boot \
    dtbo \
    init_boot \
    odm \
    odm_dlkm \
    product \
    system \
    system_dlkm \
    system_ext \
    vbmeta \
    vbmeta_system \
    vbmeta_vendor \
    vendor \
    vendor_boot \
    vendor_dlkm

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv9-a
TARGET_CPU_ABI := arm64-v8a
# No stock CPU tuning property is available; use the toolchain's generic target.
TARGET_CPU_VARIANT := generic

# Boot Image
BOARD_KERNEL_BASE         := 0x3fff8000
BOARD_KERNEL_OFFSET       := 0x00008000
BOARD_KERNEL_PAGESIZE     := 4096
BOARD_RAMDISK_OFFSET      := 0x26f08000
BOARD_KERNEL_TAGS_OFFSET  := 0x07c88000
BOARD_DTB_OFFSET          := 0x07c88000
BOARD_BOOT_HEADER_VERSION := 4
BOARD_INIT_BOOT_HEADER_VERSION := 4

BOARD_INCLUDE_DTB_IN_BOOTIMG := true

BOARD_RAMDISK_USE_LZ4 := true
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_MOVE_GSI_AVB_KEYS_TO_VENDOR_BOOT := true

BOARD_MKBOOTIMG_ARGS += --kernel_offset $(BOARD_KERNEL_OFFSET)
BOARD_MKBOOTIMG_ARGS += --ramdisk_offset $(BOARD_RAMDISK_OFFSET)
BOARD_MKBOOTIMG_ARGS += --tags_offset $(BOARD_KERNEL_TAGS_OFFSET)
BOARD_MKBOOTIMG_ARGS += --dtb_offset $(BOARD_DTB_OFFSET)
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_MKBOOTIMG_INIT_ARGS += --header_version $(BOARD_INIT_BOOT_HEADER_VERSION)

BOARD_KERNEL_CMDLINE += bootopt=64S3,32N2,64N2

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := rothko
TARGET_NO_BOOTLOADER := true

# Display
TARGET_SCREEN_DENSITY := 520

# HIDL
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/configs/fingerprint/fod-aidl.xml
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/configs/wifi/android.hardware.wifi.hostapd.xml
# The connected CN device reports hardware SKU rothko. Preserve its stock
# radio/secure-element overrides as the ODM base for this single-device tree.
ODM_MANIFEST_FILES += $(DEVICE_PATH)/manifest_odm.xml
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
    $(DEVICE_PATH)/compatibility_matrix.xml \
    hardware/mediatek/vintf/mediatek_framework_compatibility_matrix.xml \
    hardware/xiaomi/vintf/xiaomi_framework_compatibility_matrix.xml \
    hardware/lineage/interfaces/compatibility_matrices/compatibility_matrix.lineage.xml

# Original fingerprint extensions use AIDL, unlike the common HIDL entries.
DEVICE_PRODUCT_COMPATIBILITY_MATRIX_FILE += \
    $(DEVICE_PATH)/configs/fingerprint/framework_compatibility_matrix.xml

# Kernel
KERNEL_PREBUILTS_DIR := $(DEVICE_PATH)-kernel

BOARD_KERNEL_IMAGE_NAME := Image.lz4
TARGET_PREBUILT_KERNEL := $(KERNEL_PREBUILTS_DIR)/$(BOARD_KERNEL_IMAGE_NAME)

BOARD_PREBUILT_DTBIMAGE_DIR := $(KERNEL_PREBUILTS_DIR)/dtb/
BOARD_PREBUILT_DTBOIMAGE := $(KERNEL_PREBUILTS_DIR)/dtbo.img

# Kernel modules
# Preserve stock module bytes/signatures; depmod may regenerate text metadata.
BOARD_DO_NOT_STRIP_VENDOR_MODULES := true
BOARD_DO_NOT_STRIP_VENDOR_RAMDISK_MODULES := true

BOARD_SYSTEM_KERNEL_MODULES_LOAD := $(strip $(shell cat $(KERNEL_PREBUILTS_DIR)/system_dlkm/modules.load))
BOARD_SYSTEM_KERNEL_MODULES := $(wildcard $(KERNEL_PREBUILTS_DIR)/system_dlkm/*.ko)

BOARD_VENDOR_KERNEL_MODULES_LOAD := $(strip $(shell cat $(KERNEL_PREBUILTS_DIR)/vendor_dlkm/modules.load))
BOARD_VENDOR_KERNEL_MODULES := $(wildcard $(KERNEL_PREBUILTS_DIR)/vendor_dlkm/*.ko)

BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(strip $(shell cat $(KERNEL_PREBUILTS_DIR)/vendor_ramdisk/modules.load))
BOARD_VENDOR_RAMDISK_RECOVERY_KERNEL_MODULES_LOAD := $(strip $(shell cat $(KERNEL_PREBUILTS_DIR)/vendor_ramdisk/modules.load.recovery))
BOARD_VENDOR_RAMDISK_KERNEL_MODULES := $(wildcard $(KERNEL_PREBUILTS_DIR)/vendor_ramdisk/*.ko)

# Partitions
BOARD_FLASH_BLOCK_SIZE := 262144 # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 8388608
BOARD_DTBOIMG_PARTITION_SIZE := 8388608
BOARD_USES_METADATA_PARTITION := true

# Partitions - Dynamic
BOARD_SUPER_PARTITION_SIZE := 9663676416
BOARD_SUPER_PARTITION_GROUPS := mediatek_dynamic_partitions
BOARD_MEDIATEK_DYNAMIC_PARTITIONS_SIZE := 9653190656 # stock LP group maximum: super minus 10 MiB

BOARD_MEDIATEK_DYNAMIC_PARTITIONS_PARTITION_LIST := \
    odm \
    odm_dlkm \
    product \
    system \
    system_dlkm \
    system_ext \
    vendor \
    vendor_dlkm

BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_ODM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs

TARGET_COPY_OUT_ODM := odm
TARGET_COPY_OUT_ODM_DLKM := odm_dlkm
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm

# Platform
TARGET_BOARD_PLATFORM := mt6989

# Power
$(call soong_config_set,power_libperfmgr,mode_extension_lib,//$(DEVICE_PATH):libperfmgr-ext-xiaomi)

# Properties
TARGET_SYSTEM_PROP += $(DEVICE_PATH)/system.prop
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop
TARGET_ODM_PROP += $(DEVICE_PATH)/odm.prop

# Recovery
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/init/fstab.mt6989
BOARD_INCLUDE_RECOVERY_RAMDISK_IN_VENDOR_BOOT := true
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := true
TARGET_RECOVERY_PIXEL_FORMAT := BGRA_8888
TARGET_USERIMAGES_USE_F2FS := true

# RIL
ENABLE_VENDOR_RIL_SERVICE := true

# Security patch level
BOOT_SECURITY_PATCH := 2026-02-01
VENDOR_SECURITY_PATCH := $(BOOT_SECURITY_PATCH)

# SELinux
include device/mediatek/sepolicy_vndr/SEPolicy.mk
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/system_ext

# Device colour policy provider. SurfaceFlinger links the device static library
# and compiles out the inert default implementation in the same step.
SOONG_CONFIG_NAMESPACES += surfaceflinger
SOONG_CONFIG_surfaceflinger += display_color_lib
SOONG_CONFIG_surfaceflinger_display_color_lib := librothko_display_color_provider

# Verified Boot
BOARD_AVB_ENABLE := true
BOARD_AVB_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_ALGORITHM := SHA256_RSA2048

BOARD_AVB_BOOT_ALGORITHM := SHA256_RSA2048
BOARD_AVB_BOOT_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
# Stock AVB descriptors use index 0. Do not derive an increased bootloader
# rollback index from the independently built framework's patch timestamp.
BOARD_AVB_BOOT_ROLLBACK_INDEX := 0
BOARD_AVB_BOOT_ROLLBACK_INDEX_LOCATION := 3

BOARD_AVB_VBMETA_SYSTEM_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_SYSTEM := product system system_ext
BOARD_AVB_VBMETA_SYSTEM_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX := 0
BOARD_AVB_VBMETA_SYSTEM_ROLLBACK_INDEX_LOCATION := 2

BOARD_AVB_VBMETA_VENDOR_ALGORITHM := SHA256_RSA2048
BOARD_AVB_VBMETA_VENDOR := vendor
BOARD_AVB_VBMETA_VENDOR_KEY_PATH := external/avb/test/data/testkey_rsa2048.pem
BOARD_AVB_VBMETA_VENDOR_ROLLBACK_INDEX := 0
BOARD_AVB_VBMETA_VENDOR_ROLLBACK_INDEX_LOCATION := 4

# Using sha256 for dm-verity partitions.
BOARD_AVB_SYSTEM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_OTHER_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_EXT_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_PRODUCT_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_ODM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_VENDOR_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_ODM_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256
BOARD_AVB_SYSTEM_DLKM_ADD_HASHTREE_FOOTER_ARGS += --hash_algorithm sha256

# Wi-Fi
# OS3.0.307 libwifi-hal-mtk.so keeps the pre-U QPR2 table: RTT cancel
# is at 0xd8 and subsystem restart at 0x318, verified from its initializer.
$(call soong_config_set_bool,mediatek_wifi_hal,use_pre_u_qpr2_struct,true)
WPA_SUPPLICANT_VERSION := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
# The stock MT6989 driver requires the RSN version bit for SAE.
$(call soong_config_set_bool,wpa_supplicant_8,legacy_sae_rsn_version,true)
# The stock hostapd prebuilt replaces the upstream daemon and its
# non-updatable APEX. Leaving the driver unset keeps those disabled.
BOARD_HOSTAPD_DRIVER :=
WIFI_DRIVER_FW_PATH_PARAM := "/dev/wmtWifi"
WIFI_DRIVER_FW_PATH_STA := "STA"
WIFI_DRIVER_FW_PATH_AP := "AP"
WIFI_DRIVER_FW_PATH_P2P := "P2P"
WIFI_DRIVER_STATE_CTRL_PARAM := "/dev/wmtWifi"
WIFI_DRIVER_STATE_ON := "1"
WIFI_DRIVER_STATE_OFF := "0"
WIFI_HAL_INTERFACE_COMBINATIONS := {{{STA}, 2}}
WIFI_HAL_INTERFACE_COMBINATIONS += ,{{{AP}, 2},}
WIFI_HAL_INTERFACE_COMBINATIONS += ,{{{STA}, 1}, {{AP}, 1}}
WIFI_HAL_INTERFACE_COMBINATIONS += ,{{{STA}, 1}, {{P2P}, 1}}
WIFI_HAL_INTERFACE_COMBINATIONS += ,{{{STA}, 1}, {{NAN}, 1}}
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
WIFI_FEATURE_HOSTAPD_11AX := true

# Inherit the proprietary files
include vendor/xiaomi/rothko/BoardConfigVendor.mk
