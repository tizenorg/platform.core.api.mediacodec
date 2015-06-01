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
 *
 */
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlog.h>
#include <media_codec_queue.h>
#include <media_codec_port_gst.h>
#include <media_codec_util.h>

#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/app/gstappsrc.h>

#ifdef Z130H
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ion.h>
#endif

#define GST_MC_EVENT_CODEC_DATA "GstEventCodecData"
/*
* Internal Implementation
*/
static gpointer feed_task(gpointer data);
static void __mc_gst_stop_feed(GstElement *pipeline, gpointer data);
static void __mc_gst_start_feed(GstElement *pipeline, guint size, gpointer data);
static media_packet_h _mc_get_input_buffer(mc_gst_core_t *core);

static gboolean __mc_gst_init_gstreamer();
static int _mc_output_media_packet_new(mc_gst_core_t *core, bool video, bool encoder, media_format_mimetype_e out_mime);
static mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core, gchar *factory_name);
static mc_ret_e _mc_gst_destroy_pipeline(mc_gst_core_t *core);
static void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data);
static int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data);
static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps);
static GstMCBuffer* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config);
static int _mc_gst_gstbuffer_to_appsrc(mc_gst_core_t *core, GstMCBuffer *buff);
static gchar* __mc_get_gst_input_format(media_packet_h packet, bool is_hw);
static media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buffer);
static media_packet_h __mc_gst_make_media_packet(mc_gst_core_t *core, unsigned char *data, int size);
static gboolean __mc_gst_bus_callback(GstBus *bus, GstMessage *msg, gpointer data);
static GstBusSyncReply __mc_gst_bus_sync_callback(GstBus * bus, GstMessage *msg, gpointer data);
static MMVideoBuffer* __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt);
static GstPadProbeReturn event_probe_cb(GstObject *pad, GstPadProbeInfo *info, gpointer user_data);
static GType __mc_gst_buffer_get_type(void);
static void __mc_gst_buffer_class_init(gpointer g_class, gpointer class_data);
static GstMCBuffer *gst_mediacodec_buffer_new(mc_gst_core_t* core, media_packet_h pkt, uint64_t size);
static void gst_mediacodec_buffer_finalize(GstMCBuffer *buffer);

static gint __gst_handle_stream_error(mc_gst_core_t* core, GError* error, GstMessage * message);
static gint __gst_transform_gsterror( mc_gst_core_t *core, GstMessage * message, GError* error );
static gint __gst_handle_resource_error(mc_gst_core_t* core, int code );
static gint __gst_handle_library_error(mc_gst_core_t* core, int code);
static gint __gst_handle_core_error(mc_gst_core_t* core, int code );

static int _mc_link_vtable(mc_gst_core_t *core, mediacodec_codec_type_e id, gboolean is_encoder, gboolean is_hw);
#ifdef Z130H
static int __tbm_get_physical_addr_bo(tbm_bo_handle tbm_bo_handle_fd_t, int* phy_addr, int* phy_size);
#endif
static void _mc_gst_flush_buffers( mc_gst_core_t *core );
static void _mc_gst_set_flush_input(mc_gst_core_t *core);
static void _mc_gst_set_flush_output(mc_gst_core_t *core);

static int __tile_4x2_read(int x_size, int y_size, int x_pos, int y_pos);
static void __csc_tiled_to_linear_crop(unsigned char *yuv420_dest,
    unsigned char *nv12t_src, int yuv420_width, int yuv420_height,
    int left, int top, int right, int buttom);

/*for test */
static int count;
static int input_count;
#define GST_TYPE_MC_BUFFER (__mc_gst_buffer_get_type())

/* SPRD Decoder */
int(*vdec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet,  &__mc_vdec_caps};
#ifdef Z130H
/* SPRD H.264 Decoder */
int(*vdec_h264_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_packet_with_mm_video_buffer,  &__mc_sprddec_caps};
/* SPRD H.264 Encoder */
int(*venc_vtable[])() = {&__mc_fill_inbuf_with_bo, &__mc_fill_outbuf_with_packet, &__mc_sprdenc_caps};
#else
/* M0 H.264 Decoder */
int(*vdec_h264_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_packet_with_mm_video_buffer,  &__mc_h264dec_caps};
/* M0 H.264 Encoder */
int(*venc_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_h264_packet, &__mc_venc_caps};
#endif
/* AAC_LC Encoder */
int(*aenc_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_aenc_caps};
/* AAC_LC Deocder */
int(*adec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_caps};
int(*adec_aacv12_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_aacv12_caps};
/* MP3 Deocder */
int(*adec_mp3_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_mp3_caps};
/* AMR-NB Deocder */
int(*adec_amrnb_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_amrnb_caps};
/* AMR-WB Deocder */
int(*adec_amrwb_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_amrwb_caps};
/* AMR-NB Deocder */
int(*aenc_amrnb_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_aenc_amrnb_caps};


/*
 * mc_gst_object functions
*/
int __mc_fill_input_buffer(mc_gst_core_t *core, void *buf_data, int buf_size, GstMCBuffer *buff)
{
    return core->vtable[fill_inbuf](core, buf_data, buf_size, buff);
}

int __mc_fill_output_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt)
{
    return core->vtable[fill_outbuf](core, data, size, out_pkt);
}

int __mc_create_caps(mc_gst_core_t *core, GstCaps **caps)
{
    return core->vtable[create_caps](core, caps);
}

int __mc_fill_inbuf_with_bo(mc_gst_core_t *core, void *buf_data, int buf_size, GstMCBuffer *mc_buffer)
{
    int ret = MC_ERROR_NONE;

    MMVideoBuffer *imgb = NULL;
    guint n;

    mc_buffer->has_imgb = true;


    imgb = __mc_gst_make_tbm_buffer(core, mc_buffer->pkt);

    if( imgb != NULL ) {
        gst_buffer_prepend_memory(mc_buffer->buffer,
                gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY, imgb, sizeof(*imgb), 0,
                    sizeof(*imgb), mc_buffer, (GDestroyNotify)gst_mediacodec_buffer_finalize));
        LOGD("scmn_imgb is appended, %d, %d", sizeof(*imgb), gst_buffer_n_memory(mc_buffer->buffer));
    }

    if( buf_data != NULL) {
        gst_buffer_prepend_memory(mc_buffer->buffer,
                gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY, buf_data, buf_size, 0,
                    buf_size, mc_buffer, (GDestroyNotify)gst_mediacodec_buffer_finalize));
        LOGD("packet data is apended, %d, %d", buf_size, gst_buffer_n_memory(mc_buffer->buffer));
    }
    return ret;
}

int __mc_fill_inbuf_with_packet(mc_gst_core_t *core, void *buf_data, int buf_size, GstMCBuffer *mc_buffer)
{
    int ret = MC_ERROR_NONE;

    mc_buffer->has_imgb = false;

    if( buf_data != NULL) {
        gst_buffer_append_memory(mc_buffer->buffer,
                gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY, buf_data, buf_size, 0,
                    buf_size, mc_buffer, (GDestroyNotify)gst_mediacodec_buffer_finalize));
        LOGD("packet data is apended");
    }

    return ret;
}

int __mc_fill_outbuf_with_bo(mc_gst_core_t *core, void *data, int size, media_packet_h* out_pkt)
{
    int i = 0;;
    int bo_num = 0;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    MMVideoBuffer *imgb = NULL;
    tbm_surface_h tsurf = NULL;
    mc_decoder_info_t *codec_info = (mc_decoder_info_t *)core->codec_info;
#ifdef TBM_API_CHANGE
    tbm_surface_info_s tsurf_info;
    memset(&tsurf_info, 0x0, sizeof(tbm_surface_info_s));
#endif

    /* create tbm surface */
    for (i = 0; i < MM_VIDEO_BUFFER_PLANE_MAX; i++)
    {
        if (imgb->handle.bo[i])
        {
            bo_num++;
#ifdef TBM_API_CHANGE
            tsurf_info.planes[i].stride = imgb->stride_width[i];
#endif
        }
    }

    if (bo_num > 0)
    {
#ifdef TBM_API_CHANGE
        tsurf_info.width = codec_info->width;
        tsurf_info.height = codec_info->height;
        tsurf_info.format = TBM_FORMAT_NV12;          // bo_format
        tsurf_info.bpp = tbm_surface_internal_get_bpp(TBM_FORMAT_NV12);
        tsurf_info.num_planes = tbm_surface_internal_get_num_planes(TBM_FORMAT_NV12);
        tsurf_info.size = 0;
        for(i = 0; i < tsurf_info.num_planes; i++) {
            tsurf_info.planes[i].stride = imgb->stride_width[i];
            tsurf_info.planes[i].size = imgb->stride_width[i] * imgb->stride_height[i];
            if(i < bo_num)
                tsurf_info.planes[i].offset = 0;
            else
                tsurf_info.planes[i].offset = tsurf_info.planes[i - 1].size;
            tsurf_info.size += tsurf_info.planes[i].size;
        }

        tsurf = tbm_surface_internal_create_with_bos(&tsurf_info, (tbm_bo *)imgb->handle.bo, bo_num);
        LOGD("[NEW API] tbm surface %p", tsurf);
#else
        tsurf = tbm_surface_internal_create_with_bos(codec_info->width, codec_info->height, TBM_FORMAT_NV12, (tbm_bo *)imgb->handle.bo, bo_num);
        LOGD("[OLD  API] tbm surface %p", tsurf);
#endif
    }

    if (tsurf)
    {
        media_packet_create_from_tbm_surface(core->output_fmt, tsurf, (media_packet_finalize_cb)__mc_output_buffer_finalize_cb, core, out_pkt);
        LOGD("!!!! create_gst_buffer = %p", *out_pkt);
    }

    return MC_ERROR_NONE;
}


int __mc_fill_packet_with_mm_video_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt)
{
    void* pkt_data = NULL;
    MMVideoBuffer *imgb = NULL;
    GstMapInfo map = GST_MAP_INFO_INIT;
    int i;
    int bo_num = 0;
    mc_decoder_info_t *codec_info = (mc_decoder_info_t *)core->codec_info;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    imgb = (MMVideoBuffer *)data;

    LOGD("buf_share_method %d", imgb->type);

    LOGD("a[0] : %p, a[1] : %p, p[0] : %p, p[1] : %p",
        imgb->data[0], imgb->data[1], imgb->handle.paddr[0], imgb->handle.paddr[1]);
    LOGD("s[0]:%d, e[0]:%d, w[0]:%d, h[0]:%d",
        imgb->stride_width[0],imgb->stride_height[0],imgb->width[0], imgb->height[0]);

    media_packet_create_alloc(core->output_fmt, __mc_output_buffer_finalize_cb, core, out_pkt);

    if ( imgb->type == MM_VIDEO_BUFFER_TYPE_PHYSICAL_ADDRESS ) {
        media_packet_set_buffer_size(*out_pkt, imgb->width[0]*imgb->height[0]*3/2);
        media_packet_get_buffer_data_ptr(*out_pkt, &pkt_data);

        __csc_tiled_to_linear_crop(pkt_data, imgb->data[0],
                imgb->stride_width[0], imgb->stride_height[0], 0,0,0,0);
        __csc_tiled_to_linear_crop(pkt_data+imgb->stride_width[0]*imgb->stride_height[0],
                imgb->data[1], imgb->stride_width[0], imgb->stride_height[0]/2, 0,0,0,0);
    }
    else if ( imgb->type == MM_VIDEO_BUFFER_TYPE_DMABUF_FD ) {
        LOGD("FD type");
    }
    else if ( imgb->type == MM_VIDEO_BUFFER_TYPE_TBM_BO ) {
        tbm_surface_h tsurf = NULL;

#ifdef TBM_API_CHANGE
        tbm_surface_info_s tsurf_info;
        memset(&tsurf_info, 0x0, sizeof(tbm_surface_info_s));
#endif

        /* create tbm surface */
        for (i = 0; i < MM_VIDEO_BUFFER_PLANE_MAX; i++) {
            if (imgb->handle.bo[i]) {
                bo_num++;
#ifdef TBM_API_CHANGE
                tsurf_info.planes[i].stride = imgb->stride_width[i];
#endif
            }
        }

        if ( bo_num > 0 ) {
#ifdef TBM_API_CHANGE
            tsurf_info.width = codec_info->width;
            tsurf_info.height = codec_info->height;
            tsurf_info.format = TBM_FORMAT_NV12;          // bo_format
            tsurf_info.bpp = tbm_surface_internal_get_bpp(TBM_FORMAT_NV12);
            tsurf_info.num_planes = tbm_surface_internal_get_num_planes(TBM_FORMAT_NV12);
            tsurf_info.size = 0;

            for ( i = 0; i < tsurf_info.num_planes; i++ ) {
                tsurf_info.planes[i].stride = imgb->stride_width[i];
                tsurf_info.planes[i].size = imgb->stride_width[i] * imgb->stride_height[i];

                if ( i < bo_num )
                    tsurf_info.planes[i].offset = 0;
                else
                    tsurf_info.planes[i].offset = tsurf_info.planes[i - 1].size;

                tsurf_info.size += tsurf_info.planes[i].size;
            }

            tsurf = tbm_surface_internal_create_with_bos ( &tsurf_info, (tbm_bo *)imgb->handle.bo, bo_num );
            LOGD("[NEW API] tbm surface %p", tsurf);
#else
            tsurf = tbm_surface_internal_create_with_bos ( codec_info->width, codec_info->height,
                        TBM_FORMAT_NV12, (tbm_bo *)imgb->handle.bo, bo_num );
            LOGD ( "[OLD  API] tbm surface %p", tsurf );
#endif
        }

        if ( tsurf ) {
            media_packet_create_from_tbm_surface ( core->output_fmt, tsurf,
                ( media_packet_finalize_cb )__mc_output_buffer_finalize_cb, core, out_pkt );
        }
    }

DONE:

    return MC_ERROR_NONE;
}

int __mc_fill_outbuf_with_packet(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt)
{
    void* pkt_data = NULL;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);


    media_packet_create_alloc(core->output_fmt, __mc_output_buffer_finalize_cb, core, out_pkt);

    media_packet_set_buffer_size(*out_pkt, size);
    media_packet_get_buffer_data_ptr(*out_pkt, &pkt_data);
    memcpy(pkt_data, data, size);

    return MC_ERROR_NONE;
}

int __mc_fill_outbuf_with_h264_packet(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt)
{
    void* pkt_data = NULL;
    int mask = 0xfffffff0;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    if ( (core->out_mime & mask) == MEDIA_FORMAT_H264_SP)
    {
        _mc_check_out_bytestream(core, (unsigned char*)data, size);
    }

    if ( core->sps_size > 0 && core->pps_size > 0 ) {

        media_packet_create_alloc(core->output_fmt, __mc_output_buffer_finalize_cb, core, out_pkt);
        media_packet_set_buffer_size(*out_pkt, size);
        media_packet_get_buffer_data_ptr(*out_pkt, &pkt_data);
        memcpy(pkt_data, data, size);

        if ( core->need_codec_data ) {
            media_packet_set_codec_data(*out_pkt, core->codec_data, core->codec_data_size);
        }

    }

    return MC_ERROR_NONE;
}

int __mc_venc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple("video/x-raw",
                        "format", G_TYPE_STRING, "I420",
                        "width", G_TYPE_INT, enc_info->width,
                        "height", G_TYPE_INT, enc_info->height,
                        "framerate", GST_TYPE_FRACTION, enc_info->fps, 1,
                        NULL);

    LOGD("%d, %d, %d", enc_info->width, enc_info->height, enc_info->fps);

    return MC_ERROR_NONE;
}

int __mc_sprdenc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "format", G_TYPE_STRING, "SN12",
            "width", G_TYPE_INT, enc_info->width,
            "height", G_TYPE_INT, enc_info->height,
            "framerate", GST_TYPE_FRACTION, enc_info->fps, 1, NULL);

    g_object_set (GST_OBJECT(core->codec), "byte-stream", TRUE, NULL);
    g_object_set (GST_OBJECT(core->codec), "bitrate", enc_info->bitrate*1000, NULL);

    LOGD("%s, %d, %d, %d", core->format, enc_info->width, enc_info->height, enc_info->fps);


    return MC_ERROR_NONE;
}

int __mc_h264dec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    LOGD("%d, %d, ", dec_info->width, dec_info->height);
    *caps = gst_caps_new_simple(core->mime,
                                "parsed", G_TYPE_BOOLEAN, TRUE,
                                "alignment", G_TYPE_STRING, "au",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, dec_info->width,
                                "height", G_TYPE_INT, dec_info->height, NULL);

    LOGD("mime : %s, widht :%d, height : %d", core->mime, dec_info->width, dec_info->height);
    return MC_ERROR_NONE;
}

int __mc_sprddec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);


    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    LOGD("%d, %d, ", dec_info->width, dec_info->height);
    *caps = gst_caps_new_simple(core->mime,
            "width", G_TYPE_INT, dec_info->width,
            "height", G_TYPE_INT, dec_info->height,
            "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

    LOGD("mime : %s, widht :%d, height : %d", core->mime, dec_info->width, dec_info->height);

    return MC_ERROR_NONE;
}

int __mc_vdec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
                                "alignment", G_TYPE_STRING, "au",
                                "stream-format", G_TYPE_STRING, "byte-stream", NULL);

    LOGD("mime : %s, widht :%d, height : %d", core->mime, dec_info->width, dec_info->height);
    return MC_ERROR_NONE;
}

int __mc_aenc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "rate", G_TYPE_INT, enc_info->samplerate,
            "channels", G_TYPE_INT, enc_info->channel,
            "format", G_TYPE_STRING, "F32LE",
            "layout", G_TYPE_STRING, "interleaved",
            NULL);

/*
+----GstAudioEncoder
     +----avenc_aac

Element Properties:
  compliance          : Adherence of the encoder to the specifications
                                flags: readable, writable
                                Enum "GstFFMpegCompliance" Default: 0, "normal"
                                (2): verystrict         - Strictly conform to older spec
                                (1): strict                - Strictly conform to current spec
                                (0): normal             - Normal behavior
                                (-1): unofficial       - Allow unofficial extensions
                                (-2): experimental  - Allow nonstandardized experimental things
*/
    g_object_set (GST_OBJECT(core->codec), "compliance", -2, NULL);

    LOGD("mime : %s, samplerate :%d, channel : %d", core->mime, enc_info->samplerate, enc_info->channel);

    return MC_ERROR_NONE;
}

int __mc_aenc_amrnb_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "rate", G_TYPE_INT, enc_info->samplerate,
            "channels", G_TYPE_INT, enc_info->channel,
            "format", G_TYPE_STRING, "S16LE",
            "layout", G_TYPE_STRING, "interleaved",
            NULL);

    LOGD("mime : %s,  samplerate :%d, channel : %d", core->mime, enc_info->samplerate, enc_info->channel);
    return MC_ERROR_NONE;
}

int __mc_adec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    LOGD("CAPS for codec_id (MEDIACODEC_AAC_LC - normal ADTS)");
    *caps = gst_caps_new_simple(core->mime,
            "framed", G_TYPE_BOOLEAN, TRUE,
            "mpegversion", G_TYPE_INT, 4,
            "stream-format", G_TYPE_STRING, "adts",
            "rate", G_TYPE_INT, dec_info->samplerate,
            "channels", G_TYPE_INT, dec_info->channel,
            NULL);

    LOGD("mime : %s, samplerate :%d, channel : %d", core->mime, dec_info->samplerate, dec_info->channel);
    return MC_ERROR_NONE;
}

int __mc_adec_aacv12_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    LOGD("CAPS for codec_id (MEDIACODEC_AAC_HE and _PS - MP4/M4A case)");
    *caps = gst_caps_new_simple(core->mime,
            "mpegversion", G_TYPE_INT, 4,                       //ToDo : need adding version /profile
            "framed", G_TYPE_BOOLEAN, TRUE,
            "stream-format", G_TYPE_STRING, "raw",
            "rate", G_TYPE_INT, dec_info->samplerate,
            "channels", G_TYPE_INT, dec_info->channel,
            NULL);

    LOGD("mime : %s, samplerate :%d, channel : %d", core->mime, dec_info->samplerate, dec_info->channel);

    return MC_ERROR_NONE;
}

int __mc_adec_mp3_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "framed", G_TYPE_BOOLEAN, TRUE,
            "mpegversion", G_TYPE_INT, 1,       /* To-Do : plz check */
            "mpegaudioversion", G_TYPE_INT, 1,  /* To-Do : plz check */
            "layer", G_TYPE_INT, 3,             /* To-Do : plz check */
            "rate", G_TYPE_INT, dec_info->samplerate,
            "channels", G_TYPE_INT, dec_info->channel,
            NULL);

    LOGD("mime : %s, samplerate :%d, channel : %d", core->mime, dec_info->samplerate, dec_info->channel);

    return MC_ERROR_NONE;
}

int __mc_adec_amrnb_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "rate", G_TYPE_INT, 8000,
            "channels", G_TYPE_INT, dec_info->channel,    /* FIXME - by 1 */
            NULL);

    LOGD("mime : %s,  samplerate :%d, channel : %d", core->mime, dec_info->samplerate, dec_info->channel);

    return MC_ERROR_NONE;
}

int __mc_adec_amrwb_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
            "rate", G_TYPE_INT, 16000,
            "channels", G_TYPE_INT, dec_info->channel,    /* FIXME - by 1 */
            NULL);

    LOGD("mime : %s, samplerate :%d, channel : %d", core->mime, dec_info->samplerate, dec_info->channel);

    return MC_ERROR_NONE;
}

int _mc_output_media_packet_new(mc_gst_core_t *core, bool video, bool encoder, media_format_mimetype_e out_mime)
{
    if(media_format_create(&core->output_fmt) != MEDIA_FORMAT_ERROR_NONE)
    {
        LOGE("media format create failed");
        return MC_ERROR;
    }

    if(encoder)
    {
        mc_encoder_info_t *info;

        info = (mc_encoder_info_t*)core->codec_info;

        if (video)
        {
            media_format_set_video_mime(core->output_fmt, out_mime);
            media_format_set_video_width(core->output_fmt, info->width);
            media_format_set_video_height(core->output_fmt, info->height);
            media_format_set_video_avg_bps(core->output_fmt, info->bitrate);
        }
        else
        {
            media_format_set_audio_mime(core->output_fmt, out_mime);
            media_format_set_audio_channel(core->output_fmt, info->channel);
            media_format_set_audio_samplerate(core->output_fmt, info->samplerate);
            media_format_set_audio_bit(core->output_fmt, info->bit);
            media_format_set_audio_avg_bps(core->output_fmt, info->bitrate);
        }
    }
    else
    {
        mc_decoder_info_t *info;

        info = (mc_decoder_info_t*)core->codec_info;

        if (video)
        {
            media_format_set_video_mime(core->output_fmt, out_mime);
            media_format_set_video_width(core->output_fmt, info->width);
            media_format_set_video_height(core->output_fmt, info->height);
        }
        else
        {
            media_format_set_audio_mime(core->output_fmt, out_mime);
            media_format_set_audio_channel(core->output_fmt, info->channel);
            media_format_set_audio_samplerate(core->output_fmt, info->samplerate);
            media_format_set_audio_bit(core->output_fmt, info->bit);
        }
    }
    return MC_ERROR_NONE;
}

/*
 * mc_gst_core functions
*/
mc_gst_core_t *mc_gst_core_new()
{
    mc_gst_core_t *core;

    MEDIACODEC_FENTER();

    core = g_new0(mc_gst_core_t, 1);

    /* 0 : input, 1 : output */
    core->ports[0] = NULL;
    core->ports[1] = mc_gst_port_new(core);
    core->ports[1]->index = 1;

    core->available_queue = g_new0(mc_aqueue_t, 1);
    core->available_queue->input = mc_async_queue_new();

    g_mutex_init(&core->eos_mutex);
    g_mutex_init(&core->drain_mutex);
    g_cond_init(&core->eos_cond);
    g_mutex_init(&core->prepare_lock);

    core->need_feed = false;
    core->eos = false;
    core->need_codec_data = false;
    core->need_sync_flag = false;
    core->prepare_count = 0;
    core->idr_count = 0;
    core->sps_size = 0;
    core->pps_size = 0;

    if (!g_thread_supported()) {
        g_thread_init(NULL);
        LOGD("thread init");
    }
    g_atomic_int_set(&core->available_queue->running, 1);
    core->available_queue->thread = g_thread_new("feed thread", &feed_task, core);

    LOGD("gst_core(%p) is created", core);

    MEDIACODEC_FLEAVE();

    return core;
}

void mc_gst_core_free(mc_gst_core_t *core)
{
    MEDIACODEC_FENTER();

    mc_aqueue_t *async_queue;

    async_queue = core->available_queue;

    _mc_gst_set_flush_input(core);
    mc_async_queue_disable(async_queue->input);

    g_atomic_int_set(&async_queue->running, 0);
    g_thread_join(async_queue->thread);

	  g_mutex_clear(&core->drain_mutex);
    g_mutex_clear(&core->eos_mutex);
    g_mutex_clear(&core->prepare_lock);
    g_cond_clear(&core->eos_cond);

    mc_async_queue_free(async_queue->input);

    if (core->ports[1] != NULL) {
        mc_gst_port_free(core->ports[1]);
        core->ports[1] = NULL;
    }

    LOGD("gst_core(%p) is destroyed", core);
    g_free(core);

    MEDIACODEC_FLEAVE();
}

/*
 * mc_gst_port functions
 */
mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core)
{
    MEDIACODEC_FENTER();

    mc_gst_port_t *port;

    port = g_new0(mc_gst_port_t, 1);
    port->core = core;
    port->num_buffers = -1;
    port->buffer_size = 0;
    port->is_allocated = 0;
    port->buffers = NULL;

    g_mutex_init(&port->mutex);
    g_cond_init(&port->buffer_cond);
    port->queue = g_queue_new();

    LOGD("gst_port(%p) is created", port);

    MEDIACODEC_FLEAVE();
    return port;
}

void mc_gst_port_free(mc_gst_port_t *port)
{
    MEDIACODEC_FENTER();

    g_mutex_clear(&port->mutex);
    g_cond_clear(&port->buffer_cond);
    g_queue_free(port->queue);

    LOGD("gst_port(%p) is freed", port);
    g_free(port);

    MEDIACODEC_FLEAVE();

    return;
}

static void print_pad_capabilities(GstElement *element, gchar *pad_name)
{
    GstPad *pad = NULL;
    GstCaps *caps = NULL;
    gchar *format_str = NULL;
    gchar *format_tmp = NULL;
    GstStructure *structure;
    int i;

    pad = gst_element_get_static_pad(element, "src");

    if (pad == NULL) {
        LOGE("null pad");
    }

    caps = gst_pad_get_pad_template_caps(pad);
}

static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps)
{
    //TODO remove is_hw param
    core->format = __mc_get_gst_input_format(pkt, core->is_hw);

    GstPad *pad = NULL;
    GstCaps *template_caps;

    pad = gst_element_get_static_pad(core->codec, "src");
    template_caps = gst_pad_get_pad_template_caps(pad);

    __mc_create_caps(core, caps);
    g_object_set(core->appsrc, "caps", *caps, NULL);

    if (gst_caps_is_subset(*caps, template_caps)) {
        LOGD("new caps is subset of template caps");
    }
}

static GstPadProbeReturn
event_probe_cb(GstObject *pad, GstPadProbeInfo *info, gpointer user_data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)user_data;
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);

    switch (GST_EVENT_TYPE (event)) {
        case GST_EVENT_STREAM_START:
            core->codec_config = true;
            LOGD("codec_config is set %d, %d", input_count, count);
            break;
        default:
            break;
    }
    return GST_PAD_PROBE_OK;
}

static gpointer feed_task(gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    int ret = MC_ERROR_NONE;
    bool codec_config = FALSE;
    bool eos = FALSE;
    media_packet_h in_buf = NULL;
    GstMCBuffer* buff = NULL;
    GstCaps *new_caps = NULL;
    bool initiative = true;
    uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

    MEDIACODEC_FENTER();

    while(g_atomic_int_get(&core->available_queue->running))
    {
        LOGD("waiting for next input....");
        in_buf = _mc_get_input_buffer(core);

        if(!in_buf)
            goto LEAVE;

        if(media_packet_is_codec_config(in_buf, &codec_config) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_codec_config failed");
            goto ERROR;
        }

        if(media_packet_is_end_of_stream(in_buf, &eos) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_end_of_stream failed");
            goto ERROR;
        }

        if(codec_config)
            initiative = true;

        if (initiative) {
            GstPad *pad;

            _mc_gst_update_caps(core, in_buf, &new_caps);

            pad = gst_element_get_static_pad(core->appsrc, "src");
            gst_pad_push_event (pad, gst_event_new_stream_start("sejun"));
            gst_object_unref(pad);

            LOGD("caps updated");
        }

        buff = _mc_gst_media_packet_to_gstbuffer(core, &new_caps, in_buf, codec_config);
        if (!buff) {
            LOGW("gstbuffer can't make");
            goto ERROR;
        }
        LOGD("gstbuffer refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buff));

        g_mutex_lock(&core->drain_mutex);
        /* inject buffer */
        ret = _mc_gst_gstbuffer_to_appsrc(core, buff);
        if(ret != GST_FLOW_OK)
        {
            LOGE("Failed to push gst buffer");
            g_mutex_unlock(&core->drain_mutex);
            goto ERROR;
        }
        LOGD("after refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buff));
        LOGD("gst_app_src_get_current_level_bytes :%"G_GUINT64_FORMAT "\n", core->appsrc);

        initiative = false;
        g_mutex_unlock(&core->drain_mutex);

#if 1
        if (eos)
        {
            GstPad *pad;
            LOGD("end of stream");
#if 0
            pad = gst_element_get_static_pad(core->appsrc, "src");
            gst_pad_push_event (pad, gst_event_new_eos() );
            gst_object_unref(pad);
#else
            //goto EOS;

            //g_signal_emit_by_name(core->appsrc, "end-of-stream", &ret);
            gst_app_src_end_of_stream ( core->appsrc );
#endif

            while(!core->eos)
            {
                LOGD("waiting for eos signal...");
                if(!g_cond_wait_until(&core->eos_cond, &core->eos_mutex, wait_until))
                {
                    core->eos = true;
                    LOGD("time out");
                }
                else
                    LOGD("recevied signal");
            }

            core->eos = false;
            initiative = true;
#endif

        }
        continue;
ERROR:

        g_mutex_lock(&core->drain_mutex);
        _mc_gst_set_flush_input(core);
        g_mutex_unlock(&core->drain_mutex);

        if (core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
        {
            ((mc_error_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])(MC_INTERNAL_ERROR, core->user_data[_MEDIACODEC_EVENT_TYPE_ERROR]);
        }

        continue;
LEAVE:
        //LOGE("status : in_buf : %p, codec_config : %d, eos : %d, encoder : %d in feed_task", in_buf, codec_config, eos, core->encoder);
	continue;

    }

    if(new_caps)
    {
        gst_caps_unref(new_caps);
    }
    LOGI("status : in_buf : %p, codec_config : %d, eos : %d, video : %d, encoder : %d in feed_task",
        in_buf, codec_config, eos, core->video, core->encoder);
    LOGD("feed task finished %p v(%d)e(%d)", core, core->video, core->encoder);

    MEDIACODEC_FLEAVE();

    return NULL;
}

static void __mc_gst_stop_feed(GstElement *pipeline, gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;

    LOGI("stop_feed called");
    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS])
    {
        ((mc_buffer_status_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS])
                (MEDIACODEC_NEED_DATA, core->user_data[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS]);
    }
}

static void __mc_gst_start_feed(GstElement *pipeline, guint size, gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;

    LOGI("start_feed called");

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS])
    {
        ((mc_buffer_status_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS])
                (MEDIACODEC_ENOUGH_DATA, core->user_data[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS]);
    }
}

static int _mc_link_vtable(mc_gst_core_t *core, mediacodec_codec_type_e id, gboolean encoder, gboolean is_hw)
{
    MEDIACODEC_FENTER();

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    switch (id)
    {
        case MEDIACODEC_AAC:
        {
            LOGD("aac lc (adts) vtable");
            core->vtable = encoder ? aenc_vtable : adec_vtable;
            break;
        }
        case MEDIACODEC_AAC_HE:
        case MEDIACODEC_AAC_HE_PS:
        {
            LOGD("aac he v12 vtable");
            core->vtable = encoder ? aenc_vtable : adec_aacv12_vtable;
            if (encoder) {
                LOGD("[MC_NOT_SUPPORTED] aac encoder is not supported yet!!!");
                return MC_NOT_SUPPORTED;
            }
            break;
        }
        case MEDIACODEC_MP3:
        {
            LOGD("mp3 vtable - Only support decoder");
            core->vtable = encoder ? aenc_vtable : adec_mp3_vtable;
            if (encoder) {
                LOGD("[MC_NOT_SUPPORTED] mp3 encoder is not supported yet!!!");
                return MC_NOT_SUPPORTED;
            }

            break;
        }
        case MEDIACODEC_AMR_NB:
        {
            LOGD("amrnb vtable");
            core->vtable = encoder ? aenc_amrnb_vtable : adec_amrnb_vtable;
            break;
        }
        case MEDIACODEC_AMR_WB:
        {
            LOGD("amrwb vtable - Only support decoder");
            core->vtable = encoder ? aenc_vtable : adec_amrwb_vtable;
            break;
        }
        case MEDIACODEC_H263:
        {
            LOGD("h263 vtable");
            core->vtable = encoder ? venc_vtable : vdec_vtable;
            break;
        }
        case MEDIACODEC_MPEG4:
        {
            LOGD("mpeg4 vtable");
            core->vtable = encoder ? venc_vtable : vdec_vtable;
            break;
        }
        case MEDIACODEC_H264:
        {
            LOGD("h264 vtable");
            if ( is_hw )
                core->vtable = encoder ? venc_vtable : vdec_h264_vtable;
            else
                core->vtable = encoder ? venc_vtable : vdec_vtable;
            break;
        }
        default:
            break;
    }

    return MC_ERROR_NONE;
}

static int _mc_gst_gstbuffer_to_appsrc(mc_gst_core_t *core, GstMCBuffer *buff)
{
    MEDIACODEC_FENTER();

    int ret = MC_ERROR_NONE;

    LOGD(" pushed buffer to appsrc : %p, buffer of size %" G_GSIZE_FORMAT "", buff->buffer, gst_buffer_get_size (buff->buffer));

    ret = gst_app_src_push_buffer(GST_APP_SRC(core->appsrc), buff->buffer);

    return ret;
}

media_packet_h _mc_get_input_buffer(mc_gst_core_t *core)
{
    LOGD("waiting for input...");
    return mc_async_queue_pop(core->available_queue->input);
}

mc_ret_e mc_gst_prepare(mc_handle_t *mc_handle)
{
    MEDIACODEC_FENTER();

    int ret = MC_ERROR_NONE;
    media_format_mimetype_e out_mime;
    int i = 0;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    mediacodec_codec_type_e id;
    bool video;
    bool encoder;
    bool hardware;
    gchar *factory_name = NULL;
    static const mc_codec_map_t *codec_map;

    id = mc_handle->codec_id;
    video = mc_handle->is_video;
    encoder = mc_handle->is_encoder;
    hardware = mc_handle->is_hw;

    const int codec_list = encoder ? (sizeof(encoder_map) / sizeof(encoder_map[0])) : (sizeof(decoder_map) / sizeof(decoder_map[0]));

    codec_map = encoder ? encoder_map : decoder_map;

    for(i = 0; i < codec_list; i++)
    {
        if((id == codec_map[i].id) && (hardware == codec_map[i].hardware))
            break;
    }

    if( i == codec_list )
        return MC_NOT_SUPPORTED;

    factory_name = codec_map[i].type.factory_name;
    out_mime = codec_map[i].type.out_format;

    /* gst_core create */
    mc_gst_core_t* new_core = mc_gst_core_new();

    new_core->mime = codec_map[i].type.mime;
    new_core->is_hw = hardware;
    new_core->eos = false;
    new_core->encoder = encoder;
    new_core->video = video;
    new_core->codec_info = encoder ? (void*)&mc_handle->info.encoder : (void*)&mc_handle->info.decoder;
    new_core->out_mime = codec_map[i].type.out_format;

    LOGD("@%p(%p) core is initializing...v(%d)e(%d)", mc_handle, new_core, new_core->video, new_core->encoder);
    LOGD("factory name : %s, output_fmt : %x", factory_name, out_mime);

    /* create media_packet for output fmt */
    if ( (ret = _mc_output_media_packet_new(new_core, video, encoder, out_mime)) != MC_ERROR_NONE)
    {
        LOGE("Failed to create output pakcet");
        return ret;
    }

    /* link vtable */
    if((ret = _mc_link_vtable(new_core, id, encoder, hardware)) != MC_ERROR_NONE)
    {
        LOGE("vtable link failed");
        return ret;
    }

    for (i = 0; i < _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER ; i++)
    {
        LOGD("copy cb function [%d]", i);
        if (mc_handle->user_cb[i])
        {
            new_core->user_cb[i] = mc_handle->user_cb[i];
            new_core->user_data[i] = mc_handle->user_data[i];
            LOGD("user_cb[%d] %p, %p", i, new_core->user_cb[i], mc_handle->user_cb[i]);
        }
    }

    mc_handle->core = new_core;

    /* create basic core elements */
    ret = _mc_gst_create_pipeline(mc_handle->core, factory_name);

    LOGD("initialized...");
    return ret;
}

mc_ret_e mc_gst_unprepare(mc_handle_t *mc_handle)
{
    MEDIACODEC_FENTER();

    int i;
    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;

    if(core)
    {
        LOGD("@%p(%p) core is uninitializing... v(%d)e(%d)",mc_handle, core, core->video, core->encoder);

        _mc_gst_flush_buffers (core);

        /* unset callback */
        for (i = 0; i < _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER ; i++)
        {
            LOGD("unset cb function [%d]", i);
            if (mc_handle->user_cb[i])
            {
                core->user_cb[i] = NULL;
                core->user_data[i] = NULL;
                LOGD("user_cb[%d] %p, %p", i, core->user_cb[i], mc_handle->user_cb[i]);
            }
        }

        ret = _mc_gst_destroy_pipeline(core);

        if(core != NULL)
        {
            mc_gst_core_free(core);
            mc_handle->core = NULL;
        }
    }

    return ret;
}

mc_ret_e mc_gst_process_input(mc_handle_t *mc_handle, media_packet_h inbuf, uint64_t timeOutUs)
{
    MEDIACODEC_FENTER();

    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    GTimeVal nowtv;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d)process_input", core, core->video, core->encoder);
    LOGD("input buffer count :%d", input_count);

    g_get_current_time (&nowtv);
    g_time_val_add (&nowtv, 500 * 1000 );   /* usec */
/*
    if (!g_cond_timed_wait(&nowtv)) {
    }
*/
    input_count++;

    if(!core->eos)
        mc_async_queue_push(core->available_queue->input, inbuf);
    else
        ret = MC_INVALID_IN_BUF;



    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_get_output(mc_handle_t *mc_handle, media_packet_h *outbuf, uint64_t timeOutUs)
{
    MEDIACODEC_FENTER();

    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    media_packet_h out_pkt = NULL;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d) get_output", core, core->video, core->encoder);

    g_mutex_lock(&core->ports[1]->mutex);

    if(!g_queue_is_empty(core->ports[1]->queue))
    {
        out_pkt = g_queue_pop_head(core->ports[1]->queue);
        LOGD("pop from output_queue : %p", out_pkt);
    }
    else
    {
        ret = MC_OUTPUT_BUFFER_EMPTY;
        LOGD("output_queue is empty");
    }
    *outbuf = out_pkt;

    g_mutex_unlock(&core->ports[1]->mutex);

    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_flush_buffers(mc_handle_t *mc_handle)
{
    MEDIACODEC_FENTER();

    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    GstPad *pad;

    pad = gst_element_get_static_pad(core->appsrc, "src");

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d) get_output", core, core->video, core->encoder);

    _mc_gst_flush_buffers ( core );

    MEDIACODEC_FLEAVE();

    return ret;
}

static gboolean __mc_gst_init_gstreamer()
{
    MEDIACODEC_FENTER();

    static gboolean initialized = FALSE;
    static const int max_argc = 50;
    gint* argc = NULL;
    gchar** argv = NULL;
    gchar** argv2 = NULL;
    GError *err = NULL;
    int i = 0;
    int arg_count = 0;

    if ( initialized )
    {
        LOGD("gstreamer already initialized.\n");
        return TRUE;
    }

    /* alloc */
    argc = malloc( sizeof(int) );
    argv = malloc( sizeof(gchar*) * max_argc );
    argv2 = malloc( sizeof(gchar*) * max_argc );

    if ( !argc || !argv || !argv2 )
        goto ERROR;

    memset( argv, 0, sizeof(gchar*) * max_argc );
    memset( argv2, 0, sizeof(gchar*) * max_argc );

    /* add initial */
    *argc = 1;
    argv[0] = g_strdup( "media codec" );

    /* we would not do fork for scanning plugins */
    argv[*argc] = g_strdup("--gst-disable-registry-fork");
    (*argc)++;

    /* check disable registry scan */
    argv[*argc] = g_strdup("--gst-disable-registry-update");
    (*argc)++;

    /* check disable segtrap */
    argv[*argc] = g_strdup("--gst-disable-segtrap");
    (*argc)++;

    LOGD("initializing gstreamer with following parameter\n");
    LOGD("argc : %d\n", *argc);
    arg_count = *argc;

    for ( i = 0; i < arg_count; i++ )
    {
        argv2[i] = argv[i];
        LOGD("argv[%d] : %s\n", i, argv2[i]);
    }

    /* initializing gstreamer */
    if ( ! gst_init_check (argc, &argv, &err))
    {
        LOGE("Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
        if (err)
        {
            g_error_free (err);
        }

        goto ERROR;
    }

    /* release */
    for ( i = 0; i < arg_count; i++ )
    {
        MC_FREEIF( argv2[i] );
    }

    MC_FREEIF( argv );
    MC_FREEIF( argv2 );
    MC_FREEIF( argc );

    /* done */
    initialized = TRUE;

    return TRUE;

ERROR:

    /* release */
    for ( i = 0; i < arg_count; i++ )
    {
        LOGD("free[%d] : %s\n", i, argv2[i]);
        MC_FREEIF( argv2[i] );
    }

    MC_FREEIF( argv );
    MC_FREEIF( argv2 );
    MC_FREEIF( argc );

    return FALSE;
}

mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core, gchar *factory_name)
{
    GstBus *bus = NULL;
    GstPad *pad = NULL;

    MEDIACODEC_FENTER();

    g_mutex_lock(&core->prepare_lock);
    if(core->prepare_count == 0)
    {

        if (!__mc_gst_init_gstreamer())
        {
            LOGE ("gstreamer initialize fail");
            g_mutex_unlock(&core->prepare_lock);
            return MC_NOT_INITIALIZED;
        }
        core->codec = gst_element_factory_make(factory_name, NULL);

        if(!core->codec)
        {
            LOGE ("codec element create fail");
            goto ERROR;
        }

        LOGD("@%p v(%d)e(%d) create_pipeline", core, core->video, core->encoder);
        MEDIACODEC_ELEMENT_SET_STATE(core->codec, GST_STATE_READY);

        /* create common elements */
        core->pipeline = gst_pipeline_new(NULL);

        if (!core->pipeline)
        {
            LOGE ("pipeline create fail");
            goto ERROR;
        }

        core->appsrc = gst_element_factory_make("appsrc", NULL);

        if (!core->appsrc)
        {
            LOGE ("appsrc can't create");
            goto ERROR;
        }

        core->capsfilter = gst_element_factory_make("capsfilter", NULL);

        if (!core->capsfilter)
        {
            LOGE ("capsfilter can't create");
            goto ERROR;
        }

        core->fakesink = gst_element_factory_make("fakesink", NULL);

        if (!core->fakesink)
        {
            LOGE ("fakesink create fail");
            goto ERROR;
        }
        g_object_set(core->fakesink, "enable-last-buffer", FALSE, NULL);

        //__mc_link_elements(core);
        gst_bin_add_many(GST_BIN(core->pipeline), core->appsrc, core->capsfilter, core->codec, core->fakesink, NULL);

        /* link elements */
        gst_element_link_many(core->appsrc, core->capsfilter, core->codec, core->fakesink, NULL);

        /* connect signals, bus watcher */
        bus = gst_pipeline_get_bus (GST_PIPELINE (core->pipeline));
        core->bus_whatch_id = gst_bus_add_watch (bus, __mc_gst_bus_callback, core);
        core->thread_default = g_main_context_get_thread_default();

        /* set sync handler to get tag synchronously */
        gst_bus_set_sync_handler(bus, __mc_gst_bus_sync_callback, core, NULL);

        gst_object_unref (GST_OBJECT(bus));

        /* add pad probe */
        pad = gst_element_get_static_pad(core->fakesink, "sink");
        //core->probe_id = gst_pad_add_event_probe(pad, G_CALLBACK(event_probe_cb), core);
        core->probe_id =
            gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
            (GstPadProbeCallback)event_probe_cb, core, NULL);
        gst_object_unref (pad);

        /* app src */
        g_signal_connect(core->appsrc, "need-data", G_CALLBACK(__mc_gst_start_feed), core);
        g_signal_connect(core->appsrc, "enough-data", G_CALLBACK(__mc_gst_stop_feed), core);

        /* connect handoff */
        g_object_set (GST_OBJECT(core->fakesink), "signal-handoffs", TRUE, NULL);
        core->signal_handoff = g_signal_connect(core->fakesink, "handoff", G_CALLBACK(__mc_gst_buffer_add), core);

        /* set state PLAYING */
        MEDIACODEC_ELEMENT_SET_STATE(GST_ELEMENT_CAST(core->pipeline), GST_STATE_PLAYING);

    }
    core->prepare_count++;
    g_mutex_unlock(&core->prepare_lock);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;

STATE_CHANGE_FAILED:
ERROR:

    if(core->codec)
        gst_object_unref(GST_OBJECT(core->codec));

    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    if(core->appsrc)
        gst_object_unref(GST_OBJECT(core->appsrc));

    if(core->capsfilter)
        gst_object_unref(GST_OBJECT(core->capsfilter));

    if(core->fakesink)
        gst_object_unref(GST_OBJECT(core->fakesink));

    g_mutex_unlock(&core->prepare_lock);

    return MC_ERROR;
}

mc_ret_e _mc_gst_destroy_pipeline(mc_gst_core_t *core)
{
    int ret = MC_ERROR_NONE;
    GstPad *pad = NULL;

    MEDIACODEC_FENTER();

    g_mutex_lock(&core->prepare_lock);
    core->prepare_count--;
    if(core->prepare_count == 0)
    {

        if(core->pipeline)
        {
            /* disconnect signal */
            if(core->fakesink && GST_IS_ELEMENT(core->fakesink))
            {
                if(g_signal_handler_is_connected(core->fakesink, core->signal_handoff))
                {
                    g_signal_handler_disconnect(core->fakesink, core->signal_handoff);
                    LOGD("handoff signal destroy");
                }
            }

            if(core->bus_whatch_id)
            {
                GSource *source = NULL;
                source = g_main_context_find_source_by_id (core->thread_default, core->bus_whatch_id);
                g_source_destroy(source);
                //g_source_remove(core->bus_whatch_id);
                LOGD("bus_whatch_id destroy");
            }

            pad = gst_element_get_static_pad(core->fakesink, "sink");
            gst_pad_remove_probe(pad, core->probe_id);
            g_object_unref(pad);

            MEDIACODEC_ELEMENT_SET_STATE(core->pipeline, GST_STATE_NULL);

            gst_object_unref(GST_OBJECT(core->pipeline));
        }
    }

    LOGD("@%p v(%d)e(%d) destroy_pipeline : %d ", core, core->video, core->encoder, core->prepare_count);
    g_mutex_unlock(&core->prepare_lock);

    MEDIACODEC_FLEAVE();

    return ret;

STATE_CHANGE_FAILED:
    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    LOGD("@%p v(%d)e(%d) destroy_pipeline failed", core, core->video, core->encoder);
    g_mutex_unlock(&core->prepare_lock);

    return MC_ERROR;
}

void __mc_gst_append_codec_data(mc_gst_core_t *core, media_packet_h pkt)
{
    void* buf_data = NULL;
    uint64_t buf_size = 0;
    void *tmp = NULL;

    media_packet_get_buffer_size(pkt, &buf_size);
    media_packet_get_buffer_data_ptr(pkt, &buf_data);

    tmp = (unsigned char*)malloc(sizeof(unsigned char)*buf_size);
    memcpy(tmp, buf_data, buf_size);
    memcpy(buf_data, core->codec_data, core->codec_data_size);
    memcpy(buf_data + core->codec_data_size, tmp, buf_size);
    media_packet_set_buffer_size(pkt, buf_size+core->codec_data_size);
    free(tmp);
}

void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data)
{
    mc_gst_core_t* core = (mc_gst_core_t*) data;
    void* buf_data = NULL;
    uint64_t buf_size = 0;
    static gboolean initialized = FALSE;
    int mask = 0xfffffff0;
    guint n;
    GstMemory *mem;
    MMVideoBuffer *imgb = NULL;
    GstMapInfo map = GST_MAP_INFO_INIT;
    media_packet_h out_pkt = NULL;

    MEDIACODEC_FENTER();

    gst_buffer_ref(buffer);

    n = gst_buffer_n_memory(buffer);

    mem = gst_buffer_peek_memory(buffer, n-1);

    gst_memory_map(mem, &map, GST_MAP_READ);
    LOGD("n : %d, map.data : %p, map.size : %d",n, map.data, map.size);

    out_pkt = __mc_gst_make_media_packet(core, map.data, map.size);

    LOGI("@%p(%d) out_pkt : %p", core, core->encoder, out_pkt);
    gst_memory_unmap(mem, &map);


    if ( out_pkt ) {
        media_packet_set_extra(out_pkt, buffer);
        media_packet_set_pts(out_pkt, GST_BUFFER_TIMESTAMP(buffer));

        if (core->need_codec_data) {
            media_packet_set_flags(out_pkt, MEDIA_PACKET_CODEC_CONFIG);
            core->need_codec_data = false;
        }

        if ( core->need_sync_flag ) {
            media_packet_set_flags(out_pkt, MEDIA_PACKET_SYNC_FRAME);
            core->need_sync_flag = false;
        }

        g_mutex_lock(&core->ports[1]->mutex);
        /* push it to output buffer queue */
        g_queue_push_tail(core->ports[1]->queue, out_pkt);

        count++;
        LOGD("queued : %d", count);

        g_mutex_unlock(&core->ports[1]->mutex);

        if ( core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] ) {
            ((mc_fill_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])
                (out_pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER]);
        }
    }

    MEDIACODEC_FLEAVE();

    return;
}

int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data)
{
    GstMemory *mem;
    void* buffer = NULL;

    media_packet_get_extra(packet, &buffer);

    gst_buffer_unref((GstBuffer*)buffer);

    return MEDIA_PACKET_FINALIZE;
}

gchar *__mc_get_gst_input_format(media_packet_h packet, bool is_hw)
{
    gchar *format = NULL;
    media_format_h fmt = NULL;
    media_format_mimetype_e mimetype = 0;

    media_packet_get_format(packet, &fmt);
    media_format_get_video_info(fmt, &mimetype, NULL, NULL, NULL, NULL);
    LOGD("input packet mimetype : %x", mimetype);

    switch(mimetype)
    {
        case MEDIA_FORMAT_I420:
            format = "I420";
            break;
        case MEDIA_FORMAT_NV12:
            if (is_hw)
                format = "SN12";
            else
                format = "NV12";
            break;
        case MEDIA_FORMAT_ARGB:
                format = "ARGB";
            break;
        default:
            break;
    }
    LOGD("input packet format : %s", format);
    return format;
}

guint __mc_set_codecdata_buffer(mc_gst_core_t *core, GstCaps **caps, void *buf_data, int buf_size)
{
    GstBuffer *codec_data;
    guint16 codec_data_data;
    GstMapInfo map;
    guint codecdata_size;

    LOGD("[LEO] Enable codec_config, So Set caps for codec_data in codec_id (0x%x)", core->codec_id);

    if ( (!core->encoder) &&
            (core->codec_id == MEDIACODEC_AAC ||core->codec_id == MEDIACODEC_AAC_HE || core->codec_id == MEDIACODEC_AAC_HE_PS))
    {
        /*
         * The codec_data data is according to AudioSpecificConfig,
         *  ISO/IEC 14496-3, 1.6.2.1
         */
        codecdata_size = 16;          /*AUDIO_CODECDATA_SIZE = 16 (in testsuit)*/
        codec_data = gst_buffer_new_and_alloc (codecdata_size);
        gst_buffer_map (codec_data, &map, GST_MAP_WRITE);
        memcpy (map.data, buf_data, codecdata_size);
        gst_buffer_unmap (codec_data, &map);

        gst_caps_set_simple (*caps, "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
    }
    else
    {
        codecdata_size = 0;
        LOGW("codec data set fail");
    }

    return codecdata_size;
}

GstMCBuffer* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config)
{
    int ret = MEDIA_PACKET_ERROR_NONE;
    GstMCBuffer* mc_buffer = NULL;
    void* buf_data = NULL;
    uint64_t buf_size = 0;
    uint64_t pts = 0;
    uint64_t dur = 0;

    ret = media_packet_get_buffer_size(pkt, &buf_size);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return NULL;
    }

    ret = media_packet_get_buffer_data_ptr(pkt, &buf_data);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return NULL;
    }

    mc_buffer = gst_mediacodec_buffer_new(core, pkt, buf_size);
    if(mc_buffer->buffer == NULL)
    {
        LOGW("failed to create inbuf");
        return NULL;
    }

#if 1   //Added code for codec_data
    if (codec_config)
    {
        guint codec_data_size = 0;
        codec_data_size = __mc_set_codecdata_buffer(core, caps, buf_data, (int)buf_size);

        buf_data += codec_data_size;
        buf_size -= codec_data_size;

        LOGD("Enable codec_config, returned codec_data_size (%d)", codec_data_size);
    }
#endif

    ret = __mc_fill_input_buffer(core, buf_data, (int)buf_size, mc_buffer);
    if (ret != MC_ERROR_NONE)
    {
        LOGW("failed to fill inbuf");
        return NULL;
    }

    /* pts */
    media_packet_get_pts(pkt, &pts);
    GST_BUFFER_PTS(mc_buffer->buffer) = pts;

    /* duration */
    media_packet_get_duration(pkt, &dur);
    GST_BUFFER_DURATION(mc_buffer->buffer) = dur;

    return mc_buffer;
}

media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buff)
{
    media_packet_h out_pkt = NULL;

    if (!buff)
        return NULL;

    //__mc_fill_output_buffer(core, buff, &out_pkt);

    gst_buffer_ref(buff);

    return out_pkt;
}

media_packet_h __mc_gst_make_media_packet(mc_gst_core_t *core, unsigned char *data, int size)
{
    media_packet_h pkt = NULL;

    __mc_fill_output_buffer(core, data, size,  &pkt);

    return pkt;
}

gboolean __mc_gst_bus_callback (GstBus *bus, GstMessage *msg, gpointer data)
{
    int ret  = MC_ERROR_NONE;
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    LOGD("@%p v(%d)e(%d)", core, core->video, core->encoder);

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_EOS:
        {
            core->eos = true;
            g_cond_signal(&core->eos_cond);
            LOGD("send eos signal");
            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                LOGD("eos callback invoked");
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }

            LOGD ("End of stream\n");
        }
        break;

        case GST_MESSAGE_ERROR:
        {
            GError* error = NULL;

            gst_message_parse_error (msg, &error, NULL);

            if (!error)
            {
                LOGW("GST error message parsing failed");
                break;
            }

            LOGW ("Error: %s\n", error->message);

            if(error)
            {
                if(error->domain == GST_STREAM_ERROR)
                {
                    ret = __gst_handle_stream_error(core, error, msg);
                }
                else if (error->domain == GST_RESOURCE_ERROR)
                {
                    ret = __gst_handle_resource_error(core, error->code);
                }
                else if (error->domain == GST_LIBRARY_ERROR)
                {
                    ret = __gst_handle_library_error(core, error->code);
                }
                else if (error->domain == GST_CORE_ERROR)
                {
                    ret = __gst_handle_core_error(core, error->code);
                }
                else
                {
                    LOGW("Unexpected error has occured");
                }

                if (core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
                {
                    ((mc_error_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
                        (ret, core->user_data[_MEDIACODEC_EVENT_TYPE_ERROR]);
                }
            }
            g_error_free (error);
        }
        break;

        default:
        break;
    }

    //gst_object_unref(msg);

    return TRUE;
}

static gboolean
__mc_gst_check_useful_message(mc_gst_core_t *core, GstMessage *msg)
{
    gboolean retval = false;

    if(!core->pipeline)
    {
        LOGE("mediacodec pipeline handle is null");
        return true;
    }

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_TAG:
        case GST_MESSAGE_EOS:
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_WARNING:
            retval = true;
            break;
        default:
            retval = false;
            break;
    }

    return retval;
}

static GstBusSyncReply
__mc_gst_bus_sync_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    GstBusSyncReply reply = GST_BUS_DROP;

    LOGD("__mc_gst_bus_sync_callback is called");

    if(!core->pipeline)
    {
        LOGE("mediacodec pipeline handle is null");
        return GST_BUS_PASS;
    }

    if(!__mc_gst_check_useful_message(core, msg))
    {
        gst_message_unref(msg);
        return GST_BUS_DROP;
    }

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
        case GST_MESSAGE_ERROR:
          __mc_gst_bus_callback(NULL, msg, core);
          reply = GST_BUS_DROP;
        break;

        default:
          reply = GST_BUS_PASS;
          break;
    }

    if( reply == GST_BUS_DROP )
        gst_message_unref(msg);

    return reply;
}

static MMVideoBuffer * __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt)
{
    tbm_surface_h surface = NULL;
    tbm_bo bo = NULL;
    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    if (!pkt) {
        LOGE("output is null");
        return NULL;
    }

    MMVideoBuffer *imgb = NULL;
    imgb = (MMVideoBuffer *)malloc(sizeof(MMVideoBuffer));
    if (!imgb) {
        LOGE("Failed to alloc MMVideoBuffer");
        return NULL;
    }
    memset(imgb, 0x00, sizeof(MMVideoBuffer));

    media_packet_get_tbm_surface(pkt, &surface);
    bo = tbm_surface_internal_get_bo(surface, 0);

    tbm_bo_handle handle = tbm_bo_get_handle(bo, TBM_DEVICE_CPU);
#ifdef Z130H
    int phy_addr = 0;
    int phy_size = 0;
    tbm_bo_handle handle_fd = tbm_bo_get_handle(bo, TBM_DEVICE_MM);

    if (__tbm_get_physical_addr_bo(handle_fd, &phy_addr, &phy_size) == 0)
    {
        imgb->handle.paddr[0] = (void*)phy_addr;
        LOGD("imgb->paddr : %p", imgb->handle.paddr[0]);
    }
    LOGD("paddr : %p", phy_addr);
#endif

    imgb->type = MM_VIDEO_BUFFER_TYPE_TBM_BO;
    imgb->handle.bo[0] = bo;
    imgb->data[0] =  handle.ptr;
    imgb->width[0] = enc_info->width;
    imgb->height[0] = enc_info->height;
    imgb->stride_width[0] = imgb->width[0];
    imgb->stride_height[0] = imgb->height[0];

    return imgb;
}

static void gst_mediacodec_buffer_finalize(GstMCBuffer *mc_buffer)
{
    MMVideoBuffer *imgb = NULL;
    GstMemory *mem = NULL;
    GstMapInfo map = GST_MAP_INFO_INIT;
    mc_gst_core_t *core = mc_buffer->core;
    guint n;



    n = gst_buffer_n_memory(mc_buffer->buffer);

    mem = gst_buffer_peek_memory(mc_buffer->buffer, n-1);

    gst_memory_map(mem, &map, GST_MAP_READ);
    LOGD("n : %d, map.data : %p, map.size : %d",n, map.data, map.size);

    gst_memory_unmap(mem, &map);

    LOGD("gst_mediacodec_buffer_finalize()");
/*
    if(mc_buffer->has_imgb) {

        mem = gst_buffer_peek_memory(mc_buffer->buffer, 1);
        gst_memory_map(mem, &map, GST_MAP_READ);
        imgb = (MMVideoBuffer*)map.data;
        gst_memory_unmap(mem, &map);

        if(imgb) {
            free(imgb);
            imgb = NULL;
            LOGD("imgb is destroyed");
        }
    }
*/

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
    {
        ((mc_empty_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
                (mc_buffer->pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]);
    }

    return;
}

static GstMCBuffer *gst_mediacodec_buffer_new(mc_gst_core_t* core, media_packet_h pkt, uint64_t size)
{
    GstMCBuffer *mc_buffer = NULL;

    mc_buffer = (GstMCBuffer *)malloc(sizeof(*mc_buffer));
    mc_buffer->buffer = gst_buffer_new();
    mc_buffer->buf_size = size;

    LOGD("creating buffer : %p, %p", mc_buffer, mc_buffer->buffer);
    mc_buffer->core = core;
    mc_buffer->pkt = pkt;

    return mc_buffer;
}

static gint __gst_handle_core_error(mc_gst_core_t* core, int code )
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_CORE_ERROR_MISSING_PLUGIN:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
        case GST_CORE_ERROR_STATE_CHANGE:
        case GST_CORE_ERROR_SEEK:
        case GST_CORE_ERROR_NOT_IMPLEMENTED:
        case GST_CORE_ERROR_FAILED:
        case GST_CORE_ERROR_TOO_LAZY:
        case GST_CORE_ERROR_PAD:
        case GST_CORE_ERROR_THREAD:
        case GST_CORE_ERROR_NEGOTIATION:
        case GST_CORE_ERROR_EVENT:
        case GST_CORE_ERROR_CAPS:
        case GST_CORE_ERROR_TAG:
        case GST_CORE_ERROR_CLOCK:
        case GST_CORE_ERROR_DISABLED:
        default:
            trans_err =  MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}

static gint __gst_handle_library_error(mc_gst_core_t* core, int code)
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_LIBRARY_ERROR_FAILED:
        case GST_LIBRARY_ERROR_TOO_LAZY:
        case GST_LIBRARY_ERROR_INIT:
        case GST_LIBRARY_ERROR_SHUTDOWN:
        case GST_LIBRARY_ERROR_SETTINGS:
        case GST_LIBRARY_ERROR_ENCODE:
        default:
            trans_err =  MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}


static gint __gst_handle_resource_error(mc_gst_core_t* core, int code )
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
            trans_err = MEDIACODEC_ERROR_NO_FREE_SPACE;
            break;
        case GST_RESOURCE_ERROR_WRITE:
        case GST_RESOURCE_ERROR_FAILED:
        case GST_RESOURCE_ERROR_SEEK:
        case GST_RESOURCE_ERROR_TOO_LAZY:
        case GST_RESOURCE_ERROR_BUSY:
        case GST_RESOURCE_ERROR_OPEN_WRITE:
        case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
        case GST_RESOURCE_ERROR_CLOSE:
        case GST_RESOURCE_ERROR_SYNC:
        case GST_RESOURCE_ERROR_SETTINGS:
        default:
            trans_err = MEDIACODEC_ERROR_INTERNAL;
            break;
    }

    return trans_err;
}

static gint __gst_handle_stream_error(mc_gst_core_t* core, GError* error, GstMessage * message)
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);
    g_return_val_if_fail(error, MC_PARAM_ERROR);
    g_return_val_if_fail (message, MC_PARAM_ERROR);

    switch ( error->code )
    {
        case GST_STREAM_ERROR_FAILED:
        case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        case GST_STREAM_ERROR_DECODE:
        case GST_STREAM_ERROR_WRONG_TYPE:
        case GST_STREAM_ERROR_DECRYPT:
        case GST_STREAM_ERROR_DECRYPT_NOKEY:
        case GST_STREAM_ERROR_CODEC_NOT_FOUND:
            trans_err = __gst_transform_gsterror( core, message, error );
            break;

        case GST_STREAM_ERROR_NOT_IMPLEMENTED:
        case GST_STREAM_ERROR_TOO_LAZY:
        case GST_STREAM_ERROR_ENCODE:
        case GST_STREAM_ERROR_DEMUX:
        case GST_STREAM_ERROR_MUX:
        case GST_STREAM_ERROR_FORMAT:
        default:
            trans_err = MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}

static gint __gst_transform_gsterror( mc_gst_core_t *core, GstMessage * message, GError* error )
{
    gchar *src_element_name = NULL;
    GstElement *src_element = NULL;
    GstElementFactory *factory = NULL;
    const gchar* klass = NULL;


    src_element = GST_ELEMENT_CAST(message->src);
    if ( !src_element )
        goto INTERNAL_ERROR;

    src_element_name = GST_ELEMENT_NAME(src_element);
    if ( !src_element_name )
        goto INTERNAL_ERROR;

    factory = gst_element_get_factory(src_element);
    if ( !factory )
        goto INTERNAL_ERROR;

    klass = gst_element_factory_get_klass(factory);
    if ( !klass )
        goto INTERNAL_ERROR;

    LOGD("error code=%d, msg=%s, src element=%s, class=%s\n",
            error->code, error->message, src_element_name, klass);

    switch ( error->code )
    {
        case GST_STREAM_ERROR_DECODE:
            return MEDIACODEC_ERROR_INVALID_STREAM;
            break;

        case GST_STREAM_ERROR_CODEC_NOT_FOUND:
        case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        case GST_STREAM_ERROR_WRONG_TYPE:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
            break;

        case GST_STREAM_ERROR_FAILED:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
            break;

        default:
            break;
    }

    return MEDIACODEC_ERROR_INVALID_STREAM;

INTERNAL_ERROR:
    return MEDIACODEC_ERROR_INTERNAL;
}

static void _mc_gst_flush_buffers( mc_gst_core_t *core )
{
    GstPad *pad;

    pad = gst_element_get_static_pad(core->appsrc, "src");

    mc_async_queue_disable(core->available_queue->input);

    _mc_gst_set_flush_input ( core );
    gst_pad_push_event( pad, gst_event_new_flush_start());
    _mc_gst_set_flush_output ( core );
    gst_pad_push_event ( pad, gst_event_new_flush_stop (FALSE));
    gst_object_unref(pad);

    mc_async_queue_enable(core->available_queue->input);

    MEDIACODEC_FLEAVE();
}


static void _mc_gst_set_flush_input(mc_gst_core_t *core)
{
    media_packet_h pkt = NULL;

    LOGI("_mc_gst_set_flush_input is called");
    while( pkt != mc_async_queue_pop_forced(core->available_queue->input) )
    {
        LOGD("%p pkt is poped");
        if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
        {
            ((mc_empty_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
                (pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]);
        }
    }

    mc_async_queue_flush(core->available_queue->input);
}

static void _mc_gst_set_flush_output(mc_gst_core_t *core)
{
    media_packet_h pkt = NULL;

    LOGI("_mc_gst_set_flush_output is called");
    g_mutex_lock(&core->ports[1]->mutex);

    if(!g_queue_is_empty(core->ports[1]->queue))
    {
        while(pkt != g_queue_pop_head(core->ports[1]->queue))
        {
            LOGD("outpkt in output_queue : %p", pkt);
            media_packet_destroy(pkt);
        }
    }

    g_mutex_unlock(&core->ports[1]->mutex);
}

#ifdef Z130H
int __tbm_get_physical_addr_bo(tbm_bo_handle tbm_bo_handle_fd_t, int* phy_addr, int* phy_size)
{
    int tbm_bo_handle_fd;

    int ret=0;

    tbm_bo_handle_fd = tbm_bo_handle_fd_t.u32;

    int open_flags = O_RDWR;
    int ion_fd = -1;

    struct ion_mmu_data mmu_data;
    struct ion_custom_data  custom_data;

    mmu_data.fd_buffer = tbm_bo_handle_fd;
    custom_data.cmd = 4;
    custom_data.arg = (unsigned long)&mmu_data;

    ion_fd = open ("/dev/ion", open_flags);
    if (ion_fd < 0)
    {
      LOGE ("[tbm_get_physical_addr_bo] ion_fd open device failed");
    }

    if (ioctl(ion_fd, ION_IOC_CUSTOM, &custom_data)<0)
    {
        LOGE ("[tbm_get_physical_addr_bo] ION_IOC_CUSTOM fails %d %s",errno,strerror(errno));
        ret=-1;
    }

    if (!ret)
    {
        *phy_addr = mmu_data.iova_addr;
        *phy_size = mmu_data.iova_size;
    }
    else
    {
        *phy_addr = 0;
        *phy_size = 0;
        LOGW ("[tbm_get_physical_addr_bo] getting physical address is failed. phy_addr = 0");
    }

    if (ion_fd != -1)
    {
        close (ion_fd);
        ion_fd = -1;
    }

    return 0;
}
#endif

/*
 * Get tiled address of position(x,y)
 *
 * @param x_size
 *   width of tiled[in]
 *
 * @param y_size
 *   height of tiled[in]
 *
 * @param x_pos
 *   x position of tield[in]
 *
 * @param src_size
 *   y position of tield[in]
 *
 * @return
 *   address of tiled data
 */
static int __tile_4x2_read(int x_size, int y_size, int x_pos, int y_pos)
{
    int pixel_x_m1, pixel_y_m1;
    int roundup_x;
    int linear_addr0, linear_addr1, bank_addr ;
    int x_addr;
    int trans_addr;

    pixel_x_m1 = x_size -1;
    pixel_y_m1 = y_size -1;

    roundup_x = ((pixel_x_m1 >> 7) + 1);

    x_addr = x_pos >> 2;

    if ((y_size <= y_pos+32) && ( y_pos < y_size) &&
        (((pixel_y_m1 >> 5) & 0x1) == 0) && (((y_pos >> 5) & 0x1) == 0)) {
        linear_addr0 = (((y_pos & 0x1f) <<4) | (x_addr & 0xf));
        linear_addr1 = (((y_pos >> 6) & 0xff) * roundup_x + ((x_addr >> 6) & 0x3f));

        if (((x_addr >> 5) & 0x1) == ((y_pos >> 5) & 0x1))
            bank_addr = ((x_addr >> 4) & 0x1);
        else
            bank_addr = 0x2 | ((x_addr >> 4) & 0x1);
    } else {
        linear_addr0 = (((y_pos & 0x1f) << 4) | (x_addr & 0xf));
        linear_addr1 = (((y_pos >> 6) & 0xff) * roundup_x + ((x_addr >> 5) & 0x7f));

        if (((x_addr >> 5) & 0x1) == ((y_pos >> 5) & 0x1))
            bank_addr = ((x_addr >> 4) & 0x1);
        else
            bank_addr = 0x2 | ((x_addr >> 4) & 0x1);
    }

    linear_addr0 = linear_addr0 << 2;
    trans_addr = (linear_addr1 <<13) | (bank_addr << 11) | linear_addr0;

    return trans_addr;
}

/*
 * Converts tiled data to linear
 * Crops left, top, right, buttom
 * 1. Y of NV12T to Y of YUV420P
 * 2. Y of NV12T to Y of YUV420S
 * 3. UV of NV12T to UV of YUV420S
 *
 * @param yuv420_dest
 *   Y or UV plane address of YUV420[out]
 *
 * @param nv12t_src
 *   Y or UV plane address of NV12T[in]
 *
 * @param yuv420_width
 *   Width of YUV420[in]
 *
 * @param yuv420_height
 *   Y: Height of YUV420, UV: Height/2 of YUV420[in]
 *
 * @param left
 *   Crop size of left
 *
 * @param top
 *   Crop size of top
 *
 * @param right
 *   Crop size of right
 *
 * @param buttom
 *   Crop size of buttom
 */
static void __csc_tiled_to_linear_crop(unsigned char *yuv420_dest, unsigned char *nv12t_src,
    int yuv420_width, int yuv420_height,
    int left, int top, int right, int buttom)
{
    int i, j;
    int tiled_offset = 0, tiled_offset1 = 0;
    int linear_offset = 0;
    int temp1 = 0, temp2 = 0, temp3 = 0, temp4 = 0;

    temp3 = yuv420_width-right;
    temp1 = temp3-left;
    /* real width is greater than or equal 256 */
    if (temp1 >= 256) {
        for (i=top; i<yuv420_height-buttom; i=i+1) {
            j = left;
            temp3 = (j>>8)<<8;
            temp3 = temp3>>6;
            temp4 = i>>5;
            if (temp4 & 0x1) {
                /* odd fomula: 2+x+(x>>2)<<2+x_block_num*(y-1) */
                tiled_offset = temp4-1;
                temp1 = ((yuv420_width+127)>>7)<<7;
                tiled_offset = tiled_offset*(temp1>>6);
                tiled_offset = tiled_offset+temp3;
                tiled_offset = tiled_offset+2;
                temp1 = (temp3>>2)<<2;
                tiled_offset = tiled_offset+temp1;
                tiled_offset = tiled_offset<<11;
                tiled_offset1 = tiled_offset+2048*2;
                temp4 = 8;
            } else {
                temp2 = ((yuv420_height+31)>>5)<<5;
                if ((i+32)<temp2) {
                    /* even1 fomula: x+((x+2)>>2)<<2+x_block_num*y */
                    temp1 = temp3+2;
                    temp1 = (temp1>>2)<<2;
                    tiled_offset = temp3+temp1;
                    temp1 = ((yuv420_width+127)>>7)<<7;
                    tiled_offset = tiled_offset+temp4*(temp1>>6);
                    tiled_offset = tiled_offset<<11;
                    tiled_offset1 = tiled_offset+2048*6;
                    temp4 = 8;
                } else {
                    /* even2 fomula: x+x_block_num*y */
                    temp1 = ((yuv420_width+127)>>7)<<7;
                    tiled_offset = temp4*(temp1>>6);
                    tiled_offset = tiled_offset+temp3;
                    tiled_offset = tiled_offset<<11;
                    tiled_offset1 = tiled_offset+2048*2;
                    temp4 = 4;
                }
            }

            temp1 = i&0x1F;
            tiled_offset = tiled_offset+64*(temp1);
            tiled_offset1 = tiled_offset1+64*(temp1);
            temp2 = yuv420_width-left-right;
            linear_offset = temp2*(i-top);
            temp3 = ((j+256)>>8)<<8;
            temp3 = temp3-j;
            temp1 = left&0x3F;
            if (temp3 > 192) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset+temp1, 64-temp1);
                temp2 = ((left+63)>>6)<<6;
                temp3 = ((yuv420_width-right)>>6)<<6;
                if (temp2 == temp3) {
                    temp2 = yuv420_width-right-(64-temp1);
                }
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128-temp1, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+192-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+256-temp1;
            } else if (temp3 > 128) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset+2048+temp1, 64-temp1);
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+128-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+192-temp1;
            } else if (temp3 > 64) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset1+temp1, 64-temp1);
                memcpy(yuv420_dest+linear_offset+64-temp1, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+128-temp1;
            } else if (temp3 > 0) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset1+2048+temp1, 64-temp1);
                linear_offset = linear_offset+64-temp1;
            }

            tiled_offset = tiled_offset+temp4*2048;
            j = (left>>8)<<8;
            j = j + 256;
            temp2 = yuv420_width-right-256;
            for (; j<=temp2; j=j+256) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                tiled_offset1 = tiled_offset1+temp4*2048;
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, 64);
                tiled_offset = tiled_offset+temp4*2048;
                memcpy(yuv420_dest+linear_offset+192, nv12t_src+tiled_offset1+2048, 64);
                linear_offset = linear_offset+256;
            }

            tiled_offset1 = tiled_offset1+temp4*2048;
            temp2 = yuv420_width-right-j;
            if (temp2 > 192) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, 64);
                memcpy(yuv420_dest+linear_offset+192, nv12t_src+tiled_offset1+2048, temp2-192);
            } else if (temp2 > 128) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, 64);
                memcpy(yuv420_dest+linear_offset+128, nv12t_src+tiled_offset1, temp2-128);
            } else if (temp2 > 64) {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                memcpy(yuv420_dest+linear_offset+64, nv12t_src+tiled_offset+2048, temp2-64);
            } else {
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            }
        }
    } else if (temp1 >= 64) {
        for (i=top; i<(yuv420_height-buttom); i=i+1) {
            j = left;
            tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
            temp2 = ((j+64)>>6)<<6;
            temp2 = temp2-j;
            linear_offset = temp1*(i-top);
            temp4 = j&0x3;
            tiled_offset = tiled_offset+temp4;
            memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            linear_offset = linear_offset+temp2;
            j = j+temp2;
            if ((j+64) <= temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                linear_offset = linear_offset+64;
                j = j+64;
            }
            if ((j+64) <= temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 64);
                linear_offset = linear_offset+64;
                j = j+64;
            }
            if (j < temp3) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                temp2 = temp3-j;
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, temp2);
            }
        }
    } else {
        for (i=top; i<(yuv420_height-buttom); i=i+1) {
            linear_offset = temp1*(i-top);
            for (j=left; j<(yuv420_width-right); j=j+2) {
                tiled_offset = __tile_4x2_read(yuv420_width, yuv420_height, j, i);
                temp4 = j&0x3;
                tiled_offset = tiled_offset+temp4;
                memcpy(yuv420_dest+linear_offset, nv12t_src+tiled_offset, 2);
                linear_offset = linear_offset+2;
            }
        }
    }
}

int _mc_check_out_bytestream (mc_gst_core_t *core, unsigned char *nal, int byte_length)
{
    int ret = MC_ERROR_NONE;
    int stacked_length = 0;
    int nal_length = 0;
    unsigned int syntax = 0;
    int idr = 0;
    int nal_unit_type = 0;
    unsigned char tmp[1000000];
    unsigned char sps[100];
    unsigned char pps[100];

    mc_bitstream_t pstream;

    nal_unit_type = nal[2] == 1 ? (nal[3] & 0x1F) : (nal[4] & 0x1F);

    if ( nal_unit_type == 0x7 || nal_unit_type == 0x8 || nal_unit_type == 0x9 )
    {
        while (1)
        {
            nal_length = __mc_bytestream_to_nal( nal + stacked_length, byte_length - stacked_length, tmp);

            mc_init_bits (&pstream, nal + stacked_length, byte_length - stacked_length );
            mc_read_bits (&pstream, 32, &syntax);
            mc_read_bits (&pstream, 8, &syntax);

            LOGD("stacked_length : %d, byte_length : %d, nal_length : %d",
                stacked_length, byte_length, nal_length);

            switch ( syntax & 0x1F )
            {
                case NAL_SEQUENCE_PARAMETER_SET:
                    LOGD("SPS is found : ", nal_length);
                    core->sps_size = nal_length;
                    memcpy(sps, tmp, nal_length);
                    break;
                case NAL_PICTURE_PARAMETER_SET:
                    LOGD("PPS is found : %d", nal_length);
                    core->pps_size = nal_length;
                    memcpy(pps, tmp, nal_length);
                    break;
                case NAL_SLICE_IDR:
                    LOGD ("IDR is found");
                    idr++;
                    core->idr_count++;
                    break;
                default:
                    LOGD ("%x nal_unit_type is detected");
                    break;
            }

            stacked_length += nal_length;

            if ( stacked_length >= byte_length )
                break;
        }
        if( core->sps_size > 0 && core->pps_size > 0 ) {
            memcpy (core->codec_data, sps, core->sps_size);
            memcpy (core->codec_data+core->sps_size, pps, core->pps_size);
            core->codec_data_size = core->sps_size + core->pps_size;
        }

    }
    else if ( nal_unit_type == 0x5 )
    {
        if ( !core->idr_count )
            core->need_codec_data = true;
        core->idr_count++;
        LOGD("IDR : %d", core->idr_count);
    }
    else
    {
        LOGD("nal_unit_type : %x", nal_unit_type);
    }

    return ret;
}
