#ifndef __MP4_MUXER_H__
#define __MP4_MUXER_H__

#include <muxer_writer.h>
#include <mm_cache.h>

#define MAX_STREAMS 2
#define MAX_STREAMS_IN_FILE 2

#define MKTAG(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

//set it to 2K! attention it must set to below MOV_RESERVED_CACHE_ENTRIES
#define MOV_CACHE_TINY_PAGE_SIZE (1024*2)

#define MOV_CACHE_TINY_PAGE_SIZE_IN_BYTE (MOV_CACHE_TINY_PAGE_SIZE*4)
#define globalTimescale 1000

typedef struct AVRational{
    int32_t num; ///< numerator
    int32_t den; ///< denominator
} AVRational;

typedef struct AVCodecContext {

    int32_t bit_rate;
    int32_t width;
    int32_t height;
    CODEC_TYPE codec_type; /* see CODEC_TYPE_xxx */
    int32_t rotate_degree;
    uint32_t codec_tag;
    uint32_t codec_id;

    int32_t channels;
    int32_t frame_size;   //for audio, it means sample count a audioFrame contain;
                          //in fact, its value is assigned by pcmDataSize(MAXDECODESAMPLE=1024) which is transport by previous AudioSourceComponent.
                          //aac encoder will encode a frame from MAXDECODESAMPLE samples; but for pcm, one frame == one sample according to movSpec.
    int32_t frame_rate;

    int32_t bits_per_sample;
    int32_t sample_rate;

    AVRational time_base;
}AVCodecContext;

typedef struct AVStream {
    int32_t index;    /**< stream index in AVFormatContext */
    int32_t id;       /**< format specific stream id */
    AVCodecContext codec; /**< codec context */
    AVRational r_frame_rate;
    void *priv_data;

    AVRational time_base;
    long long duration;
} AVStream;

typedef struct AVFormatContext {
    AVRational time_base;
    void *priv_data;    //MOVContext
    mm_stream_info_t  *mpStream;
    int32_t mFallocateLen;
    int32_t mTrackCnt;
    AVStream *streams[MAX_STREAMS];
    char filename[1024]; /**< input or output filename */

    int8_t firstframe[2];
    uint32_t total_video_frames;
    uint8_t *mov_inf_cache;

    MediaDataSourceInfo datasource_desc;

    uint32_t mbTFCardOn;  //1:on, 0:off

    FsCacheWriter *mpCacheWriter;
    MEDIA_CACHEMODE mFsCacheMode;
    int32_t     mCacheSimpleSize;
} AVFormatContext;

typedef struct
{
    uint32_t extra_data_len;
    uint8_t extra_data[32];
} __extra_data_t;

typedef struct MOVContext MOVContext;
typedef struct MOVTrack {
    int32_t      mode;
    int32_t      entry;
    int32_t      timescale;
    int32_t      sampleDuration;
    int32_t      time;
    int64_t      trackDuration;
    int32_t      sampleCount;
    int32_t      sampleSize;
    int32_t      hasKeyframes;
    int32_t      hasBframes;
    int32_t      language;
    int32_t      trackID;
    int32_t      stream_type;
    MOVContext  *mov;//used for reverse access
    int32_t      tag; ///< stsd fourcc, e,g, 'sowt', 'mp4a'
    AVCodecContext *enc;

    int          csdLen;
    char        *csdData;

    uint32_t   stsz_size;
    uint32_t   stco_size;
    uint32_t   stsc_size;
    uint32_t   stts_size;
    uint32_t   stss_size;

    uint32_t   stsz_tiny_pages;
    uint32_t   stco_tiny_pages;
    uint32_t   stsc_tiny_pages;
    uint32_t   stts_tiny_pages;

    uint32_t   keyFrame_num;
} MOVTrack;

typedef enum CHUNKID
{
    STCO_ID = 0,//don't change all
    STSZ_ID = 1,
    STSC_ID = 2,
    STTS_ID = 3
} CHUNKID;

//1280*720. 30fps, 17.5 minute
#define STCO_CACHE_SIZE  (8*1024)          //about 1 hours !must times of tiny_page_size
#define STSZ_CACHE_SIZE  (8*1024*4) //(8*1024*16)       //about 1 hours !must times of tiny_page_size
#define STTS_CACHE_SIZE  (STSZ_CACHE_SIZE)      //about 1 hours !must times of tiny_page_size
#define STSC_CACHE_SIZE  (STCO_CACHE_SIZE)      //about 1 hours !must times of tiny_page_size

#define TOTAL_CACHE_SIZE (STCO_CACHE_SIZE*2 + STSZ_CACHE_SIZE*2 + STSC_CACHE_SIZE*2 + STTS_CACHE_SIZE + MOV_CACHE_TINY_PAGE_SIZE) //32bit

#define KEYFRAME_CACHE_SIZE (8*1024*16)

#define MOV_HEADER_RESERVE_SIZE (1024*1024)


typedef struct MOVContext {
    int32_t     mode;
    int64_t     create_time;
    int32_t     nb_streams;
    int32_t     mdat_pos;
    int32_t     mdat_start_pos;//raw bitstream start pos
    int32_t     mdat_size;
    int32_t     timescale;
    int32_t     geo_available;
    int32_t     latitudex10000;
    int32_t     longitudex10000;
    int32_t	    rotate_degree;

    MOVTrack tracks[MAX_STREAMS];

    uint32_t *cache_start_ptr[4][MAX_STREAMS_IN_FILE]; //[4]stco,stsz,stsc,stts [2]video audio
    uint32_t *cache_end_ptr[4][MAX_STREAMS_IN_FILE];
    uint32_t *cache_write_ptr[4][MAX_STREAMS_IN_FILE];
    uint32_t *cache_read_ptr[4][MAX_STREAMS_IN_FILE];
    uint32_t *cache_tiny_page_ptr;
    uint32_t *cache_keyframe_ptr;

    int32_t stsz_cache_size[MAX_STREAMS_IN_FILE];
    int32_t stco_cache_size[MAX_STREAMS_IN_FILE];
    int32_t stsc_cache_size[MAX_STREAMS_IN_FILE];
    int32_t stts_cache_size[MAX_STREAMS_IN_FILE];

    int32_t stsc_cnt;             //samples in one chunk
    int32_t last_stream_idx;
    int32_t keyframe_interval;
    int64_t last_video_packet_pts;

    int32_t keyframe_num;
    uint32_t free_pos;

    char* title_info;
} MOVContext_t;

typedef struct {
    int32_t count;
    int32_t duration;
} MOV_stts_t;

int32_t mov_write_header(AVFormatContext *s);
int32_t mov_write_packet(AVFormatContext *s, AVPacket *pkt);
int32_t mov_write_trailer(AVFormatContext *s);

#endif  // __MP4_MUXER_H__
