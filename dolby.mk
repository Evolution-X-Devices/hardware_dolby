#
# Copyright (C) 2022 FlamingoOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Dolby path
DOLBY_PATH := hardware/dolby

# Soong Namespace
PRODUCT_SOONG_NAMESPACES += \
   $(DOLBY_PATH)

# Enable codec support
AUDIO_FEATURE_ENABLED_DS2_DOLBY_DAP := true

# SEPolicy
BOARD_VENDOR_SEPOLICY_DIRS += $(DOLBY_PATH)/sepolicy/vendor \
    $(DOLBY_PATH)/sepolicy/vendor/vision

# Framework compatibility matrix for the AIDL DMS HAL
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE += \
    $(DOLBY_PATH)/configs/compatibility_matrix_dolby.xml \
    $(DOLBY_PATH)/configs/compatibility_matrix_dolby_c2.xml \
    $(DOLBY_PATH)/configs/compatibility_matrix_dolby_vision.xml

# Configs
PRODUCT_COPY_FILES += \
    $(DOLBY_PATH)/configs/dax-default.xml:$(TARGET_COPY_OUT_VENDOR)/etc/dolby/dax-default.xml \
    $(DOLBY_PATH)/configs/dax-default-spatializer.xml:$(TARGET_COPY_OUT_VENDOR)/etc/dolby/dax-default-spatializer.xml \
    $(DOLBY_PATH)/configs/media_codecs_dolby_audio.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs_dolby_audio.xml

# Dolby VNDK libs
PRODUCT_PACKAGES += \
    libstagefright_foundation-v33

PRODUCT_PACKAGES += \
    libshim_dolby

# Init
PRODUCT_PACKAGES += \
    init.dolby.rc

# Overlays    
PRODUCT_PACKAGES += \
    DolbyFrameworksResCommon

# Media (C2)
PRODUCT_PACKAGES += \
    android.hardware.media.c2@1.0.vendor \
    android.hardware.media.c2@1.1.vendor \
    android.hardware.media.c2@1.2.vendor \
    libcodec2_hidl@1.0.vendor \
    libcodec2_hidl@1.1.vendor \
    libcodec2_hidl@1.2.vendor \
    libsfplugin_ccodec_utils.vendor \
    libcodec2_soft_common.vendor

# Codec2 Props
PRODUCT_VENDOR_PROPERTIES += \
    vendor.audio.c2.preferred=true \
    debug.c2.use_dmabufheaps=1 \
    vendor.qc2audio.suspend.enabled=true \
    vendor.qc2audio.per_frame.flac.dec.enabled=true

# Dolby Props
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.dolby.dax.version=DAX3_3.7.0.8_r1 \
    vendor.audio.dolby.ds2.hardbypass=false \
    vendor.audio.dolby.ds2.enabled=false

# Remove Packages for Dolby Support
PRODUCT_PACKAGES += \
    RemovePackagesDolby


# XiaomiDolby
PRODUCT_PACKAGES += \
    XiaomiDolby

# Dolby Proprietary blobs
PRODUCT_COPY_FILES += \
    $(DOLBY_PATH)/proprietary/vendor/etc/init/dms-service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/dms-service.rc \
    $(DOLBY_PATH)/proprietary/vendor/etc/vintf/manifest/dms-service.xml:$(TARGET_COPY_OUT_VENDOR)/etc/vintf/manifest/dms-service.xml

PRODUCT_PACKAGES += \
    libdapparamstorage \
    libdlbdsservice \
    libdlbpreg \
    libdmshal \
    vendor.dolby.dms-V1-ndk \
    vendor.dolby.hardware.dms@2.0 \
    vendor.dolby.dms.service

# Codec2 (Dolby)
PRODUCT_COPY_FILES += \
    $(DOLBY_PATH)/proprietary/vendor/etc/init/vendor.dolby.media.c2-default-service-dax.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/vendor.dolby.media.c2-default-service-dax.rc \
    $(DOLBY_PATH)/proprietary/vendor/etc/vintf/manifest/vendor.dolby.media.c2-default-service-dax.xml:$(TARGET_COPY_OUT_VENDOR)/etc/vintf/manifest/vendor.dolby.media.c2-default-service-dax.xml

PRODUCT_PACKAGES += \
    libcodec2_soft_ac4dec \
    libcodec2_soft_ddpdec \
    libcodec2_store_dolby \
    libdeccfg \
    vendor.dolby.media.c2-default-service-dax

# Dolby SoundFX Blobs
PRODUCT_PACKAGES += \
    libaudio_aidl_conversion_common_ndk_prebuilt \
    libdlbvolaidl \
    libhwdapaidl \
    libspatializerparamstorage \
    libswgamedapaidl \
    libswspatializeraidl \
    libswspatializeraidl_ext

# Dolby vision Proprietary blobs
PRODUCT_COPY_FILES += \
    $(DOLBY_PATH)/proprietary/vendor/etc/dolby_vision.cfg:$(TARGET_COPY_OUT_VENDOR)/etc/dolby_vision.cfg \
    $(DOLBY_PATH)/proprietary/vendor/etc/init/vendor.dolbyvision.media.c2@1.0-service.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/vendor.dolbyvision.media.c2@1.0-service.rc \
    $(DOLBY_PATH)/proprietary/vendor/etc/init/vendor.dolby.media.dvs-service-vision.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/vendor.dolby.media.dvs-service-vision.rc

PRODUCT_PACKAGES += \
    c2.dolby.avc.dec \
    c2.dolby.avc.sec.dec \
    c2.dolby.client \
    c2.dolby.egl \
    c2.dolby.hevc.dec \
    c2.dolby.hevc.enc \
    c2.dolby.hevc.sec.dec \
    c2.dolby.store \
    dolbycodec2 \
    dvs-hal-service \
    libcodec2_aidl-dolby \
    libcodec2_hal_common-aidl \
    libcodec2_hidl_plugin-dolby \
    libcodec2_vndk-aidl \
    libdolbyottcameracontrol \
    libdolbyvision \
    libstagefright_foundation-atmos \
    libstagefright_foundation-swiitchoff \
    vendor.dolby.dvs@1.0 \
    vendor.dolby.media.dvs-service.xml \
    vendor.dolbyvision.media.c2.xml

# Dolby Vision Codec2 store over AIDL (default3)
PRODUCT_PACKAGES += \
    android.hardware.media.c2-dolby-service

# HIDL facade for the Qualcomm Codec2 store, for HIDL Dolby Vision decoders
PRODUCT_PACKAGES += \
    android.hardware.media.c2-qc2hidl-service

# Shim
PRODUCT_PACKAGES += \
    libcodec2_hidl_shim.vendor

