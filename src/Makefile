CC=gcc
RM=rm

CFLAGS=-pthread -g3 -ggdb -Wall

TARGET=observer

.PHONY: all clean dist-clean

all: $(TARGET)

$(TARGET): observer.c main.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	@$(RM) -rf *.o core

dist-clean:
	@$(RM) -rf *.o $(TARGET)
