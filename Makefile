# Kerneloid Build System
# TINX - This Is Not XNU

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2 -Iinclude

# Display components
DISPLAY_SRC = src/display/framebuffer.c \
              src/display/graphics.c \
              src/display/font.c \
              src/display/input.c

# Window manager components
WM_SRC = src/wm/window.c \
         src/wm/compositor.c \
         src/wm/decorations.c \
         src/wm/tiling.c

# Network components
NET_SRC = src/net/nic.c \
          src/net/ethernet.c \
          src/net/ipv4.c \
          src/net/transport.c \
          src/net/socket.c

# Shell and utilities
SHELL_SRC = src/shell/shell.c \
            src/shell/utils.c

# File system
FS_SRC = src/fs/fs.c

# GUI
GUI_SRC = src/gui/gui.c

# All sources
SOURCES = src/hosted.c $(DISPLAY_SRC) $(WM_SRC) $(NET_SRC) $(SHELL_SRC) $(FS_SRC) $(GUI_SRC)

# Objects
OBJECTS = $(SOURCES:.c=.o)

# Targets
HOSTED = build/hosted

.PHONY: all clean run hosted

all: hosted

hosted: $(HOSTED)

$(HOSTED): $(SOURCES)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

run: hosted
	./$(HOSTED)

clean:
	rm -rf build
