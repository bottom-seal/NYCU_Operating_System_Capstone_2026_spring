#ifndef LOADER_H
#define LOADER_H

#define MAX_CMD_LEN 64

extern unsigned long boot_hartid;
extern unsigned long boot_dtb_pa;
extern char _start[];
extern char _end[];
extern void switch_sp_and_jump(void *new_sp, void (*fn)(void));

int load_kernel(void);
void shell_execute(const char *cmd);
void shell_run(void);
uint64_t get_relocate_addr();

#endif
