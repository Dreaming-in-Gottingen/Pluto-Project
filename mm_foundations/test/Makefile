$(warning "---------------building foundations test------------------")

CC = gcc

TARGET_BIN = foundations_test

CC_FLAGS =
LD_LIBS = -L ../ -lmm_foundations -lpthread

INC_DIR = -I ../include

SRCS = foundations_test.c

OBJS = $(notdir $(patsubst %.c, %.o, $(SRCS)))

all:$(OBJS)
	$(CC) -o $(TARGET_BIN) $^ $(LD_LIBS)

$(OBJS):$(SRCS)
	$(CC) $(CC_FLAGS) $(INC_DIR) -c $^

clean:
	rm $(OBJS)
	rm $(TARGET_BIN)
