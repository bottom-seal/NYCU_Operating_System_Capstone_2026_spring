#ifndef LOADER_H
#define LOADER_H

#define MAX_CMD_LEN 64

extern unsigned long boot_hartid;
extern unsigned long boot_dtb_pa;

int load_kernel(void);
void shell_execute(const char *cmd);
void shell_run(void);


#endif
