#
# Copyright (C) 2026 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

# The stock IMS APK uses methods implemented by the existing LineageOS
# compatibility layer and the source-built MediaTek telephony JARs.
$(call inherit-product, hardware/lineage/compat/frameworks/compat.mk)
$(call inherit-product, hardware/mediatek/frameworks/mediatek-frameworks.mk)

PRODUCT_PACKAGES += \
    ImsService \
    MssiFrameworkTelephonyOverlay \
    MssiTelephonyOverlay \
    mediatek-ims-base \
    mediatek-ims-common \
    vtservice \
    libcomutils \
    libhidltransport \
    libimsma \
    libimsma_adapt \
    libimsma_rtp \
    libimsma_socketwrapper \
    libmtk_vt_service \
    libmtk_vt_wrapper \
    libsignal \
    libsink \
    libsource \
    libvcodec_cap \
    libvcodec_capenc \
    libvt_avsync \
    vendor.mediatek.hardware.videotelephony-V1-ndk \
    vendor.mediatek.hardware.videotelephony@1.0

# Both IMS JARs are on the stock boot classpath. The extension plugin is not:
# the APK loads it dynamically and can fall back to its built-in plugin base.
PRODUCT_BOOT_JARS += \
    system_ext:mediatek-ims-common \
    system_ext:mediatek-ims-base

# Keep only the package-specific grants from the stock permission file.
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/permissions/privapp-permissions-com.mediatek.ims.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/permissions/privapp-permissions-com.mediatek.ims.xml \
    $(LOCAL_PATH)/configs/sysconfig/com.mediatek.ims.config.xml:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/sysconfig/com.mediatek.ims.config.xml \
    $(LOCAL_PATH)/configs/init/init.vtservice.rc:$(TARGET_COPY_OUT_SYSTEM_EXT)/etc/init/init.vtservice.rc

# The stock modem initializer consumes this exact APN/PLMN/CA resource tree.
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*,$(LOCAL_PATH)/prebuilts/modem/md,$(TARGET_COPY_OUT_VENDOR)/etc/md)
