#ifndef UART_H
#define UART_H

void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);
void uart_hex(unsigned long h);

void shell_run(void);

#endif
