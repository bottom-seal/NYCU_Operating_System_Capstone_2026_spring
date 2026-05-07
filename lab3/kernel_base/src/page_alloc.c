#include "page_alloc.h"
#include "list.h"
#include "uart.h"
#include "dt_parse.h"

//ONLY FOR DEMO, NOT IMPORTANT
static void log_add_block(unsigned int idx, unsigned int order) {
    uart_puts("[+] Add page ");
    uart_int(idx);
    uart_puts(" to order ");
    uart_int(order);
    uart_puts(". Range of pages: [");
    uart_int(idx);
    uart_puts(", ");
    uart_int(idx + (1U << order) - 1);
    uart_puts("]\n");
}

static void log_remove_block(unsigned int idx, unsigned int order) {
    uart_puts("[-] Remove page ");
    uart_int(idx);
    uart_puts(" from order ");
    uart_int(order);
    uart_puts(". Range of pages: [");
    uart_int(idx);
    uart_puts(", ");
    uart_int(idx + (1U << order) - 1);
    uart_puts("]\n");
}

static void log_buddy_found(unsigned int idx, unsigned int order, unsigned int buddy) {
    uart_puts("[*] Buddy found! buddy idx: ");
    uart_int(buddy);
    uart_puts(" for page ");
    uart_int(idx);
    uart_puts(" with order ");
    uart_int(order);
    uart_puts("\n");
}

static void log_page_alloc(unsigned int idx, unsigned int order) {
    uart_puts("[Page] Allocate ");
    uart_hex(MEMORY_BASE + (unsigned long)idx * PAGE_SIZE);
    uart_puts(" at order ");
    uart_int(order);
    uart_puts(", page ");
    uart_int(idx);
    uart_puts(". Next address at order ");
    uart_int(order);
    uart_puts(": ");
    uart_hex(MEMORY_BASE + (unsigned long)(idx + (1U << order)) * PAGE_SIZE);
    uart_puts("\n");
}

static void log_page_free(unsigned int idx, unsigned int order) {
    uart_puts("[Page] Free ");
    uart_hex(MEMORY_BASE + (unsigned long)idx * PAGE_SIZE);
    uart_puts(" and add back to order ");
    uart_int(order);
    uart_puts(", page ");
    uart_int(idx);
    uart_puts(". Next address at order ");
    uart_int(order);
    uart_puts(": ");
    uart_hex(MEMORY_BASE + (unsigned long)(idx + (1U << order)) * PAGE_SIZE);
    uart_puts("\n");
}

static void log_chunk_alloc(void *ptr, unsigned int chunk_size) {
    uart_puts("[Chunk] Allocate ");
    uart_hex((unsigned long)ptr);
    uart_puts(" at chunk size ");
    uart_int(chunk_size);
    uart_puts("\n");
}

static void log_chunk_free(void *ptr, unsigned int chunk_size) {
    uart_puts("[Chunk] Free ");
    uart_hex((unsigned long)ptr);
    uart_puts(" at chunk size ");
    uart_int(chunk_size);
    uart_puts("\n");
}

static void log_reserve(unsigned long start, unsigned long end) {
    uart_puts("[Reserve] Reserve address [");
    uart_hex(start);
    uart_puts(", ");
    uart_hex(end);
    uart_puts(")\n");
}
//end of logging code

//this files should have most of the LAB3
//I no longer adopt the data structure from lab page, now it uses structure from exercise
//order >= 0 && refcount == 0: this entry is the head of a free block of size 2^order * 4KB
//order >= 0 && refcount > 0: this entry is the head of an allocated/reserved block
//order == -1: this frame is inside a larger block, so not directly allocable
//https://hackmd.io/@yuanC-L/HJMD9qub0
static struct page mem_map[NUM_PAGES];

static struct list_head free_area[MAX_ORDER + 1];

static struct pool pools[NUM_POOLS] = {
    {16,   0},
    {32,   0},
    {64,   0},
    {128,  0},
    {256,  0},
    {512,  0},
    {1024, 0},
    {2048, 0},
};

//the smallest pool that can hold a request of size bytes
static int size_to_pool_idx(unsigned int size) {
    int i;

    for (i = 0; i < NUM_POOLS; i++) {
        if (size <= pools[i].chunk_size)
            return i;
    }

    return -1;
}

//use idx to get the actual addr of the page
static void *page_addr_to_phys_addr(struct page *page) {
    unsigned int idx = page_addr_to_idx(page);
    return (void *)(MEMORY_BASE + (unsigned long)idx * PAGE_SIZE);
}

//find the index of a page that some addr belong to 
static struct page *phys_addr_to_page_addr(void *ptr) {
    unsigned long addr = (unsigned long)ptr;
    unsigned int idx;

    if (addr < MEMORY_BASE)//illegal access
        return 0;

    idx = (unsigned int)((addr - MEMORY_BASE) / PAGE_SIZE);//find which page contains the addr
    if (idx >= NUM_PAGES)//illegal if exceeding max page
        return 0;

    return &mem_map[idx];
}

//if dynamic allocator finds empty pool, ask buddy for a page, and partition that into chunks
static int request_page_for_chunk(int pool_idx) {
    struct page *page;//page from buddy
    char *base;//start addr of the page in memory
    unsigned int chunk_size;//
    unsigned int n_chunks;//this page can give n chunks
    unsigned int i;//loop

    //check pool index valid
    if (pool_idx < 0 || pool_idx >= NUM_POOLS)
        return -1;

    //ask buddy for page of order 0 (1 page)
    page = alloc_pages(0);   // get one 4KB page
    if (page == 0)//check if sucess
        return -1;

    chunk_size = pools[pool_idx].chunk_size;//get chunk size from pool metadata
    base = (char *)page_addr_to_phys_addr(page);//base points to first byte of the requested page
    n_chunks = PAGE_SIZE / chunk_size;

    //mark page metadata
    page->alloc_type = ALLOC_TYPE_CHUNK;
    page->pool_idx = pool_idx;
    page->allocated_chunks = 0;//initially no chunk from this page is allocated yet

    //loop all chunks
    for (i = 0; i < n_chunks; i++) {
        struct chunk *c = (struct chunk *)(base + i * chunk_size);//start addr of each chunk, treated as data type chunk
        c->next = pools[pool_idx].free_list;//chunk is free, use start of it to store pointer to the free list, can overwrite when  needed
        pools[pool_idx].free_list = c;//new chunk added to the head of the list
    }

    return 0;
}

//for request that is smaller than 2048, use this function to allocate a chunk, returns pointer to the chunk
static void *dynamic_chunk_allocate(unsigned int size) {
    int pool_idx;
    struct chunk *c;
    struct page *page;

    //find the smallest chunk for request
    pool_idx = size_to_pool_idx(size);
    if (pool_idx < 0)//the function returns -1 on not found
        return 0;

    if (pools[pool_idx].free_list == 0) {//if free list is empty
        if (request_page_for_chunk(pool_idx) < 0)
            return 0;
    }

    c = pools[pool_idx].free_list;//take first free chunk
    pools[pool_idx].free_list = c->next;//move pool head to next free chunk

    page = phys_addr_to_page_addr((void *)c);//find which page this chunk belongs to
    if (page == 0)
        return 0;

    page->allocated_chunks++;//one more allocated chunk in this page

    log_chunk_alloc((void *)c, pools[pool_idx].chunk_size);
    return (void *)c;//return addr of the chunk
}

//for request bigger than 2048, use this to find the order needed, the function to allocate page takes order as param
static unsigned int size_to_order(unsigned int size) {
    unsigned int needed_pages;
    unsigned int order;
    unsigned int block_pages;

    needed_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;//ceiling, less than 4kb would become 1 page

    order = 0;
    block_pages = 1;
    //loop until the block exceeds needed pages
    while (block_pages < needed_pages) {
        block_pages <<= 1;
        order++;
    }

    return order;//returns order since page_allocate expects order as input
} 

//this function should only be called when size > biggest chunk (2048)
static void *dynamic_page_allocate(unsigned int size) {
    struct page *page;
    unsigned int order;
    unsigned int idx;

    //convert size > 2048 to order
    order = size_to_order(size);
    if (order > MAX_ORDER)//return exceeding order 10
        return 0;
    //request page from buddy
    page = alloc_pages(order);
    if (page == 0)//return on fail
        return 0;

    idx = page_addr_to_idx(page);//get the index in mem_map to udpate the metadata
    //only update metadata regarding chunk management
    mem_map[idx].alloc_type = ALLOC_TYPE_PAGE;
    mem_map[idx].pool_idx = -1;//doesn't belong to any chunk pool
    mem_map[idx].allocated_chunks = 0;

    log_page_alloc(idx, order);
    return page_addr_to_phys_addr(page);//returns actual addr of the block
}


//remove all free chunks that belong to this page from the pool free list
//needed before returning an empty chunk page back to buddy system
static void remove_page_chunks_from_pool(struct page *page) {
    int pool_idx;
    struct chunk *prev;
    struct chunk *curr;
    unsigned long page_start;
    unsigned long page_end;

    pool_idx = page->pool_idx;
    if (pool_idx < 0 || pool_idx >= NUM_POOLS)
        return;

    page_start = (unsigned long)page_addr_to_phys_addr(page);//start addr of this page
    page_end = page_start + PAGE_SIZE;//end addr of this page

    prev = 0;
    curr = pools[pool_idx].free_list;

    while (curr != 0) {
        struct chunk *next = curr->next;
        unsigned long addr = (unsigned long)curr;

        //if this chunk is inside the page range, remove it from free list
        if (addr >= page_start && addr < page_end) {
            if (prev == 0)
                pools[pool_idx].free_list = next;
            else
                prev->next = next;
        } else {
            prev = curr;
        }

        curr = next;
    }
}

//if we are freeing a chunk, just add it to free list, need page metadata to know the pool index
static void dynamic_chunk_free(void *ptr, struct page *page) {
    int pool_idx;
    struct chunk *c;
    unsigned int idx;

    pool_idx = page->pool_idx;
    if (pool_idx < 0 || pool_idx >= NUM_POOLS)
        return;

    c = (struct chunk *)ptr;//treat pointer as a chunk node
    log_chunk_free(ptr, pools[pool_idx].chunk_size);
    c->next = pools[pool_idx].free_list;//push the chunk back to the head of free list
    pools[pool_idx].free_list = c;

    //one allocated chunk from this page is now freed
    if (page->allocated_chunks == 0)
        return;

    page->allocated_chunks--;

    //if all chunks in this page are free, return this page back to buddy system
    if (page->allocated_chunks == 0) {
        idx = page_addr_to_idx(page);

        //remove all free chunks of this page from pool free list first
        //otherwise the pool free list would keep stale pointers
        remove_page_chunks_from_pool(page);

        mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
        mem_map[idx].pool_idx = -1;
        mem_map[idx].allocated_chunks = 0;

        log_page_free(idx, 0);
        free_pages(page);
    }
}

//if we are freeing a page, before using free_pages func, clear metadata for dynamic allocator
static void dynamic_page_free(void *ptr, struct page *page) {
    unsigned int idx;
    unsigned int order;

    (void)ptr;//make compiler stop complaining, passed ptr because chunk one did

    idx = page_addr_to_idx(page);
    order = page->order;//use index to access mem_map

    //for block, only head metadata is meaningful
    mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
    mem_map[idx].pool_idx = -1;
    mem_map[idx].allocated_chunks = 0;

    log_page_free(idx, order);
    free_pages(page);
}

//free will check the allocation type to decide which func to call
void free(void *ptr) {
    struct page *page;

    if (ptr == 0)
        return;

    page = phys_addr_to_page_addr(ptr);//get the page addr in mem_map
    if (page == 0)
        return;
    //use alloc_type attribute to decide which free to use
    if (page->alloc_type == ALLOC_TYPE_CHUNK) {
        dynamic_chunk_free(ptr, page);
        return;
    }

    if (page->alloc_type == ALLOC_TYPE_PAGE) {
        if (page->order < 0)//not allocated
            return;
        dynamic_page_free(ptr, page);
        return;
    }
}

//
void *allocate(unsigned int size) {
    if (size == 0)
        return 0;

    if (size > MAX_CHUNK_SIZE)//if > 2048, allocate page
        return dynamic_page_allocate(size);
    //else allocate chunks
    return dynamic_chunk_allocate(size);
}

//helper function for initial testing, might need it to run page_alloc_test.c 
int free_area_count(unsigned int order) {
    int cnt = 0;
    struct list_head *pos;

    for (pos = free_area[order].next; pos != &free_area[order]; pos = pos->next) {
        cnt++;
    }

    return cnt;
}

void page_alloc_init(void) {
    int i;

    //initialize all free-list heads
    for (i = 0; i <= MAX_ORDER; i++) {
        INIT_LIST_HEAD(&free_area[i]);
    }

    //mark all frames as unavailable first
    for (i = 0; i < NUM_PAGES; i++) {
        mem_map[i].order = -1;
        mem_map[i].refcount = 1;
        mem_map[i].alloc_type = ALLOC_TYPE_NONE;
        mem_map[i].pool_idx = -1;
        mem_map[i].allocated_chunks = 0;
        INIT_LIST_HEAD(&mem_map[i].node);
    }

    //split the whole managed region into largest possible blocks
    //each block has 2^MAX_ORDER pages.
    for (i = 0; i < NUM_PAGES; i += (1 << MAX_ORDER)) {
        //block head
        mem_map[i].order = MAX_ORDER;
        mem_map[i].refcount = 0;
        list_add_tail(&mem_map[i].node, &free_area[MAX_ORDER]);
        log_add_block(i, MAX_ORDER);
    }
}

//basic exercise 1
//also for advanced exercise 1,
//lookup time = O(1) page_addr_to_idx or page_idx_to_addr, which are just conversions of constant time

//since we are using intrusive linked list
//need to find the offset of the connecting node, so we can get other attributes
//the byte offset of MEMBER inside struct TYPE
//pretends addr 0 is a pointer to struct TYPE,
//if struct start at addr 0, what addr would MEMBER be, the addr is the offset
#define offsetof(TYPE, MEMBER) ((unsigned long)&(((TYPE *)0)->MEMBER))
//given a pointer to a member, find the pointer to the whole struct containing it
//page = ptr of node - its offset
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))//convert to char becaues offset is in byte

unsigned int page_addr_to_idx(struct page *page) {
    return (unsigned int)(page - mem_map);
}

struct page *page_idx_to_addr(unsigned int idx) {
    return &mem_map[idx];
}

//order k block ha size of 2^k, which is 1 << order, flipping the bit to find the buddy of the idx
static unsigned int buddy_idx(unsigned int idx, unsigned int order) {
    return idx ^ (1U << order);
}


//
struct page* alloc_pages(unsigned int order) {
    if (order > MAX_ORDER)
        return 0;

    //find smallest free list with order >= requested
    for (unsigned int current_order = order; current_order <= MAX_ORDER; current_order++) {
        if (!list_empty(&free_area[current_order])) {//if found any free list, split to smallest block, return its address
            struct list_head *node;
            struct page *page;
            unsigned int idx;

            //take first block from free list
            node = free_area[current_order].next;
            list_del(node);
            //get addr of the page
            //free list stores nodes, not pages, so we get the page addr
            page = container_of(node, struct page, node);
            idx = page_addr_to_idx(page);
            log_remove_block(idx, current_order);

            //split until requested size
            while (current_order > order) {
                struct page *right_buddy;
                unsigned int right_idx;

                current_order--;

                //right half starts at idx + 2^current_order, current_order already -1
                right_idx = idx + (1U << current_order);
                right_buddy = page_idx_to_addr(right_idx);

                //left half becomes smaller free head
                mem_map[idx].order = current_order;
                mem_map[idx].refcount = 0;
                mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[idx].pool_idx = -1;

                //right half becomes free block head with same order
                right_buddy->order = current_order;
                right_buddy->refcount = 0;
                right_buddy->alloc_type = ALLOC_TYPE_NONE;
                right_buddy->pool_idx = -1;

                //initialize buddy's node, and add it to free list
                INIT_LIST_HEAD(&right_buddy->node);
                list_add_tail(&right_buddy->node, &free_area[current_order]);
                log_add_block(right_idx, current_order);
            }

            //after the while loop, the idx should point to a block of order
            //only first page has meaningful metadata
            mem_map[idx].order = order;
            mem_map[idx].refcount = 1;
            mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
            mem_map[idx].pool_idx = -1;

            return &mem_map[idx];//return address
        }
    }

    return 0;//here means no block could be allocated
}

void free_pages(struct page *page) {
    unsigned int idx;
    unsigned int order;
    //check null page
    if (page == 0)
        return;

    idx = page_addr_to_idx(page);

    //check head metadata not allocated
    if (mem_map[idx].order < 0 || mem_map[idx].refcount == 0)
        return;

    order = mem_map[idx].order;

    //if block is free, refcount should say it is free
    mem_map[idx].refcount = 0;
    mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
    mem_map[idx].pool_idx = -1;
    INIT_LIST_HEAD(&mem_map[idx].node);

    //merge upward
    while (order < MAX_ORDER) {
        unsigned int bidx = buddy_idx(idx, order);

        if (bidx >= NUM_PAGES)
            break;

        //if buddy is same order and free
        if (mem_map[bidx].order != (int)order || mem_map[bidx].refcount != 0)
            break;//breaks merging if fail

        //remove from free list
        log_buddy_found(idx, order, bidx);
        log_remove_block(bidx, order);
        list_del(&mem_map[bidx].node);

        //this old buddy head is no longer a head after merge
        mem_map[bidx].order = -1;
        mem_map[bidx].refcount = 1;
        mem_map[bidx].alloc_type = ALLOC_TYPE_NONE;
        mem_map[bidx].pool_idx = -1;

        //use the lower addr for head addr
        if (bidx < idx) {
            mem_map[idx].order = -1;
            mem_map[idx].refcount = 1;
            mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
            mem_map[idx].pool_idx = -1;
            idx = bidx;
        }

        order++;
        //now it is order +1
        mem_map[idx].order = order;
        mem_map[idx].refcount = 0;
        mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
        mem_map[idx].pool_idx = -1;
        INIT_LIST_HEAD(&mem_map[idx].node);
    }

    //insert final merged block into corresponding free list
    list_add_tail(&mem_map[idx].node, &free_area[order]);
    log_add_block(idx, order);
}


//end of basic exercise 1


//memory reserve
void memory_reserve(unsigned long base, size_t size) {
    unsigned int start_pfn;
    unsigned int end_pfn;
    int order;

    if (size == 0)
        return;

    start_pfn = (unsigned int)(base / PAGE_SIZE);
    end_pfn = (unsigned int)((base + size + PAGE_SIZE - 1) / PAGE_SIZE);

    log_reserve((unsigned long)base, (unsigned long)(base + size));

    for (order = MAX_ORDER; order >= 0; order--) {
        struct list_head *pos = free_area[order].next;

        while (pos != &free_area[order]) {
            struct list_head *next = pos->next;
            struct page *page = container_of(pos, struct page, node);
            unsigned int idx = page_addr_to_idx(page);
            unsigned int block_start = idx;
            unsigned int block_end = idx + (1U << order);

            /* no overlap */
            if (block_end <= start_pfn || block_start >= end_pfn) {
                pos = next;
                continue;
            }

            /* remove overlapped block from free list first */
            log_remove_block(idx, order);
            list_del(pos);

            /* fully covered by reserved range */
            if (start_pfn <= block_start && block_end <= end_pfn) {
                mem_map[idx].order = order;
                mem_map[idx].refcount = 1;
                mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[idx].pool_idx = -1;

                pos = next;
                continue;
            }

            /* partial overlap: split into two smaller buddies */
            if (order > 0) {
                unsigned int half = 1U << (order - 1);
                unsigned int left_idx = idx;
                unsigned int right_idx = idx + half;

                /* current big block head is no longer a valid head after split */
                mem_map[idx].order = -1;
                mem_map[idx].refcount = 1;
                mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[idx].pool_idx = -1;

                /* left block head */
                mem_map[left_idx].order = order - 1;
                mem_map[left_idx].refcount = 0;
                mem_map[left_idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[left_idx].pool_idx = -1;
                INIT_LIST_HEAD(&mem_map[left_idx].node);
                list_add_tail(&mem_map[left_idx].node, &free_area[order - 1]);
                log_add_block(left_idx, order - 1);

                /* right block head */
                mem_map[right_idx].order = order - 1;
                mem_map[right_idx].refcount = 0;
                mem_map[right_idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[right_idx].pool_idx = -1;
                INIT_LIST_HEAD(&mem_map[right_idx].node);
                list_add_tail(&mem_map[right_idx].node, &free_area[order - 1]);
                log_add_block(right_idx, order - 1);
            } else {
                /* order 0 and partial overlap means this single page must be reserved */
                mem_map[idx].order = 0;
                mem_map[idx].refcount = 1;
                mem_map[idx].alloc_type = ALLOC_TYPE_NONE;
                mem_map[idx].pool_idx = -1;
            }

            pos = next;
        }
    }
}