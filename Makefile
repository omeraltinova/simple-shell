CC=gcc
CFLAGS=`pkg-config --cflags gtk4` -Wall -g
LDFLAGS=`pkg-config --libs gtk4`

SRCS=main.c controller.c view.c model.c
OBJS=$(SRCS:.c=.o)
TARGET=terminal_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o $(TARGET)

run: all
	./$(TARGET)
