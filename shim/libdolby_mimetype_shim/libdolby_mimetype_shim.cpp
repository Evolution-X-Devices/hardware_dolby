/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * The prebuilt libaudio_aidl_conversion_common_ndk references three
 * android::MEDIA_MIMETYPE_AUDIO_* symbols that the libstagefright_foundation-v33
 * available on the device does not export, so dlopen() of every Dolby
 * AIDL effect that links it fails with:
 *   cannot locate symbol "_ZN7android30MEDIA_MIMETYPE_AUDIO_DTS_HD_MAE"
 * Provide them here. The strings match the DTS entries in the audio HAL's
 * surround sound format list.
 */
namespace android {

extern const char *MEDIA_MIMETYPE_AUDIO_DTS_HD_MA;
extern const char *MEDIA_MIMETYPE_AUDIO_DTS_UHD_P1;
extern const char *MEDIA_MIMETYPE_AUDIO_DTS_UHD_P2;

const char *MEDIA_MIMETYPE_AUDIO_DTS_HD_MA = "audio/vnd.dts.hd;profile=dtsma";
const char *MEDIA_MIMETYPE_AUDIO_DTS_UHD_P1 = "audio/vnd.dts.uhd;profile=p1";
const char *MEDIA_MIMETYPE_AUDIO_DTS_UHD_P2 = "audio/vnd.dts.uhd;profile=p2";

}  // namespace android
