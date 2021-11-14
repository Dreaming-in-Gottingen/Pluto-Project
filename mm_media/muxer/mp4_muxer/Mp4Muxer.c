#define MM_LOG_LEVEL 2
#define LOG_TAG "Mp4Muxer"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <mm_log.h>
#include "Mp4Muxer.h"

static void put_buffer_cache(MOVContext *mov, FsCacheWriter *cache, int8_t *buf, int size);
static inline void put_byte_cache(MOVContext *mov, FsCacheWriter *cache, int b);
static void put_le32_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val);
static void put_be32_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val);
static void put_be16_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val);
static void put_be24_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val);
static void put_tag_cache(MOVContext *mov, FsCacheWriter *cache, const char *tag);


static __inline int BSWAP32(unsigned int val)
{
    val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);\
    val= (val>>16) | (val<<16);
    return val;
}

static __inline short BSWAP16(unsigned short val)
{
    val= (val<<8) | (val>>8);
    return val;
}

static uint32_t movGetStcoTagSize(MOVTrack *track)
{
    uint32_t stcoTagSize = (track->stco_size+4)*4;
    return stcoTagSize;
}

/* Chunk offset atom */
static int mov_write_stco_tag(FsCacheWriter *pb, MOVTrack *track)
{
    uint32_t i;
    MOVContext *mov = track->mov;
    uint32_t stcoTagSize = movGetStcoTagSize(track);
    put_be32_cache(mov, pb, stcoTagSize); /* size */
    put_tag_cache(mov, pb, "stco");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, track->stco_size); /* entry count */

    //access stco only from dram
    if(track->stco_tiny_pages == 0){
        for (i=0; i<track->stco_size; i++) {
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STCO_ID][track->stream_type]++);
        }
    }
    //access from file
    else
    {
        ALOGW("record time too long! not support currently");
    }

    return stcoTagSize;
}

static uint32_t movGetStszTagSize(MOVTrack *track)
{
    uint32_t stszTagSize;
    stszTagSize = (track->stsz_size+5)*4;
    return stszTagSize;
}

/* Sample size atom */
static int mov_write_stsz_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t stszTagSize = movGetStszTagSize(track);
    uint32_t i;

    put_be32_cache(mov, pb, stszTagSize);       // size
    put_tag_cache(mov, pb, "stsz");
    put_be32_cache(mov, pb, 0);                 // version & flags
    put_be32_cache(mov, pb, 0);                 // sample size
    put_be32_cache(mov, pb, track->stsz_size);  // sample count

    if(track->stsz_tiny_pages == 0)
    {
        for (i=0; i<track->stsz_size; i++)
        {
            put_be32_cache(mov, pb, *mov->cache_read_ptr[STSZ_ID][track->stream_type]++);
        }
    }
    else
    {
        ALOGW("record time too long! not support currently");
    }

    return stszTagSize;
}

static uint32_t movGetStscTagSize(MOVTrack *track)
{
    uint32_t stscTagSize = (track->stsc_size*3 + 4)*4;
    return stscTagSize;
}

/* Sample to chunk atom */
static int mov_write_stsc_tag(FsCacheWriter *pb, MOVTrack *track)
{
    uint32_t i;
    MOVContext *mov = track->mov;
    uint32_t stscTagSize = movGetStscTagSize(track);

    put_be32_cache(mov, pb, stscTagSize);   // size
    put_tag_cache(mov, pb, "stsc");
    put_be32_cache(mov, pb, 0);             // version & flags

    put_be32_cache(mov, pb, track->stsc_size); // entry count

    if (track->stsc_tiny_pages == 0)
    {
        for (i=0; i<track->stsc_size; i++)
        {
            put_be32_cache(mov, pb, i+1); // first chunk
            put_be32_cache(mov, pb,*mov->cache_read_ptr[STSC_ID][track->stream_type]++);
            put_be32_cache(mov, pb, 0x1); // sample description index
        }
    }
    else
    {
        ALOGW("record time too long! not support currently");
    }

    return stscTagSize;
}

/* Sync sample atom */
static uint32_t movGetStssTagSize(MOVTrack *track)
{
    int keyframes = track->keyFrame_num;
    uint32_t stssTagSize = (keyframes+4)*4;
    return stssTagSize;
}

static int mov_write_stss_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    int i, keyframes;
    uint32_t stssTagSize = movGetStssTagSize(track);
    keyframes = track->keyFrame_num;

    put_be32_cache(mov, pb, stssTagSize);   // size
    put_tag_cache(mov, pb, "stss");
    put_be32_cache(mov, pb, 0);             // version & flags
    put_be32_cache(mov, pb, keyframes);     // entry count
    for (i=0; i<keyframes; i++)
    {
        put_be32_cache(mov, pb, mov->cache_keyframe_ptr[i]);
    }

    return stssTagSize;
}



static uint32_t descrLength(uint32_t len)
{
    int i;
    for (i=1; len>>(7*i); i++);
    return len + 1 + i;
}

static void putDescr_cache(MOVContext *mov, FsCacheWriter *pb, int tag, uint32_t size)
{
    int i= descrLength(size) - size - 2;
    put_byte_cache(mov, pb, tag);
    for (; i>0; i--)
    {
        put_byte_cache(mov, pb, (size>>(7*i)) | 0x80);
    }
    put_byte_cache(mov, pb, size & 0x7F);
}

static int movGetEsdsTagSize(MOVTrack *track)
{
    int esdsTagSize = 12;
    int DecoderSpecificInfoSize = track->csdLen ? descrLength(track->csdLen):0;
    int DecoderConfigDescriptorSize = descrLength(13 + DecoderSpecificInfoSize);
    int SLConfigDescriptorSize = descrLength(1);
    int ES_DescriptorSize = descrLength(3 + DecoderConfigDescriptorSize + SLConfigDescriptorSize);
    esdsTagSize += ES_DescriptorSize;
    return esdsTagSize;
}

static int mov_write_esds_tag(FsCacheWriter *pb, MOVTrack *track) // Basic
{
    MOVContext *mov = track->mov;
    uint32_t esdsTagSize = movGetEsdsTagSize(track);
    int decoderSpecificInfoLen = track->csdLen ? descrLength(track->csdLen):0;

    put_be32_cache(mov, pb, esdsTagSize);   // size
    put_tag_cache(mov, pb, "esds");
    put_be32_cache(mov, pb, 0);             // Version

    // ES descriptor
    putDescr_cache(mov, pb, 3, 3 + descrLength(13 + decoderSpecificInfoLen) + descrLength(1));
    put_be16_cache(mov, pb, track->trackID);
    put_byte_cache(mov, pb, 0x00);          // flags

    // DecoderConfig descriptor
    putDescr_cache(mov, pb, 0x04, 13 + decoderSpecificInfoLen);

    // Object type indication
    put_byte_cache(mov, pb, track->enc->codec_id);

    // the following fields is made of 6 bits to identify the streamtype (4 for video, 5 for audio)
    // plus 1 bit to indicate upstream and 1 bit set to 1 (reserved)
    if(track->enc->codec_type == CODEC_TYPE_AUDIO)
    {
        put_byte_cache(mov, pb, 0x15); // flags (= AudioStream)
    }
    else
    {
        put_byte_cache(mov, pb, 0x11); // flags (= VideoStream)
    }
    put_byte_cache(mov, pb,  0);        // Buffersize DB (24 bits)
    put_be16_cache(mov, pb,  0);        // Buffersize DB
    put_be32_cache(mov, pb, 0);         // maxbitrate (FIXME should be max rate in any 1 sec window)
    put_be32_cache(mov, pb, 0);         // vbr

    if (track->csdLen)
    {
        // DecoderSpecific info descriptor
        putDescr_cache(mov, pb, 0x05, track->csdLen);
        put_buffer_cache(mov, pb, (int8_t*)track->csdData, track->csdLen);
    }
    // SL descriptor
    putDescr_cache(mov, pb, 0x06, 1);
    put_byte_cache(mov, pb, 0x02);

    return esdsTagSize;
}

//add for h264 encoder
#define AV_RB32(x)  ((((const uint8_t*)(x))[0] << 24) | \
                     (((const uint8_t*)(x))[1] << 16) | \
                     (((const uint8_t*)(x))[2] <<  8) | \
                      ((const uint8_t*)(x))[3])

uint8_t *ff_avc_find_startcode(uint8_t *p, uint8_t *end)
{
    uint8_t *a = p + 4 - ((long)p & 3);

    for( end -= 3; p < a && p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    for( end -= 3; p < end; p += 4 ) {
        uint32_t x = *(const uint32_t*)p;
        if( (x - 0x01010101) & (~x) & 0x80808080 ) { // generic
            if( p[1] == 0 ) {
                if( p[0] == 0 && p[2] == 1 )
                    return p-1;
                if( p[2] == 0 && p[3] == 1 )
                    return p;
            }
            if( p[3] == 0 ) {
                if( p[2] == 0 && p[4] == 1 )
                    return p+1;
                if( p[4] == 0 && p[5] == 1 )
                    return p+2;
            }
        }
    }

    for( end += 3; p < end; p++ ) {
        if( p[0] == 0 && p[1] == 0 && p[2] == 1 )
            return p;
    }

    return end + 3;
}

int ff_avc_parse_nal_units(uint8_t *buf_in, uint8_t **buf, int *size)
{
    uint8_t *p = buf_in,*ptr_t;
    uint8_t *end = p + *size;
    uint8_t *nal_start, *nal_end;
    unsigned int nal_size,nal_size_b;

    ptr_t = *buf = malloc(*size + 256);
    nal_start = ff_avc_find_startcode(p, end);
    while (nal_start < end) {
        while(!*(nal_start++));
        nal_end = ff_avc_find_startcode(nal_start, end);
        nal_size = nal_end - nal_start;
        nal_size_b = BSWAP32(nal_size);
        memcpy(ptr_t,&nal_size_b,4);
        ptr_t += 4;
        memcpy(ptr_t,nal_start,nal_size);
        ptr_t += nal_size;
        nal_start = nal_end;
    }

    *size = ptr_t - *buf;
    return 0;
}

static uint32_t FFIsomGetAvccSize(uint8_t *data, int len)
{
    uint32_t avccSize = 0;
    if (len > 6)
    {
        /* check for h264 start code */
        if (AV_RB32(data) == 0x00000001)
        {
            uint8_t *buf=NULL, *end, *start;
            uint32_t sps_size=0, pps_size=0;
            uint8_t *sps=0, *pps=0;

            int ret = ff_avc_parse_nal_units(data, &buf, &len);
            if (ret < 0)
            {
                ALOGW("(f:%s, l:%d) fatal error! ret[%d] of ff_avc_parse_nal_units() < 0", __FUNCTION__, __LINE__, ret);
                return 0;
            }
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end)
            {
                unsigned int size;
                uint8_t nal_type;
                size = AV_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7)
                { /* SPS */
                    sps = buf + 4;
                    sps_size = size;
                }
                else if (nal_type == 8)
                { /* PPS */
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            avccSize = (6 + 2 + sps_size + 1 + 2 + pps_size);
            free(start);
        }
        else
        {
            avccSize = len;
        }
    }
    return avccSize;
}

int ff_isom_write_avcc(FsCacheWriter *pb, uint8_t *data, int len, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    if (len > 6) {
        /* check for h264 start code */
        if (AV_RB32(data) == 0x00000001) {
            uint8_t *buf=NULL, *end, *start;
            uint32_t sps_size=0, pps_size=0;
            uint8_t *sps=0, *pps=0;

            int ret = ff_avc_parse_nal_units(data, &buf, &len);
            if (ret < 0)
                return ret;
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end) {
                unsigned int size;
                uint8_t nal_type;
                size = AV_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) { /* SPS */
                    sps = buf + 4;
                    sps_size = size;
                } else if (nal_type == 8) { /* PPS */
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }

            put_byte_cache(mov, pb, 1); /* version */
            put_byte_cache(mov, pb, sps[1]); /* profile */
            put_byte_cache(mov, pb, sps[2]); /* profile compat */
            put_byte_cache(mov, pb, sps[3]); /* level */
            put_byte_cache(mov, pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            put_byte_cache(mov, pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            put_be16_cache(mov, pb, sps_size);
            put_buffer_cache(mov, pb, (int8_t*)sps, sps_size);
            put_byte_cache(mov, pb, 1); /* number of pps */
            put_be16_cache(mov, pb, pps_size);
            put_buffer_cache(mov, pb, (int8_t*)pps, pps_size);
            free(start);
        } else {
            put_buffer_cache(mov, pb, (int8_t*)data, len);
        }
    }
    return 0;
}

static uint32_t movGetAvccTagSize(MOVTrack *track)
{
    uint32_t avccTagSize = 8;
    avccTagSize += FFIsomGetAvccSize((uint8_t*)track->csdData, track->csdLen);
    return avccTagSize;
}

static int mov_write_avcc_tag(FsCacheWriter *pb, MOVTrack *track){
    MOVContext *mov = track->mov;
    uint32_t avccTagSize = movGetAvccTagSize(track);
    put_be32_cache(mov, pb, avccTagSize);   /* size */
    put_tag_cache(mov, pb, "avcC");
    ff_isom_write_avcc(pb, (uint8_t*)track->csdData, track->csdLen, track);
    return avccTagSize;
}

static uint32_t movGetAudioTagSize(MOVTrack *track)
{
    uint32_t audioTagSize = 16 + 8 + 12;
    if(track->tag == MKTAG('m','p','4','a'))
        audioTagSize += movGetEsdsTagSize(track);

    return audioTagSize;
}

static int mov_write_audio_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t audioTagSize = movGetAudioTagSize(track);
    int version = 0;

    put_be32_cache(mov, pb, audioTagSize); /* size */
    put_le32_cache(mov, pb, track->tag); // store it byteswapped
    put_be32_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 1); /* Data-reference index, XXX  == 1 */

    /* SoundDescription */
    put_be16_cache(mov, pb, version); /* Version */
    put_be16_cache(mov, pb, 0); /* Revision level */
    put_be32_cache(mov, pb, 0); /* vendor */

    { /* reserved for mp4/3gp */
        put_be16_cache(mov, pb, track->enc->channels); //channel
        put_be16_cache(mov, pb, track->enc->bits_per_sample);//bits per sample
        put_be16_cache(mov, pb, 0); /* compression id = 0*/
    }

    put_be16_cache(mov, pb, 0); /* packet size (= 0) */
    put_be16_cache(mov, pb, track->enc->sample_rate); /* Time scale !!!??? */
    put_be16_cache(mov, pb, 0); /* Reserved */

    if(version == 1) { /* SoundDescription V1 extended info */
       put_be32_cache(mov, pb, track->enc->frame_size); /* Samples per packet */
       put_be32_cache(mov, pb, track->sampleSize / track->enc->channels); /* Bytes per packet */
       put_be32_cache(mov, pb, track->sampleSize); /* Bytes per frame */
       put_be32_cache(mov, pb, 2); /* Bytes per sample */
    }

    if(track->tag == MKTAG('m','p','4','a'))
        mov_write_esds_tag(pb, track);

    return audioTagSize;
}

static int mov_find_codec_tag(MOVTrack *track)
{
    int tag = track->enc->codec_tag;

    switch(track->enc->codec_id)
    {
    case CODEC_ID_H264:
        tag = MKTAG('a','v','c','1');
        break;
    case CODEC_ID_MPEG4:
        tag = MKTAG('m','p','4','v');
        break;
    case CODEC_ID_AAC:
        tag = MKTAG('m','p','4','a');
        break;
    case CODEC_ID_PCM:
        tag = MKTAG('s','o','w','t');
        break;
    case CODEC_ID_ADPCM:
        tag = MKTAG('m','s',0x00,0x11);
        break;
    case CODEC_ID_MJPEG:
        tag = MKTAG('m','j','p','a');  /* Motion-JPEG (format A) */
        break;
    default:
        break;
    }

    return tag;
}

static uint32_t movGetVideoTagSize(MOVTrack *track)
{
    uint32_t videoTagSize = 16 + 4 + 12 + 18 + 32 + 4;
    if(track->tag == MKTAG('a','v','c','1'))
        videoTagSize += movGetAvccTagSize(track);
    else if(track->tag == MKTAG('m','p','4','v'))
        videoTagSize += movGetEsdsTagSize(track);
    return videoTagSize;
}

static int mov_write_video_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t videoTagSize = movGetVideoTagSize(track);
    int8_t compressor_name[32];

    put_be32_cache(mov, pb, videoTagSize); /* size */
    put_le32_cache(mov, pb, track->tag); // store it byteswapped
    put_be32_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 0); /* Reserved */
    put_be16_cache(mov, pb, 1); /* Data-reference index */

    put_be16_cache(mov, pb, 0); /* Codec stream version */
    put_be16_cache(mov, pb, 0); /* Codec stream revision (=0) */
    {
        put_be32_cache(mov, pb, 0); /* Reserved */
        put_be32_cache(mov, pb, 0); /* Reserved */
        put_be32_cache(mov, pb, 0); /* Reserved */
    }
    put_be16_cache(mov, pb, track->enc->width); /* Video width */
    put_be16_cache(mov, pb, track->enc->height); /* Video height */
    put_be32_cache(mov, pb, 0x00480000); /* Horizontal resolution 72dpi */
    put_be32_cache(mov, pb, 0x00480000); /* Vertical resolution 72dpi */
    put_be32_cache(mov, pb, 0); /* Data size (= 0) */
    put_be16_cache(mov, pb, 1); /* Frame count (= 1) */

    memset(compressor_name,0,32);
    put_byte_cache(mov, pb, strlen((const char *)compressor_name));
    put_buffer_cache(mov, pb, compressor_name, 31);

    put_be16_cache(mov, pb, 0x18); /* Reserved */
    put_be16_cache(mov, pb, 0xffff); /* Reserved */
    if(track->tag == MKTAG('a','v','c','1'))
        mov_write_avcc_tag(pb, track);
    else if(track->tag == MKTAG('m','p','4','v'))
        mov_write_esds_tag(pb, track);

    return videoTagSize;
}

static uint32_t movGetStsdTagSize(MOVTrack *track)
{
    uint32_t stsdTagSize = 16;
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        stsdTagSize += movGetVideoTagSize(track);
    else if (track->enc->codec_type == CODEC_TYPE_AUDIO)
        stsdTagSize += movGetAudioTagSize(track);
    return stsdTagSize;
}

static int mov_write_stsd_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t stsdTagSize = movGetStsdTagSize(track);
    put_be32_cache(mov, pb, stsdTagSize); /* size */
    put_tag_cache(mov, pb, "stsd");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, 1); /* entry count */
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_video_tag(pb, track);
    else if (track->enc->codec_type == CODEC_TYPE_AUDIO)
        mov_write_audio_tag(pb, track);

    return stsdTagSize;
}

static uint32_t movGetSttsTagSize(MOVTrack *track)
{
    uint32_t entries = 0;
    uint32_t atom_size = 0;
    if (track->enc->codec_type == CODEC_TYPE_AUDIO)
    {
        entries = 1;
        atom_size = 16 + (entries * 8);
    }
    else if (track->enc->codec_type == CODEC_TYPE_VIDEO)
    {
        entries = track->stts_size;
        atom_size = 16 + (entries * 8);
    }
    else
    {
        ALOGE("codec_type[%d] not support!", track->enc->codec_type);
    }
    return atom_size;
}

static int mov_write_stts_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    MOV_stts_t stts_entries[1];
    uint32_t entries = 0;
    uint32_t atom_size = 0;
    uint32_t i;

    if (track->enc->codec_type == CODEC_TYPE_AUDIO) {
        stts_entries[0].count = track->stsz_size;
        stts_entries[0].duration = track->enc->frame_size;
        entries = 1;
        atom_size = 16 + (entries * 8);
        put_be32_cache(mov, pb, atom_size); /* size */
        put_tag_cache(mov, pb, "stts");
        put_be32_cache(mov, pb, 0); /* version & flags */
        put_be32_cache(mov, pb, entries); /* entry count */
        for (i=0; i<entries; i++) {
            put_be32_cache(mov, pb, stts_entries[i].count);
            put_be32_cache(mov, pb, stts_entries[i].duration);
        }
    }
    else if (track->enc->codec_type == CODEC_TYPE_VIDEO) {
        int skip_first_frame = 1;
        entries = track->stts_size;
        atom_size = 16 + (entries * 8);
        put_be32_cache(mov, pb, atom_size); /* size */
        put_tag_cache(mov, pb, "stts");
        put_be32_cache(mov, pb, 0); /* version & flags */
        put_be32_cache(mov, pb, entries); /* entry count */

        if(track->stts_tiny_pages == 0){
            for (i=0; i<track->stts_size; i++) {
                if(skip_first_frame)
                {
                    skip_first_frame = 0;
                    mov->cache_read_ptr[STTS_ID][track->stream_type]++;
                    continue;
                }
                put_be32_cache(mov, pb, 1);//count
                put_be32_cache(mov, pb,*mov->cache_read_ptr[STTS_ID][track->stream_type]++);
            }
        } else {
        }

        //write last packet duration, set it to 0??
        put_be32_cache(mov, pb, 1);//count
        put_be32_cache(mov, pb, 0);
    }

    return atom_size;
}

static uint32_t movGetDrefTagSize()
{
    return 28;
}

static int mov_write_dref_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t drefTagSize = movGetDrefTagSize();
    put_be32_cache(mov, pb, drefTagSize); /* size */
    put_tag_cache(mov, pb, "dref");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be32_cache(mov, pb, 1); /* entry count */

    put_be32_cache(mov, pb, 0xc); /* size */
    put_tag_cache(mov, pb, "url ");
    put_be32_cache(mov, pb, 1); /* version & flags */

    return drefTagSize;
}

static uint32_t movGetStblTagSize(MOVTrack *track)
{
    uint32_t stblTagSize = 8;
    stblTagSize += movGetStsdTagSize(track);

    stblTagSize += movGetSttsTagSize(track);
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        stblTagSize += movGetStssTagSize(track);
    stblTagSize += movGetStscTagSize(track);
    stblTagSize += movGetStszTagSize(track);
    stblTagSize += movGetStcoTagSize(track);
    return stblTagSize;
}

static int mov_write_stbl_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t stblTagSize = movGetStblTagSize(track);
    put_be32_cache(mov, pb, stblTagSize); /* size */
    put_tag_cache(mov, pb, "stbl");
    mov_write_stsd_tag(pb, track);
    mov_write_stts_tag(pb, track);
    if (track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_stss_tag(pb, track);
    mov_write_stsc_tag(pb, track);
    mov_write_stsz_tag(pb, track);
    mov_write_stco_tag(pb, track);
    return stblTagSize;
}

static uint32_t movGetDinfTagSize()
{
    uint32_t dinfTagSize = 8;
    dinfTagSize += movGetDrefTagSize();
    return dinfTagSize;
}

static int mov_write_dinf_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t dinfTagSize = movGetDinfTagSize();
    put_be32_cache(mov, pb, dinfTagSize); /* size */
    put_tag_cache(mov, pb, "dinf");
    mov_write_dref_tag(pb, track);
    return dinfTagSize;
}

static uint32_t movGetSmhdTagSize()
{
    return 16;
}

static int mov_write_smhd_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t smhdTagSize = movGetSmhdTagSize();
    put_be32_cache(mov, pb, smhdTagSize); /* size */
    put_tag_cache(mov, pb, "smhd");
    put_be32_cache(mov, pb, 0); /* version & flags */
    put_be16_cache(mov, pb, 0); /* reserved (balance, normally = 0) */
    put_be16_cache(mov, pb, 0); /* reserved */
    return smhdTagSize;
}



static uint32_t movGetVmhdTagSize()
{
    return 0x14;
}

static int mov_write_vmhd_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t vmhdTagSize = movGetVmhdTagSize();
    put_be32_cache(mov, pb, vmhdTagSize); /* size (always 0x14) */
    put_tag_cache(mov, pb, "vmhd");
    put_be32_cache(mov, pb, 0x01); /* version & flags */
    put_be32_cache(mov, pb, 0x0);
    put_be32_cache(mov, pb, 0x0);
    return vmhdTagSize;
}

static uint32_t movGetHdlrTagSize(MOVTrack *track)
{
    uint32_t hdlrTagSize = 32 + 1;

    if (!track)
    { /* no media --> data handler */
        hdlrTagSize += strlen("DataHandler");
    }
    else
    {
        if (track->mov->title_info != NULL && track->mov->title_info[0]==0) {
            time_t raw_time;
            time(&raw_time);
            struct tm *info = gmtime(&raw_time);
            sprintf(track->mov->title_info, "ISO Media file by ZJZ. Create on: %d/%d/%d.", info->tm_mon+1, info->tm_mday, info->tm_year+1900);
        }
        hdlrTagSize += strlen(track->mov->title_info);
    }
    return hdlrTagSize;
}

static int mov_write_hdlr_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    char *descr = NULL, *hdlr, *hdlr_type = NULL;

    if (!track) { /* no media --> data handler */
        hdlr = "dhlr";
        hdlr_type = "url ";
        descr = "DataHandler";
    } else {
        hdlr = "\0\0\0\0";
        if (track->enc->codec_type == CODEC_TYPE_VIDEO) {
            hdlr_type = "vide";
        } else if (track->enc->codec_type == CODEC_TYPE_AUDIO){
            hdlr_type = "soun";
        }
        descr = track->mov->title_info;
    }
    uint32_t hdlrTagSize = movGetHdlrTagSize(track);
    put_be32_cache(mov, pb, hdlrTagSize); /* size */
    put_tag_cache(mov, pb, "hdlr");
    put_be32_cache(mov, pb, 0); /* Version & flags */
    put_buffer_cache(mov, pb, (int8_t*)hdlr, 4); /* handler */
    put_tag_cache(mov, pb, hdlr_type); /* handler type */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_be32_cache(mov, pb ,0); /* reserved */
    put_buffer_cache(mov, pb, (int8_t*)descr, strlen((const char *)descr)); /* handler description */
    put_byte_cache(mov, pb, 0); /* '\0' */
    return hdlrTagSize;
}

static uint32_t movGetMinfTagSize(MOVTrack *track)
{
    uint32_t minfTagSize = 8;
    if(track->enc->codec_type == CODEC_TYPE_VIDEO)
        minfTagSize += movGetVmhdTagSize();
    else if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        minfTagSize += movGetSmhdTagSize();

    minfTagSize += movGetDinfTagSize();
    minfTagSize += movGetStblTagSize(track);
    return minfTagSize;
}

static int mov_write_minf_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t minfTagSize = movGetMinfTagSize(track);
    put_be32_cache(mov, pb, minfTagSize); /* size */
    put_tag_cache(mov, pb, "minf");
    if(track->enc->codec_type == CODEC_TYPE_VIDEO)
        mov_write_vmhd_tag(pb, track);
    else if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        mov_write_smhd_tag(pb, track);
    mov_write_dinf_tag(pb, track);
    mov_write_stbl_tag(pb, track);
    return minfTagSize;
}

static uint32_t movGetMdhdTagSize()
{
    return 0x20;
}

static int mov_write_mdhd_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t mdhdTagSize = movGetMdhdTagSize();
    put_be32_cache(mov, pb, mdhdTagSize); /* size */
    put_tag_cache(mov, pb, "mdhd");
    put_byte_cache(mov, pb, 0);
    put_be24_cache(mov, pb, 0); /* flags */

    put_be32_cache(mov, pb, track->time); /* creation time */
    put_be32_cache(mov, pb, track->time); /* modification time */
    put_be32_cache(mov, pb, track->timescale); /* time scale (sample rate for audio) */
    put_be32_cache(mov, pb, track->trackDuration); /* duration */
    put_be16_cache(mov, pb, /*track->language*/0); /* language */
    put_be16_cache(mov, pb, 0); /* reserved (quality) */

    return mdhdTagSize;
}

static uint32_t movGetMdiaTagSize(MOVTrack *track)
{
    uint32_t mdiaTagSize = 8;
    mdiaTagSize += movGetMdhdTagSize();
    mdiaTagSize += movGetHdlrTagSize(track);
    mdiaTagSize += movGetMinfTagSize(track);
    return mdiaTagSize;
}

static int mov_write_mdia_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t mdiaTagSize = movGetMdiaTagSize(track);
    put_be32_cache(mov, pb, mdiaTagSize); /* size */
    put_tag_cache(mov, pb, "mdia");
    mov_write_mdhd_tag(pb, track);
    mov_write_hdlr_tag(pb, track);
    mov_write_minf_tag(pb, track);
    return mdiaTagSize;
}

static int av_rescale_rnd(int64_t a, int64_t b, int64_t c)
{
    return (a * b + c-1)/c;
}

static uint32_t movGetElstTagSize()
{
    uint32_t elstTagSize = 0x1c;
    return elstTagSize;
}

static int mov_write_elst_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t elstTagSize = movGetElstTagSize();
    uint32_t trakDuration = track->enc->frame_rate * track->stsz_size;
    put_be32_cache(mov, pb, elstTagSize); /* size */
    put_tag_cache(mov, pb, "elst");
    put_be32_cache(mov, pb, 0);
    put_be32_cache(mov, pb, 1);
    put_be32_cache(mov, pb, trakDuration);
    put_be32_cache(mov, pb, 0x010000);
    put_be32_cache(mov, pb, 0x00010000);
    return elstTagSize;
}

static uint32_t movGetEdtsTagSize()
{
    uint32_t edtsTagSize = 8;
    edtsTagSize += movGetElstTagSize();
    return edtsTagSize;
}

static int mov_write_edts_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t edtsTagSize = movGetEdtsTagSize();
    put_be32_cache(mov, pb, edtsTagSize); /* size */
    put_tag_cache(mov, pb, "edts");
    mov_write_elst_tag(pb, track);
    return edtsTagSize;
}

static uint32_t movGetTkhdTagSize()
{
    return 0x5c;
}

static int mov_write_tkhd_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    int64_t duration = av_rescale_rnd(track->trackDuration, globalTimescale, track->timescale);
    int version = 0;
    uint32_t tkhdTagSize = movGetTkhdTagSize();
    put_be32_cache(mov, pb, tkhdTagSize); /* size */
    put_tag_cache(mov, pb, "tkhd");
    put_byte_cache(mov, pb, version);
    put_be24_cache(mov, pb, 0xf); /* flags (track enabled) */

    put_be32_cache(mov, pb, track->time); /* creation time */
    put_be32_cache(mov, pb, track->time); /* modification time */

    put_be32_cache(mov, pb, track->trackID); /* track-id */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, duration);

    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved (Layer & Alternate group) */
    /* Volume, only for audio */
    if(track->enc->codec_type == CODEC_TYPE_AUDIO)
        put_be16_cache(mov, pb, 0x0100);
    else
        put_be16_cache(mov, pb, 0);
    put_be16_cache(mov, pb, 0); /* reserved */

#if 1
    {
        int degrees = track->enc->rotate_degree;
        uint32_t a = 0x00010000;
        uint32_t b = 0;
        uint32_t c = 0;
        uint32_t d = 0x00010000;
        switch (degrees) {
            case 0:
                break;
            case 90:
                a = 0;
                b = 0x00010000;
                c = 0xFFFF0000;
                d = 0;
                break;
            case 180:
                a = 0xFFFF0000;
                d = 0xFFFF0000;
                break;
            case 270:
                a = 0;
                b = 0xFFFF0000;
                c = 0x00010000;
                d = 0;
                break;
            default:
                ALOGE("Should never reach this unknown rotation");
                break;
        }

        put_be32_cache(mov, pb, a);           // a
        put_be32_cache(mov, pb, b);           // b
        put_be32_cache(mov, pb, 0);           // u
        put_be32_cache(mov, pb, c);           // c
        put_be32_cache(mov, pb, d);           // d
        put_be32_cache(mov, pb, 0);           // v
        put_be32_cache(mov, pb, 0);           // x
        put_be32_cache(mov, pb, 0);           // y
        put_be32_cache(mov, pb, 0x40000000);  // w
    }
#else
    /* Matrix structure */
    put_be32(pb, 0x00010000); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x00010000); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x0); /* reserved */
    put_be32(pb, 0x40000000); /* reserved */
#endif
    /* Track width and height, for visual only */
    if(track->enc->codec_type == CODEC_TYPE_VIDEO) {
        put_be32_cache(mov, pb, track->enc->width*0x10000);
        put_be32_cache(mov, pb, track->enc->height*0x10000);
    }
    else {
        put_be32_cache(mov, pb, 0);
        put_be32_cache(mov, pb, 0);
    }
    return tkhdTagSize;
}

static uint32_t movGetTrakTagSize(MOVTrack *track)
{
    uint32_t trakTagSize = 8;
    trakTagSize += movGetTkhdTagSize();
    trakTagSize += movGetMdiaTagSize(track);
    return trakTagSize;
}

static int mov_write_trak_tag(FsCacheWriter *pb, MOVTrack *track)
{
    MOVContext *mov = track->mov;
    uint32_t trakTagSize = movGetTrakTagSize(track);
    put_be32_cache(mov, pb, trakTagSize); /* size */
    put_tag_cache(mov, pb, "trak");
    mov_write_tkhd_tag(pb, track);

    mov_write_mdia_tag(pb, track);
    return trakTagSize;
}

static uint32_t movGetMvhdTagSize()
{
    return 108;
}

static int mov_write_mvhd_tag(FsCacheWriter *pb, MOVContext *mov)
{
    int maxTrackID = 1,i;

    for (i=0; i<mov->nb_streams; i++) {
        if(maxTrackID < mov->tracks[i].trackID)
            maxTrackID = mov->tracks[i].trackID;
    }
    uint32_t mvhdTagSize = movGetMvhdTagSize();
    put_be32_cache(mov, pb, mvhdTagSize); /* size */
    put_tag_cache(mov, pb, "mvhd");
    put_byte_cache(mov, pb, 0);
    put_be24_cache(mov, pb, 0); /* flags */

    put_be32_cache(mov, pb, mov->create_time); /* creation time */
    put_be32_cache(mov, pb, mov->create_time); /* modification time */
    put_be32_cache(mov, pb, mov->timescale); /* timescale */
    put_be32_cache(mov, pb, mov->tracks[0].trackDuration); /* duration of longest track */

    put_be32_cache(mov, pb, 0x00010000); /* reserved (preferred rate) 1.0 = normal */
    put_be16_cache(mov, pb, 0x0100); /* reserved (preferred volume) 1.0 = normal */
    put_be16_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */
    put_be32_cache(mov, pb, 0); /* reserved */

    /* Matrix structure */
    put_be32_cache(mov, pb, 0x00010000); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x00010000); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x0); /* reserved */
    put_be32_cache(mov, pb, 0x40000000); /* reserved */

    put_be32_cache(mov, pb, 0); /* reserved (preview time) */
    put_be32_cache(mov, pb, 0); /* reserved (preview duration) */
    put_be32_cache(mov, pb, 0); /* reserved (poster time) */
    put_be32_cache(mov, pb, 0); /* reserved (selection time) */
    put_be32_cache(mov, pb, 0); /* reserved (selection duration) */
    put_be32_cache(mov, pb, 0); /* reserved (current time) */
    put_be32_cache(mov, pb, maxTrackID+1); /* Next track id */
    return mvhdTagSize;
}

static void writeLatitude(FsCacheWriter *pb, MOVContext *mov, int degreex10000) {
    int isNegative = (degreex10000 < 0);
    char sign = isNegative? '-': '+';

    // Handle the whole part
    char str[9];
    int wholePart = degreex10000 / 10000;
    if (wholePart == 0) {
        snprintf(str, 5, "%c%.2d.", sign, wholePart);
    } else {
        snprintf(str, 5, "%+.2d.", wholePart);
    }

    // Handle the fractional part
    int fractionalPart = degreex10000 - (wholePart * 10000);
    if (fractionalPart < 0) {
        fractionalPart = -fractionalPart;
    }
    snprintf(&str[4], 5, "%.4d", fractionalPart);

    // Do not write the null terminator
    put_buffer_cache(mov, pb, (int8_t *)str, 8);
}

// Written in +/- DDD.DDDD format
static void writeLongitude(FsCacheWriter *pb, MOVContext *mov, int degreex10000) {
    int isNegative = (degreex10000 < 0);
    char sign = isNegative? '-': '+';

    // Handle the whole part
    char str[10];
    int wholePart = degreex10000 / 10000;
    if (wholePart == 0) {
        snprintf(str, 6, "%c%.3d.", sign, wholePart);
    } else {
        snprintf(str, 6, "%+.3d.", wholePart);
    }

    // Handle the fractional part
    int fractionalPart = degreex10000 - (wholePart * 10000);
    if (fractionalPart < 0) {
        fractionalPart = -fractionalPart;
    }
    snprintf(&str[5], 5, "%.4d", fractionalPart);

    // Do not write the null terminator
    put_buffer_cache(mov, pb, (int8_t *)str, 9);
}

static uint32_t movGetUdtaTagSize()
{
    uint32_t size = 4+4+4+4  //(be32_cache, tag_cache, be32_cache, tag_cache)
                +4+8+9+1;      //(0x001215c7, latitude, longitude, 0x2F)
    return size;
}

static int mov_write_udta_tag(FsCacheWriter *pb, MOVContext *mov)
{
    uint32_t udtaTagSize = movGetUdtaTagSize();

    put_be32_cache(mov, pb, udtaTagSize); /* size */
    put_tag_cache(mov, pb, "udta");

    put_be32_cache(mov, pb, udtaTagSize-8);
    put_tag_cache(mov, pb, "\xA9xyz");

    /*
     * For historical reasons, any user data start
     * with "\0xA9", must be followed by its assoicated
     * language code.
     * 0x0012: text string length
     * 0x15c7: lang (locale) code: en
     */
    put_be32_cache(mov, pb, 0x001215c7);

    writeLatitude(pb, mov, mov->latitudex10000);
    writeLongitude(pb, mov, mov->longitudex10000);
    put_byte_cache(mov, pb, 0x2F);

    return udtaTagSize;
}

static uint32_t movGetMoovTagSize(MOVContext *mov)
{
    int i;
    uint32_t size = 0;
    size += 8;  //size, "moov" tag,
    if(mov->geo_available)
    {
        size += movGetUdtaTagSize();
    }
    size += movGetMvhdTagSize();
    for (i=0; i<mov->nb_streams; i++)
    {
        if(mov->tracks[i].stsz_size> 0)
        {
            size += movGetTrakTagSize(&(mov->tracks[i]));
        }
    }
    return size;
}

static int mov_write_free_tag(FsCacheWriter *pb, MOVContext *mov, int size)
{
    put_be32_cache(mov, pb, size);
    put_tag_cache(mov, pb, "free");
    return size;
}

static int mov_write_moov_tag(FsCacheWriter *pb, MOVContext *mov)
{
    int i;
    int moov_size;
    moov_size = movGetMoovTagSize(mov);
    if (moov_size + 28 + 8 <= MOV_HEADER_RESERVE_SIZE) {
        pb->cache_seek(pb, mov->free_pos, SEEK_SET);
    }

    put_be32_cache(mov, pb, moov_size); /* size placeholder*/
    put_tag_cache(mov, pb, "moov");
    mov->timescale = globalTimescale;

    if(mov->geo_available) {
        mov_write_udta_tag(pb, mov);
    }

    for (i=0; i<mov->nb_streams; i++) {
        mov->tracks[i].time = mov->create_time;
        mov->tracks[i].trackID = i+1;
        mov->tracks[i].mov = mov;
        mov->tracks[i].stream_type = i;
    }

    mov_write_mvhd_tag(pb, mov);
    for (i=0; i<mov->nb_streams; i++) {
        if(mov->tracks[i].stsz_size> 0) {
            ALOGD("mov_write_trak_tag: track_id:[%d], stsz_size:[%d]", i, mov->tracks[i].stsz_size);
            mov_write_trak_tag(pb, &(mov->tracks[i]));
        }
    }

    if (moov_size + 28 + 8 <= MOV_HEADER_RESERVE_SIZE) {
        mov_write_free_tag(pb, mov, MOV_HEADER_RESERVE_SIZE - moov_size - 28);
    }

    return moov_size;
}

static __inline void put_byte_cache(MOVContext *mov, FsCacheWriter *cache, int b)
{
    put_buffer_cache(mov, cache, (int8_t*)&b, 1);
}

void put_buffer_cache(MOVContext *mov, FsCacheWriter *cache, int8_t *buf, int size)
{
    cache->cache_write(cache, (char*)buf, size);
    return;
}

void put_le32_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val)
{
    put_buffer_cache(mov, cache, (int8_t*)&val, 4);
}

void put_be32_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val)
{
    val= ((val<<8)&0xFF00FF00) | ((val>>8)&0x00FF00FF);
    val= (val>>16) | (val<<16);
    put_buffer_cache(mov, cache, (int8_t*)&val, 4);
}

void put_be16_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val)
{
    put_byte_cache(mov, cache, val >> 8);
    put_byte_cache(mov, cache, val);
}

void put_be24_cache(MOVContext *mov, FsCacheWriter *cache, uint32_t val)
{
    put_be16_cache(mov, cache, val >> 8);
    put_byte_cache(mov, cache, val);
}

void put_tag_cache(MOVContext *mov, FsCacheWriter *cache, const char *tag)
{
    while (*tag) {
        put_byte_cache(mov, cache, *tag++);
    }
}


static int mov_write_mdat_tag(FsCacheWriter *pb, MOVContext *mov)
{
    mov->mdat_pos = MOV_HEADER_RESERVE_SIZE;
    mov->mdat_start_pos = mov->mdat_pos + 8;
    pb->cache_seek(pb, mov->mdat_pos, SEEK_SET);

    put_be32_cache(mov, pb, 0); /* size placeholder */
    put_tag_cache(mov, pb, "mdat");
    return 0;
}

/* TODO: This needs to be more general */
static int mov_write_ftyp_tag(FsCacheWriter *pb, AVFormatContext *s)
{
    int minor = 0x200;
    MOVContext *mov = s->priv_data;
    put_be32_cache(mov, pb, 28); /* size */
    put_tag_cache(mov, pb, "ftyp");
    put_tag_cache(mov, pb, "isom");
    put_be32_cache(mov, pb, minor);

    put_tag_cache(mov, pb, "isom");
    put_tag_cache(mov, pb, "iso2");

    put_tag_cache(mov, pb, "mp41");
    return 0;
}

int mov_write_header(AVFormatContext *s)
{
    FsCacheWriter *pb = s->mpCacheWriter;
    MOVContext *mov = s->priv_data;
    int i;

    mov_write_ftyp_tag(pb,s);

    for(i=0; i<s->mTrackCnt; i++) {
        AVStream *st= s->streams[i];
        MOVTrack *track= &mov->tracks[i];

        track->enc = &st->codec;
        track->tag = mov_find_codec_tag(track);
    }
    mov_write_free_tag(pb, mov, MOV_HEADER_RESERVE_SIZE - 28);

    mov_write_mdat_tag(pb, mov);
    mov->nb_streams = s->mTrackCnt;
    mov->free_pos = 28;

    return 0;
}

int mov_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    MOVContext *mov = s->priv_data;
    FsCacheWriter *pb = s->mpCacheWriter;
    MOVTrack *trk = &mov->tracks[pkt->stream_idx];
    int size = pkt->size0 + pkt->size1;
    int tmpStrmByte = 0;

    if (NULL == mov || NULL == s->mpCacheWriter)
    {
        ALOGE("Why mov or cache_writer is null?!!");
        return -1;
    }

    if(pkt->stream_idx == -1)//last packet
    {
        *mov->cache_write_ptr[STSC_ID][mov->last_stream_idx]++ = mov->stsc_cnt;
        mov->tracks[mov->last_stream_idx].stsc_size++;
        return 0;
    }

    if(pkt->stream_idx != mov->last_stream_idx)
    {
        *mov->cache_write_ptr[STCO_ID][pkt->stream_idx]++ = mov->mdat_size + mov->mdat_start_pos;
        trk->stco_size++;
        mov->stco_cache_size[pkt->stream_idx]++;

        if(mov->last_stream_idx >= 0)
        {
            *mov->cache_write_ptr[STSC_ID][mov->last_stream_idx]++ = mov->stsc_cnt;
            mov->tracks[mov->last_stream_idx].stsc_size++;
            mov->stsc_cache_size[mov->last_stream_idx]++;

            if(mov->cache_write_ptr[STSC_ID][mov->last_stream_idx] > mov->cache_end_ptr[STSC_ID][mov->last_stream_idx])
            {
                mov->cache_write_ptr[STSC_ID][mov->last_stream_idx] = mov->cache_start_ptr[STSC_ID][mov->last_stream_idx];
            }

            if(mov->stsc_cache_size[mov->last_stream_idx] >= STSC_CACHE_SIZE)
            {
                ALOGE("record too long! stsc overflow! not support currently");
            }
        }
        mov->stsc_cnt = 0;

        if(mov->cache_write_ptr[STCO_ID][pkt->stream_idx] > mov->cache_end_ptr[STCO_ID][pkt->stream_idx])
        {
            mov->cache_write_ptr[STCO_ID][pkt->stream_idx] = mov->cache_start_ptr[STCO_ID][pkt->stream_idx];
        }

        if(mov->stco_cache_size[pkt->stream_idx] >= STCO_CACHE_SIZE)
        {
            ALOGE("record too long! stco overflow! not support currently");
        }
    }

    mov->last_stream_idx = pkt->stream_idx;
    *mov->cache_write_ptr[STSZ_ID][pkt->stream_idx]++ = size;
    trk->stsz_size++; //av stsz size
    mov->stsz_cache_size[pkt->stream_idx]++;
    mov->stsc_cnt++;

    if (mov->cache_write_ptr[STSZ_ID][pkt->stream_idx] > mov->cache_end_ptr[STSZ_ID][pkt->stream_idx])
    {
        mov->cache_write_ptr[STSZ_ID][pkt->stream_idx] = mov->cache_start_ptr[STSZ_ID][pkt->stream_idx];
    }
    if(mov->stsz_cache_size[pkt->stream_idx] >= STSZ_CACHE_SIZE)
    {
        ALOGE("record too long! stsz overflow! not support currently");
    }

    int bNeedSpecialWriteFlag = 0;
    int pkt_size = 0;
    if (pkt->stream_idx == 0)//video
    {
        *mov->cache_write_ptr[STTS_ID][pkt->stream_idx]++ = pkt->duration;

        trk->stts_size++;
        mov->stts_cache_size[pkt->stream_idx]++;

        if(mov->cache_write_ptr[STTS_ID][pkt->stream_idx] > mov->cache_end_ptr[STTS_ID][pkt->stream_idx])
        {
            mov->cache_write_ptr[STTS_ID][pkt->stream_idx] = mov->cache_start_ptr[STTS_ID][pkt->stream_idx];
        }

        if(mov->stts_cache_size[pkt->stream_idx] >= STTS_CACHE_SIZE)
        {
            ALOGE("record too long! stts overflow! not support currently");
        }

        if (pkt->size0 >= 5)
        {
            tmpStrmByte = pkt->data0[4];
        }
        else if (pkt->size0 > 0)
        {
            tmpStrmByte = pkt->data1[4 - pkt->size0];
        }
        else
        {
        }

        if (s->streams[pkt->stream_idx]->codec.codec_id == CODEC_ID_H264)
        {
            if((tmpStrmByte&0x1f) == 5)
            {
                mov->cache_keyframe_ptr[trk->keyFrame_num] = trk->stts_size;
                trk->keyFrame_num++;
            }

        }
        else
        {
            ALOGE("err video codec type!\n");
            return -1;
        }

        if(s->streams[0]->codec.codec_id == CODEC_ID_H264 && 0 != size)
        {
            pkt_size = BSWAP32(size-4);
            bNeedSpecialWriteFlag = 1;
        }
    }

    mov->mdat_size += size;

    int bNeedSkipDataFlag = 0;
    int nSkipSize = 0;
    if(pkt->size0)
    {
        if(0 == bNeedSpecialWriteFlag)
        {
            put_buffer_cache(mov, pb, (int8_t*)pkt->data0, pkt->size0);
        }
        else
        {
            put_buffer_cache(mov, pb, (int8_t*)&pkt_size, 4);
            if(pkt->size0 <= 4)
            {
                //ALOGD("(f:%s, l:%d) Be careful! pkt->size0[%d]<=4", __FUNCTION__, __LINE__, pkt->size0);
                bNeedSkipDataFlag = 1;
                nSkipSize = 4-pkt->size0;
            }
            else
            {
                put_buffer_cache(mov, pb, (int8_t*)pkt->data0+4, pkt->size0-4);
            }
        }
    }
    if(pkt->size1)
    {
        if(0 == bNeedSkipDataFlag)
        {
            put_buffer_cache(mov, pb, (int8_t*)pkt->data1, (int)pkt->size1);
        }
        else
        {
            if (pkt->size1<=nSkipSize)
            {
                ALOGE("(f:%s, l:%d) fatal error! size1[%d]<=skipSize[%d], check code!", __FUNCTION__, __LINE__, pkt->size1, nSkipSize);
            }
            put_buffer_cache(mov, pb, (int8_t*)pkt->data1+nSkipSize, (int)pkt->size1-nSkipSize);
        }
    }

    return 0;
}

int mov_write_trailer(AVFormatContext *s)
{
    MOVContext *mov = s->priv_data;
    FsCacheWriter *pb = s->mpCacheWriter;
    int filesize = 0;
    AVPacket pkt;

    pkt.data0 = 0;
    pkt.size0 = 0;
    pkt.data1 = 0;
    pkt.size1 = 0;
    pkt.stream_idx = -1;
    mov_write_packet(s, &pkt);//update last packet

    mov_write_moov_tag(pb, mov);
    pb->cache_seek(pb, mov->mdat_pos, SEEK_SET);
    put_be32_cache(mov, pb, mov->mdat_size+8);
    pb->cache_flush(pb);
    pb->cache_seek(pb, 0, SEEK_END);
    filesize = pb->cache_tell(pb);
    return filesize;
}
