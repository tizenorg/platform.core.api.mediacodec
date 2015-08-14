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
    PARSER_STATE_PIPELINE_CREATED,
    PARSER_STATE_INITIALIZED,
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
};

int mc_create_media_parser (MediaParserHandle *parser)
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
    objParser->parserState = PARASER_STATE_ALLOCATED;
    objParser->pipeline = NULL;
    memset(objParser->strPipeline,0,sizeof(objParser->strPipeline));
    objParser->maxFrame = DEFAULT_MAX_FRAME;
    objParser->qParserdFrames = g_queue_new();
    objParser->mMutex = g_mutex_new();
    objParser->pipelineModified = FALSE;

    *parser = objParser;
    return MEDIA_PARSER_ERROR_NONE;

}

int mc_destroy_media_parser (MediaParserHandle *parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) *parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

}

int mc_start_media_parser (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    if(objParser->pipelineModified == TRUE) {
        /* pipeline is not created. create here */
         GError *error;
         if(objParser->pipeline != NULL) {
             if(objParser->parserState >= PARSER_STATE_PIPELINE_CREATED && objParser->parserState <= PARSER_STATE_COMPLETED) {
                 gst_element_set_state(objParser->pipeline, GST_STATE_NULL);
                 gst_object_unref(objParser->pipeline);
                 objParser->pipeline = NULL;
             } else
                 return MEDIA_PARSER_ERROR_INVALID_STATE;
         }

         objParser->pipeline = gst_parse_launch(objParser->strPipeline, &error);
         if(objParser->pipeline == NULL) {
             g_print("\nError: %s", error->message);
             g_error_free(error);
             return MEDIA_PARSER_ERROR_INVALID_PIPELINE;
         }
         objParser->pipelineModified = FALSE;
         objParser->parserState = PARSER_STATE_PIPELINE_CREATED;
         /* SET pipeline bus callback.*/
         /* create listener thread */
    }

    if(objParser->parserState == PARSER_STATE_COMPLETED)
        return MEDIA_PARSER_ERROR_PARSING_DONE;

    if(objParser->parserState == PARSER_STATE_PIPELINE_CREATED ||
        objParser->parserState == PARSER_STATE_PAUSED) {
            gst_element_set_state(objParser->pipeline, GST_STATE_PLAYING);
            objParser->parserState = PARSER_STATE_PARSING;
    }

    return MEDIA_PARSER_ERROR_NONE;
}

int mc_stop_media_parser (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;
}

int mc_get_parsed_frame_count (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;
}

int mc_set_total_frame_count (MediaParserHandle parser)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;
}

int mc_get_next_parsed_frame (MediaParserHandle parser, GstBuffer **buf)
{
    media_parser_handle objParser = NULL;
    if(parser == NULL)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;

    objParser = (media_parser_handle) parser;

    if(objParser == NULL || objParser->objectType != PARSER_OBJECT_ID)
        return MEDIA_PARSER_ERROR_INVALID_PARAM;
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

