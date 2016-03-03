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
#include <media_backend.h>
#include <media_codec_port_gst.h>
#include <stdlib.h>
#include <dlog.h>

media_codec_backend media_codec_backend_alloc(void)
{
	media_codec_backend mc_backend;

	mc_backend = calloc(1, sizeof(struct _media_codec_backend));
	if (!mc_backend)
		return NULL;

	return mc_backend;
}

void media_codec_backend_free(media_codec_backend backend)
{
	if (!backend)
		return;

	free(backend);
	backend = NULL;
}

int media_codec_backend_init(mediacodec_mgr mcmgr, media_codec_backend mc_backend)
{

	if (!mcmgr) {
		LOGE("fail to init media codec backend... mcmgr is null");
		return 0;
	}

	if (!mc_backend) {
		LOGE("fail to init media codec backend... mc_backend is null");
		return 0;
	}
	mcmgr->backend = mc_backend;

	return 1;
}
