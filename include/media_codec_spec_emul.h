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
    {MEDIACODEC_H264,   MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H263,   MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H263,   MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_MPEG4,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_MPEG4,  MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_LC, MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_LC, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_HE, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC_HE_PS,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_MP3,    MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AMR_NB, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AMR_WB, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AMR_NB, MEDIACODEC_ENCODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_VORBIS, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_FLAC,   MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_WMAV1,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_WMAV2,  MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_WMAPRO, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_WMALSL, MEDIACODEC_DECODER | MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
};

static const mc_codec_map_t encoder_map[] =
{
    {MEDIACODEC_H263,   SOFTWARE, {"avenc_h263p", "video/x-raw-yuv", MEDIA_FORMAT_H263P}},
    {MEDIACODEC_MPEG4,  SOFTWARE, {"avenc_mpeg4", "video/x-raw-yuv", MEDIA_FORMAT_MPEG4_SP}},
    {MEDIACODEC_AAC_LC, SOFTWARE, {"avenc_aac", "audio/x-raw", MEDIA_FORMAT_AAC_LC}},
    {MEDIACODEC_AMR_NB, SOFTWARE, {"amrnbenc", "audio/x-raw", MEDIA_FORMAT_AMR_NB}}
};

static const mc_codec_map_t decoder_map[] =
{
    {MEDIACODEC_H264,   SOFTWARE, {"avdec_h264", "video/x-h264", MEDIA_FORMAT_I420}},
    {MEDIACODEC_MPEG4,  SOFTWARE, {"avdec_mpeg4", "video/mpeg", MEDIA_FORMAT_I420}},
    {MEDIACODEC_H263,   SOFTWARE, {"avdec_h263", "video/x-h263", MEDIA_FORMAT_I420}},
    {MEDIACODEC_AAC_LC, SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AAC_HE, SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AAC_HE_PS,  SOFTWARE, {"avdec_aac", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_MP3,    SOFTWARE, {"avdec_mp3", "audio/mpeg", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AMR_NB, SOFTWARE, {"avdec_amrnb", "audio/AMR", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_AMR_WB, SOFTWARE, {"avdec_amrwb", "audio/AMR-WB", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_VORBIS, SOFTWARE, {"vorbisdec", "audio/x-vorbis", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_FLAC,   SOFTWARE, {"avdec_flac", "audio/x-flac", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_WMAV1,  SOFTWARE, {"avdec_wmav1", "audio/x-wma", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_WMAV2,  SOFTWARE, {"avdec_wmav2", "audio/x-wma", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_WMAPRO, SOFTWARE, {"avdec_wmapro", "audio/x-wma", MEDIA_FORMAT_PCM}},
    {MEDIACODEC_WMALSL, SOFTWARE, {"avdec_wmalossless", "audio/x-wma", MEDIA_FORMAT_PCM}}
};
#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_EMUL_H__ */
