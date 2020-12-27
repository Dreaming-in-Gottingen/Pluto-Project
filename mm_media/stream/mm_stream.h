#ifndef __MM_STREAM_H__
#define __MM_STREAM_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <mm_base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mm_stream_callback {
    void *pComp;
    int (*cb)(void *pComp, int event);
} mm_stream_callback;

typedef struct mm_stream_info
{
  MediaDataSourceInfo mDataSrcInfo;

  mm_stream_callback  mCB;   //for notify writeErr message to RecSink till app. ref to RecSinkStreamCallback().
  int mWriteErrCnt;
  int mWriteErrNO;   //error number: eg. EIO.

  void *reserved;

  char *file_path;
  FILE *fp;
  int   fd;

  long offset;
  long length;
  long cur_offset;

  char *mpAlignBuf;
  int   mAlignBufSize;
  int   mIODirection;   //0:in(read), 1:out(write)

  //CedarXExternFdDesc fd_desc;
  long long mFileEndOffset;   //used in fwrite, logical file end position
  long long mFileSize;    //used in fwrite, physical file end position, may be bigger than mEndOffset because extend to blockSize for directIO.
  int  mFtruncateFlag;  //for directIO, when use write() instead of ftruncate(), set this flag to 1.

  int64_t (*seek)(struct mm_stream_info *stream, int64_t offset, int whence);
  int64_t (*tell)(struct mm_stream_info *stream);
  ssize_t (*read)(void *ptr, size_t size, size_t nmemb, struct mm_stream_info *stream);
  ssize_t (*write)(const void *ptr, size_t size, size_t nmemb, struct mm_stream_info *stream);
  int (*write2)(void *bs_info, struct mm_stream_info *stream);
  int (*truncate)(struct mm_stream_info *stream, int64_t length);
  int (*fallocate)(struct mm_stream_info *stream, int mode, int64_t offset, int64_t len);
  long long (*get_size)(struct mm_stream_info *stream);

  void (*destory)(struct mm_stream_info *stm_info);
} mm_stream_info_t;

static inline int64_t mm_seek(mm_stream_info_t *stream, int64_t offset, int whence)
{
    return stream->seek(stream, offset, whence);
}

static inline long mm_tell(mm_stream_info_t *stream)
{
    return stream->tell(stream);
}

static inline int mm_read(void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    return stream->read(ptr, size, nmemb, stream);
}

static inline int mm_write(const void *ptr, size_t size, size_t nmemb, mm_stream_info_t *stream)
{
    return stream->write(ptr, size, nmemb, stream);
}

static inline int mm_write_sub(void *bs_info, mm_stream_info_t *stream)
{
    return stream->write2(bs_info, stream);
}

mm_stream_info_t *stream_handle_create(MediaDataSourceInfo *data_src);

void stream_handle_destroy(mm_stream_info_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* __MM_STREAM_H__ */
