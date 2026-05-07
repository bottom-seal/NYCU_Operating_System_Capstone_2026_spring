#ifndef UART_H
#define UART_H

#define BOOT_MAGIC 0x544F4F42U
#define KERNEL_LOAD_ADDR ((unsigned char*)0x00200000UL)

#define UART_BASE uart_base
#define UART_RBR  (unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (unsigned char*)(UART_BASE + 0x0)
#define UART_LSR  (unsigned char*)(UART_BASE + 0x14)
#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)

#include "types.h"

extern uintptr_t uart_base;
extern unsigned int loaded_kernel_size;

char uart_getc(void);
unsigned char uart_get_byte(void);
unsigned int uart_get_u32_lil_end(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_hex(unsigned long h);

int streq(const char *a, const char *b);

#endif
