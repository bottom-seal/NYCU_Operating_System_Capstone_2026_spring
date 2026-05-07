#include "types.h"
#include "uart.h"
#include "loader.h"
#include "dt_parse.h"

uintptr_t uart_base;
uint64_t relocate_addr;
unsigned long boot_hartid;
unsigned long boot_dtb_pa;

void start_kernel(unsigned long hartid, unsigned long dtb_pa) {
    boot_hartid = hartid;
    boot_dtb_pa = dtb_pa;
    relocate_addr = get_relocate_addr();
    uart_base = get_uart_base((const void *)boot_dtb_pa);

    uart_puts("\nThis is just a boot loader, please input 'load' to load kernel\n");

    //while(1);
    shell_run();
}
