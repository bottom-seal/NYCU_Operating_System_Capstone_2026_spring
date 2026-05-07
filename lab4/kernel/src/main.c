#include "types.h"
#include "uart.h"
#include "dt_parse.h"
#include "page_alloc.h"
#include "shell.h"
#include "trap.h"
#include "plic.h"
#include "timer.h"
#include "task.h"
uintptr_t uart_base;
unsigned long cpu_freq;
uintptr_t plic_base;
unsigned long boot_hartid;
unsigned long boot_dtb_pa;

void start_kernel(unsigned long hartid, unsigned long dtb_pa) {
    boot_hartid = hartid;
    boot_dtb_pa = dtb_pa;
    uart_base = get_uart_base((const void *)boot_dtb_pa);
    debug_uart_puts("boot: start_kernel\n");
    cpu_freq = get_timebase_frequency((const void *)boot_dtb_pa);
    plic_base = get_plic_base((const void *)boot_dtb_pa);
    debug_uart_puts("boot: plic_base=");
    debug_uart_hex((unsigned long)plic_base);
    debug_uart_puts("\n");
    
    if (plic_base == 0) {
        debug_uart_puts("ERROR: cannot find PLIC base in DTB\n");
        while (1)
            ;
    }
    debug_uart_puts("boot: task_init\n");
    task_init();//didn't touch csr
    debug_uart_puts("boot: uart_init\n");
    uart_init();//enable interrupt on UART side only
    debug_uart_puts("boot: plic_init\n");
    plic_init();//sets sie.SEIE, and PLIC side thing
    debug_uart_puts("boot: irq_enable\n");
    irq_enable();//sets sstatus.SIE, enable global interrupts
    debug_uart_puts("boot: page_alloc_init\n");
    page_alloc_init((const void *)boot_dtb_pa);
    debug_uart_puts("boot: timer_init\n");
    timer_init();//enables sie.STIE only
    uart_puts("\nWelcome to the kernel!\n");

    shell_run();
}
