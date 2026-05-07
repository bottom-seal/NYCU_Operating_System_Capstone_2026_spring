#include "uart.h"

#define UART_BASE uart_base
#define UART_RBR  (unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (unsigned char*)(UART_BASE + 0x0)
#define UART_LSR  (unsigned char*)(UART_BASE + 0x14)
#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)

#define MAX_CMD_LEN 64

#define KERNEL_LOAD_ADDR ((unsigned char*)0x20000000UL)

unsigned int loaded_kernel_size = 0;


char uart_getc() {
  while ((*UART_LSR & LSR_DR) == 0)
        ;
  char c = (char)*UART_RBR;
  return c == '\r' ? '\n' : c;
}


unsigned char uart_get_byte() {
  while ((*UART_LSR & LSR_DR) == 0)
        ;
  char c = (char)*UART_RBR;
  return c;
}

unsigned int uart_get_u32_lil_end(void) {
    unsigned int b0 = uart_get_byte();
    unsigned int b1 = uart_get_byte();
    unsigned int b2 = uart_get_byte();
    unsigned int b3 = uart_get_byte();

    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}


void uart_putc(char c) {
    if (c == '\n')
        uart_putc('\r');

    while ((*UART_LSR & LSR_TDRQ) == 0)
        ;
    *UART_THR = c;
}

void uart_puts(const char* s) {
    while (*s)
        uart_putc(*s++);
}

void uart_hex(unsigned long h) {
    uart_puts("0x");
    unsigned long n;
    for (int c = 60; c >= 0; c -= 4) {
        n = (h >> c) & 0xf;
        n += n > 9 ? 0x57 : '0';
        uart_putc(n);
    }
}

//returns 1 if not match, returns 0 if match
int streq(const char *a, const char *b) {
    //check each character
    while (*a && *b) {
        if (*a != *b) 
          return 1;
        a++;
        b++;
    }
    //check if both are '\0'
    if(*a == *b)
      return 0;
    else
      return 1;
}


