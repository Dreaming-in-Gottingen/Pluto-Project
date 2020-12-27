//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_cache_simple"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <mm_log.h>
#include <mm_base.h>
#include <mm_cache.h>

typedef struct CacheSimpleContext
{
    mm_stream_info_t *mpStream;
    char      *mpCache;
    size_t     mCacheSize;
    size_t     mValidLen;
} CacheSimpleContext;

static ssize_t FsCacheSimpleWrite(FsCacheWriter *thiz, const char *buf, size_t size)
{
    CacheSimpleContext *pCtx = (CacheSimpleContext*)thiz->mPriv;
    size_t left_size = 0;
    if (0 == size || NULL == buf)
    {
        ALOGE("(f:%s, l:%d) error size[%d] or buf[%p]!!!", __FUNCTION__, __LINE__, (int)size, buf);
        return -1;
    }
    //if (size > pCtx->mCacheSize)
    //{
    //    ALOGD("(f:%s, l:%d) ready to write[%d] bytes!", __FUNCTION__, __LINE__, size);
    //}
    if (pCtx->mValidLen >= pCtx->mCacheSize)
    {
        ALOGE("(f:%s, l:%d) fatal error! why mValidLen[%ld]>=mCacheSize[%ld] ?!", __FUNCTION__, __LINE__, (long)pCtx->mValidLen, (long)pCtx->mCacheSize);
        return -1;
    }
    if (pCtx->mValidLen > 0)
    {
        if (pCtx->mValidLen + size <= pCtx->mCacheSize)
        {
            memcpy(pCtx->mpCache+pCtx->mValidLen, buf, size);
            pCtx->mValidLen += size;
            if (pCtx->mValidLen == pCtx->mCacheSize)
            {
                file_cache_write(pCtx->mpStream, pCtx->mpCache, pCtx->mCacheSize);
                pCtx->mValidLen = 0;
            }
            return size;
        }
        else
        {
            size_t nSize0 = pCtx->mCacheSize - pCtx->mValidLen;
            left_size = size - nSize0;
            memcpy(pCtx->mpCache+pCtx->mValidLen, buf, nSize0);
            file_cache_write(pCtx->mpStream, pCtx->mpCache, pCtx->mCacheSize);
            pCtx->mValidLen = 0;
        }
    }
    else
    {
        left_size = size;
    }

    if (left_size >= pCtx->mCacheSize)
    {
        size_t size2 = (left_size/pCtx->mCacheSize) * pCtx->mCacheSize;
        //ALOGD("(f:%s, l:%d) another fwrite[%d] kB!", __FUNCTION__, __LINE__, size2/1024);
        file_cache_write(pCtx->mpStream, buf+(size-left_size), size2);
        left_size -= size2;
    }
    if (left_size > 0)
    {
        //ALOGD("(f:%s, l:%d)need cache!", __FUNCTION__, __LINE__);
        memcpy(pCtx->mpCache, buf+(size-left_size), left_size);
        pCtx->mValidLen = left_size;
    }
    return size;
}

static int FsCacheSimpleSeek(FsCacheWriter *thiz, int64_t offset, int whence)
{
    CacheSimpleContext *pCtx = (CacheSimpleContext*)thiz->mPriv;
    int ret;
    thiz->cache_flush(thiz);
    ret = pCtx->mpStream->seek(pCtx->mpStream, offset, whence);
    return ret;
}

static int64_t FsCacheSimpleTell(FsCacheWriter *thiz)
{
    CacheSimpleContext *pCtx = (CacheSimpleContext*)thiz->mPriv;
    int64_t pos;
    thiz->cache_flush(thiz);
    pos = pCtx->mpStream->tell(pCtx->mpStream);
    return pos;
}

static int FsCacheSimpleTruncate(FsCacheWriter *thiz, int64_t length)
{
    int ret;
    CacheSimpleContext *pCtx = (CacheSimpleContext*)thiz->mPriv;
    thiz->cache_flush(thiz);
    ret = pCtx->mpStream->truncate(pCtx->mpStream, length);
    return ret;
}

static int FsCacheSimpleFlush(FsCacheWriter *thiz)
{
    CacheSimpleContext *pCtx = (CacheSimpleContext*)thiz->mPriv;
    if(pCtx->mValidLen > 0)
    {
        file_cache_write(pCtx->mpStream, pCtx->mpCache, pCtx->mValidLen);
        pCtx->mValidLen = 0;
    }
    return 0;
}

FsCacheWriter *create_cache_simple(mm_stream_info_t *pStream, int cache_size)
{
    if (cache_size <= 0)
    {
        ALOGE("(f:%s, l:%d) cache_size[%d] is wrong!", __FUNCTION__, __LINE__, cache_size);
            return NULL;
    }
    FsCacheWriter *pFsCacheWriter = (FsCacheWriter*)malloc(sizeof(FsCacheWriter));
    if (NULL == pFsCacheWriter)
    {
        ALOGE("(f:%s, l:%d) Failed to alloc FsCacheWriter!", __FUNCTION__, __LINE__);
        return NULL;
    }
    memset(pFsCacheWriter, 0, sizeof(FsCacheWriter));
    CacheSimpleContext *pContext = (CacheSimpleContext*)malloc(sizeof(CacheSimpleContext));
    if (NULL == pContext) {
        ALOGE("(f:%s, l:%d) Failed to alloc CacheSimpleContext", __FUNCTION__, __LINE__);
        goto _ERROR_0;
    }
    memset(pContext, 0, sizeof(CacheSimpleContext));
    pContext->mpStream = pStream;
    int ret = posix_memalign((void **)&pContext->mpCache, 4096, cache_size);
    if (0 != ret)
    {
        ALOGE("(f:%s, l:%d) fatal error! malloc[%d] KB fail!", __FUNCTION__, __LINE__, cache_size/1024);
        goto _ERROR_1;
    }
    pContext->mValidLen = 0;
    pContext->mCacheSize = cache_size;
    pFsCacheWriter->cache_write = FsCacheSimpleWrite;
    pFsCacheWriter->cache_seek = FsCacheSimpleSeek;
    pFsCacheWriter->cache_tell = FsCacheSimpleTell;
    pFsCacheWriter->cache_truncate = FsCacheSimpleTruncate;
    pFsCacheWriter->cache_flush = FsCacheSimpleFlush;
    pFsCacheWriter->mMode = CACHEMODE_SIMPLE;
    pFsCacheWriter->mPriv = (void*)pContext;
    return pFsCacheWriter;

_ERROR_1:
    free(pContext);
_ERROR_0:
    free(pFsCacheWriter);
    return NULL;
}

int destroy_cache_simple(FsCacheWriter *pFsCacheWriter)
{
    if (NULL == pFsCacheWriter) {
        ALOGE("(f:%s, l:%d) why pFsCacheWriter==NULL ?!!", __FUNCTION__, __LINE__);
        return -1;
    }
    CacheSimpleContext *pContext = (CacheSimpleContext*)pFsCacheWriter->mPriv;
    if (NULL == pContext) {
        ALOGE("(f:%s, l:%d) why pContext==NULL ?!!", __FUNCTION__, __LINE__);
        return -1;
    }
    if (pContext->mpCache) {
        free(pContext->mpCache);
        pContext->mpCache = NULL;
    }
    free(pContext);
    free(pFsCacheWriter);
    return 0;
}

