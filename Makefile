CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -Iinclude
TARGET  = calc-pgm
SRCS    = src/main.c src/lexer.c src/parser.c src/display.c
OBJS    = $(patsubst src/%.c, build/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)
