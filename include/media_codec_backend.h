/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TIZEN_MEDIA_CODEC_BACKEND_H__
#define __TIZEN_MEDIA_CODEC_BACKEND_H__

#include <media_codec.h>
#include <mm_types.h>
#include <media_packet.h>
//#include <media_codec_port_gst.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file media_backend.h
 * \brief backend header for Media codec Module
 *   This header is for the implementation of the Mediacodec backend module.
*/

typedef struct _media_codec_backend media_codec_backend;

typedef int (*ModuleInitProc) (mc_backend *, int);


//#define MODULEINITPPROTO(func) int func(mediacodec_mgr, int) /* prototype for init symbol of bakcend module */
#define MODULEINITPPROTO(func) int func(mc_backend *, int) /* prototype for init symbol of bakcend module */

/**
 * @brief media codec module data
 *  data type for the entry point of the backend module
*/

typedef struct {
	ModuleInitProc init;		/* init function of a backend module */
} MEDIACODECModuleData;

media_codec_backend *media_codec_backend_alloc(void);
void media_codec_backend_free(media_codec_backend *backend);
//int media_codec_backend_init(mediacodec_mgr mc_mgr, media_codec_backend backend);
//int media_codec_backend_init(media_codec_backend backend);

#ifdef __cplusplus
}
#endif
#endif	/* __TIZEN_MEDIA_CODEC_BACKEND_H__ */
