/*
 * Tinx Kernel - Serial Debug Port Implementation
 * Provides serial port output for panic debugging
 */

#include "serial.h"
#include "io.h"

static int serial_initialized = 0;

void serial_init(void) {
    /* Enable DLAB (set baud rate divisor) - LCR bit 7 */
    outb(SERIAL_COM1_BASE + SERIAL_LCR, 0x80);

    /* Set divisor to 3 (38400 baud) -> DLL=0x03, DLM=0x00 */
    outb(SERIAL_COM1_BASE + SERIAL_THR, 0x03);   /* DLL when DLAB=1 */
    outb(SERIAL_COM1_BASE + SERIAL_IER, 0x00);   /* DLM when DLAB=1 */

    /* Clear DLAB, set 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1_BASE + SERIAL_LCR, 0x03);

    /* Disable interrupts (IER) after divisor is set */
    outb(SERIAL_COM1_BASE + SERIAL_IER, 0x00);

    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_COM1_BASE + SERIAL_FCR, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(SERIAL_COM1_BASE + SERIAL_MCR, 0x0B);
    
    serial_initialized = 1;
    
    /* Send test character */
    serial_write_str("\n[Tinx Serial Debug Initialized]\n");
}

int serial_is_transmit_empty(void) {
    return (inb(SERIAL_COM1_BASE + SERIAL_LSR) & SERIAL_LSR_THRE);
}

void serial_write_byte(uint8_t byte) {
    /* Always mirror to QEMU debugcon 0xE9 for -debugcon capture, even before init */
    __asm__ volatile ("outb %0, %1" :: "a"(byte), "Nd"((uint16_t)0xE9));
    if (!serial_initialized) {
        /* Still try to send to COM1 even if not initialized for early boot */
        /* Wait a bit then try */
        for (volatile int i=0;i<1000;i++) __asm__ volatile("pause");
        // return; // don't return, try anyway
    }
    
    /* Wait until transmit buffer is empty - but don't hang forever if serial not ready */
    int timeout = 10000;
    while (!serial_is_transmit_empty() && timeout-- > 0) {
        __asm__ volatile("pause");
    }
    if (timeout <= 0) {
        /* If serial not ready, still continue */
        return;
    }
    
    outb(SERIAL_COM1_BASE + SERIAL_THR, byte);
}

void serial_write_str(const char* str) {
    if (!str) {
        return;
    }
    
    while (*str) {
        serial_write_byte((uint8_t)*str++);
    }
}

void serial_writeln(const char* str) {
    serial_write_str(str);
    serial_write_byte('\r');
    serial_write_byte('\n');
}

void serial_write_hex_digit(uint8_t nibble) {
    static const char hex_chars[] = "0123456789ABCDEF";
    serial_write_byte((uint8_t)hex_chars[nibble & 0xF]);
}

void serial_write_hex32(uint32_t value) {
    serial_write_str("0x");
    serial_write_hex_digit((value >> 28) & 0xF);
    serial_write_hex_digit((value >> 24) & 0xF);
    serial_write_hex_digit((value >> 20) & 0xF);
    serial_write_hex_digit((value >> 16) & 0xF);
    serial_write_hex_digit((value >> 12) & 0xF);
    serial_write_hex_digit((value >> 8) & 0xF);
    serial_write_hex_digit((value >> 4) & 0xF);
    serial_write_hex_digit(value & 0xF);
}

void serial_write_hex8(uint8_t value) {
    serial_write_str("0x");
    serial_write_hex_digit((value >> 4) & 0xF);
    serial_write_hex_digit(value & 0xF);
}

void serial_write_dec32(uint32_t value) {
    if (value == 0) {
        serial_write_byte('0');
        return;
    }
    
    /* Buffer for digits (max 10 digits for 32-bit) */
    char buf[12];
    int i = 0;
    
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    /* Print in reverse order */
    while (i > 0) {
        serial_write_byte(buf[--i]);
    }
}
