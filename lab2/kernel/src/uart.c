#include "uart.h"
#include "sbi.h"
#include "initrd_parse.h"
#include "dt_parse.h"
#include "types.h"
#include "str.h"


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

void uart_int(unsigned long x) {
    char buf[21];//64 bit has atmost 20 digits plus \0
    int i = 0;

    if (x == 0) {
        uart_putc('0');
        return;
    }

    while (x > 0) {
        buf[i] = '0' + (x % 10);//char code + last digit
        i++;
        x /= 10;
    }

    while (i > 0) {//print backward
        i--;
        uart_putc(buf[i]);
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

/* legacy: below removed to shell.c, keep it here in case i mess up
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
        uart_puts("Available commands:\n");
        uart_puts("   help  - show all commands.\n");
        uart_puts("   hello - print Hello World.\n");
        uart_puts("   info  - print system info.\n");
        uart_puts("   ls    - file list.\n");
        uart_puts("   cat   - show content of a txt.\n");
    } 
    else if (streq(cmd, "hello") == 0)
    {
        uart_puts("Hello World.\n");
    } 
    else if (streq(cmd, "info") == 0)
    {
        uart_puts("System information:\n");
        info_print();
    }
    else if (streq(cmd, "ls") == 0)
    {   
        uintptr_t initrd_start = get_initrd_start((const void *)boot_dtb_pa);
        //uart_hex(initrd_start);
        initrd_list((const void *)initrd_start);
    }
    else if (strncmp(cmd, "cat ", 4) == 0)
    {
        uintptr_t initrd_start = get_initrd_start((const void *)boot_dtb_pa);
        if (initrd_start == 0) {
            uart_puts("cat: initrd not found\n");
            return;
        }
        const char *filename = cmd + 4;   // skip "cat "
        if (*filename == '\0') {
            uart_puts("usage: cat <filename>\n");
        } else {
            initrd_cat((const void *)initrd_start, filename);
        }
    }
    else if (cmd[0] != '\0')
    {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_puts("\r\n");
        uart_puts("Use help to get commands.\n");
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
*/