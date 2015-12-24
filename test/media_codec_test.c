/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <Elementary.h>
#include <appcore-efl.h>
#include <gst/gst.h>

#include <media_codec.h>
#include <media_packet.h>
#include <tbm_surface.h>
#include <dlog.h>
#include <time.h>

#define PACKAGE "media_codec_test"
#define MAX_HANDLE		        4
//#define DUMP_OUTBUF           1
#define TEST_FILE_SIZE	      (10 * 1024 * 1024)
#define MAX_STRING_LEN	      256

#define DEFAULT_SAMPPLERATE   44100
#define DEFAULT_CHANNEL		    2
#define DEFAULT_BIT			      16
#define DEFAULT_BITRATE       128
#define DEFAULT_SAMPLEBYTE	  1024
#define ADTS_HEADER_SIZE      7
#define AMRNB_PCM_INPUT_SIZE      320
#define AMRWB_PCM_INPUT_SIZE      640

#define CHECK_BIT(x, y) (((x) >> (y)) & 0x01)
#define GET_IS_ENCODER(x) CHECK_BIT(x, 0)
#define GET_IS_DECODER(x) CHECK_BIT(x, 1)
#define GET_IS_HW(x) CHECK_BIT(x, 2)
#define ES_DEFAULT_VIDEO_PTS_OFFSET 33000000
#define CHECK_VALID_PACKET(state, expected_state) \
	((state & (expected_state)) == (expected_state))

static int samplerate = DEFAULT_SAMPPLERATE;
static int channel = DEFAULT_CHANNEL;
static int bit = DEFAULT_BIT;
static int bitrate = DEFAULT_BITRATE;
static int samplebyte = DEFAULT_SAMPLEBYTE;
unsigned char buf_adts[ADTS_HEADER_SIZE];

enum {
	MC_EXIST_SPS    = 1 << 0,
	MC_EXIST_PPS    = 1 << 1,
	MC_EXIST_IDR    = 1 << 2,
	MC_EXIST_SLICE  = 1 << 3,

	MC_VALID_HEADER = (MC_EXIST_SPS | MC_EXIST_PPS),
	MC_VALID_FIRST_SLICE = (MC_EXIST_SPS | MC_EXIST_PPS | MC_EXIST_IDR)
};

typedef struct _App App;

enum {
	CURRENT_STATUS_MAINMENU,
	CURRENT_STATUS_FILENAME,
	CURRENT_STATUS_CREATE,
	CURRENT_STATUS_DESTROY,
	CURRENT_STATUS_SET_CODEC,
	CURRENT_STATUS_SET_VDEC_INFO,
	CURRENT_STATUS_SET_VENC_INFO,
	CURRENT_STATUS_SET_ADEC_INFO,
	CURRENT_STATUS_SET_AENC_INFO,
	CURRENT_STATUS_PREPARE,
	CURRENT_STATUS_UNPREPARE,
	CURRENT_STATUS_PROCESS_INPUT,
	CURRENT_STATUS_GET_OUTPUT,
	CURRENT_STATUS_RESET_OUTPUT_BUFFER,
	CURRENT_STATUS_SET_SIZE,
	CURRENT_STATUS_AUTO,
};

typedef enum {
	NAL_SLICE_NO_PARTITIONING = 1,
	NAL_SLICE_PART_A,
	NAL_SLICE_PART_B,
	NAL_SLICE_PART_C,
	NAL_SLICE_IDR,
	NAL_SEI,
	NAL_SEQUENCE_PARAMETER_SET,
	NAL_PICTURE_PARAMETER_SET,
	NAL_PICTURE_DELIMITER,
	NAL_END_OF_SEQUENCE,
	NAL_END_OF_STREAM,
	NAL_FILLER_DATA,
	NAL_PREFIX_SVC = 14
} nal_unit_type;

typedef enum {
	VIDEO_DEC,
	VIDEO_ENC,
	AUDIO_DEC,
	AUDIO_ENC
} type_e;


struct _App {
	GMainLoop *loop;
	guint sourceid;

	GMappedFile *file;
	guint8 *data;
	gsize length;
	guint64 offset;
	guint obj;

	GTimer *timer;
	long start;
	long finish;
	long process_time;
	int frame_count;

	int codecid;
	int flag;
	bool is_video[MAX_HANDLE];
	bool is_encoder[MAX_HANDLE];
	bool hardware;
	type_e type;
	/* video */
	mediacodec_h mc_handle[MAX_HANDLE];
	guint width;
	guint height;
	guint fps;
	guint target_bits;
	media_format_mimetype_e mime;

	/* Audio */
	guint samplerate;
	guint channel;
	guint bit;
	guint bitrate;
	bool is_amr_nb;


	/* Render */
	guint w;
	guint h;
	Evas_Object *win;
	Evas_Object *img;
	media_packet_h packet;
	Ecore_Pipe *pipe;
	GList *packet_list;
	GMutex lock;
};

App s_app;

media_format_h aenc_fmt = NULL;
media_format_h adec_fmt = NULL;
media_format_h vdec_fmt = NULL;
media_format_h venc_fmt = NULL;

#if DUMP_OUTBUF
FILE *fp_out = NULL;
#endif
FILE *fp = NULL;
char filename[100]="/tmp/mc_dump.out";
int allow_dump = 1;

/* Internal Functions */
static int _create_app(void *data);
static int _terminate_app(void *data);
static void displaymenu(void);
static void display_sub_basic();

/* For debugging */
static void mc_hex_dump(char *desc, void *addr, int len);
static void decoder_output_dump(App *app, media_packet_h pkt);

/* */

void (*extractor)(App *app, unsigned char** data, int *size, bool *have_frame);

int g_menu_state = CURRENT_STATUS_MAINMENU;

static int _create_app(void *data)
{
	printf("My app is going alive!\n");
	App *app = (App*)data;

	g_mutex_init(&app->lock);
	return 0;
}

static int _terminate_app(void *data)
{
	printf("My app is going gone!\n");
	App *app = (App*)data;

	g_mutex_clear(&app->lock);
	return 0;
}


struct appcore_ops ops = {
	.create = _create_app,
	.terminate = _terminate_app,
};

static const guint mp3types_bitrates[2][3][16] = {
	{
		{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,},
		{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384,},
		{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,}
	},
	{
		{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256,},
		{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,},
		{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160,}
	},
};

static const guint mp3types_freqs[3][3] = { {44100, 48000, 32000},
	{22050, 24000, 16000},
	{11025, 12000, 8000}
};

void h264_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int nal_length = 0;
	unsigned char val, zero_count;
	unsigned char *pNal = app->data + app->offset;
	int max = app->length - app->offset;
	int i = 0;
	int index = 0;
	int nal_unit_type = 0;
	bool init;
	bool slice;
	bool idr;
	static int state;
	int read;

	zero_count = 0;

	val = pNal[index++];
	while (!val) {
		zero_count++;
		val = pNal[index++];
	}

	zero_count = 0;

	while (1) {
		if (index >= max) {
			read = (index - 1);
			goto DONE;
		}

		val = pNal[index++];

		if (!val)
			zero_count++;
		else {
			if ((zero_count >= 2) && (val == 1))
				break;
			else {
				zero_count = 0;
			}
		}
	}

	if(zero_count > 3)
		zero_count = 3;

	read = (index - zero_count - 1);

	nal_unit_type = *(app->data+app->offset+4) & 0x1F;
	g_print("nal_unit_type : %x\n", nal_unit_type);

	switch ( nal_unit_type )
	{
		case NAL_SEQUENCE_PARAMETER_SET:
			g_print("nal_unit_type : SPS\n");
			state |= MC_EXIST_SPS;
			break;
		case NAL_PICTURE_PARAMETER_SET:
			g_print("nal_unit_type : PPS\n");
			state |= MC_EXIST_PPS;
			break;
		case NAL_SLICE_IDR:
		case NAL_SEI:
			g_print ("nal_unit_type : IDR\n");
			state |= MC_EXIST_IDR;
			break;
		case NAL_SLICE_NO_PARTITIONING:
		case NAL_SLICE_PART_A:
		case NAL_SLICE_PART_B:
		case NAL_SLICE_PART_C:
			state |= MC_EXIST_SLICE;
			break;
		default:
			g_print ("nal_unit_type : %x", nal_unit_type);
			break;
	}

	init = CHECK_VALID_PACKET(state, MC_VALID_FIRST_SLICE) ? 1 :0;
	slice = CHECK_VALID_PACKET(state, MC_EXIST_SLICE) ? 1 :0;
	idr = CHECK_VALID_PACKET(state, MC_EXIST_IDR) ? 1 : 0;
	g_print("status : %d, slice : %d, idr : %d\n", init, slice, idr);

	if (init || idr || slice) {
		*have_frame = TRUE;
		if (init) {
			*data = app->data;
			*size = app->offset + read;
		} else {
			*data = app->data+app->offset;
			*size = read;
		}
		state = 0;
	} else {
		*data = app->data+app->offset;
		*size = read;
		//app->offset += read;
	}
DONE:
	app->offset += read;
}

void nv12_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int yuv_size;
	int offset = app->length - app->offset;

	yuv_size = app->width * app->height * 3 / 2;

	if (offset >= yuv_size)
		*size = offset;

	*have_frame = TRUE;
	*data = app->data + app->offset;

	if (offset >= yuv_size)
		*size = offset;
	else
		*size = yuv_size;
}

void yuv_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int yuv_size;
	int offset = app->length - app->offset;

	yuv_size = app->width * app->height * 3 / 2;

	if (yuv_size >= offset)
		*size = offset;

	*have_frame = TRUE;
	*data = app->data + app->offset;

	if (yuv_size >= offset)
		*size = offset;
	else
		*size = yuv_size;

	app->offset += *size;

}

void aacenc_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int read_size;
	int offset = app->length - app->offset;

	read_size = ((samplebyte*app->channel)*(app->bit/8));


	*have_frame = TRUE;

	if (offset >= read_size)
		*size = offset;
	else
		*size = read_size;

	app->offset += *size;
}

void amrenc_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int read_size;
	int offset = app->length - app->offset;

	if(app->is_amr_nb)
		read_size = AMRNB_PCM_INPUT_SIZE;
	else
		read_size = AMRWB_PCM_INPUT_SIZE;

	*have_frame = TRUE;

	if (offset >= read_size)
		*size = offset;
	else
		*size = read_size;

	app->offset += *size;
}

/**
 * Extract Input data for AAC decoder
 * (case of (LC profile) ADTS format)
 * codec_data : Don't need
 **/
void aacdec_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int read_size;
	int offset = app->length - app->offset;
	unsigned char *pData = app->data + app->offset;

	if ((pData != NULL) && (pData[0] == 0xff) && ((pData[1] & 0xf6) == 0xf0)) {
		read_size = ((pData[3] & 0x03) << 11) | (pData[4] << 3) | ((pData[5] & 0xe0) >> 5);
	} else {
		read_size = 0;
		g_print("[FAIL] Not found aac frame sync.....\n");
	}

	*have_frame = TRUE;
	*data = app->data + app->offset;

	if (read_size >= offset)
		*size = offset;
	else
		*size = read_size;

	app->offset += *size;

}

void mp3dec_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int read_size;
	guint header;
	guint padding, bitrate, lsf, layer, mpg25;
	guint hdr_bitrate, sf;
	int offset = app->length - app->offset;
	unsigned char *pData = app->data + app->offset;

	header = GST_READ_UINT32_BE(pData);


	if (header == 0) {
		g_print ("[ERROR] read header size is 0\n");
		*have_frame = FALSE;
	}

	/* if it's not a valid sync */
	if ((header & 0xffe00000) != 0xffe00000) {
		g_print ("[ERROR] invalid sync\n");
		*have_frame = FALSE;
	}

	if (((header >> 19) & 3) == 0x1) {
		g_print ("[ERROR] invalid MPEG version: %d\n", (header >> 19) & 3);
		*have_frame = FALSE;
	} else {
		if (header & (1 << 20)) {
			lsf = (header & (1 << 19)) ? 0 : 1;
			mpg25 = 0;
		} else {
			lsf = 1;
			mpg25 = 1;
		}
	}

	/* if it's an invalid layer */
	if (!((header >> 17) & 3)) {
		g_print("[ERROR] invalid layer: %d\n", (header >> 17) & 3);
		*have_frame = FALSE;
	} else {
		layer = 4 - ((header >> 17) & 0x3);
	}

	/* if it's an invalid bitrate */
	if (((header >> 12) & 0xf) == 0xf) {
		g_print ("[ERROR] invalid bitrate: %d\n", (header >> 12) & 0xf);
		*have_frame = FALSE;
	} else {
		bitrate = (header >> 12) & 0xF;
		hdr_bitrate = mp3types_bitrates[lsf][layer - 1][bitrate] * 1000;
		/* The caller has ensured we have a valid header, so bitrate can't be zero here. */
		if(hdr_bitrate == 0)
			*have_frame = FALSE;
	}

	/* if it's an invalid samplerate */
	if (((header >> 10) & 0x3) == 0x3) {
		g_print ("[ERROR] invalid samplerate: %d\n", (header >> 10) & 0x3);
		*have_frame = FALSE;
	} else {
		sf = (header >> 10) & 0x3;
		sf = mp3types_freqs[lsf + mpg25][sf];
	}

	padding = (header >> 9) & 0x1;

	switch (layer) {
		case 1:
			read_size = 4 * ((hdr_bitrate * 12) / sf + padding);
			break;
		case 2:
			read_size = (hdr_bitrate * 144) / sf + padding;
			break;
		default:
		case 3:
			read_size = (hdr_bitrate * 144) / (sf << lsf) + padding;
			break;
	}
	g_print("header : %d, read : %d\n", header, read_size);

	*have_frame = TRUE;
	*data = app->data + app->offset;

	if (read_size >= offset)
		*size = offset;
	else
		*size = read_size;

	app->offset += *size;
}
#if 0
void extract_input_aacdec_m4a_test(App *app, unsigned char* data, int *size, bool *have_frame)
{
	int readsize = 0;
	size_t result;
	unsigned int header_size = ADTS_HEADER_SIZE;
	unsigned char buffer[1000000];
	unsigned char codecdata[AAC_CODECDATA_SIZE] = {0,};

	/*
	 * It is not support full parsing MP4 container box.
	 * So It MUST start as RAW valid frame sequence.
	 * Testsuit that are not guaranteed to be available on functionality of all General DEMUXER/PARSER.
	 */

	if (feof(fd))
		return 0;

	if (frame_count == 0)
	{
		/*
		 * CAUTION : Codec data is needed only once  in first time
		 * Codec data is made(or extracted) by MP4 demuxer in 'esds' box.
		 * So I use this data (byte) as hard coding for temporary our testing.
		 */
#if 1
		/*
		 * The codec_data data is according to AudioSpecificConfig,
		 *  ISO/IEC 14496-3, 1.6.2.1
		 *
		 *  below example is test for using "test.aac" or "TestSample-AAC-LC.m4a"
		 * case : M4A - LC profile
		 * codec_data=(buffer)119056e5000000000000000000000000
		 * savs aac decoder get codec_data. size: 16  (Tag size : 5 byte)
		 *     - codec data: profile  : 2
		 *     - codec data: samplrate: 48000
		 *     - codec data: channels : 2
		 */
		/* 2 bytes are mandatory */
		codecdata[0] = 0x11;         //ex) (5bit) 2 (LC) / (4bit) 3 (48khz)
		codecdata[1] = 0x90;         //ex) (4bit) 2 (2ch)
		/* othter bytes are (optional) epconfig information */
		codecdata[2] = 0x56;
		codecdata[3] = 0xE5;
		codecdata[4] = 0x00;
#else
		/*
		 *  below example is test for using "TestSample-EAAC+.m4a"
		 *
		 * case : M4A - HE-AAC v1 and v2 profile
		 * codec_data=(buffer)138856e5a54880000000000000000000
		 * savs aac decoder get codec_data. size: 16  (Tag size : 7 byte)
		 *     - codec data: profile  : 2
		 *     - codec data: samplrate: 22050
		 *     - codec data: channels : 1
		 */
		/* 2 bytes are mandatory */
		codecdata[0] = 0x13;         //ex) (5bit) 2 (LC) / (4bit) 9 (22khz)
		codecdata[1] = 0x88;         //ex) (4bit) 1 (1ch)
		/* othter bytes are (optional) epconfig information */
		codecdata[2] = 0x56;
		codecdata[3] = 0xE5;
		codecdata[4] = 0xA5;
		codecdata[5] = 0x48;
		codecdata[6] = 0x80;
#endif

		memcpy(aacdata, codecdata, AAC_CODECDATA_SIZE);

		result = fread(buffer, 1, header_size, fd);   //adts header
		if(result != header_size)
		{
			return -1;
		}

		if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
			readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
			readsize = readsize -header_size;
			result = fread(buffer, 1, readsize, fd);    //Make only RAW data, so exclude header 7 bytes
			memcpy(aacdata+AAC_CODECDATA_SIZE, buffer, readsize);
		}

		g_print( "[example] Insert 'codec_data' in 1st frame buffer size (%d)\n", readsize+AAC_CODECDATA_SIZE);
		return (readsize + AAC_CODECDATA_SIZE);           //return combination of (codec_data + raw_data)
	}

	result = fread(buffer, 1, header_size, fd);   //adts header
	if(result != header_size)
	{
		exit(1);
	}

	if ((buffer != NULL) && (buffer[0] == 0xff) && ((buffer[1] & 0xf6) == 0xf0)) {
		readsize = ((buffer[3] & 0x03) << 11) | (buffer[4] << 3) | ((buffer[5] & 0xe0) >> 5);
		readsize = readsize -header_size;
		result = fread(buffer, 1, readsize, fd);    //Make only RAW data, so exclude header 7 bytes
		memcpy(aacdata, buffer, readsize);
	} else {
		readsize = 0;
		g_print("[FAIL] Not found aac frame sync.....\n");
	}

	return readsize;            //return only raw_data
}
#endif

/**
 * Extract Input data for AAC encoder
 **/
/*
   void aacenc_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
   {
   int read_size;
   int offset = app->length - app->offset;

   read_size = ((DEFAULT_SAMPLEBYTE*DEFAULT_CHANNEL)*(DEFAULT_BIT/8));

   if (read_size >= offset)
 *size = offset;

 *have_frame = TRUE;
 *data = app->data + app->offset;

 if (read_size >= offset)
 *size = offset;
 else
 *size = read_size;

 app->offset += *size;
 }
 */
static void _mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data)
{
	if (pkt != NULL)
	{
		g_print("Used input buffer = %p\n", pkt);
		media_packet_destroy(pkt);
	}
	return;
}

static int GetChunkSize(App *app)
{
	/* The return values are based on testing trails and are not unique */
	if(app->type==AUDIO_ENC)
		   return 640;
	switch (app->mime)
	{
		case MEDIA_FORMAT_AMR_NB:
		case MEDIA_FORMAT_AMR_WB:
			return 50;

		case MEDIA_FORMAT_WMALSL:
		case MEDIA_FORMAT_WMAPRO:
		case MEDIA_FORMAT_WMAV1:
		case MEDIA_FORMAT_WMAV2:
		case MEDIA_FORMAT_AAC_LC:
		case MEDIA_FORMAT_AAC_HE:
		case MEDIA_FORMAT_AAC_HE_PS:
			return 500;

		case MEDIA_FORMAT_FLAC:
			return 5000;

		case MEDIA_FORMAT_MPEG4_SP:
		case MEDIA_FORMAT_MPEG4_ASP:
		case MEDIA_FORMAT_VC1:
		case MEDIA_FORMAT_VP8:
		case MEDIA_FORMAT_VP9:
			return app->height*app->width/200;
	}
	if(app->type==AUDIO_DEC)
		return samplerate/10;
	return app->height*app->width/50;
}

void common_extractor(App *app, unsigned char **data, int *size, bool *have_frame)
{
	int offset = app->length - app->offset;
	static int chunk_size = 0;

	if(!chunk_size)
		chunk_size =  GetChunkSize(app);

	*have_frame = TRUE;
	*data = app->data + app->offset;
	*size = (chunk_size <= offset) ? chunk_size : offset;

	app->offset += *size;
}

int  _mediacodec_set_codec(int typ, int codecid, int flag, int *hardware)
{
	bool encoder;
	media_format_mimetype_e mime;
	encoder = GET_IS_ENCODER(flag) ? 1 : 0;
	*hardware = GET_IS_HW(flag) ? 1 : 0;

	switch (codecid) {
		case MEDIACODEC_H264:
			if (encoder) {
				extractor = yuv_extractor;
				mime = *hardware ? MEDIA_FORMAT_NV12 : MEDIA_FORMAT_I420;
			} else {
				extractor = h264_extractor;
				mime = MEDIA_FORMAT_H264_SP;
			}
			break;
		case MEDIACODEC_MPEG4:
			if (encoder) {
				extractor = yuv_extractor;
				mime = *hardware ? MEDIA_FORMAT_NV12 : MEDIA_FORMAT_I420;
			} else {
				extractor = h264_extractor;
				mime = MEDIA_FORMAT_MPEG4_SP;
			}
			break;
		case MEDIACODEC_H263:
			if (encoder) {
				extractor = yuv_extractor;
				mime = *hardware ? MEDIA_FORMAT_NV12 : MEDIA_FORMAT_I420;
			} else {
				extractor = h264_extractor;
				mime = MEDIA_FORMAT_H263P;
			}
			break;
		case MEDIACODEC_AAC:
			if (encoder) {
				extractor = aacenc_extractor;
				mime = MEDIA_FORMAT_PCM;
			} else {
				extractor = aacdec_extractor;
				mime = MEDIA_FORMAT_AAC;
			}
			break;
		case MEDIACODEC_AAC_HE:
			if (encoder) {
				extractor = aacenc_extractor;
				mime = MEDIA_FORMAT_PCM;
			} else {
				//extractor = aacdec_extractor;
				mime = MEDIA_FORMAT_AAC_HE;
			}
			break;
		case MEDIACODEC_AAC_HE_PS:
			break;
		case MEDIACODEC_MP3:
			extractor = mp3dec_extractor;
			mime = MEDIA_FORMAT_MP3;
			break;
		case MEDIACODEC_VORBIS:
			break;
		case MEDIACODEC_FLAC:
			break;
		case MEDIACODEC_WMAV1:
			break;
		case MEDIACODEC_WMAV2:
			break;
		case MEDIACODEC_WMAPRO:
			break;
		case MEDIACODEC_WMALSL:
			break;
		case MEDIACODEC_H261:
			mime = MEDIA_FORMAT_H261;
			break;
		case MEDIACODEC_MPEG2:
			mime = MEDIA_FORMAT_MPEG2_SP;
			break;
		case MEDIACODEC_VC1:
			mime = MEDIA_FORMAT_MPEG4_SP;
			break;
		case MEDIACODEC_VP8:
			mime = MEDIA_FORMAT_VP8;
			break;
		case MEDIACODEC_VP9:
			mime = MEDIA_FORMAT_VP9;
			break;
		case MEDIACODEC_HEVC:
			mime = MEDIA_FORMAT_HEVC;
			break;
		default:
			LOGE("NOT SUPPORTED!!!!");
			break;
	}

	{
		/* Temporary Internal function for getting parser status from INI file */
		/* Will be removed after parser testing OR will be moved to API layer */
		int _mc_get_parser_status(void);
		if(_mc_get_parser_status())
		{
			if(typ != VIDEO_ENC)
				extractor = common_extractor;
			else
			{
				extractor = yuv_extractor;
				mime = *hardware ? MEDIA_FORMAT_NV12 : MEDIA_FORMAT_I420;
			}
		}
	}
	return mime;
}

static gboolean read_data(App *app)
{
	guint len;
	bool have_frame = FALSE;
	int ret;
	static guint64 pts = 0L;
	void *buf_data_ptr = NULL;
	media_packet_h pkt = NULL;
	unsigned char *tmp;
	int read;
	int plane_num;
	int offset;
	int stride_width, stride_height;

	g_print("----------read data------------\n");
	//read = extractor(app, app->data + app->offset, app->length - app->offset );
	extractor(app, &tmp, &read, &have_frame);

	if ( app->offset >= app->length - 1 ) {
		/* EOS */
		//media_packet_set_flags(in_buf, MEDIA_PACKET_END_OF_STREAM);
		g_print("EOS\n");
		app->finish = clock();
		g_main_loop_quit (app->loop);
		return FALSE;
	}
	g_print("length : %d, offset : %d\n", app->length, app->offset);

	if ( app->offset + len > app->length )
		len = app->length - app->offset;

	g_print("%p, %d, have_frame :%d, read: %d\n", tmp, (int)read, have_frame, read);

	if (have_frame) {
		if (media_packet_create_alloc(vdec_fmt, NULL, NULL, &pkt) != MEDIA_PACKET_ERROR_NONE) {
			fprintf(stderr, "media_packet_create_alloc failed\n");
			return FALSE;
		}

		if (media_packet_set_pts(pkt, (uint64_t)(pts)) != MEDIA_PACKET_ERROR_NONE) {
			fprintf(stderr, "media_packet_set_pts failed\n");
			return FALSE;
		}

		if (app->type != VIDEO_ENC) {
			media_packet_get_buffer_data_ptr(pkt, &buf_data_ptr);
			media_packet_set_buffer_size(pkt, (uint64_t)read);

			memcpy(buf_data_ptr, tmp, read);
			g_print("tmp:%p, read:%d\n",tmp, read);
		} else {
			/* Y */
			media_packet_get_video_plane_data_ptr(pkt, 0, &buf_data_ptr);
			media_packet_get_video_stride_width(pkt, 0, &stride_width);
			media_packet_get_video_stride_height(pkt, 0, &stride_height);

			offset = stride_width*stride_height;

			memcpy(buf_data_ptr, tmp, offset);

			/* UV or U*/
			media_packet_get_video_plane_data_ptr(pkt, 1, &buf_data_ptr);
			media_packet_get_video_stride_width(pkt, 1, &stride_width);
			media_packet_get_video_stride_height(pkt, 1, &stride_height);
			memcpy(buf_data_ptr, tmp + offset, stride_width*stride_height);

			if (app->hardware == FALSE) {
				/* V */
				media_packet_get_video_plane_data_ptr(pkt, 2, &buf_data_ptr);
				media_packet_get_video_stride_width(pkt, 2, &stride_width);
				media_packet_get_video_stride_height(pkt, 2, &stride_height);

				offset += stride_width * stride_height;


				memcpy(buf_data_ptr, tmp + offset, stride_width*stride_height);
			}
		}
		mc_hex_dump("inbuf", tmp, 48);

		ret = mediacodec_process_input(app->mc_handle[0], pkt, 0);
		if(ret != MEDIACODEC_ERROR_NONE) {
			return FALSE;
		}
		pts += ES_DEFAULT_VIDEO_PTS_OFFSET;
	}
	//app->offset += read;

	return TRUE;
}

static void start_feed(App *app)
{
	if (app->sourceid == 0) {
		//start feeding
		app->sourceid = g_idle_add((GSourceFunc)read_data, app);
		g_print("start_feed\n");
	}
}

static void stop_feed(App *app)
{
	if (app->sourceid != 0) {
		//stop feeding
		g_source_remove(app->sourceid);
		app->sourceid = 0;
		g_print("stop_feed\n");
	}
}

static bool _mediacodec_inbuf_used_cb(media_packet_h pkt, void *user_data)
{
	g_print("_mediacodec_inbuf_used_cb!!!\n");
	media_packet_destroy(pkt);
	return TRUE;
}

static bool _mediacodec_outbuf_available_cb(media_packet_h pkt, void *user_data)
{
	media_packet_h out_pkt = NULL;
	int ret;

	App *app = (App*)user_data;

	g_print("_mediacodec_outbuf_available_cb\n");

	g_mutex_lock(&app->lock);

	ret = mediacodec_get_output(app->mc_handle[0], &out_pkt, 0);
	if (ret != MEDIACODEC_ERROR_NONE) {
		g_print("get_output failed\n");
	}
	decoder_output_dump(app, out_pkt);

#if DUMP_OUTBUF
	void *data;
	uint64_t buf_size;
	int stride_width, stride_height;

	media_packet_get_buffer_data_ptr(out_pkt, &data);
	media_packet_get_buffer_size(out_pkt, &buf_size);
	g_print("output data : %p, size %d\n",data, (int)buf_size);

	fwrite(data, 1, buf_size, fp_out);

#endif

	app->frame_count++;


	g_mutex_unlock(&app->lock);

	media_packet_destroy(out_pkt);
	out_pkt = NULL;
	g_print("done\n");

	return TRUE;
}

static bool _mediacodec_buffer_status_cb(mediacodec_status_e status, void *user_data)
{
	g_print("_mediacodec_buffer_status_cb %d\n", status);

	App *app = (App*)user_data;

	if (status == MEDIACODEC_NEED_DATA) {
		start_feed(app);
	} else if (status == MEDIACODEC_ENOUGH_DATA ){
		stop_feed(app);
	}
	return TRUE;
}

static bool _mediacodec_error_cb(mediacodec_error_e error, void *user_data)
{
	return TRUE;
}

static bool _mediacodec_eos_cb(void *user_data)
{
	return TRUE;
}

static void _mediacodec_prepare(App *app)
{
	int ret;
	media_format_mimetype_e mime;

#if DUMP_OUTBUF
	fp_out = fopen("/tmp/codec_dump.out", "wb");
#endif
	if(allow_dump && filename[0])
	{
		fp = fopen(filename, "wb");
		if(fp == NULL)
			g_print("File %s create failed\n", filename);
		else if(app->type == AUDIO_ENC && app->codecid == MEDIACODEC_AMR_NB)
		{
			const char AMR_header [] = "#!AMR\n";
			fwrite(&AMR_header[0], 1, sizeof(AMR_header)-1, fp);
		}
	}

	/* create instance */
	ret = mediacodec_create(&app->mc_handle[0]);
	if (ret  != MEDIACODEC_ERROR_NONE) {
		g_print("mediacodec_create  failed\n");
		return;
	}

	/* set codec */
	ret = mediacodec_set_codec(app->mc_handle[0], app->codecid, app->flag);
	if (ret  != MEDIACODEC_ERROR_NONE) {
		g_print("mediacodec_set_codec failed\n");
		return;
	}


	app->mime = _mediacodec_set_codec(app->type, app->codecid, app->flag, &app->hardware);

	/* set codec info */
	ret = media_format_create(&vdec_fmt);

	switch (app->type) {
		case VIDEO_DEC:
			ret = mediacodec_set_vdec_info(app->mc_handle[0], app->width, app->height);
			media_format_set_video_mime(vdec_fmt, app->mime);
			media_format_set_video_width(vdec_fmt, app->width);
			media_format_set_video_height(vdec_fmt, app->height);
			break;
		case VIDEO_ENC:
			ret = mediacodec_set_venc_info(app->mc_handle[0], app->width, app->height, app->fps, app->target_bits);
			media_format_set_video_mime(vdec_fmt, app->mime);
			media_format_set_video_width(vdec_fmt, app->width);
			media_format_set_video_height(vdec_fmt, app->height);
			media_format_set_video_avg_bps(vdec_fmt, app->target_bits);
			break;
		case AUDIO_DEC:
			ret = mediacodec_set_adec_info(app->mc_handle[0], app->samplerate, app->channel, app->bit);
			media_format_set_audio_mime(vdec_fmt, app->mime);
			media_format_set_audio_channel(vdec_fmt, app->channel);
			media_format_set_audio_samplerate(vdec_fmt, app->samplerate);
			media_format_set_audio_bit(vdec_fmt, app->bit);
			break;
		case AUDIO_ENC:
			ret = mediacodec_set_aenc_info(app->mc_handle[0], app->samplerate, app->channel, app->bit, app->bitrate);
			media_format_set_audio_mime(vdec_fmt, app->mime);
			media_format_set_audio_channel(vdec_fmt, app->channel);
			media_format_set_audio_samplerate(vdec_fmt, app->samplerate);
			media_format_set_audio_bit(vdec_fmt, app->bit);
			break;
		default:
			g_print("invaild type\n");
			break;
	}
	if (ret  != MEDIACODEC_ERROR_NONE) {
		g_print("mediacodec_set_xxxc(%d)_info failed\n", app->type);
		return;
	}


	/* set callback */
	mediacodec_set_input_buffer_used_cb(app->mc_handle[0], _mediacodec_inbuf_used_cb, NULL);
	mediacodec_set_output_buffer_available_cb(app->mc_handle[0], _mediacodec_outbuf_available_cb, app);
	mediacodec_set_buffer_status_cb(app->mc_handle[0], _mediacodec_buffer_status_cb, app);
	mediacodec_set_eos_cb(app->mc_handle[0],_mediacodec_eos_cb, NULL);
	mediacodec_set_error_cb(app->mc_handle[0], _mediacodec_error_cb, NULL);


	/* prepare */
	ret = mediacodec_prepare(app->mc_handle[0]);
	if (ret  != MEDIACODEC_ERROR_NONE) {
		g_print("mediacodec_prepare failed\n");
		return;
	}

	app->frame_count = 0;
	app->start = clock();
	g_main_loop_run(app->loop);

	g_print("Average FPS = %3.3f\n", ((double)app->frame_count*1000000/(app->finish - app->start)));

	g_print("---------------------------\n");

	if(fp)
		fclose(fp);

	return;
}
/**
 *
 *
 * @param filename
 */

static void input_filepath(char *filename, App *app)
{
	GError *error = NULL;

	app->obj++;
	app->file = g_mapped_file_new (filename, FALSE, &error);
	if (error) {
		g_print ("failed to open file : %s\n", error->message);
		g_error_free(error);
		return -1;
	}

	app->length = g_mapped_file_get_length (app->file);
	app->data = (guint8 *) g_mapped_file_get_contents (app->file);
	app->offset = 0;
	g_print("len : %d, offset : %d, obj : %d", app->length, (int)app->offset, app->obj);

	/*
	   if ( mediacodec_create(&app->mc_handle[app->obj]) != MEDIACODEC_ERROR_NONE) {
	   g_print("mediacodec_create failed\n");
	   }
	   */
#if 0
	if(len < 0 || len > MAX_STRING_LEN)
		return;

	for(i = 0; i < g_handle_num; i++)
	{
		if(g_media_codec[i] != NULL)
		{
			mediacodec_unprepare(g_media_codec[i]);
			mediacodec_destroy(g_media_codec[i]);
			g_media_codec[i] = NULL;
		}

		if (mediacodec_create(&g_media_codec[i]) != MEDIACODEC_ERROR_NONE)
		{
			g_print("mediacodec create is failed\n");
		}
	}
	//input_fmt = (media_format_s *) malloc(sizeof(media_format_s));
	//memset(input_fmt, 0, sizeof(media_format_s));
	media_format_create(&input_fmt);

#if DUMP_OUTBUF
	fp_out = fopen("/opt/usr/media/codec_dump.out", "wb");
#endif

	strncpy (g_uri, filename, len);
#endif
	return;
}

void quit_program()
{
	//g_main_loop_quit (app->loop);
#if DUMP_OUTBUF
	if (fp_out)
		fclose(fp_out);
#endif

}

void reset_menu_state()
{
	g_menu_state = CURRENT_STATUS_MAINMENU;
	return;
}

void auto_test(int n);
void print_list();
void _interpret_main_menu(char *cmd, App *app)
{
	int len =  strlen(cmd);
	if (len == 1)
	{
		if (strncmp(cmd, "a", 1) == 0) {
			g_menu_state = CURRENT_STATUS_FILENAME;
		} else if (strncmp(cmd, "o", 1) == 0) {
			g_menu_state = CURRENT_STATUS_GET_OUTPUT;
		} else if (strncmp(cmd, "z", 1) == 0) {
			g_menu_state = CURRENT_STATUS_AUTO;
		} else if (strncmp(cmd, "q", 1) == 0)
		{
			quit_program();
			//g_main_loop_quit (app->loop);
		} else {
			g_print("unknown menu \n");
		}
	} else if (len == 2) {
		if (strncmp(cmd, "pr", 2) == 0) {
			_mediacodec_prepare(app);
		} else if (strncmp(cmd, "sc", 2) == 0) {
			g_menu_state = CURRENT_STATUS_SET_CODEC;
		} else if (strncmp(cmd, "vd", 2) == 0) {
			g_menu_state = CURRENT_STATUS_SET_VDEC_INFO;
		} else if (strncmp(cmd, "ve", 2) == 0) {
			g_menu_state = CURRENT_STATUS_SET_VENC_INFO;
		} else if (strncmp(cmd, "ad", 2) == 0) {
			g_menu_state = CURRENT_STATUS_SET_ADEC_INFO;
		} else if (strncmp(cmd, "ae", 2) == 0) {
			g_menu_state = CURRENT_STATUS_SET_AENC_INFO;
		} else if (strncmp(cmd, "pi", 2) == 0) {
			g_menu_state = CURRENT_STATUS_PROCESS_INPUT;
		} else if (strncmp(cmd, "rb", 2) == 0) {
			//_mediacodec_reset_output_buffer();
		} else if (strncmp(cmd, "pa", 2) == 0) {
			//_mediacodec_process_all();
		} else if (strncmp(cmd, "un", 2) == 0) {
			//_mediacodec_unprepare();
		} else if (strncmp(cmd, "dt", 2) == 0) {
			//_mediacodec_destroy();
		} else {
			//g_print("unknown menu \n");
			display_sub_basic();
		}
	} else {
		g_print("unknown menu \n");
	}

	return;
}

static void displaymenu(void)
{
	if (g_menu_state == CURRENT_STATUS_MAINMENU)
	{
		display_sub_basic();
	}
	else if (g_menu_state == CURRENT_STATUS_FILENAME)
	{
		g_print("*** input mediapath.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SET_CODEC)
	{
		g_print("*** Codec id : Select Codec ID Numbe  (e.g. AAC_LC = 96)\n");
		g_print("               L16    =  16 (0x10)\n");
		g_print("               ALAW   =  32 (0x20)\n");
		g_print("               ULAW   =  48 (0x30)\n");
		g_print("               AMR_NB =  64 (0x40)\n");
		g_print("               AMR_WB =  65 (0x41)\n");
		g_print("               G729   =  80 (0x50)\n");
		g_print("               AAC_LC =  96 (0x60)\n");
		g_print("               AAC_HE =  97 (0x61)\n");
		g_print("               AAC_PS =  98 (0x62)\n");
		g_print("               MP3    = 112 (0x70)\n");
		g_print("               VORBIS = 128 (0x80)\n");
		g_print("               FLAC   = 144 (0x90)\n");
		g_print("               WMAV1  = 160 (0xA0)\n");
		g_print("               WMAV2  = 161 (0xA1)\n");
		g_print("               WMAPRO = 162 (0xA2)\n");
		g_print("               WMALSL = 163 (0xA3)\n");
		g_print("               -------------------\n");
		g_print("               H261   = 101\n");
		g_print("               H263   = 102\n");
		g_print("               H264   = 103\n");
		g_print("               MJPEG  = 104\n");
		g_print("               MPEG1  = 105\n");
		g_print("               MPEG2  = 106\n");
		g_print("               MPEG4  = 107\n");
		g_print("               HEVC   = 108\n");
		g_print("               VP8    = 109\n");
		g_print("               VC1    = 111\n");
		g_print("               -------------------\n");
		g_print("*** Flags : Select Combination Number (e.g. DEOCDER + TYPE_SW = 10)\n");
		g_print("               CODEC : ENCODER =  1       DECODER =  2\n");
		g_print("               TYPE  : HW      =  4       SW      =  8\n");
		g_print("*** input codec id, falgs.\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SET_VDEC_INFO)
	{
		g_print("*** input video decode configure.(width, height)\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SET_VENC_INFO)
	{
		g_print("*** input video encode configure.(width, height, fps, target_bits)\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SET_ADEC_INFO)
	{
		g_print("*** input audio decode configure.(samplerate, channel, bit (e.g. 48000,  2, 16))\n");
	}
	else if (g_menu_state == CURRENT_STATUS_SET_AENC_INFO)
	{
		g_print("*** input audio encode configure.(samplerate, channel, bit, bitrate (e.g. 48000,  2, 16, 128000))\n");
	}
	else if (g_menu_state == CURRENT_STATUS_PROCESS_INPUT)
	{
		g_print("*** input dec process number\n");
	}
	else if (g_menu_state == CURRENT_STATUS_GET_OUTPUT)
	{
		g_print("*** input get output buffer number\n");
	}
	else if (g_menu_state == CURRENT_STATUS_AUTO)
	{
		print_list();
		g_print("*** get test number\n");
	}
	else
	{
		g_print("*** unknown status.\n");
		//quit_program(app);
	}
	g_print(" >>> ");
}

gboolean timeout_menu_display(void* data)
{
	displaymenu();
	return FALSE;
}


static void interpret (char *cmd, App *app)
{
	switch (g_menu_state)
	{
		case CURRENT_STATUS_MAINMENU:
			{
				_interpret_main_menu(cmd, app);
			}
			break;
		case CURRENT_STATUS_FILENAME:
			{
				input_filepath(cmd, app);
				reset_menu_state();
			}
			break;
		case CURRENT_STATUS_SET_CODEC:
			{
				int tmp;
				static int cnt = 0;
				char **ptr = NULL;
				switch (cnt)
				{
					case 0:
						tmp = atoi(cmd);

						if (tmp == 110 || tmp == 111)               /* 0x20A0 to 0x20B0 */
							app->codecid = 0x20A0 + 0x10*(tmp-110);
						else if(tmp > 100 &&
								(tmp != 112) &&
								(tmp != 128) &&
								(tmp != 144) &&
								(tmp != 160) && (tmp != 161) && (tmp != 162) && (tmp != 163))     //Temporary
						{
							tmp = strtol(cmd, ptr, 16);
							app->codecid = 0x2000 + ((tmp & 0xFF) << 4);
						} else {
							app->codecid = 0x1000 + tmp;
						}
						cnt++;
						break;
					case 1:
						app->flag = atoi(cmd);
						cnt = 0;
						reset_menu_state();
						break;
					default:
						break;
				}
			}
			break;
		case CURRENT_STATUS_SET_VDEC_INFO:
			{
				static int cnt = 0;
				switch (cnt)
				{
					case 0:
						app->width = atoi(cmd);
						cnt++;
						break;
					case 1:
						app->height = atoi(cmd);
						app->type = VIDEO_DEC;

						reset_menu_state();
						cnt = 0;
						break;
					default:
						break;
				}
			}break;
		case CURRENT_STATUS_SET_VENC_INFO:
			{
				static int cnt = 0;
				switch (cnt) {
					case 0:
						app->width = atoi(cmd);
						cnt++;
						break;
					case 1:
						app->height = atoi(cmd);
						cnt++;
						break;
					case 2:
						app->fps = atol(cmd);
						cnt++;
						break;
					case 3:
						app->target_bits = atoi(cmd);
						app->type = VIDEO_ENC;

						reset_menu_state();
						cnt = 0;
						break;
					default:
						break;
				}
			}
			break;
		case CURRENT_STATUS_SET_ADEC_INFO:
			{
				static int cnt = 0;
				switch (cnt)
				{
					case 0:
						app->samplerate = atoi(cmd);
						cnt++;
						break;
					case 1:
						app->channel = atoi(cmd);
						cnt++;
						break;
					case 2:
						app->bit = atoi(cmd);
						app->type = AUDIO_DEC;

						reset_menu_state();
						cnt = 0;
						break;
					default:
						break;
				}
			}break;
		case CURRENT_STATUS_SET_AENC_INFO:
			{
				static int cnt = 0;
				switch (cnt)
				{
					case 0:
						app->samplerate = atoi(cmd);
						cnt++;
						break;
					case 1:
						app->channel = atoi(cmd);
						cnt++;
						break;
					case 2:
						app->bit = atoi(cmd);
						cnt++;
						break;
					case 3:
						app->bitrate = atoi(cmd);
						app->type = AUDIO_ENC;

						reset_menu_state();
						cnt = 0;
						break;
					default:
						break;
				}
			}break;
		case CURRENT_STATUS_PROCESS_INPUT:
			{
				static int num = 0;
				num = atoi(cmd);
				reset_menu_state();
			}
			break;
		case CURRENT_STATUS_GET_OUTPUT:
			{
				static int num = 0;
				allow_dump = num = atoi(cmd);
				reset_menu_state();
			}
			break;
		case CURRENT_STATUS_AUTO:
			{
				int num = atoi(cmd);
				auto_test(num);
				reset_menu_state();
			}
			break;
		default:
			break;
	}

	g_timeout_add(100, timeout_menu_display, 0);
}

static void display_sub_basic()
{
	g_print("\n");
	g_print("=========================================================================================\n");
	g_print("                                    media codec test\n");
	g_print("-----------------------------------------------------------------------------------------\n");
	g_print("a. Create \t\t");
	g_print("sc. Set codec \n");
	g_print("vd. Set vdec info \t");
	g_print("ve. Set venc info \n");
	g_print("ad. Set adec info \t");
	g_print("ae. Set aenc info \n");
	g_print("pr. Prepare \t\t");
	g_print("pi. Process input \n");
	g_print("o. Get output \t\t");
	g_print("rb. Reset output buffer \n");
	g_print("pa. Process all frames \t");
	g_print("un. Unprepare\n");
	g_print("dt. Destroy \t\t");
	g_print("q. quite test suite \n");
	g_print("\n");
	g_print("=========================================================================================\n");
}

gboolean input(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	gchar buf[MAX_STRING_LEN];
	gsize read;
	GError *error = NULL;
	App *context = (App*)data;

	g_io_channel_read_chars(channel, buf, MAX_STRING_LEN, &read, &error);
	buf[read] = '\0';
	g_strstrip(buf);
	interpret(buf, context);

	return TRUE;
}

int main(int argc, char *argv[])
{
	App *app = &s_app;

	GError *error = NULL;
	GIOChannel *stdin_channel;
	stdin_channel = g_io_channel_unix_new(0);
	g_io_channel_set_flags(stdin_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch(stdin_channel, G_IO_IN, (GIOFunc)input, app);


	app->loop = g_main_loop_new(NULL, TRUE);
	app->timer = g_timer_new();

	displaymenu();

	ops.data = app;

	return appcore_efl_main(PACKAGE, &argc, &argv, &ops);
}



void mc_hex_dump(char *desc, void *addr, int len)
{
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	if (desc != NULL)
		printf("%s:\n", desc);

	for (i = 0; i < len; i++) {

		if ((i % 16) == 0) {
			if (i != 0)
				printf("  %s\n", buff);

			printf("  %04x ", i);
		}

		printf(" %02x", pc[i]);

		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}
	printf("  %s\n", buff);
}

/**
 *  Add ADTS header at the beginning of each and every AAC packet.
 *  This is needed as MediaCodec encoder generates a packet of raw AAC data.
 *  Note the packetLen must count in the ADTS header itself.
 **/
void add_adts_header_for_aacenc(unsigned char *buffer, int packetLen) {
	int profile = 2;	//AAC LC (0x01)
	int freqIdx = 3;	//48KHz (0x03)
	int chanCfg = 2;	//CPE (0x02)

	if (samplerate == 96000) freqIdx = 0;
	else if (samplerate == 88200) freqIdx = 1;
	else if (samplerate == 64000) freqIdx = 2;
	else if (samplerate == 48000) freqIdx = 3;
	else if (samplerate == 44100) freqIdx = 4;
	else if (samplerate == 32000) freqIdx = 5;
	else if (samplerate == 24000) freqIdx = 6;
	else if (samplerate == 22050) freqIdx = 7;
	else if (samplerate == 16000) freqIdx = 8;
	else if (samplerate == 12000) freqIdx = 9;
	else if (samplerate == 11025) freqIdx = 10;
	else if (samplerate == 8000) freqIdx = 11;

	if ((channel == 1) || (channel == 2))
		chanCfg = channel;

	// fill in ADTS data
	buffer[0] = (char)0xFF;
	buffer[1] = (char)0xF1;
	buffer[2] = (char)(((profile-1)<<6) + (freqIdx<<2) +(chanCfg>>2));
	buffer[3] = (char)(((chanCfg&3)<<6) + (packetLen>>11));
	buffer[4] = (char)((packetLen&0x7FF) >> 3);
	buffer[5] = (char)(((packetLen&7)<<5) + 0x1F);
	buffer[6] = (char)0xFC;
}

static void decoder_output_dump(App *app, media_packet_h pkt)
{
	unsigned char *temp;
	int i = 0;
	int stride_width, stride_height;
	int ret = 0;

	if(fp == NULL)
		return;

	if(app->type != VIDEO_DEC)
	{
		void *data = NULL;
		uint64_t buf_size = 0;

		media_packet_get_buffer_size(pkt, &buf_size);
		media_packet_get_buffer_data_ptr(pkt, &data);

		if(app->type == AUDIO_ENC && app->codecid == MEDIACODEC_AAC)
		{
			/* This is used only AAC encoder case for adding each ADTS frame header */
			add_adts_header_for_aacenc(buf_adts, (buf_size+ADTS_HEADER_SIZE));
			fwrite(&buf_adts[0], 1, ADTS_HEADER_SIZE, fp);
		}
		fwrite(data, 1, buf_size, fp);
		fflush(fp);
		g_print("other than video dec output dumped\n");
		return;
	}

	media_packet_get_video_plane_data_ptr(pkt, 0, &temp);
	media_packet_get_video_stride_width(pkt, 0, &stride_width);
	media_packet_get_video_stride_height(pkt, 0, &stride_height);

	for (i = 0; i < app->height; i++) {
		ret = fwrite(temp, app->width, 1, fp);
		temp += stride_width;
	}

	if (app->hardware == TRUE) {
		media_packet_get_video_plane_data_ptr(pkt, 1, &temp);
		media_packet_get_video_stride_width(pkt, 1, &stride_width);
		for (i = 0; i < app->height/2; i++) {
			ret = fwrite(temp, app->width, 1, fp);
			temp += stride_width;
		}
	} else {
		media_packet_get_video_plane_data_ptr(pkt, 1, &temp);
		media_packet_get_video_stride_width(pkt, 1, &stride_width);
		for (i = 0; i < app->height/2; i++) {
			ret = fwrite(temp, app->width/2, 1, fp);
			temp += stride_width;
		}

		media_packet_get_video_plane_data_ptr(pkt, 2, &temp);
		media_packet_get_video_stride_width(pkt, 2, &stride_width);
		for (i = 0; i < app->height/2; i++) {
			ret = fwrite(temp, app->width/2, 1, fp);
			temp += stride_width;
		}
	}
	fflush(fp);
	g_print("video decoder output dumped!!%d\n", ret);
}

#define BASE_DIR                "./"
#define T(a)                    a,#a
struct aa {
	int use_encoder;
	int use_video;
	int use_hw;
	int codec;
	char* codec_str;
	int width, height;
	char* path;
}list[] =
{
	{0, 1, 1, T(MEDIACODEC_H264), 1280, 544, "Simpsons.h264"},
	{0, 1, 1, T(MEDIACODEC_H263), 352, 288, "sample_352x288.h263"},
	{0, 1, 1, T(MEDIACODEC_MPEG4), 352, 288, "sample_352x288.mpeg4"},

	{0, 1, 0, T(MEDIACODEC_H264),  1280, 544, "Simpsons.h264"},
	{0, 1, 0, T(MEDIACODEC_H263),  352, 288, "sample_352x288.h263"},
	{0, 1, 0, T(MEDIACODEC_MPEG4),  352, 288, "sample_352x288.mpeg4"},
	{0, 1, 0, T(MEDIACODEC_MPEG2),  352, 288, "sample_352x288.mpeg2"},
	{0, 1, 0, T(MEDIACODEC_H261),  352, 288, "sample_352x288.h261"},

	{1, 1, 0, T(MEDIACODEC_H261), 352, 288, "I420_352x288.yuv"},
	{1, 1, 0, T(MEDIACODEC_H263), 352, 288, "I420_352x288.yuv"},
	{1, 1, 0, T(MEDIACODEC_MPEG4), 352, 288, "I420_352x288.yuv"},
	{1, 1, 0, T(MEDIACODEC_MPEG2), 352, 288, "I420_352x288.yuv"},
	{1, 1, 0, T(MEDIACODEC_HEVC), 352, 288, "I420_352x288.yuv"},
	{1, 1, 0, T(MEDIACODEC_VP8), 352, 288, "I420_352x288.yuv"},

	{1, 1, 1, T(MEDIACODEC_H264), 352, 288, "I420_352x288.yuv"},
	{1, 1, 1, T(MEDIACODEC_H263), 352, 288, "I420_352x288.yuv"},
	{1, 1, 1, T(MEDIACODEC_MPEG4), 352, 288, "I420_352x288.yuv"},

	{0, 0, 0, T(MEDIACODEC_AAC_LC),  44100, 2, "SampleAAC.aac"},
	{0, 0, 0, T(MEDIACODEC_AMR_NB),  8000, 1, "SampleAMR-NB.amr"},
	{0, 0, 0, T(MEDIACODEC_AMR_WB),  16000, 1, "SampleAMR-WB.amr"},
	{0, 0, 0, T(MEDIACODEC_MP3),  44100, 2, "SampleMP3.mp3"},

	{1, 0, 0, T(MEDIACODEC_AAC_LC), 44100, 1, "aac.pcm"},	//F32LE
	{1, 0, 0, T(MEDIACODEC_AMR_NB), 8000, 1, "s16le.pcm"},	// S16LE
};

void print_list()
{
	int i;
	char str[200];
	printf("AUTO TESTS --------->\n");
	for(i=0 ; i<sizeof(list)/sizeof(list[0]) ; ++i)
	{
		if(list[i].use_video)
			snprintf(str,sizeof(str),"[%%2d] %%s VIDEO %%s | %%-6s | Width=%%-5d Height=%%-4d | File=%%s [%%d]\n");
		else
			snprintf(str,sizeof(str),"[%%2d] %%s AUDIO %%s | %%-6s | SRate=%%-5d Chanel=%%-4d | File=%%s [%%d]\n");
		printf(str, i+1, list[i].use_hw ? "HW" : "SW", list[i].use_encoder ? "Encoder" : "Decoder",
						  &list[i].codec_str[11], list[i].width, list[i].height, list[i].path, i+1);
	}
	printf("------------------->\n");
}

void auto_test(int n)
{
	App* app = &s_app;
	static char* extn[] = {"yuv", "venc", "pcm", "aenc"};

	if(n<1 || n>sizeof(list)/sizeof(list[0]))
		return;
	n--;
	app->type = 0;
	if(!list[n].use_video)
		app->type += 2;
	if(list[n].use_encoder)
		app->type++;

	snprintf(filename, sizeof(filename), BASE_DIR"%s", list[n].path);
	input_filepath(filename,app);
	app->codecid = list[n].codec;
	app->flag = 10 - (4*list[n].use_hw) - list[n].use_encoder;
	app->width = list[n].width, app->height = list[n].height;
	app->fps = 30.0; app->target_bits = 24;
	app->bit = 16;
	app->samplerate = app->width, app->channel = app->height;

	snprintf(filename, sizeof(filename), "/tmp/mc_%s_%d_%s.%s", list[n].codec_str, n+1, list[n].path, extn[app->type]);
}

