# Kerneloid Build System
# TINX - This Is Not XNU

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2

# Targets
HOSTED = build/hosted

.PHONY: all clean run hosted

all: hosted

hosted: $(HOSTED)

$(HOSTED): src/hosted.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

run: hosted
	./$(HOSTED)

clean:
	rm -rf build
