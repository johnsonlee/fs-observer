CC=gcc
RM=rm

CFLAGS=-pthread

TARGET=observer

default: all clean

all: $(TARGET)

$(TARGET): observer.c main.c
	$(CC) $(CFLAGS) -o $@ $^

clean:

dist-clean:
	@$(RM) -rf *.o $(TARGET)
