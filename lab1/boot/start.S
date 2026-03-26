.section ".text.boot"
.global _start
_start:
    // TODO: Clear the .bss section
    la t0, __bss_start
    la t1, __bss_stop
bss_loop:
    bge t0, t1, bss_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j bss_loop
bss_done:
    // TODO: Set up the stack pointer
    la sp, _end
    tail start_kernel
