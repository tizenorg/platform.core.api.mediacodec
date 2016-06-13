
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

#ifndef __TIZEN_MEDIA_CODEC_PORT_GST_H__
#define __TIZEN_MEDIA_CODEC_PORT_GST_H__

#include <unistd.h>
#include <tizen.h>
#include <media_codec.h>
#include <media_codec_private.h>
#include <media_codec_port.h>
#include <media_codec_bitstream.h>

#include <tbm_type.h>
#include <tbm_surface.h>
#include <tbm_bufmgr.h>
#include <tbm_surface_internal.h>
#include <gst/video/video-format.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GST_INIT_STRUCTURE(param) \
    memset(&(param), 0, sizeof(param));

#define MEDIACODEC_ELEMENT_SET_STATE( x_element, x_state )                                          \
    do {                                                                                            \
        LOGD("setting state [%s:%d] to [%s]\n", #x_state, x_state, GST_ELEMENT_NAME( x_element ) ); \
        if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state ( x_element, x_state) )              \
        {                                                                                           \
            LOGE("failed to set state %s to %s\n", #x_state, GST_ELEMENT_NAME( x_element ));        \
            goto STATE_CHANGE_FAILED;                                                               \
        }                                                                                           \
    } while (0)

#define SCMN_IMGB_MAX_PLANE 4
#define TBM_API_CHANGE
#define DEFAULT_POOL_SIZE 20
#define AAC_CODECDATA_SIZE	16
#define WMA_CODECDATA_SIZE	64
#define VORBIS_CODECDATA_SIZE	4096

/* gst port layer */
typedef struct _mc_gst_port_t mc_gst_port_t;
typedef struct _mc_gst_core_t mc_gst_core_t;
typedef struct _GstMCBuffer GstMCBuffer;

typedef enum {
	BUF_SHARE_METHOD_PADDR = 0,
	BUF_SHARE_METHOD_FD,
	BUF_SHARE_METHOD_TIZEN_BUFFER,
	BUF_SHARE_METHOD_FLUSH_BUFFER
} buf_share_method_t;

#ifdef TIZEN_PROFILE_LITE
struct ion_mmu_data {
	int master_id;
	int fd_buffer;
	unsigned long iova_addr;
	size_t iova_size;
};
#endif

struct _mc_gst_port_t
{
	mc_gst_core_t *core;
	unsigned int num_buffers;
	unsigned int buffer_size;
	unsigned int index;
	bool is_allocated;
	media_packet_h *buffers;
	GQueue *queue;
	GMutex mutex;
	GCond buffer_cond;
};

struct _mc_gst_core_t
{
	int(**vtable)();
	const char *mime;
	gchar *format;
	GstElement *pipeline;
	GstElement *appsrc;
	GstElement *capsfilter;
	GstElement *parser;
	GstElement *fakesink;
	GstElement *codec;
	GstCaps *caps;
	tbm_bufmgr bufmgr;
	int drm_fd;

	GMainContext *thread_default;
	gulong signal_handoff;
	gint bus_whatch_id;
	gint probe_id;

	GMutex eos_mutex;
	GMutex eos_wait_mutex;
	GMutex prepare_lock;
	GMutex drain_lock;
	GCond eos_cond;
	GCond eos_waiting_cond;

	bool output_allocated;
	bool encoder;
	bool video;
	bool is_hw;
	bool eos;
	bool eos_waiting;
	bool codec_config;
	bool need_feed;
	bool need_codec_data;
	bool need_sync_flag;
	bool unprepare_flag;
	unsigned int prepare_count;
	unsigned int num_live_buffers;
	unsigned int etb_count;
	unsigned int ftb_count;

	mediacodec_codec_type_e codec_id;
	media_format_mimetype_e out_mime;
	media_format_h output_fmt;
	mc_gst_port_t *ports[2];
	mc_bitstream_t bits;

	mc_aqueue_t *available_queue;
	GQueue *output_queue;

	void *codec_info;

	void* user_cb[_MEDIACODEC_EVENT_TYPE_NUM];
	void* user_data[_MEDIACODEC_EVENT_TYPE_NUM];
};

struct _GstMCBuffer
{
	GstBuffer *buffer;
	int buf_size;
	mc_gst_core_t* core;
	media_packet_h pkt;
	bool has_imgb;
};

enum { fill_inbuf, fill_outbuf };

int __mc_fill_input_buffer(mc_gst_core_t *core, media_packet_h pkt, GstMCBuffer *buff);
int __mc_fill_output_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt);

int __mc_fill_input_buffer_with_packet(mc_gst_core_t *core, media_packet_h pkt, GstMCBuffer *buff);
int __mc_fill_input_buffer_with_venc_packet(mc_gst_core_t *core, media_packet_h pkt, GstMCBuffer *mc_buffer);

int __mc_fill_packet_with_output_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt);
int __mc_fill_venc_packet_with_output_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *out_pkt);
int __mc_fill_vdec_packet_with_output_buffer(mc_gst_core_t *core, void *data, int size, media_packet_h *pkt);

mc_gst_core_t *mc_gst_core_new();
void mc_gst_core_free(mc_gst_core_t *core);

mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core);
void mc_gst_port_free(mc_gst_port_t *port);

mc_ret_e mc_gst_prepare(mc_handle_t *mc_handle);
mc_ret_e mc_gst_unprepare(mc_handle_t *mc_handle);

mc_ret_e mc_gst_process_input(mc_handle_t *mc_handle, media_packet_h inbuf, uint64_t timeOutUs);
mc_ret_e mc_gst_get_output(mc_handle_t *mc_handle, media_packet_h *outbuf, uint64_t timeOutUs);

mc_ret_e mc_gst_flush_buffers(mc_handle_t *mc_handle);

mc_ret_e mc_gst_get_packet_pool(mc_handle_t *mc_handle, media_packet_pool_h *pkt_pool);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PORT_GST_H__ */
