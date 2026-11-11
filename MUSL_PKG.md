# musl + pkg (non-apk) layer for Tinx

## musl compatibility layer
- Location: src/musl.h, src/musl.c
- Provides a small subset of libc suitable for freestanding/static linking:
  - Memory: malloc, ree, calloc, ealloc
  - Formatted I/O: printf, printf, sprintf, snprintf
  - Stream I/O: open, close, read, write, puts, puts, putc
  - String/memory helpers delegating to lib_string
  - Process control: exit

## pkg package manager (not Alpine apk)
- Command: pkg <init|update|add|del|list|search|info|world> [pkg...]
- Database: /var/lib/pkg/installed/<pkg>, /var/lib/pkg/world
- Repository path: /repo
- Cache: /var/cache/pkg

### Example
`
tinx:/$ pkg init
tinx:/$ pkg add hello world
tinx:/$ pkg list
tinx:/$ pkg info hello
tinx:/$ pkg world
tinx:/$ pkg del world
`

## Non-GPL utilities
- All shell utilities (ls, cat, grep, wc, cp, mv, etc.) are original Tinx implementations.
- Licensed under the project license (MIT), not GPL.

## VFS improvements
- fs_create() added to support file creation.
- 	ouch now creates files when missing.
- Package installation uses fs_create + fs_write.
