#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2025-2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)

from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

blob_fixups: blob_fixups_user_type = {
    # Only default9 is ours, default comes from the common vendor manifest
    'vendor/etc/vintf/manifest/vendor.dolby.media.c2-default-service-dax.xml': blob_fixup()
        .regex_replace(r'[ \t]*<fqname>IComponentStore/default</fqname>\n', ''),
    # The ComponentStore is allocated 0x108 bytes, but the newer HIDL
    # libraries need 0x140, which corrupts the heap and crashes the service
    'vendor/bin/hw/dolbycodec2': blob_fixup()
        .sig_replace('00 21 80 52', '00 28 80 52'),
    'vendor/lib64/soundfx/libdlbvolaidl.so': blob_fixup()
        .replace_needed(
            'android.media.audio.common.types-V4-ndk.so',
            'android.media.audio.common.types-V3-ndk.so',
        ),
    (
        'vendor/lib64/soundfx/libswdapaidl.so',
        'vendor/lib64/soundfx/libswgamedapaidl.so',
        'vendor/lib64/soundfx/libswspatializeraidl.so',
    ): blob_fixup()
        .replace_needed(
            'libaudio_aidl_conversion_common_ndk.so',
            'libaudio_aidl_conversion_common_ndk_prebuilt.so',
        ),
    # Also needs the DTS mimetype symbols that libstagefright_foundation-v33
    # lacks, see shim/libdolby_mimetype_shim
    'vendor/lib64/libaudio_aidl_conversion_common_ndk_prebuilt.so': blob_fixup()
        .replace_needed(
            'libstagefright_foundation.so',
            'libstagefright_foundation-v33.so',
        )
        .add_needed('libdolby_mimetype_shim.so'),
    (
        'vendor/lib64/libcodec2_soft_ac4dec.so',
        'vendor/lib64/libcodec2_soft_ddpdec.so',
        'vendor/lib64/libdlbdsservice.so',
        'vendor/lib64/libdlbpreg.so',
        'vendor/lib64/soundfx/libdlbvolaidl.so',
        'vendor/lib64/soundfx/libswdapaidl.so',
        'vendor/lib64/soundfx/libswgamedapaidl.so',
        'vendor/lib64/soundfx/libswspatializeraidl.so',
    ): blob_fixup()
        .replace_needed(
            'libstagefright_foundation.so',
            'libstagefright_foundation-v33.so',
        ),
} # fmt: skip

module = ExtractUtilsModule(
    'dolby',
    'aidl',
    blob_fixups=blob_fixups,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
