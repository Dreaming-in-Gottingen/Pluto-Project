//#define LOG_NDEBUG 0
#define MM_LOG_LEVEL 2
#define LOG_TAG "mm_stream_file"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

//#include <mm_semaphore.h>

#define _LARGEFILE64_SOURCE
#include <mm_log.h>
#include "mm_stream.h"
#include "mm_stream_file.h"


static char *generateFilePathByFd(const int fd)
{
    char fdPath[1024];
    char absoluteFilePath[1024];
    int ret;
    if(fd < 0)
    {
        ALOGE("(f:%s, l:%d) fatal error! why fd=[%d]?", __FUNCTION__, __LINE__, fd);
        return NULL;
    }

    snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);
    ret = readlink(fdPath, absoluteFilePath, 1023);
    if (ret == -1)
    {
        ALOGE("(f:%s, l:%d) fatal error! readlink(%s) failure:(%s)", __FUNCTION__, __LINE__, fdPath, strerror(errno));
        return NULL;
    }
    absoluteFilePath[ret] = '\0';
    ALOGD("(f:%s, l:%d) readlink(%s), filePath(%s)", __FUNCTION__, __LINE__, fdPath, absoluteFilePath);
    return strdup(absoluteFilePath);
}

//for fp
int64_t mm_seek_stream_file(mm_stream_info_t *stream, int64_t offset, int whence)
{
    return fseeko(stream->fp, offset, whence);
}

int64_t mm_tell_stream_file(mm_stream_info_t *stream)
{
    return ftello(stream->fp);
}

ssize_t mm_read_stream_file(void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    return fread(ptr, size, nmemb, stream->fp);
}

ssize_t mm_write_stream_file(const void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    return fwrite(ptr, size, nmemb, stream->fp);
}

long long mm_get_stream_size_file(mm_stream_info_t *stream)
{
    long long size = -1;
    int64_t cur_offset;

    if (stream->mDataSrcInfo.stream_type == MEDIA_STREAMTYPE_NETWORK)
        return -1;

    if (stream->mDataSrcInfo.source_type == MEDIA_SOURCETYPE_FD) {
        if (stream->mDataSrcInfo.ext_info.length >= 0) {
            return stream->mDataSrcInfo.ext_info.length;
        }
        return 0;
    } else if (stream->mDataSrcInfo.source_type == MEDIA_SOURCETYPE_FILEPATH) {
        cur_offset = ftello(stream->fp);
        fseeko(stream->fp, 0, SEEK_END);
        size = ftello(stream->fp);
        fseeko(stream->fp, cur_offset, SEEK_SET);
        return size;
    } else {
        ALOGE("(f:%s, l:%d) unsupport source_type[%d]", __FUNCTION__, __LINE__, stream->mDataSrcInfo.source_type);
        return -1;
    }
}


int mm_truncate_stream_file(mm_stream_info_t *stream, int64_t length)
{
    int ret;
    int fd = fileno(stream->fp);
    ret = ftruncate(fd, length);
    if (ret != 0) {
        ALOGE("(f:%s, l:%d) fatal error! ftruncate fail:(%s)!", __FUNCTION__, __LINE__, strerror(errno));
    }
    return ret;
}

int mm_fallocate_stream_file(mm_stream_info_t *stream, int mode, int64_t offset, int64_t len)
{
    int fd = fileno(stream->fp);
    if (fd >= 0)
    {
        int64_t tm1, tm2;
        tm1 = GetSysTimeMonoUs();
        if (fallocate(fd, mode, offset, len) < 0)
        {
            ALOGE("(f:%s, l:%d) failed to fallocate(%ld):(%s)", __FUNCTION__, __LINE__, (long)len, strerror(errno));
            return -1;
        }
        tm2 = GetSysTimeMonoUs();
        ALOGD("(f:%s, l:%d) stream[%p] fallocate(%ld) Bytes, duration(%lld) ms", __FUNCTION__, __LINE__, stream, (long)len, (tm2-tm1)/1000LL);
        return 0;
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! why fd[%d] ?!", __FUNCTION__, __LINE__, fd);
        return -1;
    }
}


//for fd
int64_t mm_seek_fd_file(mm_stream_info_t *stream, int64_t offset, int whence)
{
    ALOGV("stream:%p, cur_offset:%#lx, seek_offset:%#lx, whence:%d, length:%ld",
        stream, stream->cur_offset, (long)offset, whence, stream->length);
    int64_t seek_result;
    if (whence == SEEK_SET) {
        //set from start, add origial offset.
        offset += stream->offset;
    } else if (whence == SEEK_CUR) {
        //seek from current position, seek to cur offset at first.
        seek_result = lseek64(stream->fd, stream->cur_offset, SEEK_SET);
        if(-1 == seek_result)
        {
            ALOGE("(f:%s, l:%d) fatal error! seek failed:(%s)", __FUNCTION__, __LINE__, strerror(errno));
            return -1;
        }
    } else {
    }

    seek_result = lseek64(stream->fd, offset, whence);

    int seek_ret = 0;
    if (seek_result == -1)
    {
        ALOGE("(f:%s, l:%d) fatal error! seek failed:(%s)", __FUNCTION__, __LINE__, strerror(errno));
        seek_ret = -1;
    }
    else
    {
        stream->cur_offset = seek_result < stream->offset ? stream->offset : seek_result;
        if(seek_result < stream->offset)
        {
            ALOGE("(f:%s, l:%d) fatal error! seek pos[%ld]<offset[%ld]", __FUNCTION__, __LINE__, (long)seek_result, stream->offset);
        }
    }
    return seek_ret;
}

int64_t mm_tell_fd_file(mm_stream_info_t *stream)
{
    return stream->cur_offset - stream->offset;
}

ssize_t mm_read_fd_file(void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    //seek to where current actual pos is.
    ssize_t result = lseek64(stream->fd, stream->cur_offset, SEEK_SET);
    if(result == -1) {
        ALOGE("(f:%s, l:%d) fatal error! seek fail!", __FUNCTION__, __LINE__);
        return -1;
    }

    int rd_sz = size * nmemb;
    result = read(stream->fd, ptr, rd_sz);
    stream->cur_offset = lseek64(stream->fd, 0, SEEK_CUR);
    if (-1 == stream->cur_offset)
    {
        ALOGE("(f:%s, l:%d) fatal error! seek fail!", __FUNCTION__, __LINE__);
    }
    assert(stream->cur_offset >= stream->offset);

    if (result != -1)
    {
        result /= size;
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! expected_read[%ld] != real_read[%ld]", __FUNCTION__, __LINE__, (long)(size*nmemb), (long)result);
    }
    return result;
}

ssize_t mm_write_fd_file(const void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    int result = lseek64(stream->fd, stream->cur_offset, SEEK_SET);
    if (result == -1) {
        ALOGE("(f:%s, l:%d) fatal error! seek fail!", __FUNCTION__, __LINE__);
        return -1;
    }

    ssize_t wr_sz = write(stream->fd, ptr, (size * nmemb));
    int64_t seek_ret = lseek64(stream->fd, 0, SEEK_CUR);
    if(-1 == seek_ret)
    {
        ALOGE("(f:%s, l:%d) fatal error! seek error!", __FUNCTION__, __LINE__);
        return -1;
    }
    stream->cur_offset = seek_ret < stream->offset ? stream->offset: seek_ret;
    if (seek_ret < stream->offset)
    {
        ALOGE("(f:%s, l:%d) fatal error! write pos[%ld] < offset[%ld]", __FUNCTION__, __LINE__, (long)seek_ret, stream->offset);
    }
    if (wr_sz != -1)
    {
        wr_sz /= size;
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! expected_write[%ld] != real_write[%ld] because:(%s)!", __FUNCTION__, __LINE__, (long)(size*nmemb), (long)wr_sz, strerror(errno));
    }

    return wr_sz;
}


long long mm_get_fd_size_file(mm_stream_info_t *stream)
{
    if (stream->mFileEndOffset != stream->mFileSize)
    {
        ALOGE("(f:%s, l:%d) fatal error! [%lld]!=[%lld]", __FUNCTION__, __LINE__, stream->mFileEndOffset, stream->mFileSize);
    }
    //return stream->length;
    int64_t fdSize = lseek64(stream->fd, 0, SEEK_END);
    if(-1 == fdSize)
    {
        ALOGE("(f:%s, l:%d) fatal error! seek fail!", __FUNCTION__, __LINE__);
    }
    int64_t size = fdSize - stream->offset;
    int64_t seekResult = lseek64(stream->fd, stream->cur_offset, SEEK_SET);
    if(-1 == seekResult)
    {
        ALOGE("(f:%s, l:%d) fatal error! seek fail!", __FUNCTION__, __LINE__);
    }
    if(size < 0)
    {
        ALOGE("(f:%s, l:%d) fatal error! size[%ld]<0, fdSize[%ld], offset[%ld]", __FUNCTION__, __LINE__, (long)size, (long)fdSize, stream->offset);
    }
    return size;
}

int mm_truncate_fd_file(mm_stream_info_t *stream, int64_t length)
{
    int ret = ftruncate(stream->fd, length);
    if(ret!=0)
    {
        ALOGE("(f:%s, l:%d) fatal error! ftruncate fail[%d]", __FUNCTION__, __LINE__, ret);
    }
    stream->mFileSize = length;
    stream->mFileEndOffset = length;
    return ret;
}

int mm_fallocate_fd_file(mm_stream_info_t *stream, int mode, int64_t offset, int64_t len)
{
    int ret = -1;
    if(stream->fd >= 0)
    {
        int64_t tm1, tm2;
        tm1 = GetSysTimeMonoUs();
        if (fallocate(stream->fd, mode, offset, len) < 0)
        {
            ALOGE("(f:%s, l:%d) fatal error! Failed to fallocate[%ld]:(%s)", __FUNCTION__, __LINE__, (long)len, strerror(errno));
            return -1;
        }
        tm2 = GetSysTimeMonoUs();
        ALOGD("(f:%s, l:%d) stream[%p] fallocate[%ld] Bytes, duration[%lld] ms", __FUNCTION__, __LINE__, stream, (long)len, (tm2-tm1)/1000LL);
        ret = 0;
    }
    return ret;
}

void file_destory_instream_handle(mm_stream_info_t * stream)
{
    if (stream->mDataSrcInfo.source_type == MEDIA_SOURCETYPE_FILEPATH)
    {
        if (stream->fp != NULL)
        {
            fclose(stream->fp);
            stream->fp = NULL;
        }
    }

    if (stream->fd >= 0)
    {
        close(stream->fd);
        stream->fd = -1;
    }

    if (stream->file_path)
    {
        free(stream->file_path);
        stream->file_path = NULL;
    }

    if (stream->mpAlignBuf)
    {
        free(stream->mpAlignBuf);
        stream->mpAlignBuf = NULL;
    }
    stream->mAlignBufSize = 0;
}

int create_instream_handle_file(mm_stream_info_t *stream, MediaDataSourceInfo *data_src)
{
    int ret = 0;
    stream->mIODirection = 0;   //read
    stream->fd = -1;
    if(data_src->source_type == MEDIA_SOURCETYPE_FILEPATH)
    {
        stream->fp = fopen(data_src->src_url, "rb");
        if (stream->fp == NULL)
        {
            ALOGE("(f:%s, l:%d) fopen(%s) error(%s)", __FUNCTION__, __LINE__, data_src->src_url, strerror(errno));
            ret = -1;
        }
        if(-1 == ret)
        {
            return ret;
        }
        stream->seek  = mm_seek_stream_file ;
        stream->tell  = mm_tell_stream_file ;
        stream->read  = mm_read_stream_file ;
        stream->write = mm_write_stream_file;
        stream->get_size = mm_get_stream_size_file;
        stream->destory = file_destory_instream_handle;
        return ret;
    }
    else if (data_src->source_type == MEDIA_SOURCETYPE_FD)
    {
        //Must dup a new file descirptor here and close it when destroying stream handle.
        stream->fd         = dup(data_src->fd);
        stream->offset     = data_src->ext_info.offset;
        stream->cur_offset = data_src->ext_info.offset;
        stream->length     = data_src->ext_info.length;
        ALOGD("(f:%s, l:%d) stream[%p], open fd[%d], offset[%ld], length[%ld]", __FUNCTION__, __LINE__, stream, stream->fd, stream->offset, stream->length);
        stream->seek      = mm_seek_fd_file;
        stream->tell      = mm_tell_fd_file;
        stream->read      = mm_read_fd_file;
        stream->write     = mm_write_fd_file;
        stream->get_size   = mm_get_fd_size_file;
        stream->destory = file_destory_instream_handle;
        stream->seek(stream, 0, SEEK_SET);
        return ret;
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! source_type[%d] stream_type[%d] wrong!", __FUNCTION__, __LINE__, data_src->source_type, data_src->stream_type);
        ret = -1;
        return ret;
    }
}

static void destory_outstream_handle_file(mm_stream_info_t *stream)
{
    if(stream->fp != NULL)
    {
        fclose(stream->fp);
        stream->fp = NULL;
    }

    if(stream->fd >= 0)
    {
        close(stream->fd);
        stream->fd = -1;
    }

    if(stream->file_path)
    {
        free(stream->file_path);
        stream->file_path = NULL;
    }

    if(stream->mpAlignBuf)
    {
        free(stream->mpAlignBuf);
        stream->mpAlignBuf = NULL;
        stream->mAlignBufSize = 0;
    }
}

int create_outstream_handle_file(mm_stream_info_t *stream, MediaDataSourceInfo *data_src)
{
    stream->mIODirection = 1;
    if (MEDIA_SOURCETYPE_FD == data_src->source_type)
    {
        stream->fp = fdopen(data_src->fd, "wb+");
        if (stream->fp == NULL)
        {
            ALOGE("(f:%s, l:%d) fatal error! fdopen[%d] failed:(%s)", __FUNCTION__, __LINE__, data_src->fd, strerror(errno));
            return -1;
        }
    }
    else if (MEDIA_SOURCETYPE_FILEPATH == data_src->source_type)
    {
        stream->fp = fopen(data_src->src_url, "wb+");
        if (stream->fp == NULL)
        {
            ALOGE("(f:%s, l:%d) failed to fopen(%s):(%s)", __FUNCTION__, __LINE__, data_src->src_url, strerror(errno));
            return -1;
        }
    }
    else
    {
        ALOGE("(f:%s, l:%d) fatal error! wrong source_type[%d]", __FUNCTION__, __LINE__, data_src->source_type);
        return -1;
    }

    stream->seek  = mm_seek_stream_file;
    stream->tell  = mm_tell_stream_file;
    stream->read  = mm_read_stream_file;   //not use for outstream(write storage device)
    stream->write = mm_write_stream_file;
    stream->truncate = mm_truncate_stream_file;
    stream->fallocate = mm_fallocate_stream_file;
    //stream->getsize = mm_get_stream_size_file;
    stream->destory = destory_outstream_handle_file;

    return 0;
}
