#include "sbi.h"
#include "uart.h"
void start_kernel() {
    uart_puts("\n314553011\n");

    shell_run();
}
