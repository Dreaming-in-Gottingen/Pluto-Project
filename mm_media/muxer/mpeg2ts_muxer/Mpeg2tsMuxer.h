#ifndef __MPEG2TS_MUXER_H__
#define __MPEG2TS_MUXER_H__

#include <stdbool.h>


#include <mm_stream.h>
#include <mm_cache.h>

#include <muxer_writer.h>

#define VIDEO_STREAM_IDX    0
#define AUDIO_STREAM_IDX    1
#define MAX_STREAMS         2


#define TS_PACKAGE_SZ   188

typedef struct TrackCSDInfo {
    int     nCsdLen;
    char   *npCsdData;
} TrackCSDInfo;

typedef struct AVFormatContext {


    //av param
    int         mWidth;
    int         mHeight;
    int         mFrameRate;
    int         mKeyFrameInterval;
    AV_ENC_TYPE mVideoEncType;
    int         mChannelCnt;
    int         mBitWidht;
    int         mSampleRate;
    int         mFrameSize;     //1024
    AV_ENC_TYPE mAudioEncType;

    // counter for every program, which only increase in [0, f]
    char                mPATContinuityCounter;
    char                mPMTContinuityCounter;
    char                mTSContinuityCounter[MAX_STREAMS];

    unsigned int        mCrcTable[256];
    unsigned char       mpTSBuf[TS_PACKAGE_SZ];

    int                 mTSPktTotalNumber[MAX_STREAMS];
    int                 mTrackPid[MAX_STREAMS];  // [0]:0x1e1 for which track come firstly, but we fix it to video, [1]:0x1e2 if audio exist
    TrackCSDInfo        mTrackCSD[MAX_STREAMS];  // [0]:video; [1]:audio

    int                 mTrackCnt;
    int                 mTrackDuration[MAX_STREAMS];

    char                mFileName[1024];
    mm_stream_info_t   *mpStream;
    FsCacheWriter      *mpCacheWriter;
    MEDIA_CACHEMODE     mFsCacheMode;
    int                 mCacheSimpleSize;
    int                 mFallocateLen;
    bool                mbTFCardState;         // true:on, false:off
} AVFormatContext;


#endif // __MPEG2TS_MUXER_H__
