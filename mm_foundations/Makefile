$(warning "---------------building foundations lib------------------")

CC = gcc

TARGET_LIB = libmm_foundations.so

CC_FLAGS = -Wno-pointer-to-int-cast
#LD_LIBS = -lpthread

INC_DIR = -I include -I .

SRCS = srcs/mm_semaphore.c \
       srcs/mm_message.c \

OBJS = $(notdir $(patsubst %.c, %.o, $(SRCS)))

all:$(OBJS)
	$(CC) -shared -o $(TARGET_LIB) $(LD_LIBS) $^

$(OBJS):$(SRCS)
	$(CC) $(CC_FLAGS) $(INC_DIR) -fPIC -c $^

clean:
	rm $(OBJS)
	rm $(TARGET_LIB)
