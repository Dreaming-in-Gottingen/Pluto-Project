//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 1
#define LOG_TAG "Mpeg2tsMuxerDrv"

#include <stdlib.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <muxer_writer.h>
#include "Mpeg2tsMuxer.h"

#include <mm_log.h>
#include <mm_base.h>
#include <mm_cache.h>
#include <mm_stream.h>


int Mpeg2tsMuxerWriteCSD(MuxerWriter *handle, char *csdData, int csdLen, int idx)
{
    AVFormatContext *ctx = (AVFormatContext *)handle->mPrivContext;
    TrackCSDInfo *pInfo = &ctx->mTrackCSD[idx];
    if (csdLen)
    {
        pInfo->npCsdData = (char *)malloc(csdLen);
        pInfo->nCsdLen = csdLen;
        memcpy(pInfo->npCsdData, csdData, csdLen);
    }
    else
    {
        pInfo->npCsdData = NULL;
        pInfo->nCsdLen = 0;
    }

    return 0;
}

int Mpeg2tsMuxerWriteHeader(MuxerWriter *handle)
{
    AVFormatContext *ctx = (AVFormatContext *)handle->mPrivContext;

    int nCacheSize = 0;
    if (ctx->mpCacheWriter)
    {
        ALOGE("(f:%s, l:%d) fatal error! CacheWriter should be NULL now!", __FUNCTION__, __LINE__);
        return -1;
    }
    if (ctx->mpStream)
    {
        MEDIA_CACHEMODE mode = ctx->mFsCacheMode;
        if (CACHEMODE_THREAD == mode)
        {
            ALOGE("(f:%s, l:%d) CACHEMODE_THREAD NOT support currently!", __FUNCTION__, __LINE__);
            return -1;
        }
        else if (CACHEMODE_SIMPLE == mode)
        {
            nCacheSize = ctx->mCacheSimpleSize;
            ctx->mpCacheWriter = cache_handle_create(mode, ctx->mpStream, NULL, nCacheSize);
            if (NULL == ctx->mpCacheWriter)
            {
                ALOGE("(f:%s, l:%d) fatal error! cache_handle_create fail!", __FUNCTION__, __LINE__);
                return -1;
            }
        } else {
            ALOGE("(f:%s, l:%d) fatal error! unknown cache_mode[%d]? check code!!!", __FUNCTION__, __LINE__, mode);
            return -1;
        }
    }

    if (ctx->mSampleRate!=0 && ctx->mChannelCnt!=0)
    {
        ALOGD("(f:%s, l:%d) TS muxer have audio! initialize context's internal param!", __FUNCTION__, __LINE__);
        ctx->mTrackCnt = 2;
        ctx->mTrackPid[0] = 0x1E1;
        ctx->mTrackPid[1] = 0x1E2;
    }
    else
    {
        ALOGD("(f:%s, l:%d) TS muxer have NO audio! initialize context's internal param!", __FUNCTION__, __LINE__);
        ctx->mTrackCnt = 1;
        ctx->mTrackPid[0] = 0x1E1;
        ctx->mTrackPid[1] = 0;
    }
    initCrcTable(ctx);

    return 0;
}

int Mpeg2tsMuxerWritePacket(MuxerWriter *handle, AVPacket *pkt)
{
    AVFormatContext *ctx = (AVFormatContext *)handle->mPrivContext;
    return mpeg2ts_write_packet(ctx, pkt);
}

int Mpeg2tsMuxerWriteTrailer(MuxerWriter *handle)
{
    AVFormatContext *ctx = (AVFormatContext *)handle->mPrivContext;
    return mpeg2ts_write_trailer(ctx);
}

int Mpeg2tsMuxerIOCtrl(MuxerWriter *handle, int cmd, int param, void *data)
{
    AVFormatContext *ctx = (AVFormatContext *)handle->mPrivContext;
    MediaTrackInfo *track_info = NULL;
    MediaDataSourceInfo src_info;

    switch (cmd)
    {
    case SET_TRACK_INFO:
        track_info = (MediaTrackInfo *)data;
        if (NULL == track_info)
        {
            ALOGE("(f:%s, l:%d) fatal error! why track_info is NULL ?!", __FUNCTION__, __LINE__);
            return -1;
        }

        //set video parameters
        ctx->mWidth = track_info->mWidth;
        ctx->mHeight = track_info->mHeight;
        ctx->mFrameRate = track_info->mFrameRate;
        ctx->mKeyFrameInterval = track_info->mMaxKeyInterval;   //30
        if (VENC_CODEC_H264 == track_info->mVideoEncType)
        {
            ctx->mVideoEncType = CODEC_ID_H264;
        }
        else
        {
            ALOGE("(f:%s, l:%d) fatal error! video_enc_type[%d] NOT support!", __FUNCTION__, __LINE__, track_info->mVideoEncType);
            return -1;
        }

        //set audio parameters
        ctx->mChannelCnt = track_info->mChannelCnt;
        ctx->mBitWidht = track_info->mBitWidth;
        ctx->mSampleRate = track_info->mSampleRate;
        ctx->mFrameSize = track_info->mFrameSize;
        ctx->mAudioEncType = CODEC_ID_AAC;
        break;

    case SET_TOTAL_TIME: //ms
        ctx->mTrackDuration[0] = param;
        ctx->mTrackDuration[1] = param;
        break;

    case SET_FALLOCATE_LEN:
        ctx->mFallocateLen = param;
        break;

    case SET_STREAM_FD:
        memset(&src_info, 0, sizeof(src_info));
        src_info.stream_type = MEDIA_STREAMTYPE_LOCALFILE;
        src_info.source_type = MEDIA_SOURCETYPE_FD;
        src_info.fd = param;
        src_info.io_direction = 1;
        ctx->mpStream = stream_handle_create(&src_info);
        if (NULL == ctx->mpStream)
        {
            ALOGE("(f:%s, l:%d) fatal error! stream_handle_create fail!", __FUNCTION__, __LINE__);
            return -1;
        }
        if (ctx->mFallocateLen > 0)
        {
            if (ctx->mpStream->fallocate(ctx->mpStream, 1, 0, ctx->mFallocateLen) < 0)
            {
                ALOGE("(f:%s, l:%d) fatal error! fallocate[%d] for fd[%d] fail:(%s)", __FUNCTION__, __LINE__, ctx->mFallocateLen, src_info.fd, strerror(errno));
            }
            else
            {
                ALOGD("(f:%s, l:%d) fallocate[%d] success!", __FUNCTION__, __LINE__, ctx->mFallocateLen);
            }
        }
        break;

    case SET_STREAM_FILEPATH:
        memset(&src_info, 0, sizeof(src_info));
        src_info.stream_type = MEDIA_STREAMTYPE_LOCALFILE;
        src_info.source_type = MEDIA_SOURCETYPE_FILEPATH;
        src_info.src_url = (char*)data;
        src_info.io_direction = 1;
        ctx->mpStream = stream_handle_create(&src_info);
        if (NULL == ctx->mpStream)
        {
            ALOGE("(f:%s, l:%d) fatal error! stream_handle_create fail!", __FUNCTION__, __LINE__);
            return -1;
        }
        if (ctx->mFallocateLen > 0)
        {
            if (ctx->mpStream->fallocate(ctx->mpStream, 1, 0, ctx->mFallocateLen) < 0)
            {
                ALOGE("(f:%s, l:%d) fatal error! fallocate[%d] for filepath[%s] fail:(%s)", __FUNCTION__, __LINE__, ctx->mFallocateLen, src_info.src_url, strerror(errno));
            }
            else
            {
                ALOGD("(f:%s, l:%d) fallocate[%d] success!", __FUNCTION__, __LINE__, ctx->mFallocateLen);
            }
        }
        break;

    case SET_TFCARD_STATE:
        ctx->mbTFCardState = param;    //true-on; false-off
        ALOGD("(f:%s, l:%d) card state:[%d]", __FUNCTION__, __LINE__, param);
        break;

    case SET_CACHE_SIZE:
        ctx->mCacheSimpleSize = param;
        break;

    case SET_CACHE_MODE:
        ctx->mFsCacheMode = param;
        break;

    case SET_STREAM_CALLBACK:
        if (ctx->mpStream != NULL)
        {
            ctx->mpStream->mCB = *(mm_stream_callback*)data;
        }
        else
        {
            ALOGW("(f:%s, l:%d) fail: mpStream==NULL when set_callback!", __FUNCTION__, __LINE__);
        }
        break;

    default:
        ALOGE("(f:%s, l:%d) unknown cmd[%d]!!!", __FUNCTION__, __LINE__, cmd);
        break;
    }

    return 0;
}

int Mpeg2tsMuxerOpen(MuxerWriter *handle)
{
    ALOGD("(f:%s, l:%d) Mpeg2tsMuxerOpen", __FUNCTION__, __LINE__);
    AVFormatContext *context;

    context = (AVFormatContext *)malloc(sizeof(AVFormatContext));
    if (!context)
    {
        return -1;
    }
    memset(context, 0, sizeof(AVFormatContext));
    handle->mPrivContext = (void*)context;

    return 0;
}

int Mpeg2tsMuxerClose(MuxerWriter *handle)
{
    ALOGD("(f:%s, l:%d) Mpeg2tsMuxerClose", __FUNCTION__, __LINE__);
    AVFormatContext *context = (AVFormatContext *)handle->mPrivContext;

    if (context->mpCacheWriter)
    {
        cache_handle_destroy(context->mpCacheWriter);
        context->mpCacheWriter = NULL;
    }
    if (context->mpStream)
    {
        stream_handle_destroy(context->mpStream);
        context->mpStream = NULL;
    }
    free(context);
    handle->mPrivContext = NULL;

    return 0;
}

MuxerWriter *create_mpeg2ts_muxer_handle(void)
{
    MuxerWriter *handle = (MuxerWriter*)malloc(sizeof(MuxerWriter));
    if (NULL == handle)
    {
        ALOGE("(f:%s, l:%d) fatal error! malloc mpeg2ts_muxer fail!", __FUNCTION__, __LINE__);
        return NULL;
    }

    handle->info                = "mpeg2ts muxer writer";
    handle->mPrivContext        = NULL;

    handle->MuxerOpen           = Mpeg2tsMuxerOpen;
    handle->MuxerClose          = Mpeg2tsMuxerClose;
    handle->MuxerIOCtrl         = Mpeg2tsMuxerIOCtrl;

    handle->MuxerWriteCSD       = Mpeg2tsMuxerWriteCSD;
    handle->MuxerWriteHeader    = Mpeg2tsMuxerWriteHeader;
    handle->MuxerWriteTrailer   = Mpeg2tsMuxerWriteTrailer;
    handle->MuxerWritePacket    = Mpeg2tsMuxerWritePacket;

    return handle;
}
