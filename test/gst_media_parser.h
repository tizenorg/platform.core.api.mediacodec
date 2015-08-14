#include <gst/gst.h>

typedef void* MediaParserHandle;

enum media_parser_error
{
    MEDIA_PARSER_ERROR_NONE = 0,
    MEDIA_PARSER_ERROR_UNKNOW,
    MEDIA_PARSER_ERROR_INVALID_PARAM,
    MEDIA_PARSER_ERROR_ALLOC_FAILED,
    MEDIA_PARSER_ERROR_PATH_TOO_LONG,
    MEDIA_PARSER_ERROR_INVALID_OPERATION,
    MEDIA_PARSER_ERROR_INVALID_PIPELINE,
    MEDIA_PARSER_ERROR_INVALID_STATE,
    MEDIA_PARSER_ERROR_PARSING_DONE,
};


int mc_create_media_parser (MediaParserHandle *parser);

int mc_destroy_media_parser (MediaParserHandle *parser);

int mc_start_media_parser (MediaParserHandle parser);

int mc_stop_media_parser (MediaParserHandle parser);

int mc_get_parsed_frame_count (MediaParserHandle parser);

int mc_set_total_frame_count (MediaParserHandle parser);

int mc_get_next_parsed_frame (MediaParserHandle parser, GstBuffer **buf);

int mc_media_parser_set_pipeline(MediaParserHandle parser, char *strpipeline);





