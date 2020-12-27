#ifndef __MM_STREAM_FILE_H__
#define __MM_STREAM_FILE_H__

#include "mm_stream.h"

//create fp or fd
int create_outstream_handle_file(mm_stream_info_t *stm_info_t, MediaDataSourceInfo *data_src);     //write
int create_instream_handle_file(mm_stream_info_t *stm_info_t, MediaDataSourceInfo *data_src);      //read

#endif
