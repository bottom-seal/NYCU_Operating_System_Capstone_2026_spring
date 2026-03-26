#include "uart.h"
#include "loader.h"


#define MAX_CMD_LEN 64

int load_kernel(void) {
    unsigned int magic;
    unsigned int size;
    unsigned char *dst = KERNEL_LOAD_ADDR;

    while ((*UART_LSR) & LSR_DR) {
        (void)(*UART_RBR);
    }

    magic = uart_get_u32_lil_end();
    size  = uart_get_u32_lil_end();

    if (magic != BOOT_MAGIC) {
        uart_puts("bad magic: ");
        uart_hex(magic);
        uart_puts("\n");
        return -1;
    }

    uart_puts("receiving kernel, size = ");
    uart_hex(size);
    uart_puts("\n");

    for (unsigned int i = 0; i < size; i++) {
        dst[i] = uart_get_byte();
    }

    loaded_kernel_size = size;
    

    uart_puts("Kernel loaded at ");
    uart_hex((unsigned long)KERNEL_LOAD_ADDR);
    uart_puts("\n");

    void (*kernel_entry)(unsigned long, unsigned long) =
    (void (*)(unsigned long, unsigned long))KERNEL_LOAD_ADDR;

    kernel_entry(boot_hartid, boot_dtb_pa);

    return 0;
}

void shell_execute(const char *cmd) {
    if (streq(cmd, "load") == 0)
    {
        uart_puts("loading kernel...\n");
        if(load_kernel() == 0)
            uart_puts("load success\n");
        else
            uart_puts("load failed\n");
    } 
    else if (cmd[0] != '\0')
    {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_puts("\r\n");
        uart_puts("the only command is load.\n");
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
