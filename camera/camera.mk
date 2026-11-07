# SPDX-License-Identifier: Apache-2.0
# Selected for the next unified build; retain Aperture until device testing passes.
PRODUCT_PACKAGES += \
    MiuiCameraRothko \
    rothko-camera-privapp-permissions \
    rothko-camera-hiddenapi \
    rothko-camera-bgservice.xml

# Original system-side ISP clients and their compatibility dependency.
PRODUCT_PACKAGES += \
    libcamera_ispinterface_jni.xiaomi_rothko_camera_system \
    libcamera_algoup_jni.xiaomi_rothko_camera_system \
    libcamera_mianode_jni.xiaomi_rothko_camera_system \
    libmtkisp_metadata_sys_rothko_camera_system \
    vendor.mediatek.hardware.camera.isphal-V1-ndk_rothko_camera_system \
    vendor.mediatek.hardware.camera.isphal@1.0_rothko_camera_system

# Local firmware resources; cloud materials are not supplied here.
PRODUCT_PACKAGES += \
    rothko-camera-filter-102_SummerDay \
    rothko-camera-filter-103_Fantasy \
    rothko-camera-filter-104_Meet \
    rothko-camera-filter-105_WindSing \
    rothko-camera-filter-106_Lost \
    rothko-camera-filter-107_Central \
    rothko-camera-filter-108_NorthernEurope \
    rothko-camera-filter-109_Rome \
    rothko-camera-filter-111_GreenOrange \
    rothko-camera-filter-112_CyberPunk \
    rothko-camera-filter-113_BlackIce \
    rothko-camera-filter-121_Bleach \
    rothko-camera-filter-122_Thriller \
    rothko-camera-filter-123_TealOrange \
    rothko-camera-filter-124_JapaneseStyle \
    rothko-camera-filter-125_Latin \
    rothko-camera-filter-126_Fantasy2 \
    rothko-camera-filter-127_Creamy \
    rothko-camera-filter-128_Sepia \
    rothko-camera-filter-129_LeicaVivid \
    rothko-camera-filter-130_LeicaNatural \
    rothko-camera-filter-131_MonochromeTrue \
    rothko-camera-filter-132_MonochromeHighContrast \
    rothko-camera-filter-133_MonochromeBrown \
    rothko-camera-filter-134_MonochromeBlue \
    rothko-camera-filter-135_Classical \
    rothko-camera-filter-136_Filene \
    rothko-camera-filter-137_GreenNight \
    rothko-camera-filter-138_LiteratureAndArt \
    rothko-camera-filter-139_OrangeHoney \
    rothko-camera-filter-140_Romance \
    rothko-camera-filter-141_FR500 \
    rothko-camera-filter-142_FE250 \
    rothko-camera-filter-143_RedLantern \
    rothko-camera-filter-144_RedHero \
    rothko-camera-filter-145_TactileBlackWhite \
    rothko-camera-filter-146_TactileGreen \
    rothko-camera-filter-147_Neutral \
    rothko-camera-filter-167_CC \
    rothko-camera-filter-168_NC

# Stock camdfx configuration is tracked in proprietary-files.txt and installed
# by rothko-vendor.mk, keeping extraction/regeneration reproducible.
