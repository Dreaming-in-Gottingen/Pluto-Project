//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "Mpeg2tsMuxer"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <mm_log.h>
#include "Mpeg2tsMuxer.h"

#define NAL_AUD_SZ 6
const char nal_aud[NAL_AUD_SZ] = {0,0,0,1,0x09,0xf0};

void initCrcTable(AVFormatContext *ctx)
{
    unsigned int poly = 0x04C11DB7;
    unsigned int crc, i, j;

    for (i = 0; i < 256; i++) {
        crc = i << 24;
        for (j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80000000) ? (poly) : 0);
        }
        ctx->mCrcTable[i] = crc;
    }
}

/**
 * Compute CRC32 checksum for buffer starting at offset start and for length bytes.
 */
static unsigned int crc32(AVFormatContext *ctx, const unsigned char *start, int length)
{
    unsigned int crc = 0xFFFFFFFF;
    const unsigned char *p;

    for (p = start; p < start + length; p++) {
        crc = (crc << 8) ^ ctx->mCrcTable[((crc >> 24) ^ *p) & 0xFF];
    }

    return crc;
}

void write_program_association_table(AVFormatContext *ctx)
{
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0000000000000 (13 bits)
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // --- payload follows
    // table_id = 0x00
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x00d
    // transport_stream_id = 0x0000
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    //   one program follows:
    //   program_number = 0x0001
    //   reserved = b111
    //   program_map_PID = 0x01e0 (13 bits!)
    // CRC = 0x????????
    static char kPATStartData[] = {
        0x47,
        0x40, 0x00, 0x10, 0x00,  // b0100 0000 0000 0000 0001 ???? 0000 0000
        0x00, 0xb0, 0x0d, 0x00,  // b0000 0000 1011 0000 0000 1101 0000 0000
        0x00, 0xc3, 0x00, 0x00,  // b0000 0000 1100 0011 0000 0000 0000 0000
        0x00, 0x01, 0xe1, 0xe0,  // b0000 0000 0000 0001 1110 0001 1110 0000
        0xe8, 0x5f, 0x74, 0xec   //CRC val of kData[5-16]
    };
    memset(ctx->mpTSBuf, 0xff, TS_PACKAGE_SZ);
    memcpy(ctx->mpTSBuf, kPATStartData, sizeof(kPATStartData));
    ctx->mpTSBuf[3] |= ctx->mPATContinuityCounter++;
    if (ctx->mPATContinuityCounter >= 0x10) {
        ctx->mPATContinuityCounter = 0;
    }
    //unsigned int crc = htonl(crc32(ctx, &ctx->mpTSBuf[5], 12));
    //memcpy(ctx->mpTSBuf+17, &crc, 4);

    ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mpTSBuf, TS_PACKAGE_SZ);
}


void write_program_map_table(AVFormatContext *ctx)
{
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 0000 (13 bits) [0x1e0]
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // -- payload follows
    // table_id = 0x02
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x???
    // program_number = 0x0001
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    // reserved = b111
    // PCR_PID = b? ???? ???? ???? (13 bits)
    // reserved = b1111
    // program_info_length = 0x000
    //   one or more elementary stream descriptions follow:
    //   stream_type = 0x??
    //   reserved = b111
    //   elementary_PID = b? ???? ???? ???? (13 bits)
    //   reserved = b1111
    //   ES_info_length = 0x000
    // CRC = 0x????????
    static char kPMTStartData[] = {
        0x47,
        0x41, 0xe0, 0x10, 0x00,  // b0100 0001 1110 0000 0001 ???? 0000 0000
        0x02, 0xb0, 0x00, 0x00,  // b0000 0010 1011 ???? ???? ???? 0000 0000
        0x01, 0xc3, 0x00, 0x00,  // b0000 0001 1100 0011 0000 0000 0000 0000
        0xe0, 0x00, 0xf0, 0x00   // b111? ???? ???? ???? 1111 0000 0000 0000
    };
    memset(ctx->mpTSBuf, 0xff, TS_PACKAGE_SZ);
    memcpy(ctx->mpTSBuf, kPMTStartData, sizeof(kPMTStartData));
    ctx->mpTSBuf[3] |= ctx->mPMTContinuityCounter++;
    if (ctx->mPMTContinuityCounter >= 0x10) {
        ctx->mPMTContinuityCounter = 0;
    }

    int section_length = 9 + 5 * ctx->mTrackCnt + 4;
    ctx->mpTSBuf[6] |= section_length >> 8;
    ctx->mpTSBuf[7] = section_length & 0xff;

    static const int kPcrPid = 0x1e1;
    ctx->mpTSBuf[13] |= (kPcrPid >> 8) & 0x1f;
    ctx->mpTSBuf[14] |= kPcrPid & 0xff;

    char *ptr = ctx->mpTSBuf+sizeof(kPMTStartData);
    int i;
    for (i = 0; i < ctx->mTrackCnt; ++i) {
        *ptr++ = (i==0 ? 0x1b : 0x0f);      //stream_type: 0-avc-0x1b; 1-aac-0x0f
        const unsigned ES_PID = ctx->mTrackPid[i];
        *ptr++ = 0xe0 | (ES_PID >> 8);
        *ptr++ = ES_PID & 0xff;
        *ptr++ = 0xf0;
        *ptr++ = 0x00;
    }

    unsigned int crc = htonl(crc32(ctx, ctx->mpTSBuf+5, 12+5*ctx->mTrackCnt));
    memcpy(ctx->mpTSBuf+17+5*ctx->mTrackCnt, &crc, sizeof(crc));
    ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mpTSBuf, TS_PACKAGE_SZ);
}

static void write_av_ts_packet(AVFormatContext *ctx, AVPacket *pkt,  int tsPktLeftSize, int *pStreamLeftSize, char **pStreamPtr)
{
    if (tsPktLeftSize <= *pStreamLeftSize)
    {
        int curDataBlockNum = 0;    // current position is in data0 or data1
        int curDataBlockLeftSize;   // left size need to write in data0 or data1
        if ((*pStreamPtr >= pkt->data0) && (*pStreamPtr < pkt->data0 + pkt->size0))
        {
            curDataBlockNum = 0;
            curDataBlockLeftSize = pkt->data0 + pkt->size0 - *pStreamPtr;
        }
        else
        {
            curDataBlockNum = 1;
            curDataBlockLeftSize = pkt->data1 + pkt->size1 - *pStreamPtr;
        }

        if (tsPktLeftSize <= curDataBlockLeftSize)
        {
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, *pStreamPtr, tsPktLeftSize);
            *pStreamLeftSize -= tsPktLeftSize;
            *pStreamPtr += tsPktLeftSize;
        }
        else
        {
            assert(curDataBlockNum == 0);
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, *pStreamPtr, curDataBlockLeftSize);
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, pkt->data1, tsPktLeftSize - curDataBlockLeftSize);
            *pStreamLeftSize -= tsPktLeftSize;
            *pStreamPtr = pkt->data1 + (tsPktLeftSize - curDataBlockLeftSize);
        }
    }
    else
    {
        ALOGE("(f:%s, l:%d) this case can NOT happen! check code!!!", __FUNCTION__, __LINE__);
        ALOGE("(f:%s, l:%d) dump info: (data0[%p],size0[%d]), (data1[%p],size1[%d]), tsPktLeftSize[%d] > *pStreamLeftSize[%d]",
            __FUNCTION__, __LINE__, pkt->data0, pkt->size0, pkt->data1, pkt->size1, tsPktLeftSize, *pStreamLeftSize);
        assert(0);
    }
}


static void write_access_unit(AVFormatContext *ctx, AVPacket *pkt)
{
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 ???? (13 bits) [0x1e0 + 1 + sourceIndex]
    // transport_scrambling_control = b00
    // adaptation_field_control = b??
    // continuity_counter = b????
    // adaptation_field_length = b????????
    // pcr_flag = 0x10
    // pcr_value = 6Bytes
    // -- payload follows
    // packet_startcode_prefix = 0x000001
    // stream_id = 0x?? (0xe0 for avc video, 0xc0 for aac audio)
    // PES_packet_length = 0x????
    // reserved = b10
    // PES_scrambling_control = b00
    // PES_priority = b0
    // data_alignment_indicator = b1
    // copyright = b0
    // original_or_copy = b0
    // PTS_DTS_flags = b10  (PTS only)
    // ESCR_flag = b0
    // ES_rate_flag = b0
    // DSM_trick_mode_flag = b0
    // additional_copy_info_flag = b0
    // PES_CRC_flag = b0
    // PES_extension_flag = b0
    // PES_header_data_length = 0x05
    // reserved = b0010 (PTS)
    // PTS[32..30] = b???
    // reserved = b1
    // PTS[29..15] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // PTS[14..0] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // the first fragment of "buffer" follows

    bool isVideo;
    unsigned int stream_size; // csd(aud+spspps/adts_header) + size0 + size1

    if (pkt->stream_idx == CODEC_TYPE_VIDEO)
    {
        isVideo = true;

        if (pkt->flags) {   //AVPACKET_FLAG_KEYFRAME
            stream_size = pkt->size0 + pkt->size1 + NAL_AUD_SZ + ctx->mTrackCSD[CODEC_TYPE_VIDEO].nCsdLen;
        } else {
            stream_size = pkt->size0 + pkt->size1 + NAL_AUD_SZ;
        }
    }
    else if (pkt->stream_idx == CODEC_TYPE_AUDIO)
    {
        isVideo = false;
        stream_size = pkt->size0 + pkt->size1 + 7;
    }
    else
    {
        assert(0);
    }

    unsigned int PID = ctx->mTrackPid[pkt->stream_idx];
    //unsigned int continuity_counter = ctx->mTSContinuityCounter[pkt->stream_idx]++;
    float tm = pkt->pts/1000 + (pkt->pts%1000)*0.001;
    long long PCR = tm * 27000000;
    //long long PTS = pkt->pts * 900LL;
    unsigned int PTS = (pkt->pts * 1000 * 9ll)/100ll;

    unsigned int stream_id = isVideo ? 0xe0 : 0xc0;
    unsigned int PES_packet_length = stream_size + 8;   //8:the bytes below untill nal_aud or adts_header
    if (PES_packet_length >= 65535)
    {
        // This really should only happen for video.
        // It's valid to set this to 0 for video according to the specs.
        PES_packet_length = 0;
    }

    unsigned int padding_size = 0;
    if (stream_size < (TS_PACKAGE_SZ - 18 - 8))
    {
        ALOGW("(f:%s, l:%d) Be careful: stream_size[%d] too small! size0[%d], size1[%d], type[%d], this should be aac frame!",
            __FUNCTION__, __LINE__, stream_size, pkt->size0, pkt->size1, pkt->stream_idx);
        padding_size = TS_PACKAGE_SZ - 18 - 8;
    }

    memset(ctx->mpTSBuf, 0xff, TS_PACKAGE_SZ);
    unsigned char *ptr = ctx->mpTSBuf;

    // common header, 4Bytes
    *ptr++ = 0x47;
    *ptr++ = 0x40 | (PID>>8);
    *ptr++ = PID & 0xff;
    *ptr++ = 0x30 | ctx->mTSContinuityCounter[pkt->stream_idx]++;
    if (ctx->mTSContinuityCounter[pkt->stream_idx] >= 0x10) {
        ctx->mTSContinuityCounter[pkt->stream_idx] = 0;
    }

    // adaptation field, 8Bytes or more(8+padding_size)
    *ptr++ = padding_size + 7;   // adaptation_field_length
    *ptr++ = 0x10;        // PCR FLAG
    long long pcr_low = PCR % 300;
    long long pcr_high = PCR / 300;
    *ptr++ = pcr_high >> 25;
    *ptr++ = pcr_high >> 17;
    *ptr++ = pcr_high >> 9;
    *ptr++ = pcr_high >> 1;
    *ptr++ = pcr_high << 7 | pcr_low>>8 | 0x7e;
    *ptr++ = pcr_low;

    // payload, 14Bytes
    ptr += padding_size;
    *ptr++ = 0x00;
    *ptr++ = 0x00;
    *ptr++ = 0x01;
    *ptr++ = stream_id;
    *ptr++ = PES_packet_length >> 8;
    *ptr++ = PES_packet_length & 0xff;
    *ptr++ = 0x84;
    *ptr++ = 0x80;
    *ptr++ = 0x05;
    // restore formula: PTS = ((val>>1)&0x7f) | ((val>>8)&0xff)<<7 | ((val>>(16+1))&0x7f)<<15 | ((val>>24)&0xff)<<22 | ((val>>(32+1))&7)<<30
    // PTS len: 33 bits = 3 + 8 + 7 + 8 + 7
    *ptr++ = 0x20 | (((PTS >> 30) & 7) << 1) | 1;
    *ptr++ = (PTS >> 22) & 0xff;
    *ptr++ = (((PTS >> 15) & 0x7f) << 1) | 1;
    *ptr++ = (PTS >> 7) & 0xff;
    *ptr++ = ((PTS & 0x7f) << 1) | 1;

    // deep copy data0+data1 to cache_writer
    ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mpTSBuf, /*4+8+14*/(ptr - ctx->mpTSBuf));
    int ts_pkt_left_size = TS_PACKAGE_SZ - (ptr - ctx->mpTSBuf);

    int stream_left_size = pkt->size0 + pkt->size1; //left bs size need to write file, this var will decrease till size0+size1
    //int stream_offset = 0;      // real offset in data0+data1, not include csd, we assume data0 and data1 are in a plane address, increasing
    //int copy_size = 0;          // have copied size, until to size0+size1
    char* stream_ptr = pkt->data0;  // next position for next copy
    if (isVideo)
    {
        ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, nal_aud, NAL_AUD_SZ);
        ts_pkt_left_size -= NAL_AUD_SZ;
        if (pkt->flags)
        {
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mTrackCSD[CODEC_TYPE_VIDEO].npCsdData, ctx->mTrackCSD[CODEC_TYPE_VIDEO].nCsdLen);
            ts_pkt_left_size -= ctx->mTrackCSD[CODEC_TYPE_VIDEO].nCsdLen;
        }

        write_av_ts_packet(ctx, pkt, ts_pkt_left_size, &stream_left_size, &stream_ptr);
    }
    else
    {
        unsigned char *csd_data = (unsigned char *)ctx->mTrackCSD[CODEC_TYPE_AUDIO].npCsdData;
        // csd width: 5 + 4 + 4 + 3 = 2 Bytes
        // 5:profile, 4:sample_idx, 4:channel, 3:not use

        unsigned profile = (csd_data[0] >> 3) - 1;
        unsigned sampling_freq_index = ((csd_data[0] & 7) << 1) | (csd_data[1] >> 7);
        unsigned channel_configuration = (csd_data[1] >> 3) & 0x0f;
        int aac_frame_length = stream_size;

        unsigned char adts_header[7];
        adts_header[0] = 0xff;
        adts_header[1] = 0xf1;  // b11110001, ID=0, layer=0, protection_absent=1
        adts_header[2] = (profile << 6) | (sampling_freq_index << 2) | ((channel_configuration >> 2) & 1);// private_bit=0
        adts_header[3] = ((channel_configuration & 3) << 6) | (aac_frame_length >> 11);    // original_copy=0, home=0, copyright_id_bit=0, copyright_id_start=0
        adts_header[4] = (aac_frame_length >> 3) & 0xff;
        adts_header[5] = (aac_frame_length & 7) << 5;
        adts_header[6] = 0;     // adts_buffer_fullness=0, number_of_raw_data_blocks_in_frame=0
        ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, adts_header, 7);
        ts_pkt_left_size -= 7;
        write_av_ts_packet(ctx, pkt, ts_pkt_left_size, &stream_left_size, &stream_ptr);
    }

    while (stream_left_size)
    {
        // for subsequent fragments of "buffer":
        // 0x47
        // transport_error_indicator = b0
        // payload_unit_start_indicator = b0
        // transport_priority = b0
        // PID = b0 0001 1110 ???? (13 bits) [0x1e0 + 1 + sourceIndex]
        // transport_scrambling_control = b00
        // adaptation_field_control = b??
        // continuity_counter = b????
        // the fragment of "buffer" follows.

        bool lastAccessUnit = (stream_left_size < (TS_PACKAGE_SZ-4));

        memset(ctx->mpTSBuf, 0xff, TS_PACKAGE_SZ);
        ctx->mpTSBuf[0] = 0x47;
        ctx->mpTSBuf[1] = PID>>8;
        ctx->mpTSBuf[2] = PID & 0xff;
        ctx->mpTSBuf[3] = (lastAccessUnit ? 0x30 : 0x10) | ctx->mTSContinuityCounter[pkt->stream_idx];
        if (ctx->mTSContinuityCounter[pkt->stream_idx]++ >= 0x0f)
        {
            ctx->mTSContinuityCounter[pkt->stream_idx] = 0;
        }
        if (lastAccessUnit)
        {
            // Pad packet using an adaptation field
            // Adaptation header all to 0 execpt size
            int paddingSize = (TS_PACKAGE_SZ-4) - stream_left_size;
            ctx->mpTSBuf[4] = paddingSize - 1;
            if (paddingSize >= 2)
            {
                ctx->mpTSBuf[5] = 0;
            }
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mpTSBuf, paddingSize+4);
            ts_pkt_left_size = TS_PACKAGE_SZ - 4 - paddingSize;
        }
        else
        {
            ctx->mpCacheWriter->cache_write(ctx->mpCacheWriter, ctx->mpTSBuf, 4);
            ts_pkt_left_size = TS_PACKAGE_SZ - 4;
        }
        write_av_ts_packet(ctx, pkt, ts_pkt_left_size, &stream_left_size, &stream_ptr);
    }
}

int mpeg2ts_write_packet(AVFormatContext *ctx, AVPacket *pkt)
{
    if (pkt->stream_idx == VIDEO_STREAM_IDX)
    {
        write_program_association_table(ctx);
        write_program_map_table(ctx);
        write_access_unit(ctx, pkt);
    }
    else if (pkt->stream_idx == AUDIO_STREAM_IDX)
    {
        write_access_unit(ctx, pkt);
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! unknown stream_idx[%d], check code!!!", __FUNCTION__, __LINE__, pkt->stream_idx);
    }

    return 0;
}

int mpeg2ts_write_trailer(AVFormatContext *ctx)
{
    int filesize = 0;
    ctx->mpCacheWriter->cache_flush(ctx->mpCacheWriter);
    ctx->mpCacheWriter->cache_seek(ctx->mpCacheWriter, 0, SEEK_END);
    filesize = ctx->mpCacheWriter->cache_tell(ctx->mpCacheWriter);
    return filesize;
}
