#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define IDX_VIDEO 0
#define IDX_AUDIO 1

void main()
{
    FILE *fp0 = fopen("V3_avbs", "rb");
    FILE *fp1 = fopen("vbs.h264", "wb");
    FILE *fp2 = fopen("abs.aac", "wb");
    unsigned char *buf = malloc(1024*1024);
    unsigned char *text = malloc(64);
    int size0, size1;
    char *data0, *data1;
    int idx, flag, dura;
    long long pts;
    int ret;

    memset(text, 0, 64);
    // video csd extract
    fread(text, 1, 15, fp0);
    ret = sscanf(text, "$$AV$$csd%d,%d:", &idx, &size0);
    printf("ret:%d, idx:%d, vcsd_len:%d\n", ret, idx, size0);
    fread(buf, 1, size0, fp0);
    fwrite(buf, 1, size0, fp1);

    // audio csd extract
    fread(text, 1, 15, fp0);
    ret = sscanf(text, "$$AV$$csd%d,%d:", &idx, &size0);
    printf("ret:%d, idx:%d, vcsd_len:%d\n", ret, idx, size0);
    fread(buf, 1, size0, fp0);
    fwrite(buf, 1, size0, fp2);

    // generate adts header
    assert(size0==2);
    unsigned char *audio_csd = (unsigned char*)buf;
    unsigned profile = (audio_csd[0] >> 3) - 1;
    unsigned sampling_freq_index = ((audio_csd[0]&7)<<1) | (audio_csd[1]>>7);
    unsigned channel_configuration = (audio_csd[1]>>3)&0x0f;
    unsigned char adts[7];
    adts[0] = 0xff;
    adts[1] = 0xf1;
    adts[2] = profile<<6 | sampling_freq_index<<2 | ((channel_configuration>>2)&1);

    // av packet extract
    int cnt = 0;
    while (fread(text, 1, 42, fp0))
    {
        ret = sscanf(text, "$$AV$$%d,%d,%lld,%d,%d,%d:", &idx, &flag, &pts, &dura, &size0, &size1);
        printf("ret=%d, offset=%#lx, cnt=%d, idx=%d, flag=%d, pts=%lld, dura=%d, size0=%#x, size1=%#x\n", ret, ftell(fp0), cnt++, idx, flag, pts, dura, size0, size1);
        ret = fread(buf, 1, size0, fp0);
        if (idx == IDX_VIDEO)
        {
            //printf("[%#x,%#x,%#x,%#x]\n", buf[0], buf[1], buf[2], buf[3]);
            fwrite(buf, 1, size0, fp1);
        }
        else if (idx == IDX_AUDIO)
        {
            int abs_len = size0 + size1 + sizeof(adts);
            adts[3] = ((channel_configuration&3)<<6) | (abs_len>>11);
            adts[4] = (abs_len>>3) & 0xff;
            adts[5] = (abs_len&7) << 5;
            adts[6] = 0;
            fwrite(adts, 1, 7, fp2);
            fwrite(buf, 1, size0, fp2);
        }

        if (size1)
        {
            ret = fread(buf, 1, size1, fp0);
            if (idx == IDX_VIDEO)
            {
                fwrite(buf, 1, size1, fp1);
            }
            else if (idx == IDX_AUDIO)
            {
                fwrite(buf, 1, size1, fp2);
            }
        }
    }

    free(text);
    free(buf);
    fclose(fp0);
    fclose(fp1);
    fclose(fp2);
}
