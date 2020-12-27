$(warning "---------------building media framework lib------------------")

CC = gcc

TARGET_LIB = libmm_framework.so

CC_FLAGS = -fPIC
#LD_LIBS = -lpthread -lrt

INC_DIR = -I base 	\
          -I stream \
          -I cache  \
          -I muxer/include  \
          -I ../mm_foundations/include

SRCS =  base/mm_base.c \
        stream/mm_stream.c \
        stream/mm_stream_file.c \
        cache/mm_cache.c \
        cache/mm_cache_simple.c \
        muxer/muxers/muxer_writer.c \
        muxer/mpeg2ts_muxer/Mpeg2tsMuxerDrv.c \
        muxer/mpeg2ts_muxer/Mpeg2tsMuxer.c \

OBJS = $(notdir $(patsubst %.c, %.o, $(SRCS)))

all:$(OBJS)
	$(CC) -shared -o $(TARGET_LIB) $(LD_LIBS) $^

$(OBJS):$(SRCS)
	$(CC) $(CC_FLAGS) $(INC_DIR) -c $^

clean:
	rm $(OBJS)
	rm $(TARGET_LIB)
