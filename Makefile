TARGET = lfs

ifndef OS
OS := $(shell uname -s)
endif

ifndef CC
CC = cc
endif

ifndef AR
AR = ar
endif

ifndef SIZE
SIZE = size
endif

SRC += $(wildcard *.c littlefs/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

ifdef DEBUG
override CFLAGS += -O0 -g3
else
override CFLAGS += -Os
endif
ifdef WORD
override CFLAGS += -m$(WORD)
endif
override CFLAGS += -I. -Ilittlefs
override CFLAGS += -std=c99 -Wall -pedantic
override CFLAGS += -D_FILE_OFFSET_BITS=64
override CFLAGS += -D_XOPEN_SOURCE=700
# enable multiversion support in littlefs
override CFLAGS += -DLFS_MULTIVERSION
# enable migrate support in littlefs
override CFLAGS += -DLFS_MIGRATE

ifdef DEFAULT_LFS_BLOCK_SIZE
override CFLAGS += -DDEFAULT_LFS_BLOCK_SIZE=$(DEFAULT_LFS_BLOCK_SIZE)
endif

override LFLAGS += -lfuse

ifeq ($(OS), FreeBSD)
override CFLAGS += -I /usr/local/include
override CFLAGS += -D __BSD_VISIBLE
override LFLAGS += -L /usr/local/lib
endif

all: $(TARGET)

asm: $(ASM)

size: $(OBJ)
	$(SIZE) -t $^

-include $(DEP)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LFLAGS) -o $@

%.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -c -MMD $(CFLAGS) $< -o $@

%.s: %.c
	$(CC) -S $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGET)
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(ASM)
