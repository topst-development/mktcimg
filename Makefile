

#CC=arm-none-linux-gnueabi-gcc
CC=gcc


SRCS=$(wildcard *.c)
OBJ=$(SRCS:.c=.o)

INC_DIR=.
#TARGET=./unpackimg
TARGET=./mktcimg

CFLAGS += -O2 -g -W -Wall -Werror -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS += -L./ -lutil --static
CFLAGS += -I$(INC_DIR) -I./libs/include

all:linux

linux:$(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)  $^ ./libs/lib/libuuid.a

windows:$(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $^ /lib/libuuid.dll.a

clean :
	rm -rf $(OBJ) $(TARGET)
