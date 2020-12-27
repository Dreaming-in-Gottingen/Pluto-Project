$(warning "---------------building avbs_extractor------------------")

CC = gcc

TARGET_BIN = avbs_extractor

SRCS = avbs_extractor.c

OBJS = $(notdir $(patsubst %.c, %.o, $(SRCS)))

all:$(TARGET_BIN)
	@echo target_bin: $(TARGET_BIN)
	@echo sources: $(SRCS)

$(TARGET_BIN): $(OBJS)
	gcc $(CC_FLAGS) -o $@ $^ $(LD_FLAGS)

%.o : %.c
	gcc -c $^ -o $@

.PHONE:clean
clean:
	rm $(OBJS)
	rm $(TARGET_BIN)
