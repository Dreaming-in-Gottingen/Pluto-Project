#define MM_LOG_LEVEL 1
#define LOG_TAG "stream/cache_test"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <mm_log.h>
#include <mm_cache.h>
#include <mm_stream.h>

int main()
{
    ALOGW("--------------stream/cache_test begin--------------");

    MediaDataSourceInfo src_info;
    memset(&src_info, 0, sizeof(src_info));
    src_info.source_type = MEDIA_SOURCETYPE_FILEPATH;
    src_info.src_url = "stream_fp.log";
    //src_info.source_type = MEDIA_SOURCETYPE_FD;
    //int fd = open("stream_fd.log", O_RDWR|O_CREAT, 0666);
    //if (fd < 0) {
    //    ALOGE("open file failed:%s", strerror(errno));
    //    return -1;
    //}
    //src_info.fd = fd;

    src_info.stream_type = MEDIA_STREAMTYPE_LOCALFILE;
    src_info.io_direction = 1;

    mm_stream_info_t *pStream = stream_handle_create(&src_info);

    FsCacheWriter* writer = cache_handle_create(CACHEMODE_SIMPLE, pStream, NULL, 4096);

    int *buf = (int*)malloc(10000);

    for (int i=0; i<10000/sizeof(int); i++)
        buf[i] = i;
    writer->cache_write(writer, (char*)buf, 10000);
    int64_t offset = writer->cache_tell(writer);
    ALOGD("before flush, file_offset:%p", (void*)offset);
    writer->cache_flush(writer);
    offset = writer->cache_tell(writer);
    ALOGD("11. after flush, file_offset:%p", (void*)offset);
    writer->cache_seek(writer, 0x1000, SEEK_SET);
    writer->cache_write(writer, "hello,world", 12);
    offset = writer->cache_tell(writer);
    ALOGD("22. after seek/write, file_offset:%p", (void*)offset);
    writer->cache_seek(writer, 0, SEEK_END);
    offset = writer->cache_tell(writer);
    ALOGD("33. after seek(END), file_offset:%p", (void*)offset);
    writer->cache_truncate(writer, 0x1300);
    offset = writer->cache_tell(writer);
    ALOGD("44. after truncate, file_offset:%p", (void*)offset);

    //close(fd);
    free(buf);
    cache_handle_destroy(writer);
    stream_handle_destroy(pStream);

    ALOGW("--------------stream/cache_test end--------------");
    return 0;
}
