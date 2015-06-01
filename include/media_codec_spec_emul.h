/*
* Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef __TIZEN_MEDIA_CODEC_EMUL_H__
#define __TIZEN_MEDIA_CODEC_EMUL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <media_codec_private.h>

enum { DECODER, ENCODER };
enum { SOFTWARE, HARDWARE };

typedef struct _mc_codec_spec_t mc_codec_spec_t;
typedef struct _mc_codec_map_t mc_codec_map_t;
typedef struct _mc_codec_type_t mc_codec_type_t;

struct _mc_codec_spec_t
{
    mediacodec_codec_type_e codec_id;
    mediacodec_support_type_e codec_type;
    mediacodec_port_type_e port_type;
};

struct _mc_codec_type_t
{
    char *factory_name;
    char *mime;
    media_format_mimetype_e out_format;
};

struct _mc_codec_map_t
{
    mediacodec_codec_type_e id;
    bool hardware;
    mc_codec_type_t type;
};

static const mc_codec_spec_t spec_emul[] =
{
    {MEDIACODEC_H264,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H264,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_HW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H264,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_HW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H263,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_MPEG4,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_MPEG4,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_LC,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_LC,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    /* Extension for supporting codec @since_tizen 2.4 */
    {MEDIACODEC_AAC_HE,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},        /* added for 2.4 extension */
    {MEDIACODEC_AAC_HE_PS,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},        /* added for 2.4 extension */
    {MEDIACODEC_MP3,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},        /* added for 2.4 extension */
    {MEDIACODEC_AMR_NB,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},        /* added for 2.4 extension */
    {MEDIACODEC_AMR_WB,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},        /* added for 2.4 extension */
    {MEDIACODEC_AMR_NB,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST}        /* added for 2.4 extension */
};

static const mc_codec_map_t encoder_map[] =
{
#ifdef ENABLE_FFMPEG_CODEC
    //{MEDIACODEC_H264, HARDWARE, {"omxh264enc", "video/x-raw", MEDIA_FORMAT_H264_HP}},
    {MEDIACODEC_H264, HARDWARE, {"sprdenc_h264", "video/x-raw", MEDIA_FORMAT_H264_HP}},
    {MEDIACODEC_H263, SOFTWARE, {"avenc_h263p", "video/x-raw-yuv", MEDIA_FORMAT_H263P}},
    {MEDIACODEC_MPEG4, SOFTWARE, {"avenc_mpeg4", "video/x-raw-yuv", MEDIA_FORMAT_MPEG4_SP}},
    {MEDIACODEC_AAC_LC,  SOFTWARE, {"avenc_aac", "audio/x-raw", MEDIA_FORMAT_AAC_LC}},
    {MEDIACODEC_AMR_NB,  SOFTWARE, {"amrnbenc", "audio/x-raw", MEDIA_FORMAT_AMR_NB}}          /* for 2.4 temporary - use ffmpeg */
#else
    {MEDIACODEC_H264, HARDWARE, {"sprdenc_h264", "video/x-raw", MEDIA_FORMAT_H264_HP}},
    {MEDIACODEC_H263, SOFTWARE, {"maru_h263penc", "video/x-raw-yuv",  MEDIA_FORMAT_H263P}},
    {MEDIACODEC_MPEG4, SOFTWARE, {"maru_mpeg4", "video/x-raw-yuv", MEDIA_FORMAT_MPEG4_SP}},
    {MEDIACODEC_AAC_LC,  SOFTWARE, {"avenc_aac", "audio/x-raw", MEDIA_FORMAT_AAC_LC}},          /* for 2.4 temporary - use ffmpeg instead of maru_aacenc */
    {MEDIACODEC_AMR_NB,  SOFTWARE, {"amrnbenc", "audio/x-raw", MEDIA_FORMAT_AMR_NB}}          /* for 2.4 temporary - use ffmpeg instead of ?? */
#endif
};

static const mc_codec_map_t decoder_map[] =
{
#ifdef ENABLE_FFMPEG_CODEC
    {MEDIACODEC_H264, SOFTWARE, {"avdec_h264", "video/x-h264", MEDIA_FORMAT_I420}},
    {MEDIACODEC_H264, HARDWARE, {"sprddec_h264", "video/x-h264", MEDIA_FORMAT_NV12}},
    //{MEDIACODEC_H264, SOFTWARE, {"omxh264dec", "video/x-h264", MEDIA_FORMAT_I420}},
    {MEDIACODEC_MPEG4, SOFTWARE, {"avdec_mpeg4", "video/mpeg", MEDIA_FORMAT_I420}},
    {MEDIACODEC_AAC_LC,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AAC_HE,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},                   /* added for 2.4 extension */
    {MEDIACODEC_AAC_HE_PS,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},                   /* added for 2.4 extension */
    {MEDIACODEC_MP3,  SOFTWARE, {"avdec_mp3", "audio/mpeg", MEDIA_FORMAT_PCM}},                    /* added for 2.4 extension */
    {MEDIACODEC_AMR_NB,  SOFTWARE, {"avdec_amrnb", "audio/AMR", MEDIA_FORMAT_PCM}},                    /* added for 2.4 extension */
    {MEDIACODEC_AMR_WB,  SOFTWARE, {"avdec_amrwb", "audio/AMR-WB", MEDIA_FORMAT_PCM}}                    /* added for 2.4 extension */
#else
    {MEDIACODEC_H264, SOFTWARE, {"avdec_h264", "video/x-h264", MEDIA_FORMAT_I420}},
    {MEDIACODEC_H264, HARDWARE, {"omxh264dec", "video/x-h264", MEDIA_FORMAT_NV12}},
    {MEDIACODEC_MPEG4, SOFTWARE, {"maru_mpeg4", "video/mpeg", MEDIA_FORMAT_I420}},
    {MEDIACODEC_AAC_LC,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},          /* for 2.4 temporary - use ffmpeg instead of maru_aacdec */
    {MEDIACODEC_AAC_HE,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},          /* for 2.4 temporary - use ffmpeg instead of maru_aacdec */
    {MEDIACODEC_AAC_HE_PS,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},           /* for 2.4 temporary - use ffmpeg instead of maru_aacdec */
    {MEDIACODEC_MP3,  SOFTWARE, {"avdec_mp3", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AMR_NB,  SOFTWARE, {"avdec_amrnb", "audio/AMR", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AMR_WB,  SOFTWARE, {"avdec_amrwb", "audio/AMR-WB", MEDIA_FORMAT_PCM}}
#endif
};
#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_EMUL_H__ */
