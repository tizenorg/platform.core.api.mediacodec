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

#ifndef __TIZEN_MEDIA_CODEC_INI_H__
#define __TIZEN_MEDIA_CODEC_INI_H__

#include <glib.h>
#include <mm_types.h>
#include <media_codec.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_CODEC_INI_DEFAULT_PATH   SYSCONFDIR"/multimedia/mmfw_media_codec.ini"
#define MEDIA_CODEC_INI_MAX_STRLEN     256
#define DEFAULT_PORT "GST_PORT"
#define MEDIA_CODEC_MAX_CODEC_TYPE 100
#define MEDIA_CODEC_MAX_CODEC_ROLE 4

#define MEDICODEC_INI_MAX_ELEMENT 10
#define MEDIA_CODEC_MAX_VIDEO_CODEC 100
#define MEDIA_CODEC_MAX_AUDIO_CODEC 100

typedef struct _codec_list_t codec_list_t;
typedef struct _codec_info_t codec_info_t;
typedef struct _codec_t codec_t;
typedef struct _mc_ini_t mc_ini_t;

struct _codec_list_t {
	gchar cname[MEDIA_CODEC_INI_MAX_STRLEN];
	mediacodec_codec_type_e ctype;
};

typedef enum {
	GST_PORT = 0,
	FFMPEG_PORT,
	CUSTOM_PORT,
} port_mode;

struct _codec_info_t {
	gchar name[MEDIA_CODEC_INI_MAX_STRLEN];
	gchar mime[MEDIA_CODEC_INI_MAX_STRLEN];
	gchar format[MEDIA_CODEC_INI_MAX_STRLEN];
};

struct _codec_t {
	gint codec_id;
	codec_info_t codec_info[MEDIA_CODEC_MAX_CODEC_ROLE];
};


/* @ mark means the item has tested */
struct _mc_ini_t {
	int codec_list;
	port_mode port_type;
	/* general */
	gchar port_name[MEDIA_CODEC_INI_MAX_STRLEN];
	codec_t  codec[MEDIA_CODEC_MAX_CODEC_TYPE];
};

/*Default sink ini values*/
/* General*/

/* NOTE : following content should be same with above default values */
/* FIXIT : need smarter way to generate default ini file. */
/* FIXIT : finally, it should be an external file */
#define MEDIA_CODEC_DEFAULT_INI \
"\
[general] \n\
\n\
;Add general config parameters here\n\
\n\
\n\
\n\
[port_in_use] \n\
\n\
;media_codec_port = GST_PORT \n\
;media_codec_port = FFMPEG_PORT \n\
;media_codec_port = CUSTOM_PORT \n\
media_codec_port = GST_PORT \n\
\n\
[gst_port] \n\
\n\
;Add gst port specific config paramters here\n\
\n\
\n\
[custom_port] \n\
\n\
;Add custom port specific config paramters here\n\
\n\
\n\
\n\
"

int mc_ini_load(mc_ini_t *ini);
media_format_mimetype_e _mc_convert_media_format_str_to_int(char *sformat);


#ifdef __cplusplus
}
#endif
#endif /*__TIZEN_MEDIA_CODEC_INI_H__*/
