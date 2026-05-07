#include "dt_parse.h"
#include "types.h"
#include "str.h"


static inline uint32_t bswap32(uint32_t x) {
    //move 2-16 bit with mask, AND the 4 32 bits
    return ((x & 0x000000FFU) << 24) |
           ((x & 0x0000FF00U) << 8)  |
           ((x & 0x00FF0000U) >> 8)  |
           ((x & 0xFF000000U) >> 24);
}

static inline uint64_t bswap64(uint64_t x) {
    return ((x & 0x00000000000000FFULL) << 56) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x00000000FF000000ULL) << 8)  |
           ((x & 0x000000FF00000000ULL) >> 8)  |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0xFF00000000000000ULL) >> 56);
}

static inline const void* align_up(const void* ptr, size_t align) {
    //deal with address, return next aligned addr
    //plus align - 1 to round up.
    return (const void*)(((uintptr_t)ptr + align - 1) & ~(align - 1));
}


//just get size from header, needed for dynamic relocate
int fdt_get_size(const void* fdt) {
    if (!fdt) return -1;
    //
    const struct fdt_header* hdr = (const struct fdt_header*)fdt;
    // Check magic 
    if (bswap32(hdr->magic) != 0xd00dfeed)
    {
        return -1;
    }
    return (int)bswap32(hdr->totalsize);
}

int fdt_path_offset(const void* fdt, const char* path) {
    //return null on invalid inputs
    if (!fdt || !path) return -1;

    //treat starting addr as header structure (assume it has layout)
    const struct fdt_header* hdr = (const struct fdt_header*)fdt;

    /* Check magic */
    if (bswap32(hdr->magic) != 0xd00dfeed)
    {
        return -1;
    }
    //get staring addr of structure block 
    const char* struct_base = (const char*)fdt + bswap32(hdr->off_dt_struct);
    const char* p = struct_base;// p is cursor

    /* Keep node names of current path */
    const char* names[128];
    int depth = -1;//for tree depth, 0 when root

    while (1) {
        const char* token_pos = p;// need to return offset (token_pos - base)
        //read a 4 bytes each time (derefence at p)
        uint32_t token = bswap32(*(const uint32_t*)p);
        p += sizeof(uint32_t);//move past token

        if (token == FDT_BEGIN_NODE) {
            const char* node_name = p;// The name is stored as a null-terminated string (spec)
            size_t name_len = strlen(node_name);

            depth++;//found new node, depth ++
            if (depth >= 128)//to prevent overflow of name array, need to limit depth
            {
                return -1;
            } 
            names[depth] = node_name;//build the current name path

            /* Build current full path */
            char cur_path[1024];
            size_t pos = 0;//writing the path

            //rebuild the path each time
            //if only root, path should be "/"
            if (depth == 0 && node_name[0] == '\0') {// root has node name of \0
                //root
                cur_path[pos++] = '/';
                cur_path[pos] = '\0';//result "/"
            //else start with '/', then add each node name with '/' in between
            } else {
                cur_path[pos++] = '/';
                for (int i = 1; i <= depth; i++) {// for all the heirachy, try build the path
                    size_t len = strlen(names[i]);
                    if (pos + len + 1 >= sizeof(cur_path))// if the name we about to past make overflow
                    {
                        return -1;
                    }
                    memcpy(cur_path + pos, names[i], len);// add name to the comparing string
                    pos += len;
                    if (i != depth)//if not last node
                        cur_path[pos++] = '/';
                }
                cur_path[pos] = '\0';//append null
            }
            /* Return first match */
            if (strcmp(cur_path, path) == 0)//if found
            {
                return (int)(token_pos - struct_base);//return offset
            }
            char cmp_path[1024];
            strcpy(cmp_path, cur_path);
            //becuase some input doesn't provide addr, also try compare string without @
            char* last_slash = strrchr(cmp_path, '/');//function to find the last '/'
            if (last_slash) {
                char* at = strchr(last_slash + 1, '@');   //at stores pointer to @
                if (at) {
                    *at = '\0';   // temporarily cut "@..."
                    if (strcmp(cmp_path, path) == 0)
                    {
                        return (int)(token_pos - struct_base);
                    }
                }
            }
            p = align_up(p + name_len + 1, 4);//align since name has variable length, 1 byte for null
        } else if (token == FDT_END_NODE) {
            depth--;// no need to clear name, next begin node will only compare up to depth
        } else if (token == FDT_PROP) {
            uint32_t len = bswap32(*(const uint32_t*)p);
            //the struct is from spec
            p += sizeof(uint32_t);              /* len */
            p += sizeof(uint32_t);              /* nameoff */
            p = align_up(p + len, 4);           /* data + padding */
        } else if (token == FDT_NOP) {
            /* ignore */
        } else if (token == FDT_END) {
            break;
        } else {
            //weird node
            return -1;
        }
    }
    return -1;
}

const void* fdt_getprop(const void* fdt,
                        int nodeoffset,
                        const char* name,
                        int* lenp) {
    if (!fdt || !name || nodeoffset < 0) {
        if (lenp) *lenp = -1;//if lenp pointer is not null, write -1 to len (*lenp)
        return NULL;
    }

    const struct fdt_header* hdr = (const struct fdt_header*)fdt;

    /* Check magic */
    if (bswap32(hdr->magic) != 0xd00dfeed) {
        if (lenp) *lenp = -1;
        return NULL;
    }

    const char* struct_base  = (const char*)fdt + bswap32(hdr->off_dt_struct);//need to find property offset from structure block
    const char* strings_base = (const char*)fdt + bswap32(hdr->off_dt_strings);//find the actual name in string block
    const char* p = struct_base + nodeoffset;

    //check offset is valid and token is begin node
    uint32_t token = bswap32(*(const uint32_t*)p);//should jump to being_node token
    if (token != FDT_BEGIN_NODE) {
        if (lenp) *lenp = -1;
        return NULL;
    }

    p += sizeof(uint32_t);//move past node
    p = align_up(p + strlen(p) + 1, 4);//skip name

    int depth = 0;//depth tracks only target node

    while (1) {
        token = bswap32(*(const uint32_t*)p);//read token of current pos
        p += sizeof(uint32_t);//move forward 1 token
        // read next token in structure
        if (token == FDT_PROP) {
            // [FDT_PROP]
            // [len] : length of property value in bytes
            // [nameoff] : offset into strings block for the property name
            // [data bytes] : a byte string of length len
            // [padding] : actual property name string from strings block
            uint32_t len = bswap32(*(const uint32_t*)p);//len 
            p += sizeof(uint32_t);//move forward 32 bit len

            uint32_t nameoff = bswap32(*(const uint32_t*)p);//name offset
            p += sizeof(uint32_t);//move forward 32 bit nameoff

            const void* data = p;//pointer to the a byte string of length len
            const char* prop_name = strings_base + nameoff;// for string block
            if (depth == 0 && strcmp(prop_name, name) == 0) {//depth must have not incremented to be target node
                if (lenp) *lenp = (int)len;//store length in byte in property
                return data;// data was pointed to the [data bytes] section
            }

            p = align_up(p + len, 4);//data might not align
        } else if (token == FDT_BEGIN_NODE) {
            const char* child_name = p;
            p = align_up(p + strlen(child_name) + 1, 4);
            depth++;
        } else if (token == FDT_END_NODE) {
            if (depth == 0)
                break;  
            depth--;
        } else if (token == FDT_NOP) {
            /* ignore */
        } else if (token == FDT_END) {
            break;
        } else {
            if (lenp) *lenp = -1;
            return NULL;
        }
    }
    if (lenp) *lenp = -1;
    return NULL;
}

uintptr_t get_uart_base(const void *fdt) {
    int len;
    int offset = fdt_path_offset(fdt, "/soc/serial");
    const void* prop = fdt_getprop(fdt, offset, "reg", &len);
    if (!prop || len < 8)
        return 0;
    const uint64_t* reg = (const uint64_t*)prop;
    
    //printf("offset = %d\n", offset);
    //printf("len = %d\n", len);
    //printf("prop addr = %p\n", prop);
    //printf("UART: base=0x%lx size=0x%lx\n", bswap64(reg[0]), bswap64(reg[1]));
    
    return bswap64(reg[0]);
};

uintptr_t get_mem_start(const void *fdt) {
    // device tree is at fdt
    /* Find the node offset */
    int len;
    int offset = fdt_path_offset(fdt, "/memory");
    const void* prop = fdt_getprop(fdt, offset, "reg", &len);
    if (!prop || len < 8)
        return 0;
    const uint64_t* reg = (const uint64_t*)prop;
    //printf("offset = %d\n", offset);
    //printf("len = %d\n", len);
    //printf("prop addr = %p\n", prop);
    //printf("UART: base=0x%lx size=0x%lx\n", bswap64(reg[0]), bswap64(reg[1]));
    uint32_t mem_start_addr = bswap64(reg[0]);
    return mem_start_addr;
};

uintptr_t get_mem_end(const void *fdt) {
    // device tree is at fdt
    /* Find the node offset */
    int len;
    int offset = fdt_path_offset(fdt, "/memory");
    const void* prop = fdt_getprop(fdt, offset, "reg", &len);
    if (!prop || len < 8)
        return 0;
    const uint64_t* reg = (const uint64_t*)prop;
    //printf("offset = %d\n", offset);
    //printf("len = %d\n", len);
    //printf("prop addr = %p\n", prop);
    //printf("UART: base=0x%lx size=0x%lx\n", bswap64(reg[0]), bswap64(reg[1]));
    uint32_t mem_end_addr = bswap64(reg[0]) + bswap64(reg[1]);
    return mem_end_addr;
};

uintptr_t get_ramdisk_start(const void *fdt) {
    // device tree is at fdt
    /* Find the node offset */
    int len;
    int offset = fdt_path_offset(fdt, "/chosen");
    const void* prop = fdt_getprop(fdt, offset, "linux,initrd-start", &len);
    if (!prop || len < 8)
        return 0;
    const uint64_t* reg = (const uint64_t*)prop;
    //printf("offset = %d\n", offset);
    //printf("len = %d\n", len);
    //printf("prop addr = %p\n", prop);
    //printf("UART: base=0x%lx size=0x%lx\n", bswap64(reg[0]), bswap64(reg[1]));
    uint32_t ramdisk_start_addr = bswap64(reg[0]);
    return ramdisk_start_addr;
};

uintptr_t get_ramdisk_end(const void *fdt) {
    // device tree is at fdt
    /* Find the node offset */
    int len;
    int offset = fdt_path_offset(fdt, "/chosen");
    const void* prop = fdt_getprop(fdt, offset, "linux,initrd-end", &len);
    if (!prop || len < 8)
        return 0;
    const uint64_t* reg = (const uint64_t*)prop;
    //printf("offset = %d\n", offset);
    //printf("len = %d\n", len);
    //printf("prop addr = %p\n", prop);
    //printf("UART: base=0x%lx size=0x%lx\n", bswap64(reg[0]), bswap64(reg[1]));
    uint32_t ramdisk_end_addr = bswap64(reg[0]);
    return ramdisk_end_addr;
};
