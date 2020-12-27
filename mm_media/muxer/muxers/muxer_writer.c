//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "muxer_writer"

#include <stdlib.h>
#include <string.h>

#include <mm_log.h>
#include <muxer_writer.h>

//extern MuxerWriter* create_mp4_muxer_handle();
extern MuxerWriter* create_mpeg2ts_muxer_handle();

MuxerWriter *muxer_writer_create(int mode)
{
    MuxerWriter *handle = NULL;

    switch (mode) {
    case MUXER_MODE_MP4:
        //handle = create_mp4_muxer_handle();
        break;
    case MUXER_MODE_TS:
        handle = create_mpeg2ts_muxer_handle();
        break;
    default:
        ALOGE("(f:%s, l:%d) muxer_mode[%d] NOT support currently!", __func__, __LINE__, mode);
        break;
    }

    return handle;
}

void muxer_writer_destroy(MuxerWriter *handle)
{
    if (handle->mPrivContext) {
        ALOGE("(f:%s, l:%d) why mPrivContext(%s) != NULL ?!!! may forget MuxerClose!", __func__, __LINE__, handle->info);
    }
    if (handle != NULL) {
        free(handle);
    }
}
