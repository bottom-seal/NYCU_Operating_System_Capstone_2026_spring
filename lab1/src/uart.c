#include "uart.h"
#include "sbi.h"


#define UART_BASE 0xD4017000UL
#define UART_RBR  (unsigned char*)(UART_BASE + 0x0)
#define UART_THR  (unsigned char*)(UART_BASE + 0x0)
#define UART_LSR  (unsigned char*)(UART_BASE + 0x14)
#define LSR_DR    (1 << 0)
#define LSR_TDRQ  (1 << 5)

#define MAX_CMD_LEN 64

char uart_getc() {
  while ((*UART_LSR & LSR_DR) == 0)
        ;
  char c = (char)*UART_RBR;
  return c == '\r' ? '\n' : c;
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

//prints stuff when given info command
void info_print() {
    uart_puts("   OpenSBI specification version:  ");
    uart_hex(sbi_get_spec_version());
    uart_puts("\n");
    uart_puts("   Implementation ID:  ");
    uart_hex(sbi_get_impl_id());
    uart_puts("\n");
    uart_puts("   Implementation version:  ");
    uart_hex(sbi_get_impl_version());
    uart_puts("\n");
}

void shell_execute(const char *cmd) {
    if (streq(cmd, "help") == 0)
    {
        uart_puts("Available commands:\r\n");
        uart_puts("   help  - show all commands.\r\n");
        uart_puts("   hello - print Hello World.\r\n");
        uart_puts("   info  - print system info.\r\n");
    } 
    else if (streq(cmd, "hello") == 0)
    {
        uart_puts("Hello World.\r\n");
    } 
    else if (streq(cmd, "info") == 0)
    {
        uart_puts("System information:\r\n");
        info_print();
    }
    else if (cmd[0] != '\0')
    {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_puts("\r\n");
        uart_puts("Use help to get commands.\r\n");
    }
}

void shell_run(void) {
    char buf[MAX_CMD_LEN];
    int idx = 0;
    
    while(1)//loop for commands
    {
        uart_puts("opi-rv2> ");
        idx = 0;
        
        while ( 1)//look for each command
        {
            char c = uart_getc();
            if (c == '\r' || c == '\n') {//enter: see if command hit, then enter new command
                uart_puts("\r\n");
                buf[idx] = '\0';
                shell_execute(buf);
                break;
            }
            if (c == 8 || c == 127) {//backspace: cover with space then go back 1.
                if (idx > 0) {
                    idx--;
                    uart_puts("\b \b");
                }
                continue;
            }
            if (idx < MAX_CMD_LEN - 1) {//normal, append to *cmd, print to terminal
                buf[idx++] = c;
                uart_putc(c);
            }
        }
    }
}
