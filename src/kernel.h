#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* Kernel version */
#define TINK_VERSION_MAJOR 0
#define TINK_VERSION_MINOR 1
#define TINK_VERSION_PATCH 0

/* Main kernel entry point */
void kernel_main(uint32_t magic, uint32_t* mboot_info);

/* Panic function - called on fatal errors */
__attribute__((noreturn)) void kernel_panic(const char* message);

#endif /* KERNEL_H */
