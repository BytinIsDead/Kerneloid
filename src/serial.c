/*
 * Tinx Kernel - Serial Debug Port Implementation
 * Provides serial port output for panic debugging
 */

#include "serial.h"
#include "io.h"

static int serial_initialized = 0;

void serial_init(void) {
    /* Disable interrupts */
    outb(SERIAL_COM1_BASE + SERIAL_IER, 0x00);
    
    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_COM1_BASE + SERIAL_LCR, 0x80);
    
    /* Set divisor to 3 (38400 baud) */
    outb(SERIAL_COM1_BASE + SERIAL_THR, 0x03);
    outb(SERIAL_COM1_BASE + SERIAL_IER, 0x00);
    
    /* Clear DLAB, set 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1_BASE + SERIAL_LCR, 0x03);
    
    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_COM1_BASE + SERIAL_FCR, 0xC7);
    
    /* IRQs enabled, RTS/DSR cleared */
    outb(SERIAL_COM1_BASE + SERIAL_MCR, 0x0B);
    
    /* Set serial line status */
    outb(SERIAL_COM1_BASE + SERIAL_LSR, 0x00);
    
    serial_initialized = 1;
    
    /* Send test character */
    serial_write_str("\n[Tinx Serial Debug Initialized]\n");
}

int serial_is_transmit_empty(void) {
    return (inb(SERIAL_COM1_BASE + SERIAL_LSR) & SERIAL_LSR_THRE);
}

void serial_write_byte(uint8_t byte) {
    if (!serial_initialized) {
        return;
    }
    
    /* Wait until transmit buffer is empty */
    while (!serial_is_transmit_empty());
    
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
