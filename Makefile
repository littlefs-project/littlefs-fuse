TARGET = lfs

OS := $(shell uname -s)

ifeq ($(OS), FreeBSD)
	CC = cc
else
	CC = gcc
endif

AR = ar
SIZE = size

SRC += $(wildcard *.c littlefs/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)
ASM := $(SRC:.c=.s)

ifdef DEBUG
CFLAGS += -O0 -g3
else
CFLAGS += -Os
endif
ifdef WORD
CFLAGS += -m$(WORD)
endif
CFLAGS += -I. -Ilittlefs
CFLAGS += -std=c99 -Wall -pedantic
CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -D_XOPEN_SOURCE=700

LFLAGS += -lfuse

ifeq ($(OS), FreeBSD)
	CFLAGS += -I /usr/local/include
	CFLAGS += -D __BSD_VISIBLE
	LFLAGS += -L /usr/local/lib
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
