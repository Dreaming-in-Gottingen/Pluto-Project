$(warning "---------------building media framework test------------------")

CC = gcc

TARGET_BIN = stream_test muxer_test check_ts_muxer

CC_FLAGS = --std=c99
LD_FLAGS = -L. libmm_foundations.so -L. libmm_framework.so -lpthread

INC_DIR = -I ../base   \
          -I ../stream \
          -I ../cache  \
          -I ../muxer/include  \
          -I ../../mm_foundations/include

SRCS = 	stream_test.c muxer_test.c check_ts_muxer.c

DIR = $(nodir $SRCS)

OBJS = $(notdir $(patsubst %.c, %.o, $(SRCS)))


all:$(TARGET_BIN)
	@echo target_bin: $(TARGET_BIN)
	@echo sources: $(SRCS)

#	$(CC) -o $(TARGET_BIN) $^ $(LD_FLAGS)

stream_test: stream_test.o
	gcc $(CC_FLAGS) -o $@ $^ $(LD_FLAGS)

muxer_test: muxer_test.o
	gcc $(CC_FLAGS) -o $@ $^ $(LD_FLAGS)

check_ts_muxer: check_ts_muxer.o
	gcc $(CC_FLAGS) -o $@ $^ $(LD_FLAGS)

%.o : %.c
	gcc -c $(INC_DIR) $(CC_FLAGS) $^ -o $@

#$(OBJS):$(SRCS)
#	$(CC) $(CC_FLAGS) $(INC_DIR) -c $^

.PHONE:clean
clean:
	rm $(OBJS)
	rm $(TARGET_BIN)
