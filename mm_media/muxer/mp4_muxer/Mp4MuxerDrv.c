//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "Mp4MuxerDrv"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <muxer_writer.h>
#include "Mp4Muxer.h"

#include <mm_log.h>
#include <mm_base.h>
#include <mm_cache.h>
#include <mm_stream.h>

int Mp4MuxerWriteCSD(MuxerWriter *handle, char *csdData, int csdLen, int idx)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    MOVContext *mov = Mp4MuxerCtx->priv_data;
    MOVTrack *trk = &mov->tracks[idx];
    if (csdLen)
    {
        trk->csdData = (char *)malloc(csdLen);
        trk->csdLen = csdLen;
        memcpy(trk->csdData, csdData, csdLen);
    }
    else
    {
        trk->csdData = NULL;
        trk->csdLen = 0;
    }

    return 0;
}

int Mp4MuxerWriteHeader(MuxerWriter *handle)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    MOVContext *mov = Mp4MuxerCtx->priv_data;
    int8_t *pCache = NULL;
    int nCacheSize = 0;
    if (Mp4MuxerCtx->mpCacheWriter)
    {
        ALOGE("(f:%s, l:%d) fatal error! CacheWriter should be NULL now!", __FUNCTION__, __LINE__);
        return -1;
    }
    if (Mp4MuxerCtx->mpStream)
    {
        MEDIA_CACHEMODE mode = Mp4MuxerCtx->mFsCacheMode;
        if (CACHEMODE_THREAD == mode)
        {
            ALOGE("(f:%s, l:%d) CACHEMODE_THREAD NOT support currently!", __FUNCTION__, __LINE__);
        }
        else if (CACHEMODE_SIMPLE == mode)
        {
            pCache = NULL;
            nCacheSize = Mp4MuxerCtx->mCacheSimpleSize;
            Mp4MuxerCtx->mpCacheWriter = cache_handle_create(mode, Mp4MuxerCtx->mpStream, pCache, nCacheSize);
        }
        if (NULL == Mp4MuxerCtx->mpCacheWriter)
        {
            ALOGE("(f:%s, l:%d) fatal error! cache_handle_create fail!", __FUNCTION__, __LINE__);
            return -1;
        }
    }
    return mov_write_header(Mp4MuxerCtx);
}

int Mp4MuxerWriteTrailer(MuxerWriter *handle)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    return mov_write_trailer(Mp4MuxerCtx);
}

int Mp4MuxerWritePacket(MuxerWriter *handle, AVPacket *pkt)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    return mov_write_packet(Mp4MuxerCtx, pkt);
}

int Mp4MuxerIOCtrl(MuxerWriter *handle, int cmd, int param, void *data)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    MOVContext *mov = Mp4MuxerCtx->priv_data;
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

        //mov->create_time = track_info->mCreateTime;
        //set video parameters
        Mp4MuxerCtx->streams[0]->codec.width = track_info->mWidth;
        Mp4MuxerCtx->streams[0]->codec.height = track_info->mHeight;
        Mp4MuxerCtx->streams[0]->codec.frame_rate = track_info->mFrameRate;
        Mp4MuxerCtx->streams[0]->codec.rotate_degree = track_info->mRotateDegree;
        mov->keyframe_interval = track_info->mMaxKeyInterval;   //30
        mov->tracks[0].timescale = 1000;
        if (VENC_CODEC_H264 == track_info->mVideoEncType)
        {
            Mp4MuxerCtx->streams[0]->codec.codec_id = CODEC_ID_H264;
        }
        else if (VENC_CODEC_JPEG == track_info->mVideoEncType)
        {
            Mp4MuxerCtx->streams[0]->codec.codec_id = CODEC_ID_MJPEG;
        }
        else
        {
            ALOGE("(f:%s, l:%d) fatal error! video_enc_type[%d] NOT support!", __FUNCTION__, __LINE__, track_info->mVideoEncType);
            return -1;
        }
        Mp4MuxerCtx->mTrackCnt++;

        //set audio parameters
        Mp4MuxerCtx->streams[1]->codec.channels = track_info->mChannelCnt;
        Mp4MuxerCtx->streams[1]->codec.bits_per_sample = track_info->mBitWidth;
        Mp4MuxerCtx->streams[1]->codec.sample_rate = track_info->mSampleRate;
        Mp4MuxerCtx->streams[1]->codec.frame_size = track_info->mFrameSize;
        mov->tracks[1].timescale = track_info->mSampleRate;
        mov->tracks[1].sampleDuration = 1;
        Mp4MuxerCtx->streams[1]->codec.codec_id = CODEC_ID_AAC;
//        if (AENC_CODEC_AAC == track_info->mAudioEncType)
//        {
//            Mp4MuxerCtx->streams[1]->codec.codec_id = CODEC_ID_AAC;
//        }
//        else
//        {
//            ALOGE("(f:%s, l:%d) fatal error! audio_enc_type[%d] NOT support!", __FUNCTION__, __LINE__, track_info->mAudioEncType);
//            return -1;
//        }
        Mp4MuxerCtx->mTrackCnt++;
        break;

    case SET_TOTAL_TIME: //ms
        mov->tracks[0].trackDuration = (((int64_t)param * mov->tracks[0].timescale) / 1000);
        mov->tracks[1].trackDuration = (((int64_t)param * mov->tracks[1].timescale) / 1000);
        break;

    case SET_FALLOCATE_LEN:
        Mp4MuxerCtx->mFallocateLen = param;
        break;

    case SET_STREAM_FD:
        memset(&src_info, 0, sizeof(src_info));
        src_info.stream_type = MEDIA_STREAMTYPE_LOCALFILE;
        src_info.source_type = MEDIA_SOURCETYPE_FD;
        src_info.fd = param;
        src_info.io_direction = 1;
        Mp4MuxerCtx->mpStream = stream_handle_create(&src_info);
        if (NULL == Mp4MuxerCtx->mpStream)
        {
            ALOGE("(f:%s, l:%d) fatal error! stream_handle_create fail!", __FUNCTION__, __LINE__);
            return -1;
        }
        if (Mp4MuxerCtx->mFallocateLen > 0)
        {
            if (Mp4MuxerCtx->mpStream->fallocate(Mp4MuxerCtx->mpStream, 1, 0, Mp4MuxerCtx->mFallocateLen) < 0)
            {
                ALOGE("(f:%s, l:%d) fatal error! fallocate[%d] for fd[%d] fail:(%s)", __FUNCTION__, __LINE__, Mp4MuxerCtx->mFallocateLen, src_info.fd, strerror(errno));
            }
            else
            {
                ALOGD("(f:%s, l:%d) fallocte[%d] success!", __FUNCTION__, __LINE__, Mp4MuxerCtx->mFallocateLen);
            }
        }
        break;

    case SET_STREAM_FILEPATH:
        memset(&src_info, 0, sizeof(src_info));
        src_info.stream_type = MEDIA_STREAMTYPE_LOCALFILE;
        src_info.source_type = MEDIA_SOURCETYPE_FILEPATH;
        src_info.src_url = (char*)data;
        src_info.io_direction = 1;
        Mp4MuxerCtx->mpStream = stream_handle_create(&src_info);
        if (NULL == Mp4MuxerCtx->mpStream)
        {
            ALOGE("(f:%s, l:%d) fatal error! stream_handle_create fail!", __FUNCTION__, __LINE__);
            return -1;
        }
        if (Mp4MuxerCtx->mFallocateLen > 0)
        {
            if (Mp4MuxerCtx->mpStream->fallocate(Mp4MuxerCtx->mpStream, 1, 0, Mp4MuxerCtx->mFallocateLen) < 0)
            {
                ALOGE("(f:%s, l:%d) fatal error! fallocate[%d] for filepath[%s] fail:(%s)", __FUNCTION__, __LINE__, Mp4MuxerCtx->mFallocateLen, src_info.src_url, strerror(errno));
            }
            else
            {
                ALOGD("(f:%s, l:%d) fallocte[%d] success!", __FUNCTION__, __LINE__, Mp4MuxerCtx->mFallocateLen);
            }
        }
        break;

    case SET_TFCARD_STATE:
        Mp4MuxerCtx->mbTFCardOn = param;    //1-on; 0-off
        ALOGD("(f:%s, l:%d) card state:[%d]", __FUNCTION__, __LINE__, param);
        break;

    case SET_CACHE_SIZE:
        Mp4MuxerCtx->mCacheSimpleSize = param;
        break;

    case SET_CACHE_MODE:
        Mp4MuxerCtx->mFsCacheMode = param;
        break;

    case SET_STREAM_CALLBACK:
        if (Mp4MuxerCtx->mpStream != NULL)
        {
            Mp4MuxerCtx->mpStream->mCB = *(mm_stream_callback*)data;
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

int Mp4MuxerOpen(MuxerWriter *handle)
{
    ALOGD("(f:%s, l:%d) Mp4MuxerOpen", __FUNCTION__, __LINE__);
    AVFormatContext *Mp4MuxerCtx;
    MOVContext *mov;
    AVStream *stream;
    int i;

    Mp4MuxerCtx = (AVFormatContext *)malloc(sizeof(AVFormatContext));
    if (!Mp4MuxerCtx)
    {
        return -1;
    }
    memset(Mp4MuxerCtx, 0, sizeof(AVFormatContext));

    mov = (MOVContext *)malloc(sizeof(MOVContext));
    if (!mov) {
        return -1;
    }
    memset(mov, 0, sizeof(MOVContext));

    Mp4MuxerCtx->priv_data = (void *)mov;
    for (i=0; i<MAX_STREAMS_IN_FILE; i++)
    {
        stream = (AVStream *)malloc(sizeof(AVStream));
        memset(stream, 0, sizeof(AVStream));
        Mp4MuxerCtx->streams[i] = stream;
        stream->codec.codec_type = (i==0) ? CODEC_TYPE_VIDEO : CODEC_TYPE_AUDIO;
        mov->tracks[i].mov = mov;
    }
    mov->title_info = calloc(1, 128);
    time_t raw_time;
    time(&raw_time);
    mov->create_time = raw_time + 0x7c25b080; // 1904 based

    Mp4MuxerCtx->mov_inf_cache = (uint8_t*)malloc(TOTAL_CACHE_SIZE*4);

    if (!Mp4MuxerCtx->mov_inf_cache)
    {
        ALOGE("(f:%s, l:%d) malloc moov_info_cache fail!!!", __FUNCTION__, __LINE__);
        return -1;
    }

    mov->cache_keyframe_ptr = (uint32_t*)malloc(KEYFRAME_CACHE_SIZE);
    mov->cache_start_ptr[STCO_ID][CODEC_TYPE_VIDEO] = mov->cache_read_ptr[STCO_ID][CODEC_TYPE_VIDEO]
    = mov->cache_write_ptr[STCO_ID][CODEC_TYPE_VIDEO] = (uint32_t*)Mp4MuxerCtx->mov_inf_cache;
    mov->cache_start_ptr[STCO_ID][CODEC_TYPE_AUDIO] = mov->cache_read_ptr[STCO_ID][CODEC_TYPE_AUDIO]
    = mov->cache_write_ptr[STCO_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STCO_ID][CODEC_TYPE_VIDEO] + STCO_CACHE_SIZE;

    mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_VIDEO] = mov->cache_read_ptr[STSZ_ID][CODEC_TYPE_VIDEO]
    = mov->cache_write_ptr[STSZ_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STCO_ID][CODEC_TYPE_AUDIO] + STCO_CACHE_SIZE;
    mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_AUDIO] = mov->cache_read_ptr[STSZ_ID][CODEC_TYPE_AUDIO]
    = mov->cache_write_ptr[STSZ_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_VIDEO] + STSZ_CACHE_SIZE;

    mov->cache_start_ptr[STSC_ID][CODEC_TYPE_VIDEO] = mov->cache_read_ptr[STSC_ID][CODEC_TYPE_VIDEO]
    = mov->cache_write_ptr[STSC_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_AUDIO] + STSZ_CACHE_SIZE;
    mov->cache_start_ptr[STSC_ID][CODEC_TYPE_AUDIO] = mov->cache_read_ptr[STSC_ID][CODEC_TYPE_AUDIO]
    = mov->cache_write_ptr[STSC_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STSC_ID][CODEC_TYPE_VIDEO] + STSC_CACHE_SIZE;

    mov->cache_start_ptr[STTS_ID][CODEC_TYPE_VIDEO] = mov->cache_read_ptr[STTS_ID][CODEC_TYPE_VIDEO]
    = mov->cache_write_ptr[STTS_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STSC_ID][CODEC_TYPE_AUDIO] + STSC_CACHE_SIZE;
    mov->cache_tiny_page_ptr = mov->cache_start_ptr[STTS_ID][CODEC_TYPE_VIDEO] + STTS_CACHE_SIZE;

    mov->cache_end_ptr[STCO_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STCO_ID][CODEC_TYPE_VIDEO] + (STCO_CACHE_SIZE - 1);
    mov->cache_end_ptr[STSZ_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_VIDEO] + (STSZ_CACHE_SIZE - 1);
    mov->cache_end_ptr[STSC_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STSC_ID][CODEC_TYPE_VIDEO] + (STSC_CACHE_SIZE - 1);
    mov->cache_end_ptr[STTS_ID][CODEC_TYPE_VIDEO] = mov->cache_start_ptr[STTS_ID][CODEC_TYPE_VIDEO] + (STTS_CACHE_SIZE - 1);

    mov->cache_end_ptr[STCO_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STCO_ID][CODEC_TYPE_AUDIO] + (STCO_CACHE_SIZE - 1);
    mov->cache_end_ptr[STSZ_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STSZ_ID][CODEC_TYPE_AUDIO] + (STSZ_CACHE_SIZE - 1);
    mov->cache_end_ptr[STSC_ID][CODEC_TYPE_AUDIO] = mov->cache_start_ptr[STSC_ID][CODEC_TYPE_AUDIO] + (STSC_CACHE_SIZE - 1);

    mov->last_stream_idx = -1;
    //Mp4MuxerCtx->firstframe[0] = 1;
    //Mp4MuxerCtx->firstframe[1] = 1;
    //mov->keyframe_interval = 1;

    handle->mPrivContext = (void*)Mp4MuxerCtx;

    return 0;
}

int Mp4MuxerClose(MuxerWriter *handle)
{
    AVFormatContext *Mp4MuxerCtx = (AVFormatContext *)handle->mPrivContext;
    MOVContext *mov = Mp4MuxerCtx->priv_data;
    free(mov->title_info);
    if (Mp4MuxerCtx->mov_inf_cache)
    {
        free(Mp4MuxerCtx->mov_inf_cache);
        Mp4MuxerCtx->mov_inf_cache = NULL;
    }

    for (int i=0; i<MAX_STREAMS_IN_FILE; i++)
    {
        if (mov->tracks[i].csdData)
        {
            free(mov->tracks[i].csdData);
            mov->tracks[i].csdData = NULL;
        }
        if (Mp4MuxerCtx->streams[i])
        {
            free(Mp4MuxerCtx->streams[i]);
            Mp4MuxerCtx->streams[i] = NULL;
        }
    }

    if(mov->cache_keyframe_ptr)
    {
        free(mov->cache_keyframe_ptr);
        mov->cache_keyframe_ptr = NULL;
    }

    if(Mp4MuxerCtx->mpCacheWriter)
    {
        cache_handle_destroy(Mp4MuxerCtx->mpCacheWriter);
        Mp4MuxerCtx->mpCacheWriter = NULL;
    }
    if(Mp4MuxerCtx->mpStream)
    {
        stream_handle_destroy(Mp4MuxerCtx->mpStream);
        Mp4MuxerCtx->mpStream = NULL;
    }
    if(Mp4MuxerCtx->priv_data)
    {
        free(Mp4MuxerCtx->priv_data);
        Mp4MuxerCtx->priv_data = NULL;
    }

    if(Mp4MuxerCtx)
    {
        free(Mp4MuxerCtx);
        Mp4MuxerCtx = NULL;
    }

    handle->mPrivContext = NULL;

    return 0;
}

MuxerWriter *create_mp4_muxer_handle(void)
{
    MuxerWriter *handle = (MuxerWriter*)malloc(sizeof(MuxerWriter));
    if (NULL == handle)
    {
        ALOGE("(f:%s, l:%d) fatal error! malloc mp4_muxer fail!", __FUNCTION__, __LINE__);
        return NULL;
    }

    handle->info                = "mp4 muxer writer";
    handle->mPrivContext        = NULL;

    handle->MuxerOpen           = Mp4MuxerOpen;
    handle->MuxerClose          = Mp4MuxerClose;
    handle->MuxerIOCtrl         = Mp4MuxerIOCtrl;

    handle->MuxerWriteCSD       = Mp4MuxerWriteCSD;
    handle->MuxerWriteHeader    = Mp4MuxerWriteHeader;
    handle->MuxerWriteTrailer   = Mp4MuxerWriteTrailer;
    handle->MuxerWritePacket    = Mp4MuxerWritePacket;

    return handle;
}
