#include "uart.h"
#include "str.h"
#include "dt_parse.h"
#include "sbi.h"
#include "trap.h"
#include "plic.h"
#include "timer.h"
#include "task.h"
//ABOUT EXTERNAL INTERRUPT

void irq_enable() {
    //csr set immediate, set the bit in the immediate position
    asm volatile("csrsi sstatus, 2");//bit 2 in sstatus is SIE, global interrupt
}

void irq_disable() {
    //csr clear immediate
    asm volatile("csrci sstatus, 2");
}
void do_trap(struct pt_regs *regs) {
    unsigned long cause = regs->scause;//get saved scause from trapframe
    unsigned long is_interrupt = cause >> 63;//last bit in scause says whether it is interrupt or exception
    unsigned long code = cause & 0xfffUL;//mask of 12 low bits, code is the 12 low bits

    if (is_interrupt && code == 5) {   // supervisor timer interrupt
        // top half only:
        // timer_interrupt_handler() should now enqueue expired callbacks
        // instead of calling cb(arg) directly
        timer_interrupt_handler();
    }
    else if (!is_interrupt && code == 8) {
        uart_puts("=== S-Mode trap ===\n");
        uart_puts("scause: ");
        uart_int(regs->scause);
        uart_puts("\n");
        uart_puts("sepc: ");
        uart_hex(regs->sepc);
        uart_puts("\n");
        uart_puts("stval: ");
        uart_hex(regs->stval);
        uart_puts("\n");

        regs->sepc += 4;//when it is excpetion, it is expected to not execute the instruction that causes it
    }
    else if (is_interrupt && code == 9) {   // supervisor external interrupt
        int irq = plic_claim();

        if (irq == UART_IRQ) {
            uart_isr();   // all uart isr is about moving data, there no deferred task
        }

        if (irq)
            plic_complete(irq);
    }

    // run deferred work before restoring context / sret
    task_run_all();
}