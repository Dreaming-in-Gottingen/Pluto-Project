#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define TS_PKT_SIZE 188
// private define, not use this any more.
#define VIDEO_PID 0xE1
#define AUDIO_PID 0xE2

#define AUDIO_STREAM_ID 0xC0
#define VIDEO_STREAM_ID 0xE0

void main()
{
    puts("--------------------------check_ts_muxer begin---------------------------------");
    FILE *fp0 = fopen("test_muxer.ts", "rb");
    FILE *fp1, *fp2;
    //FILE *fp1 = fopen("check_ts_video.h264", "wb");
    //FILE *fp2 = fopen("check_ts_audio.aac", "wb");
    //printf("open file -> fp0:%p, fp1:%p, fp2:%p\n", fp0, fp1, fp2);
    //if (fp0==NULL || fp1==NULL || fp2==NULL)
    //{
    //    printf("open file fail! exit exe!");
    //    return;
    //}

    unsigned char *buf = malloc(1000*1000);

    int audio_pid;
    int video_pid;
    // below is Android mpeg2ts container format, not FFmpeg libavformat format.
    // PAT: 47 40 00 10 00 00 B0 0D 00 00 C3 00 00 00 01 E1 E0 E8 5F 74 EC
    // PMT: 47 41 E0 10 00 02 B0 17 00 01 C3 00 00 E1 E1 F0 00 0F E1 E1 F0 00 1B E1 E2 F0 00 15 F2 E7 9C
    // AAC: 47 41 E1 3F 07 10 00 F9 F2 B6 FE B3 00 00 01 C0 01 6A 84 80 05 21 07 CF CA DB ...(bitstream)...
    // AVC: 47 41 E2 3D 07 10 00 F9 F2 B6 FE B3 00 00 01 E0 98 65 84 80 05 21 07 CF CA DB ...(bitstream)...
    // get track_pid by stream_id
    int pat_found = 0;
    int pmt_found = 0;
    int audio_found = 0;
    int video_found = 0;
    while ((pat_found==0) || (pmt_found==0) || (audio_found==0) || (video_found==0))
    {
        fread(buf, 1, TS_PKT_SIZE, fp0);
        if (buf[0]==0x47 && buf[1]==0x40 && buf[2]==0x00)
        {
            if (pat_found) continue;
            printf("[0-4]: %#x, %#x, %#x, %#x, %#x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            printf("    PAT element found!\n");
            pat_found = 1;
        }
        else if (buf[0]==0x47 && buf[1]==0x41 && buf[2]==0xE0)
        {
            if (pmt_found) continue;
            printf("[0-4]: %#x, %#x, %#x, %#x, %#x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            printf("    PMT element found!\n");
            pmt_found = 1;
        }
        else if (buf[0]==0x47 && buf[1]==0x41 && (buf[2]&0xE0)==0xE0)
        {
            if (buf[4] == 0x07)
            {
                assert(buf[12]==0 && buf[13]==0 && buf[14]==1);
                if (buf[15] == AUDIO_STREAM_ID)
                {
                    if (audio_found) continue;
                    printf("[0-4]: %#x, %#x, %#x, %#x, %#x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
                    audio_pid = ((buf[1]&0xf)<<8) + buf[2];
                    printf("    audio element found! pid=%#x\n", audio_pid);
                    audio_found = 1;
                    fp1 = fopen("check_ts_audio.aac", "wb");
                }
                else if (buf[15] == VIDEO_STREAM_ID)
                {
                    if (video_found) continue;
                    printf("[0-4]: %#x, %#x, %#x, %#x, %#x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
                    video_pid = ((buf[1]&0xf)<<8) + buf[2];
                    printf("    video element found! pid=%#x\n", video_pid);
                    video_found = 1;
                    fp2 = fopen("check_ts_video.h264", "wb");
                }
            }
            else
            {
                assert(buf[4]==0 && buf[5]==0 && buf[6]==1);
                if (buf[7] == AUDIO_STREAM_ID)
                {
                    if (audio_found) continue;
                    printf("[0-4]: %#x, %#x, %#x, %#x, %#x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
                    audio_pid = ((buf[1]&0xf)<<8) + buf[2];
                    printf("    audio element found! pid=%#x\n", audio_pid);
                    audio_found = 1;
                    fp1 = fopen("check_ts_audio.aac", "wb");
                }
                else if (buf[7] == VIDEO_STREAM_ID)
                {
                    if (video_found) continue;
                    video_pid = ((buf[1]&0xf)<<8) + buf[2];
                    printf("    video element found! pid=%#x\n", video_pid);
                    video_found = 1;
                    fp2 = fopen("check_ts_video.h264", "wb");
                }
            }
        }
        else
        {
            continue;
        }
    }
    fseek(fp0, 0, SEEK_SET);

    int cnt;
    while (cnt = fread(buf, 1, TS_PKT_SIZE, fp0))
    {
        //printf("cnt=%d, %#x, %#x, %#x, %#x\n", cnt, buf[0], buf[1], buf[2], buf[3]);
        if ((buf[0]==0x47) && (((buf[1]&0xf)<<8)+buf[2]==video_pid))
        {
            if (buf[1]==0x41)
            {
                // the first ts packet
                fwrite(buf+32, 1, TS_PKT_SIZE-32, fp2);
            }
            else if ((buf[3]&0xf0) == 0x10)
            {
                // the middle
                fwrite(buf+4, 1, TS_PKT_SIZE-4, fp2);
            }
            else if ((buf[3]&0xf0) == 0x30)
            {
                // the last
                int padding_sz = buf[4]+1;
                fwrite(buf+4+padding_sz, 1, TS_PKT_SIZE-4-padding_sz, fp2);
            }
            else
            {
                assert(0);
            }
        }
        else if ((buf[0]==0x47) && (((buf[1]&0xf)<<8)+buf[2]==audio_pid))
        {
            if (buf[1]==0x41)
            {
                int pes_len = (buf[16]<<8) + buf[17];
                if (pes_len >= 170)
                {
                    // the most case for bs_size>=162
                    fwrite(buf+26, 1, TS_PKT_SIZE-26, fp1);
                }
                else
                {
                    // for bs_size(adts+aac_frame)<162
                    int padding_sz = buf[4]-7;
                    fwrite(buf+26+padding_sz, 1, pes_len-8, fp1);
                }
            }
            else if ((buf[3]&0xf0) == 0x10)
            {
                fwrite(buf+4, 1, TS_PKT_SIZE-4, fp1);
            }
            else if ((buf[3]&0xf0) == 0x30)
            {
                int padding_sz = buf[4]+1;
                fwrite(buf+4+padding_sz, 1, TS_PKT_SIZE-4-padding_sz, fp1);
            }
            else
            {
                assert(0);
            }
        }
    }

    fclose(fp0);
    if (fp1)
        fclose(fp1);
    if (fp2)
        fclose(fp2);
    free(buf);

    puts("--------------------------check_ts_muxer end---------------------------------");
}
