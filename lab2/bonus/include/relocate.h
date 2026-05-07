#ifndef RELOCATE_H
#define RELOCATE_H

#include "types.h"

extern int64_t relocate_addr;

#define BOOT_RELOC_ADDR   relocate_addr
#define KERNEL_LOAD_ADDR  ((unsigned char*)0x00200000UL)

extern char __boot_start[];
extern char __boot_end[];
extern char start_kernel;

void *reloc_sym(void *sym);
int already_relocated(void);
void relocate_self(void);

#endif