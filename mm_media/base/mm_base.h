#ifndef __MM_BASE__
#define __MM_BASE__

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>

typedef enum MediaStreamType {
    MEDIA_STREAMTYPE_LOCALFILE,
    MEDIA_STREAMTYPE_NETWORK,
} MediaStreamType;

typedef enum MediaSourceType {
    MEDIA_SOURCETYPE_FD,
    MEDIA_SOURCETYPE_FP,
    MEDIA_SOURCETYPE_FILEPATH,
    MEDIA_SOURCETYPE_CALLBACK,
} MediaSourceType;

typedef struct ExtFdInfo{
    int fd;
    long offset;
    long cur_offset;
    long length;
} ExtFdInfo;

typedef struct MediaDataSourceInfo {
    MediaStreamType stream_type;
    MediaSourceType source_type;
    char *src_url;

    char file_path;
    FILE *fp;
    int fd;
    int io_direction;

    ExtFdInfo ext_info;
} MediaDataSourceInfo;

int64_t GetSysTimeMonoUs();



#endif //__MM_BASE__
