#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "gst_media_parser.h"


#define MAX_PATH 1024
#define DEFAULT_MAX_FRAME 100;
#define PARSER_OBJECT_ID 0x75ade23

typedef struct __media_parser_t * media_parser_handle;
typedef struct __media_parser_t media_parser_t;

enum __media_parser_state
{
    PARSER_STATE_INVALID=-1,
    PARASER_STATE_ALLOCATED,
    PARSER_STATE_INITIALIZED,
    PARSER_STATE_PIPELINE_CREATED,
    PARSER_STATE_PARSING,
    PARSER_STATE_PAUSED,
    PARSER_STATE_COMPLETED,
    PARSER_STATE_UNINITIALIZED,
};

struct __media_parser_t
{
    int objectType;
    GstElement *pipeline;
    int maxFrame;
    int parserState;
    char strPipeline[MAX_PATH];
    GQueue* qParserdFrames;
    GMutex* mMutex;
    int pipelineModified;
	GThread *listenerThread;
	GCond *cond_var;
	GMutex *cond_mutex;
	int stopThread;
	int frameCount;
	MediaParserOnFrameCallback cb_frame;
	MediaParserOnEosCallback cb_eos;
	void* cb_frame_userdata;
	void* cb_eos_userdata;
};

static void cb_frame_parsed(GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data)
{
	media_parser_handle objParser = (media_parser_handle) data;

	if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return;

	g_mutex_lock(objParser->mMutex);

	if(objParser->cb_frame) {
		objParser->cb_frame(buffer, objParser->cb_frame_userdata);
	} else {
		++objParser->frameCount;
		g_queue_push_tail(objParser->qParserdFrames, gst_buffer_ref(buffer));
	}
	if(objParser->frameCount == objParser->maxFrame) {
		objParser->stopThread = TRUE;
		objParser->parserState = PARSER_STATE_PAUSED;
		if(objParser->listenerThread) {
			g_mutex_lock(objParser->cond_mutex);
			g_cond_signal(objParser->cond_var);
			g_mutex_unlock(objParser->cond_mutex);
			g_thread_join(objParser->listenerThread);
			objParser->listenerThread = NULL;
		}
	}
	g_mutex_unlock(objParser->mMutex);
}

static gboolean cb_bus_message(GstBus *bus, GstMessage *msg, gpointer data)
{
	media_parser_handle objParser = (media_parser_handle) data;

	if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return TRUE;

	switch(GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");

	  g_mutex_lock(objParser->mMutex);
	  objParser->parserState = PARSER_STATE_COMPLETED;
	  if(objParser->cb_eos) {
		  objParser->cb_eos(objParser->cb_eos_userdata);
	  }
	  if(objParser->listenerThread) {
		  g_mutex_lock(objParser->cond_mutex);
		  g_cond_signal(objParser->cond_var);
		  g_mutex_unlock(objParser->cond_mutex);
		  g_thread_join(objParser->listenerThread);
		  objParser->listenerThread = NULL;
	  }

	  g_mutex_unlock(objParser->mMutex);

      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      break;
    }
    default:
      break;
  }
	return TRUE;
}

static void* frame_parse_listener(void * data)
{
	media_parser_handle objParser = (media_parser_handle) data;

	g_mutex_lock(objParser->cond_mutex);
	while(objParser->parserState == PARSER_STATE_PARSING)
		g_cond_wait(objParser->cond_var, objParser->cond_mutex);
	g_mutex_unlock(objParser->cond_mutex);

	/*
	g_mutex_lock(objParser->mMutex);
	objParser->parserState = PARSER_STATE_PAUSED;
	g_mutex_unlock(objParser->mMutex);
	*/

	g_thread_exit(data);

}

static GstElement* mc_find_element_by_factoryname(GstBin *bin, char* elementName)
{
	GList *elements = NULL;
	GstElement *fakesink = NULL;
	GstElementFactory *factory = NULL;
	gboolean found = FALSE;

	elements = bin->children;
	while(elements) {
		fakesink = GST_ELEMENT (elements->data);
		factory = gst_element_get_factory(fakesink);
		g_print("\n [%s]", GST_ELEMENT_NAME(factory));
		if(strncmp(elementName, GST_ELEMENT_NAME(factory), 8) == 0) {
			found = TRUE;
			break;
		}
		elements = g_list_next(elements);
	}
	if(!found) {
		g_print("\nError: unable to find fakesink element in pipeline.");
		fakesink = NULL;
	}
	return fakesink;
}

int mc_media_parser_create (MediaParserHandle *parser)
{
    media_parser_handle objParser;
    if(parser == NULL) {
        return MEDIA_PARSER_ERROR_INVALID_PARAM;
    }

    objParser = (media_parser_handle) malloc (sizeof(media_parser_t));
    if(objParser == NULL) {
        g_print("\n[%s]:[%d] memory alloc failed.",__FUNCTION__,__LINE__);
        return MEDIA_PARSER_ERROR_ALLOC_FAILED;
    }

    objParser->objectType = PARSER_OBJECT_ID;
    objParser->parserState = PARSER_STATE_INITIALIZED;
    objParser->pipeline = NULL;
    memset(objParser->strPipeline,0,sizeof(objParser->strPipeline));
    objParser->maxFrame = DEFAULT_MAX_FRAME;
    objParser->qParserdFrames = g_queue_new();
    objParser->mMutex = g_mutex_new();
    objParser->pipelineModified = FALSE;
	objParser->listenerThread = NULL;
	objParser->cond_var = g_cond_new();
	objParser->cond_mutex = g_mutex_new();
	objParser->stopThread = FALSE;
	objParser->frameCount = 0;
	objParser->cb_frame = NULL;
	objParser->cb_eos = NULL;
	objParser->cb_frame_userdata = NULL;
	objParser->cb_eos_userdata = NULL;

    *parser = objParser;
    return MEDIA_PARSER_ERROR_NONE;

}

int mc_media_parser_destroy (MediaParserHandle *parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) *parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	g_mutex_lock(objParser->mMutex);

	*parser = NULL;
	if(objParser->pipeline) {
		if(objParser->parserState == PARSER_STATE_PARSING ||
			objParser->parserState == PARSER_STATE_PAUSED) {
				gst_element_set_state(objParser->pipeline, GST_STATE_NULL);
				gst_object_unref(objParser->pipeline);
		}
		objParser->pipeline = NULL;
	}
	//free(objParser->strPipeline);

    objParser->qParserdFrames = g_queue_new();
    objParser->mMutex = g_mutex_new();

	objParser->listenerThread = NULL;
	objParser->cond_var = g_cond_new();
	objParser->cond_mutex = g_mutex_new();

	g_mutex_unlock(objParser->mMutex);
	g_mutex_free(objParser->mMutex);
	free(objParser);
	return MEDIA_PARSER_ERROR_NONE;
}

int mc_media_parser_start (MediaParserHandle parser)
{
	media_parser_handle objParser = NULL;
	if(parser == NULL)
		return MEDIA_PARSER_ERROR_INVALID_PARAM;

	objParser = (media_parser_handle) parser;

	if(objParser->objectType != PARSER_OBJECT_ID)
		return MEDIA_PARSER_ERROR_INVALID_PARAM;

	if(objParser->parserState == PARSER_STATE_INVALID)
		return MEDIA_PARSER_ERROR_INVALID_OPERATION;

	g_mutex_lock(objParser->mMutex);

	if(objParser->parserState == PARSER_STATE_COMPLETED) {
		g_mutex_unlock(objParser->mMutex);
		return MEDIA_PARSER_ERROR_PARSING_DONE;
	}

	if(objParser->pipelineModified == TRUE) {
		/* pipeline is not created. create here */
		GError *error = NULL;
		GstElement *fakesink = NULL;
		GstBus *bus = NULL;

		if(objParser->pipeline != NULL) {
			if(objParser->parserState >= PARSER_STATE_PIPELINE_CREATED && objParser->parserState <= PARSER_STATE_COMPLETED) {
				gst_element_set_state(objParser->pipeline, GST_STATE_NULL);
				gst_object_unref(objParser->pipeline);
				objParser->pipeline = NULL;
			} else { 
				g_mutex_unlock(objParser->mMutex);
				return MEDIA_PARSER_ERROR_INVALID_STATE;
			}
		}

		objParser->pipeline = gst_parse_launch(objParser->strPipeline, &error);
		if(objParser->pipeline == NULL) {
			g_print("\nError: %s", error->message);
			g_error_free(error);
			g_mutex_unlock(objParser->mMutex);
			return MEDIA_PARSER_ERROR_INVALID_PIPELINE;
		}
		objParser->pipelineModified = FALSE;
		fakesink = mc_find_element_by_factoryname(GST_BIN(objParser->pipeline), "fakesink");
		if(fakesink == NULL) {
			g_object_unref(objParser->pipeline);
			objParser->pipeline = NULL;
			g_mutex_unlock(objParser->mMutex);
			return MEDIA_PARSER_ERROR_INVALID_PIPELINE;
		}
		/* SET pipeline bus callback.*/
		bus = gst_element_get_bus(objParser->pipeline);
		gst_bus_add_watch(bus, cb_bus_message, objParser);
		gst_object_unref(bus);

		/* SET handoff signal handler of fakesink */
		g_signal_connect(fakesink,"handoff", (GCallback)cb_frame_parsed, objParser);
		g_object_set(fakesink, "signal-handoffs", TRUE, NULL);

		objParser->parserState = PARSER_STATE_PIPELINE_CREATED;
	}

	if(objParser->parserState == PARSER_STATE_PIPELINE_CREATED ||
		objParser->parserState == PARSER_STATE_PAUSED) {
			GstStateChangeReturn ret;

			objParser->frameCount = 0;
			objParser->stopThread = FALSE;
			/* create listener thread */
			objParser->listenerThread = g_thread_new("ParserListerThread", frame_parse_listener, objParser);
			ret = gst_element_set_state(objParser->pipeline, GST_STATE_PLAYING);
			objParser->parserState = PARSER_STATE_PARSING;
	}
	g_mutex_unlock(objParser->mMutex);
	return MEDIA_PARSER_ERROR_NONE;
}

int mc_media_parser_stop (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	if(objParser->parserState == PARSER_STATE_INVALID)
		return MEDIA_PARSER_ERROR_INVALID_OPERATION;

	g_mutex_lock(objParser->mMutex);

	if(objParser->parserState == PARSER_STATE_PARSING) {
		GstStateChangeReturn ret;
		ret = gst_element_set_state(objParser->pipeline, GST_STATE_PAUSED);
		if(ret == GST_STATE_CHANGE_FAILURE) {
			g_mutex_unlock(objParser->mMutex);
			return MEDIA_PARSER_ERROR_STOP_FAILED;
		}
		objParser->parserState = PARSER_STATE_PAUSED;
	}

	g_mutex_unlock(objParser->mMutex);
	return MEDIA_PARSER_ERROR_NONE;
}

int mc_get_parsed_frame_count (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
	int count = 0;

    if(parser == NULL)
        return 0;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return 0;

	g_mutex_lock(objParser->mMutex);
	count = objParser->frameCount;
	g_mutex_unlock(objParser->mMutex);

	return count;
}

int mc_set_max_frame_count (MediaParserHandle parser, int count)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL || count <= 0)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	g_mutex_lock(objParser->mMutex);

	objParser->maxFrame = count;
	g_mutex_unlock(objParser->mMutex);

	return MEDIA_PARSER_ERROR_NONE;
}

int mc_get_next_parsed_frame (MediaParserHandle parser, GstBuffer **buf)
{
    media_parser_handle objParser = NULL;
	int ret = MEDIA_PARSER_ERROR_NONE;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	g_mutex_lock(objParser->mMutex);

	if(objParser->frameCount > 0) {
		*buf = g_queue_pop_head(objParser->qParserdFrames);
	} else {
		*buf = NULL;
		ret = MEDIA_PARSER_ERROR_NO_MORE_FRAMES;
	}
	g_mutex_unlock(objParser->mMutex);
	return ret;
}

int mc_media_parser_set_pipeline(MediaParserHandle parser, char *strpipeline)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL || strpipeline == NULL || strpipeline[0] == '\0')
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    if(strlen(strpipeline) > MAX_PATH)
        return MEDIA_PARSER_ERROR_PATH_TOO_LONG;

    strcpy(objParser->strPipeline,strpipeline);
    objParser->pipelineModified = TRUE;

    return MEDIA_PARSER_ERROR_NONE;
}

//on frame parsed callback
int mc_media_parser_set_frame_cb(MediaParserHandle parser, MediaParserOnFrameCallback c_handler, void *user_data)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	g_mutex_lock(objParser->mMutex);

	objParser->cb_frame = c_handler;
	objParser->cb_frame_userdata = user_data;

	g_mutex_unlock(objParser->mMutex);
	return MEDIA_PARSER_ERROR_NONE;

}

//on eos callback
int mc_media_parser_set_eos_cb (MediaParserHandle parser, MediaParserOnEosCallback c_handler, void *user_data)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

	g_mutex_lock(objParser->mMutex);

	objParser->cb_eos = c_handler;
	objParser->cb_eos_userdata = user_data;

	g_mutex_unlock(objParser->mMutex);
	return MEDIA_PARSER_ERROR_NONE;
}

