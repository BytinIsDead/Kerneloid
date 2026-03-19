# 1. OBJECTIVE

Build a complete from-scratch GUI and system stack for Kerneloid, including:
- A custom display server (X11-like protocol)
- A window manager (OpenBox-like, tiling/floating)
- A TCP/IP networking stack (IPv4)
- POSIX utilities (ls, ping, cat, echo, etc.)
- All implemented in C with the "Kerneloid way" - educational, from-scratch implementation

The goal is to create a functional graphical environment running on bare metal or in a VM that demonstrates how these systems work under the hood.

# 2. CONTEXT SUMMARY

**Current State:**
- Kerneloid is a custom x86_64 OS kernel written in C
- Has basic memory management (page allocator, heap), VGA terminal driver
- Runs in hosted mode (userspace test) or bare metal (via GRUB)

**Target Architecture:**
- Display Server: Custom framebuffer-based server (VESA/VGA graphics mode)
- Window Manager: Compositing window manager with tiling/floating modes
- Networking: TCP/IPv4 stack with ARP, ICMP, UDP, TCP
- Shell/Utilities: POSIX-compatible command-line tools
- Target: x86_64, runs in QEMU/VM or bare metal

**Key Files to Create:**
- `src/display/` - Display server and rendering
- `src/wm/` - Window manager
- `src/net/` - Networking stack
- `src/shell/` - Shell and utilities
- `src/gui/` - GUI widgets and components

# 3. APPROACH OVERVIEW

**Strategy: Incremental Development with Hosted Mode Testing**

1. **Phase 1: Display & Graphics Foundation**
   - Start with framebuffer graphics (VESA VBE 2.0+ for linear framebuffer)
   - Build 2D rendering primitives (rectangles, text, bitmaps)
   - Create event system for mouse/keyboard input

2. **Phase 2: Window Manager**
   - Implement window creation, moving, resizing
   - Add title bars, borders, decorations
   - Support tiling and floating layouts
   - Window stacking and focus management

3. **Phase 3: Networking Stack**
   - Implement network driver interface
   - Build ARP, ICMP (ping), IPv4
   - Add UDP and TCP protocols
   - Create socket API

4. **Phase 4: Shell & Utilities**
   - Build simple shell (command parser)
   - Implement core utilities (ls, cat, echo, cd, pwd, etc.)
   - Add ping utility for network testing
   - File system interface (FAT12/16 for simplicity)

**Why This Approach:**
- Phased approach allows testing each component in hosted mode
- Framebuffer graphics is simpler than implementing a full X11 server
- Networking starts with ICMP/ping before full TCP
- All components can be tested via the hosted build before bare metal

# 4. IMPLEMENTATION STEPS

## Phase 1: Display & Graphics Foundation

### Step 1.1 - Framebuffer Driver
- **Goal:** Set up graphics mode and direct framebuffer access
- **Method:** Implement VESA VBE 2.0+ detection and linear framebuffer mode selection
- **Reference:** Create `src/display/framebuffer.c`, `include/display/framebuffer.h`
- **Details:**
  - Detect available VESA modes via VBE interrupt
  - Set 800x600 or 1024x768 32-bit color mode
  - Store framebuffer base address and pitch
  - Implement double-buffering for smooth rendering

### Step 1.2 - 2D Graphics Primitives
- **Goal:** Basic drawing functions for GUI
- **Method:** Implement fill, blit, line, rectangle, circle drawing
- **Reference:** Create `src/display/graphics.c`, `include/display/graphics.h`
- **Details:**
  - `fill_rect(x, y, w, h, color)` - solid rectangles
  - `blit_bitmap(x, y, bitmap, w, h)` - bitmaps/images
  - `draw_line(x1, y1, x2, y2, color)` - Bresenham line algorithm
  - `draw_circle(x, y, r, color)` - mid-point circle algorithm

### Step 1.3 - Font Rendering
- **Goal:** Display text on screen
- **Method:** Implement bitmap font renderer (PC Screen Font or custom)
- **Reference:** Create `src/display/font.c`, `include/display/font.h`
- **Details:**
  - Include 8x8 or 8x16 bitmap font data
  - `draw_char(x, y, c, fg, bg)` - single character
  - `draw_string(x, y, str, fg, bg)` - text with word wrap

### Step 1.4 - Input Event System
- **Goal:** Handle keyboard and mouse events
- **Method:** PS/2 keyboard/mouse driver + event queue
- **Reference:** Create `src/display/input.c`, `include/display/input.h`
- **Details:**
  - PS/2 keyboard controller driver (scan code set 1 → ASCII)
  - PS/2 mouse driver (basic packet parsing)
  - Event queue for keyboard/mouse events
  - Event types: KEY_PRESS, KEY_RELEASE, MOUSE_MOVE, MOUSE_CLICK

## Phase 2: Window Manager

### Step 2.1 - Window Data Structures
- **Goal:** Define window objects and management structures
- **Method:** Create window, desktop, taskbar structures
- **Reference:** Create `src/wm/window.c`, `include/wm/window.h`
- **Details:**
  - `window_t` - title, position, size, flags, content buffer
  - `desktop_t` - collection of windows, active focus
  - Window states: NORMAL, MINIMIZED, MAXIMIZED, FLOATING, TILING

### Step 2.2 - Window Compositing
- **Goal:** Render windows to screen with proper layering
- **Method:** Implement damage tracking and composite rendering
- **Reference:** Create `src/wm/compositor.c`, `include/wm/compositor.h`
- **Details:**
  - Track damaged/dirty regions per window
  - Z-order stacking (bottom to top rendering)
  - Clip windows to their bounds
  - Double-buffer compositing output

### Step 2.3 - Window Decorations
- **Goal:** Title bars, borders, close/minimize/maximize buttons
- **Method:** Render decorations around window content
- **Reference:** Create `src/wm/decorations.c`
- **Details:**
  - 4-pixel border with configurable color
  - Title bar with centered window title
  - Three buttons (close=red, minimize=yellow, maximize=green)
  - Button click detection and action handling

### Step 2.4 - Window Management
- **Goal:** Interactive window manipulation
- **Method:** Implement drag, resize, focus, keyboard shortcuts
- **Reference:** Extend `src/wm/window.c`
- **Details:**
  - Click to focus, raise to top
  - Drag title bar to move
  - Drag borders/corners to resize
  - Alt+Tab for window switching
  - Windows key for start menu

### Step 2.5 - Tiling Engine
- **Goal:** Automatic window layout (like OpenBox/i3)
- **Method:** Tile windows automatically based on layout mode
- **Reference:** Create `src/wm/tiling.c`, `include/wm/tiling.h`
- **Details:**
  - Layout modes: FLOATING, TILE_HORIZ, TILE_VERT, MONOCLE
  - Split containers horizontally/vertically
  - Respect window "master" flag
  - Keyboard shortcuts for layout switching

## Phase 3: Networking Stack

### Step 3.1 - Network Driver Interface
- **Goal:** Abstract network hardware access
- **Method:** Define driver interface and implement e1000/rtl8139
- **Reference:** Create `src/net/nic.h`, `src/net/e1000.c`, `src/net/rtl8139.c`
- **Details:**
  - `nic_driver_t` - init, transmit, receive, interrupt handling
  - Ethernet frame RX/TX ring buffers
  - Basic packet DMA setup
  - MAC address reading

### Step 3.2 - Ethernet & ARP
- **Goal:** Handle Ethernet frames and address resolution
- **Method:** Parse Ethernet headers, implement ARP protocol
- **Reference:** Create `src/net/ethernet.c`, `src/net/arp.c`
- **Details:**
  - Ethernet header parsing (type, src/dst MAC)
  - ARP request/reply handling
  - ARP cache table (IP → MAC mapping)
  - Broadcast packet handling

### Step 3.3 - IPv4 & ICMP
- **Goal:** IP packet routing and ping support
- **Method:** Implement IPv4 header processing and ICMP echo
- **Reference:** Create `src/net/ipv4.c`, `src/net/icmp.c`
- **Details:**
  - IPv4 header parsing and validation
  - IP fragmentation handling (basic)
  - ICMP echo request/reply (ping)
  - IP checksum calculation

### Step 3.4 - UDP & TCP
- **Goal:** Transport layer protocols
- **Method:** Implement UDP and TCP state machines
- **Reference:** Create `src/net/udp.c`, `src/net/tcp.c`
- **Details:**
  - UDP: port-based demultiplexing, checksum
  - TCP: state machine (LISTEN, SYN_SENT, ESTABLISHED, etc.)
  - Sequence/acknowledgment numbers
  - Basic flow control (sliding window)

### Step 3.5 - Socket API
- **Goal:** POSIX-like socket interface
- **Method:** Create socket(), bind(), connect(), send(), recv()
- **Reference:** Create `src/net/socket.c`, `include/net/socket.h`
- **Details:**
  - `socket(domain, type, protocol)` - create socket
  - `bind(sockfd, addr, len)` - bind to port
  - `connect(sockfd, addr, len)` - connect to remote
  - `send()/recv()` - data transfer

## Phase 4: Shell & Utilities

### Step 4.1 - Simple Shell
- **Goal:** Command-line interpreter
- **Method:** Implement prompt, parsing, built-in commands
- **Reference:** Create `src/shell/shell.c`, `include/shell/shell.h`
- **Details:**
  - Command prompt with basic line editing (backspace)
  - Parse command + arguments (no piping initially)
  - Built-ins: help, clear, exit
  - External command execution (fork/exec pattern)

### Step 4.2 - File System Interface
- **Goal:** File operations support
- **Method:** Implement FAT12/16 read-only for simplicity, or ramdisk
- **Reference:** Create `src/fs/fat.c`, `include/fs/fs.h`
- **Details:**
  - Mount floppy/USB FAT filesystem (read-only initially)
  - File descriptor table
  - open, read, close system calls
  - Directory reading (ls support)

### Step 4.3 - Core Utilities
- **Goal:** Essential POSIX commands
- **Method:** Implement each utility as separate function
- **Reference:** Create `src/shell/utils.c`, individual command files
- **Details:**
  - `ls [-l]` - list directory contents
  - `cat <file>` - display file contents
  - `echo <text>` - print text
  - `cd <dir>` - change directory
  - `pwd` - print working directory
  - `mkdir <dir>` - create directory
  - `touch <file>` - create empty file

### Step 4.4 - Network Utilities
- **Goal:** Network testing tools
- **Method:** Implement ping and basic network status
- **Reference:** Create `src/shell/netutils.c`
- **Details:**
  - `ping <ip>` - ICMP echo request using net stack
  - `ifconfig` - show network interfaces and IP
  - `netstat` - show connections (optional)

## Phase 5: Integration & Testing

### Step 5.1 - System Startup
- **Goal:** Tie all components together
- **Method:** Create main.c that initializes display, network, shell
- **Reference:** Modify `src/hosted.c` or create `src/main.c`
- **Details:**
  - Initialize framebuffer and input
  - Start window manager as main GUI process
  - Initialize network stack
  - Launch shell (embedded terminal window or serial)

### Step 5.2 - Hosted Mode Testing
- **Goal:** Test all components on Linux before bare metal
- **Method:** Use SDL2 or Linux framebuffer for hosted graphics
- **Reference:** Add hosted graphics backend in `src/display/`
- **Details:**
  - SDL2 backend for hosted mode (maps to X11/Wayland)
  - Test window management interactions
  - Test network stack with loopback
  - Test shell and utilities

### Step 5.3 - Bare Metal Boot
- **Goal:** Run on real hardware or QEMU
- **Method:** Update build system for ISO/USB boot
- **Reference:** Update `Makefile`, `grub.cfg`
- **Details:**
  - Add GRUB menu entry for GUI mode
  - Include necessary drivers for QEMU (e1000, virtio)
  - VESA mode setup in boot code
  - Memory map detection for proper allocation

# 5. TESTING AND VALIDATION

## Phase 1 Testing - Display & Graphics
- **Framebuffer:** Verify VESA mode detection works; confirm 800x600/1024x768 resolution
- **Graphics:** Test drawing primitives with test patterns (rectangles, lines, circles)
- **Fonts:** Display character set, verify text wraps correctly at screen edges
- **Input:** Verify keyboard produces correct characters; mouse movement tracked

## Phase 2 Testing - Window Manager
- **Window Creation:** Create test windows, verify they appear with decorations
- **Interactivity:** Drag windows by title bar, resize by borders
- **Focus:** Click window to bring to front; verify keyboard input goes to focused window
- **Tiling:** Open 3+ windows, switch between layouts, verify automatic positioning
- **Compositing:** Overlapping windows render correctly with proper z-order

## Phase 3 Testing - Networking
- **Loopback:** Test ping to 127.0.0.1; test TCP connection to localhost
- **ARP:** Verify ARP cache populates when pinging other addresses
- **ICMP:** Ping external IP (e.g., 8.8.8.8) from QEMU with e1000 NIC
- **TCP:** Implement simple echo server; connect from host and verify data exchange

## Phase 4 Testing - Shell & Utilities
- **Shell:** Run interactive session, test command parsing, history (if implemented)
- **File System:** Mount FAT image, verify `ls` shows files, `cat` displays content
- **Utilities:** Test each command with various inputs and edge cases
- **Network Utils:** Run `ping`, verify ICMP packets sent and responses received

## Phase 5 Testing - Integration
- **Hosted Mode:** Full GUI boot in hosted mode (via SDL2); all features functional
- **Bare Metal:** Boot in QEMU; verify graphics, network, input all work
- **Performance:** GUI responsive with multiple windows; network throughput acceptable

## Success Criteria
- [ ] Framebuffer displays at 800x600 or higher with 32-bit color
- [ ] Windows can be created, moved, resized, and closed
- [ ] Tiling layouts work with 3+ windows
- [ ] Ping to 8.8.8.8 works from QEMU with e1000
- [ ] TCP echo server accepts connections from host
- [ ] Shell runs, `ls`/`cat`/`echo` commands work
- [ ] Full GUI boots in hosted mode with working window manager
