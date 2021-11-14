#define MM_LOG_LEVEL 1
#define LOG_TAG "muxer_test"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <mm_log.h>
#include <mm_cache.h>
#include <mm_stream.h>
#include <muxer_writer.h>

#define USE_STREAM_TYPE 0 //0:fd; 1:filepath

int main()
{
    ALOGW("--------------muxer_test begin--------------");

    MuxerWriter *muxer = muxer_writer_create(MUXER_MODE_MP4);
    muxer->MuxerOpen(muxer);

#if USE_STREAM_TYPE == 0
    int fd = open("test_muxer.mp4", O_CREAT|O_RDWR, 0666);
    if (fd<0) {
        ALOGE("open file for test_muxer.mp4 fail!");
        return -1;
    }
    muxer->MuxerIOCtrl(muxer, SET_STREAM_FD, fd, NULL);
#elif USE_STREAM_TYPE == 1
    muxer->MuxerIOCtrl(muxer, SET_STREAM_FILEPATH, 0, "xyz.mp4");
#else
#error "-----error stream type!-----"
#endif

    muxer->MuxerIOCtrl(muxer, SET_CACHE_MODE, CACHEMODE_SIMPLE, NULL);
    muxer->MuxerIOCtrl(muxer, SET_CACHE_SIZE, 4096, NULL);

    MediaTrackInfo track_info;
    memset(&track_info, 0, sizeof(track_info));
    track_info.mWidth = 1280;
    track_info.mHeight = 720;
    track_info.mFrameRate = 30;
    track_info.mMaxKeyInterval = 30;
    track_info.mVideoEncType = VENC_CODEC_H264;
    track_info.mChannelCnt = 1;
    track_info.mBitWidth = 16;
    track_info.mSampleRate = 8000;
    track_info.mFrameSize = 1024;
    track_info.mAudioEncType = AENC_CODEC_AAC;
    muxer->MuxerIOCtrl(muxer, SET_TRACK_INFO, 0, &track_info);


    FILE *avbs_fp = fopen("extractor/V3_avbs", "rb");
    unsigned char *buf = malloc(1024*1024);
    unsigned char *text = malloc(64);
    int size0, size1;
    char *data0, *data1;
    int idx, flag, dura;
    long long pts;
    int ret;
    AVPacket pkt;

    memset(text, 0, 64);
    // video csd extract
    fread(text, 1, 15, avbs_fp);
    ret = sscanf(text, "$$AV$$csd%d,%d:", &idx, &size0);
    ALOGD("rd_cnt:%d, idx:%d, vcsd_len:%d", ret, idx, size0);
    fread(buf, 1, size0, avbs_fp);
    muxer->MuxerWriteCSD(muxer, buf, size0, idx);

    // audio csd extract
    fread(text, 1, 15, avbs_fp);
    ret = sscanf(text, "$$AV$$csd%d,%d:", &idx, &size0);
    ALOGD("rd_cnt:%d, idx:%d, acsd_len:%d", ret, idx, size0);
    fread(buf, 1, size0, avbs_fp);
    muxer->MuxerWriteCSD(muxer, buf, size0, idx);

    muxer->MuxerWriteHeader(muxer);

    // av packet extract
    while (fread(text, 1, 42, avbs_fp))
    {
        sscanf(text, "$$AV$$%d,%d,%lld,%d,%d,%d:", &idx, &flag, &pts, &dura, &size0, &size1);
        ALOGD("idx=%d, flag=%d, pts=%lld, dura=%d, size0=%#x, size1=%#x", idx, flag, pts, dura, size0, size1);
        ret = fread(buf, 1, size0, avbs_fp);
        if (0 != size1) {
            fread(buf+size0, 1, size1, avbs_fp);
        }
        pkt.pts = pts;
        pkt.data0 = buf;
        pkt.size0 = size0;
        pkt.data1 = size1?buf+size0:0;
        pkt.size1 = size1;
        pkt.stream_idx = idx;
        pkt.flags = flag;
        pkt.duration = dura;
        muxer->MuxerWritePacket(muxer, &pkt);
    }

    muxer->MuxerIOCtrl(muxer, SET_TOTAL_TIME, pts+dura, NULL);
    muxer->MuxerWriteTrailer(muxer);
    muxer->MuxerClose(muxer);

    muxer_writer_destroy(muxer);

    free(text);
    free(buf);
    fclose(avbs_fp);
#if USE_STREAM_TYPE == 0
    close(fd);
#endif

    ALOGW("--------------muxer_test end--------------");
    return 0;
}
