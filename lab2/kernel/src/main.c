#include "types.h"
#include "uart.h"
#include "dt_parse.h"
#include "shell.h"

uintptr_t uart_base;
unsigned long boot_hartid;
unsigned long boot_dtb_pa;

void start_kernel(unsigned long hartid, unsigned long dtb_pa) {
    boot_hartid = hartid;
    boot_dtb_pa = dtb_pa;

    uart_base = get_uart_base((const void *)boot_dtb_pa);

    uart_puts("\nWelcome to the kernel!\n");

    shell_run();
}
