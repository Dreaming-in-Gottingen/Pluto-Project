//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_stream"

#include <string.h>
#include <assert.h>
#include <errno.h>

#include <mm_log.h>
#include "mm_stream.h"
#include "mm_stream_file.h"

mm_stream_info_t *stream_handle_create(MediaDataSourceInfo *data_src)
{
    int ret = -1;
    mm_stream_info_t *stream;
    stream = (mm_stream_info_t *)malloc(sizeof(mm_stream_info_t));
    assert(stream != NULL);
    memset(stream, 0, sizeof(mm_stream_info_t));

    //Must init fd as -1, it is valid if we get a fd as 0.
    stream->fd = -1;
    memcpy(&stream->mDataSrcInfo, data_src, sizeof(MediaDataSourceInfo));

    if((data_src->source_type == MEDIA_SOURCETYPE_FILEPATH || data_src->source_type == MEDIA_SOURCETYPE_FD)
        && data_src->stream_type == MEDIA_STREAMTYPE_LOCALFILE)
    {
        if (0 == data_src->io_direction)
            ret = create_instream_handle_file(stream, data_src);
        else if (1 == data_src->io_direction)
            ret = create_outstream_handle_file(stream, data_src);
        else
            ALOGE("(f:%s, l:%d) error io_direction[%d]! 0=read, 1=write", __FUNCTION__, __LINE__, data_src->io_direction);
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! source_type[%d] stream_type[%d]? not support currently!", __FUNCTION__, __LINE__, data_src->source_type, data_src->stream_type);
    }

    if(ret < 0)
    {
        ALOGE("(f:%s, l:%d) create stream_handle failed!", __FUNCTION__, __LINE__);
        free(stream);
        stream = NULL;
    }

    return stream;
}

void stream_handle_destroy(mm_stream_info_t *stream)
{
    if(!stream){
        return;
    }
    stream->destory(stream);
    free(stream);
    stream = NULL;
}

