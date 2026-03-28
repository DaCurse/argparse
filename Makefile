CC ?= gcc
CFLAGS ?= -ggdb -Wall -Wextra
TARGET := example
SRC := example.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC) argparse.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
