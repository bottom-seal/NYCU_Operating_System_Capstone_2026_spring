#include "shell.h"
#include "uart.h"
#include "sbi.h"
#include "initrd_parse.h"
#include "dt_parse.h"
#include "types.h"
#include "str.h"

#define MAX_CMD_LEN 64


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
