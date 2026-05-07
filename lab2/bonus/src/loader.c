#include "uart.h"
#include "loader.h"
#include "relocate.h"
#include "dt_parse.h"

#define MAX_CMD_LEN 64



int load_kernel(void) {
    unsigned int magic;
    unsigned int size;
    unsigned char *dst = KERNEL_LOAD_ADDR;

    unsigned long hartid = boot_hartid;
    unsigned long dtb_pa = boot_dtb_pa;

    while ((*UART_LSR) & LSR_DR) {//clear the input buffer
        (void)(*UART_RBR);//read the register, UART will put next byte in RBR
    }

    magic = uart_get_u32_lil_end();//read 32 bit from uart, convert to little endian
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

    //size is unsigned
    for (unsigned int i = 0; i < size; i++) {
        dst[i] = uart_get_byte();
    }

    loaded_kernel_size = size;


    uart_puts("Kernel loaded at ");
    uart_hex((unsigned long)KERNEL_LOAD_ADDR);
    uart_puts("\n");

    //"fence.i" to make sure all previous instructions are executed before jumping to kernel
    //volatile prevents compiler optimization, make sure all memory operations before this line are completed before jumping to kernel
    //::: "memory" assume this assembly affects memory visibility/order, so do not freely reorder memory operations across it
    asm volatile("fence.i" ::: "memory");

    //this jumps to the starting address of kernel, and pass hartid and dtb_pa as arguments in a0 and a1
    //start script will handle bss and stack pointer
    //syntax :
    //create a variable called kernel_entry
    //whose type is “pointer to a function taking two unsigned long arguments and returning void”
    //and set it to the address KERNEL_LOAD_ADDR

    //basically we treat the loaded kernel as a function, and jump to it with arguments
    //RISCV boot convention a0 = hart ID, a1 = DTB address
    void (*kernel_entry)(unsigned long, unsigned long) =
    (void (*)(unsigned long, unsigned long))KERNEL_LOAD_ADDR;

    kernel_entry(hartid, dtb_pa);

    return 0;
}

//functions to help me find valid addr to put bootloader

static uint64_t align_down(uint64_t x, uint64_t a) {
    //a - 1 has the bits needing mask be 1, invert to get the mask, AND with x to clear those bits, get the aligned down addr
    return x & ~(a - 1);
}

static int is_overlap(uint64_t s1, uint64_t e1, uint64_t s2, uint64_t e2) {
    if (e1 <= s2 || e2 <= s1)
        return 0;
    return 1;
}

uint64_t get_relocate_addr() {
    unsigned long dtb_pa = boot_dtb_pa;
    uint64_t fdt_size = fdt_get_size((const void*)dtb_pa);
    uint64_t mem_start = get_mem_start((const void*)dtb_pa);
    uint64_t mem_end = get_mem_end((const void*)dtb_pa);
    uint64_t ramdisk_start = get_ramdisk_start((const void*)dtb_pa);
    uint64_t ramdisk_end = get_ramdisk_end((const void*)dtb_pa);

    uint64_t dtb_start = (uint64_t)dtb_pa;
    uint64_t dtb_end = dtb_start + fdt_size;

    uint64_t boot_start = (uint64_t)_start;//from start.S
    uint64_t boot_end = (uint64_t)_end;
    uint64_t boot_size = boot_end - boot_start;

    uint64_t candidate = align_down(mem_end - boot_size, 16);//start from high addr of DRAM, reserved whole bootloader
    uint64_t candidate_end = candidate + boot_size;

    while (candidate >= mem_start) {
        if (!is_overlap(candidate, candidate_end, boot_start, boot_end) &&
            !is_overlap(candidate, candidate_end, dtb_start, dtb_end) &&
            !is_overlap(candidate, candidate_end, ramdisk_start, ramdisk_end)) {
            return candidate;
        }
        //check before decrement
        if (candidate < mem_start + 0x1000)
            break;

        candidate = align_down(candidate - 0x1000, 16);
        candidate_end = candidate + boot_size;
    }
    uart_puts("no suitable relocate address found\n");
    return 0;
}

//test only, was used to print relocated addr
void relocated_test_target(void) {
    uart_puts("entered relocated_test_target\n");
    uart_puts("current function addr: ");
    uart_hex((unsigned long)&relocated_test_target);
    uart_puts("\n");

    while (1) { }
}

//load integrated, should jump to this function in relocated addr
void actual_load()
{
    uart_puts("loading kernel...\n");
    if(load_kernel() == 0)
        uart_puts("load success\n");
    else
        uart_puts("load failed\n");
}

void shell_execute(const char *cmd) {
    if (streq(cmd, "load") == 0)
    { 
        uart_puts("relocating...\n");

        if (!already_relocated()) {
            uart_puts("copy bootloader to relocated address...\n");
            relocate_self();//all params are extern, see relocate.c

            void *new_sp = reloc_sym(_end);//function to get some symbol's new addr
            //function pointer takes void, returns void (actaul_load function)
            void (*relocated_main)(void) =
                (void (*)(void))reloc_sym(&actual_load);//acutal_load's addr in relocated space

            uart_puts("new sp: ");
            uart_hex((unsigned long)new_sp);
            uart_puts("\n");

            uart_puts("jump target: ");
            uart_hex((unsigned long)relocated_main);
            uart_puts("\n");

            uart_puts("switch stack and jump...\n");
            switch_sp_and_jump(new_sp, relocated_main);//jump and set new sp after everything is set, see new_sp.S

            uart_puts("jump failed, please reboot\n");//see this is bad
            while (1) { }
        }
        else{//in itegrated version, this will not run either
            uart_puts("already relocated, load directly\n");
            actual_load();
        }
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
