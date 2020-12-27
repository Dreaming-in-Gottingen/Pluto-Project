#ifndef __MM_CACHE_H__
#define __MM_CACHE_H__

//#include <utils/Log.h>
#include <errno.h>
#include <mm_stream.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum MEDIA_CACHEMODE {
    CACHEMODE_THREAD = 0,
    CACHEMODE_SIMPLE = 1,
} MEDIA_CACHEMODE;

typedef struct CacheDataInfo {
    char *mpCache;
    int  mCacheSize;
} CacheDataInfo;

//typedef struct FsCacheWriter FsCacheWriter;
typedef struct FsCacheWriter
{
    MEDIA_CACHEMODE mMode;
    void *mPriv;
    ssize_t (*cache_write)(struct FsCacheWriter *thiz, const char *buf, size_t size);
    int32_t (*cache_seek)(struct FsCacheWriter *thiz, int64_t offset, int whence);
    int64_t (*cache_tell)(struct FsCacheWriter *thiz);
    int32_t (*cache_truncate)(struct FsCacheWriter *thiz, int64_t length);
    int32_t (*cache_flush)(struct FsCacheWriter *thiz);
} FsCacheWriter;

FsCacheWriter* cache_handle_create(MEDIA_CACHEMODE mode, mm_stream_info_t *pStream, int8_t *pCache, int cache_size);
int cache_handle_destroy(FsCacheWriter *thiz);

extern ssize_t file_cache_write(mm_stream_info_t *pStream, const char *buffer, size_t size);

#if defined(__cplusplus)
}
#endif

#endif
