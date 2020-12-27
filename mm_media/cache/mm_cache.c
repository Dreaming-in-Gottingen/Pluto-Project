//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_cache"

//#include <utils/Log.h>
#include <string.h>

#include <mm_log.h>
#include "mm_cache.h"

extern FsCacheWriter *create_cache_simple(mm_stream_info_t *pStream, int cache_size);
extern int destroy_cache_simple(FsCacheWriter *pCacheWriter);


FsCacheWriter* cache_handle_create(MEDIA_CACHEMODE mode, mm_stream_info_t *pStream, int8_t *pCache, int cache_size)
{
    if(CACHEMODE_THREAD == mode)
    {
        return NULL;
        //return create_cache_thread(pStream, pCache, nCacheSize);
    }
    else if (CACHEMODE_SIMPLE == mode)
    {
        return create_cache_simple(pStream, cache_size);
    }
    else
    {
        ALOGE("(f:%s, l:%d) not support cache_mode[%d] currently", __FUNCTION__, __LINE__, mode);
        return NULL;
    }
}

int cache_handle_destroy(FsCacheWriter *thiz)
{
    if(NULL == thiz) {
        ALOGE("(f:%s, l:%d) CacheWriter is NULL", __FUNCTION__, __LINE__);
        return -1;
    }
    if(CACHEMODE_THREAD == thiz->mMode)
    {
        //return deinitFsCacheThreadContext(thiz);
    }
    else if (CACHEMODE_SIMPLE == thiz->mMode)
    {
        return destroy_cache_simple(thiz);
    }
    else
    {
        ALOGE("(f:%s, l:%d) not support cache_mode[%d]", __FUNCTION__, __LINE__, thiz->mMode);
        return -1;
    }
    return 0;
}


#define FWRITE_DURATION_DEBUG
#define TIMEOUT_THRESHOLD   (1000*1000ll)

ssize_t file_cache_write(mm_stream_info_t *pStream, const char *buffer, size_t size)
{
    ssize_t totalWriten = 0;
#ifdef FWRITE_DURATION_DEBUG
    int64_t tm1, tm2;
#endif

    if (pStream->mWriteErrNO == EIO) {
        return -EIO;
    }

#ifdef FWRITE_DURATION_DEBUG
    tm1 = GetSysTimeMonoUs();
#endif
    if ((size_t)(totalWriten = pStream->write(buffer, 1, size, pStream)) != size)
    {
        ALOGE("(f:%s, l:%d) Stream[%p] fwrite error: [%d]!=[%d](%s)", __FUNCTION__, __LINE__, pStream, (int)totalWriten, (int)size, strerror(errno));
        if (errno == EIO) {
            ALOGE("(f:%s, l:%d) tf io error, stop write disk!!", __FUNCTION__, __LINE__);
        }
        pStream->mWriteErrNO = errno;
        if (pStream->mWriteErrCnt++ > 10) {
            if (pStream->mCB.cb != NULL && pStream->mCB.pComp != NULL) {
                pStream->mCB.cb(pStream->mCB.pComp, 0);
            }
        }
    } else {
        if (pStream->mWriteErrCnt != 0) {
            pStream->mWriteErrCnt = 0;
        }
    }
#ifdef FWRITE_DURATION_DEBUG
    tm2 = GetSysTimeMonoUs();
    if (tm2-tm1 > TIMEOUT_THRESHOLD) {
        ALOGW("(f:%s, l:%d) fwrite[%zd] Bytes, duration=[%lld] ms! write is slow!", __FUNCTION__, __LINE__, totalWriten, (tm2-tm1)/1000LL);
    }
#endif
    return totalWriten;
}

