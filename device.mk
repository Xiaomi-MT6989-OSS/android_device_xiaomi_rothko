#
# Copyright (C) 2025 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Original FodEngine lifecycle integration, loaded by SystemServer from system_ext.
PRODUCT_PACKAGES += rothko-fod-services
PRODUCT_SYSTEM_SERVER_JARS_EXTRA += system_ext:rothko-fod-services

# Device-owned factory thermal brightness table; upstream clampers retain all other limits.
PRODUCT_PACKAGES += rothko-brightness-policy
PRODUCT_SYSTEM_SERVER_JARS_EXTRA += system_ext:rothko-brightness-policy
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/displayconfig/common_multi_factor_thermal_brightness_control.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/displayconfig/common_multi_factor_thermal_brightness_control.xml

# Feed real stock Goodix FOD touches into the standard Doze one-shot wakeup path.
# The original sensors multihal also loads /odm/etc/sensors/hals.conf.
PRODUCT_PACKAGES += sensors.rothko.udfps
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/sensors/udfps/hals.conf:$(TARGET_COPY_OUT_ODM)/etc/sensors/hals.conf

# Device colour controller: serves the LiveDisplay colour-profile interface and
# owns the only channel to the stock Xiaomi DisplayFeature service.
PRODUCT_PACKAGES += vendor.lineage.livedisplay-service.rothko

# SF reapplies these thread profiles after init's service profiles. Use the
# supported vendor override rather than replacing the upstream init service.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/task_profiles.json:$(TARGET_COPY_OUT_VENDOR)/etc/task_profiles.json

# Android 16 camera NDK client for the stock face HAL (its Android 14 client
# cannot register a listener with the Android 16 cameraserver).
PRODUCT_PACKAGES += libcamera2ndk_vendor_rothko

# Inherit generic_ramdisk product configuration
$(call inherit-product, $(SRC_TARGET_DIR)/product/generic_ramdisk.mk)

# # Project ID Quota
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# Setup dalvik vm configs
$(call inherit-product, frameworks/native/build/phone-xhdpi-6144-dalvik-heap.mk)

# A/B
$(call inherit-product, $(SRC_TARGET_DIR)/product/virtual_ab_ota/launch_with_vendor_ramdisk.mk)

PRODUCT_PACKAGES += \
    com.android.hardware.boot \
    android.hardware.boot-service.default_recovery

PRODUCT_PACKAGES += \
    update_engine \
    update_engine_sideload \
    update_verifier

PRODUCT_PACKAGES_DEBUG += \
    update_engine_client

AB_OTA_POSTINSTALL_CONFIG += \
    RUN_POSTINSTALL_system=true \
    POSTINSTALL_PATH_system=system/bin/otapreopt_script \
    FILESYSTEM_TYPE_system=$(BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE) \
    POSTINSTALL_OPTIONAL_system=true

AB_OTA_POSTINSTALL_CONFIG += \
    RUN_POSTINSTALL_vendor=true \
    POSTINSTALL_PATH_vendor=bin/checkpoint_gc \
    FILESYSTEM_TYPE_vendor=$(BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE) \
    POSTINSTALL_OPTIONAL_vendor=true

PRODUCT_PACKAGES += \
    checkpoint_gc \
    otapreopt_script

# LK/preloader zero-touch: never install create_pl_dev in system or recovery.

# Audio
$(call soong_config_set_bool,audio_bluetooth_hw,use_stock_hal,true)
PRODUCT_PACKAGES += \
    android.hardware.audio.service.mediatek \
    android.hardware.audio.effect@7.0-impl

PRODUCT_PACKAGES += \
    audio.primary.default \
    audio.bluetooth.default.rothko \
    audio.usb.default

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/configs/audio/,$(TARGET_COPY_OUT_VENDOR)/etc)

# Restore the stock parameter directories before the original primary HAL loads.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/init/init.rothko.audio.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.rothko.audio.rc

# Bluetooth
PRODUCT_PACKAGES += \
    android.hardware.bluetooth.audio-impl \
    android.hardware.bluetooth-service.mediatek

# NFC: the stock TMS AIDL HAL implements the frozen android.hardware.nfc V1
# contract. Core reader and host-card-emulation declarations are followed by
# stock off-host features; UICC/eSE functionality still needs runtime acceptance.
PRODUCT_PACKAGES += nfc-service-tms.xml

PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.hcef.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.nfc.hcef.xml

# These four permission XMLs are byte-identical to rothko stock ODM files.
# Off-host declarations do not replace OMAPI access rules or provision applets.
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.nfc.ese.xml:$(TARGET_COPY_OUT_ODM)/etc/permissions/android.hardware.nfc.ese.xml \
    frameworks/native/data/etc/android.hardware.nfc.uicc.xml:$(TARGET_COPY_OUT_ODM)/etc/permissions/android.hardware.nfc.uicc.xml \
    frameworks/native/data/etc/android.hardware.se.omapi.ese.xml:$(TARGET_COPY_OUT_ODM)/etc/permissions/android.hardware.se.omapi.ese.xml \
    frameworks/native/data/etc/android.hardware.se.omapi.uicc.xml:$(TARGET_COPY_OUT_ODM)/etc/permissions/android.hardware.se.omapi.uicc.xml

# Chip information: pinned source script is byte-identical to stock.
PRODUCT_PACKAGES += \
    chipinfo

# ConsumerIR
PRODUCT_PACKAGES += \
    android.hardware.ir-service.example

# Display
PRODUCT_PACKAGES += \
    android.hardware.memtrack-service.mediatek

# Display: stock calibration with device-owned thermal/content bounds for SDR and HDR.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/displayconfig/display_id_4627039422300187648.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/displayconfig/display_id_4627039422300187648.xml

# DRM
PRODUCT_PACKAGES += \
    android.hardware.drm-service.clearkey

# Matches the vendor APEX library export in rothko stock linker.config.pb.
PRODUCT_VENDOR_LINKER_CONFIG_FRAGMENTS += \
    $(LOCAL_PATH)/configs/linker.config.json

# Dynamic Partitions
PRODUCT_USE_DYNAMIC_PARTITIONS := true

PRODUCT_PACKAGES += \
    android.hardware.fastboot@1.1-impl.custom \
    fastbootd

# Health
PRODUCT_PACKAGES += \
    android.hardware.health-service.rothko \
    android.hardware.health-service.mediatek-recovery

# IFAA manager
PRODUCT_PACKAGES += \
    IFAAService

# Light
PRODUCT_PACKAGES += \
    android.hardware.light-service.lineage

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.audio.low_latency.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.audio.low_latency.xml \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.concurrent.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.concurrent.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.camera.full.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.full.xml \
    frameworks/native/data/etc/android.hardware.camera.raw.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.camera.raw.xml \
    frameworks/native/data/etc/android.hardware.consumerir.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.consumerir.xml \
    frameworks/native/data/etc/android.hardware.faketouch.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.faketouch.xml \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.fingerprint.xml \
    frameworks/native/data/etc/android.hardware.keystore.app_attest_key.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.keystore.app_attest_key.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.dynamic.head_tracker.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.dynamic.head_tracker.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.telephony.ims.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.telephony.ims.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.distinct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.distinct.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.multitouch.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.touchscreen.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.vulkan.compute-0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.compute.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-1.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.level.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_3.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.vulkan.version.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.passpoint.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.passpoint.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.software.device_id_attestation.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.device_id_attestation.xml \
    frameworks/native/data/etc/android.software.ipsec_tunnels.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.ipsec_tunnels.xml \
    frameworks/native/data/etc/android.software.midi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/android.software.opengles.deqp.level-2022-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.opengles.deqp.level.xml \
    frameworks/native/data/etc/android.software.verified_boot.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.verified_boot.xml \
    frameworks/native/data/etc/android.software.vulkan.deqp.level-2022-03-01.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/android.software.vulkan.deqp.level.xml

# Power
PRODUCT_PACKAGES += \
    android.hardware.power-service.lineage-libperfmgr \
    vendor.mediatek.hardware.mtkpower@1.2-service.stub \
    libmtkperf_client_vendor \
    libmtkperf_client

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/powerhint.json:$(TARGET_COPY_OUT_VENDOR)/etc/powerhint.json

# Properties
include $(LOCAL_PATH)/vendor_logtag.mk

# Media
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/configs/seccomp,$(TARGET_COPY_OUT_VENDOR)/etc/seccomp_policy) \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/configs/media,$(TARGET_COPY_OUT_VENDOR)/etc)

# Sensors: retain the verified stock AIDL multihal and MTK subHAL as prebuilts.

PRODUCT_PACKAGES += \
    sensors.dynamic_sensor_hal

# Shipping API level
PRODUCT_SHIPPING_API_LEVEL := 34

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH) \
    hardware/google/pixel \
    hardware/google/interfaces \
    hardware/lineage/interfaces/power-libperfmgr \
    hardware/mediatek \
    hardware/mediatek/libmtkperf_client \
    hardware/xiaomi

# Thermal
PRODUCT_PACKAGES += \
    android.hardware.thermal-service.mediatek

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/thermal/thermal_info_config.json:$(TARGET_COPY_OUT_VENDOR)/etc/thermal_info_config.json

# USB
$(call soong_config_set_bool,android_hardware_mediatek_usb,audio_accessory_supported,true)

PRODUCT_PACKAGES += \
    android.hardware.usb-service.mediatek \
    android.hardware.usb.gadget-service.mediatek

# Vendor service manager
PRODUCT_PACKAGES += \
    vndservicemanager

# Vibrator: use the verified stock ODM Xiaomi vibratorfeature service.

# Rootdir
PRODUCT_PACKAGES += \
    fstab.mt6989 \
    fstab.mt6989.vendor_ramdisk \
    init.batterysecret.rc \
    init.connectivity.rc \
    init.mi_thermald.rc \
    init.modem.rc \
    init.project.rc \
    init.mt6989.rc \
    init.mt6989.usb.rc \
    init.rothko.usb.rc \
    init.mtkgki.rc \
    init.sensor_2_0.rc \
    ueventd.mt6989.rc

# The reused module-loading service must have its script and configuration.
PRODUCT_PACKAGES += \
    init.insmod.sh \
    init.insmod.mt6989.cfg

# Wi-Fi: stock hostapd and its private VNDK34 TLS closure are inherited
# from vendor; keep the source supplicant until client-mode WPA3 is verified.
PRODUCT_PACKAGES += \
    wpa_supplicant \
    RothkoWifiResourcesOverlay \
    libwifi-hal-wrapper \
    android.hardware.wifi-service

PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/configs/wifi/,$(TARGET_COPY_OUT_VENDOR)/etc/wifi)

# Inherit the proprietary files
$(call inherit-product, vendor/xiaomi/rothko/rothko-vendor.mk)

# Goodix requests firmware before logical ODM is mounted in Recovery.
# Keep stock files for kernel lookup and the /odm/firmware runtime symlink.
# Copy to its physical destination: host cp must not follow an absolute symlink.
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/goodix_cfg_group_rothko.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko_nowater.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/goodix_cfg_group_rothko_nowater.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko_weijing.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/goodix_cfg_group_rothko_weijing.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_firmware_rothko.bin:$(TARGET_COPY_OUT_RECOVERY)/root/lib/firmware/goodix_firmware_rothko.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko.bin:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/goodix_cfg_group_rothko.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko_nowater.bin:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/goodix_cfg_group_rothko_nowater.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_cfg_group_rothko_weijing.bin:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/goodix_cfg_group_rothko_weijing.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/goodix_firmware_rothko.bin:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/goodix_firmware_rothko.bin
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/rothko_gtp_thp_config.ini:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/rothko_gtp_thp_config.ini
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/rothko_fts_thp_config.ini:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/rothko_fts_thp_config.ini
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/film_model.tflite:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/film_model.tflite
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/glove_model.tflite:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/glove_model.tflite
PRODUCT_COPY_FILES += vendor/xiaomi/rothko/proprietary/odm/firmware/water_check.tflite:$(TARGET_COPY_OUT_RECOVERY)/root/vendor/odm/firmware/water_check.tflite

# Stock DisplayFeature backend: started by init as part of class hal.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/init/init.rothko.displayfeature.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.rothko.displayfeature.rc \
    $(LOCAL_PATH)/configs/ueventd/ueventd.displayfeature.rc:$(TARGET_COPY_OUT_ODM)/etc/ueventd.rc

# Original fingerprint HAL and its FOD vendor declarations.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/init/init.rothko.fingerprint.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.rothko.fingerprint.rc

# Original convenience-strength IFace HAL; service and feature XML come from stock.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/init/init.rothko.face.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.rothko.face.rc

# Restore the factory network location provider without MIUI framework dependencies.
$(call inherit-product, $(LOCAL_PATH)/network-location.mk)

# Domestic network validation and time services, also when no SIM is inserted.
PRODUCT_PACKAGES += RothkoNetworkStackOverlay

# TelephonyProvider prefers the ODM OEM table over the common product table.
# This is the unchanged OS3.0.307.0 product APN data, repackaged as ODM data.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/network/apns-conf.xml:$(TARGET_COPY_OUT_ODM)/telephony/apns-conf.xml

# Stock camera candidate; retain Aperture until full device acceptance.
$(call inherit-product, $(LOCAL_PATH)/camera/camera.mk)

# Device-local control for the byte-identical stock Dolby DAP stack.
PRODUCT_PACKAGES += RothkoDolby
