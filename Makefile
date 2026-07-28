CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lusb-1.0 -lhidapi-hidraw

TARGET = stadia-flash
SRCS = main.c utils.c firmware.c controller.c sdp.c kboot.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
