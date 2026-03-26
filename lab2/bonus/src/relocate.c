#include "relocate.h"
#include "types.h"


static void *memcpy_kernel(void *dst, const void *src, size_t n) {
    //uint8 for byte
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;//copy whole kernel
    return dst;//return start of dst
}

static unsigned long boot_size(void) {
    return (unsigned long)(__boot_end - __boot_start);
}

static unsigned long sym_offset(void *sym) {
    return (unsigned long)((char *)sym - __boot_start);
}

void *reloc_sym(void *sym) {
    return (void *)(BOOT_RELOC_ADDR + sym_offset(sym));
}

int already_relocated(void) {
    unsigned long me = (unsigned long)&already_relocated;
    return (me >= BOOT_RELOC_ADDR) &&
           (me <  BOOT_RELOC_ADDR + boot_size());
}

void relocate_self(void) {
    memcpy_kernel((void *)BOOT_RELOC_ADDR, __boot_start, boot_size());
}