TARGET = lfs

CC = gcc
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
