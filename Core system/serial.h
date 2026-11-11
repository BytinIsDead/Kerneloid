/*
 * Tinx Kernel - Serial Debug Port Handler
 * Provides serial port output for panic debugging
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* COM1 Port addresses */
#define SERIAL_COM1_BASE    0x3F8
#define SERIAL_COM2_BASE    0x2F8

/* Serial register offsets */
#define SERIAL_RHR          0   /* Receiver Holding Register (read) */
#define SERIAL_THR          0   /* Transmitter Holding Register (write) */
#define SERIAL_IER          1   /* Interrupt Enable Register */
#define SERIAL_FCR          2   /* FIFO Control Register */
#define SERIAL_LCR          3   /* Line Control Register */
#define SERIAL_MCR          4   /* Modem Control Register */
#define SERIAL_LSR          5   /* Line Status Register */

/* Line Status Register bits */
#define SERIAL_LSR_DR       0x01    /* Data Ready */
#define SERIAL_LSR_THRE     0x20    /* Transmit Holding Register Empty */

/* Initialize serial port (COM1) */
void serial_init(void);

/* Write a byte to serial port */
void serial_write_byte(uint8_t byte);

/* Write a string to serial port */
void serial_write_str(const char* str);

/* Write a hex digit */
void serial_write_hex_digit(uint8_t nibble);

/* Write a hex number (32-bit) */
void serial_write_hex32(uint32_t value);

/* Write a hex number (8-bit/byte) */
void serial_write_hex8(uint8_t value);

/* Write a decimal number */
void serial_write_dec32(uint32_t value);

/* Write a newline */
void serial_writeln(const char* str);

/* Check if serial port is ready to transmit */
int serial_is_transmit_empty(void);

#endif /* SERIAL_H */
