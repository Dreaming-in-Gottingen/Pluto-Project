#ifndef __MUXER_WRITER_H__
#define __MUXER_WRITER_H__

typedef enum MUXER_MODE {
    MUXER_MODE_MP4  = 0,
    MUXER_MODE_TS   = 1,
} MUXER_MODE;

typedef enum MUXER_IOCTRL_CMD {
    SET_TRACK_INFO      = 0,
    SET_TOTAL_TIME      = 1,
    SET_FALLOCATE_LEN   = 2,
    SET_TFCARD_STATE    = 3,
    SET_CACHE_MODE      = 4,
    SET_CACHE_SIZE      = 5,
    SET_STREAM_CALLBACK = 6,
    SET_STREAM_FD       = 7,
    SET_STREAM_FILEPATH = 8,
} MUXER_IOCTRL_CMD;

typedef enum AV_ENC_TYPE {
    VENC_CODEC_H264     = 0,
    VENC_CODEC_H265     = 1,
    VENC_CODEC_JPEG     = 2,
    AENC_CODEC_AAC      = 3,
} AV_ENC_TYPE;

typedef enum CODEC_TYPE {
    CODEC_TYPE_VIDEO    = 0,
    CODEC_TYPE_AUDIO    = 1,
    CODEC_TYPE_MAX      = 2,
} CODEC_TYPE;

typedef enum CODEC_ID {
    CODEC_ID_H264   = 0,
    CODEC_ID_H265   = 1,
    CODEC_ID_MPEG4  = 3,
    CODEC_ID_MJPEG  = 4,
    CODEC_ID_PCM    = 5,
    CODEC_ID_AAC    = 0x40,
    CODEC_ID_ADPCM  = 6,
} CODEC_ID;

typedef struct MediaTrackInfo {
    int mCreateTime;

    //video
    int mWidth;
    int mHeight;
    int mFrameRate;
    int mRotateDegree;
    int mMaxKeyInterval;
    AV_ENC_TYPE mVideoEncType;

    //audio
    int mChannelCnt;
    int mBitWidth;
    int mSampleRate;
    int mFrameSize;     // 1024 for aac encoder
    AV_ENC_TYPE mAudioEncType;
} MediaTrackInfo;

typedef struct AVPacket {
    int64_t pts;    // ms

    char* data0;
    int size0;
    char* data1;
    int size1;

    int stream_idx; // 0-video, 1-audio
    int flags;
    int duration;   // ms
} AVPacket;


//typedef struct MuxerWriter MuxerWriter;
typedef struct MuxerWriter {
    const char *info;
    void *mPrivContext;

    int (*MuxerOpen)(struct MuxerWriter *handle);
    int (*MuxerClose)(struct MuxerWriter *handle);
    int (*MuxerWriteCSD)(struct MuxerWriter *handle, char *csdData, int csdLen, int idx);
    int (*MuxerWriteHeader)(struct MuxerWriter *handle);
    int (*MuxerWriteTrailer)(struct MuxerWriter *handle);
    int (*MuxerWritePacket)(struct MuxerWriter *handle, AVPacket *pkt);
    int (*MuxerIOCtrl)(struct MuxerWriter *handle, int cmd, int param, void *data);
} MuxerWriter;

MuxerWriter *muxer_writer_create(int mode);
void muxer_writer_destroy(MuxerWriter *handle);

#endif // __MUXER_WRITER_H__
