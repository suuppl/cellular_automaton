CC = gcc
CFLAGS = 
TARGETS = main
SRC = $(TARGETS:.c=)

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean

