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
	MEDIA_PARSER_ERROR_STOP_FAILED,
	MEDIA_PARSER_ERROR_START_FAILED,
	MEDIA_PARSER_ERROR_NO_MORE_FRAMES,
};


typedef void (*MediaParserOnFrameCallback) (GstBuffer *, gpointer user_data) ;
typedef void (*MediaParserOnEosCallback) (gpointer user_data);

int mc_media_parser_create (MediaParserHandle *parser);

int mc_media_parser_destroy (MediaParserHandle *parser);

int mc_media_parser_start (MediaParserHandle parser);

int mc_media_parser_stop (MediaParserHandle parser);

int mc_get_parsed_frame_count (MediaParserHandle parser);

int mc_set_max_frame_count (MediaParserHandle parser, int count);

int mc_get_next_parsed_frame (MediaParserHandle parser, GstBuffer **buf);

int mc_media_parser_set_pipeline(MediaParserHandle parser, char *strpipeline);

//on frame parsed callback

int mc_media_parser_set_frame_cb(MediaParserHandle parser, MediaParserOnFrameCallback c_handler, void *user_data);
//on eos callback

int mc_media_parser_set_eos_cb (MediaParserHandle parser, MediaParserOnEosCallback c_handler, void *user_data);
//on error callback





