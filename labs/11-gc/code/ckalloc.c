#include "rpi.h"
#include "libc/helper-macros.h"
#include "ckalloc-internal.h"

// simplistic heap management: a single, contiguous heap given to us by calling
// ck_init
static uint8_t *heap = 0, *heap_end, *heap_start;
static unsigned nbytes_freed = 0, nbytes_alloced = 0;
void ck_init(void *start, unsigned n) {
    assert(aligned(heap_start, 8));
    printk("sizeof hdr=%d, redzone=%d\n", sizeof(hdr_t), REDZONE);
    heap = heap_start = start;
    heap_end = heap + n;
}

struct heap_info heap_info(void) {
    return (struct heap_info){
        .heap_start = heap_start,
        .heap_end = heap,
        .nbytes_freed = nbytes_freed,
        .nbytes_alloced = nbytes_alloced
    };
}

// compute checksum on header.  need to do w/ cksum set to known value!
uint32_t hdr_cksum(hdr_t *h) {
    unsigned old = h->cksum;
    h->cksum = 0;
    uint32_t cksum = fast_hash(h,sizeof *h);
    h->cksum = old;
    return cksum;
}

// check the header checksum and that its state == ALLOCED or FREED
int check_hdr(hdr_t *h) {
    // XXX
    /*printk("check_hdr: h = %p, h->cksum = %u, hdr_cksum(h) = %u\n", h, h->cksum, hdr_cksum(h));*/
    return h->cksum == hdr_cksum(h) && 
        (h->state == ALLOCED || h->state == FREED);
}

static int check_mem(hdr_t *h, char *p, unsigned nbytes) {
    int i;
    for(i = 0; i < nbytes; i++) {
        if(p[i] != SENTINAL) {
            int offset = (p + i) - (char *)b_alloc_ptr(h);
            ck_error(h, "block %p corrupted at offset %d\n", b_alloc_ptr(h), offset);
            return 0;
        }
    }
    return 1;
}
static void mark_mem(void *p, unsigned nbytes) {
    memset(p, SENTINAL, nbytes);
}

/*
 * check that:
 *  1. header is not corrupted (checksum passes).
 *  2. redzone 1 and redzone 2 are not corrupted.
 *
 */
static int check_block(hdr_t *h) {
    // short circuit the checks.
    return check_hdr(h)
        && check_mem(h, b_rz1_ptr(h), REDZONE)
        && check_mem(h, b_rz2_ptr(h), b_rz2_nbytes(h));
}

/*
 *  give an error if so.
 *  1. header is in allocated state.
 *  2. allocated block does not pass checks.
 */
void (ckfree)(void *addr, const char *file, const char *func, unsigned lineno) {
    hdr_t *h = 0;

    demand(heap, "not initialized?");
    trace("freeing %p\n", addr);

    // XXX
    h = b_addr_to_hdr(addr);
    if (!check_block(h)) {
        return;
    }
    if (h->state == FREED) {
        ck_error(h, "double free of block %p", addr);
        return;
    }
    h->free_loc = (src_loc_t){
        .file = file,
        .func = func,
        .lineno = lineno
    };
    // bookkeeping
    nbytes_freed += h->nbytes_alloc;

    h->state = FREED;
    h->refs_start = 0;
    h->refs_middle = 0;

    // set payload to sentinel
    mark_mem(addr, h->nbytes_alloc);
    h->cksum = hdr_cksum(h);
}

// check if nbytes + overhead causes an overflow.
void *(ckalloc)(uint32_t nbytes, const char *file, const char *func, unsigned lineno) {
    hdr_t *h = 0;
    void *ptr = 0;

    demand(heap, "not initialized?");
    trace("allocating %d bytes\n", nbytes);

    unsigned tot = pi_roundup(nbytes, 8);
    unsigned n = tot + OVERHEAD_NBYTES;
    
    // this can overflow.
    if(n < nbytes)
        trace_panic("size overflowed: %d bytes is too large\n", nbytes);

    if((heap + n) >= heap_end)
        trace_panic("out of memory!  have only %d left, need %d\n", 
            heap_end - heap, n);

    // XXX
    
    // create header at the end of the existing alloced blocks
    h = (hdr_t *)heap;
    src_loc_t alloc_loc = {
        .file = file,
        .func = func,
        .lineno = lineno
    };
    src_loc_t free_loc = {
        .file = 0,
        .func = 0,
        .lineno = 0
    };
    *h = (hdr_t){ 
        .nbytes_alloc = nbytes, 
        .nbytes_rem = tot-nbytes,
        .state = ALLOCED,
        .cksum = 0,
        .alloc_loc = alloc_loc,
        .free_loc = free_loc
    };
    h->cksum = hdr_cksum(h);

    // set redzones
    mark_mem(b_rz1_ptr(h), REDZONE);
    mark_mem(b_rz2_ptr(h), b_rz2_nbytes(h));

    // update end of the heap
    heap += n;
    
    // go from header to payload
    ptr = b_alloc_ptr(h);

    // bookkeping
    nbytes_alloced += nbytes;

    assert(check_hdr(h));
    assert(check_block(h));

    trace("ckalloc:allocated %d bytes, (total=%d), ptr=%p\n", nbytes, n, ptr);
    return ptr;
}

// integrity check the allocated / freed blocks in the heap
// if the header of a block is corrupted, just return.
// return the error count.
int ck_heap_errors(void) {
    unsigned alloced = heap - heap_start;
    unsigned left = heap_end - heap;

    trace("going to check heap: %d bytes allocated, %d bytes left\n", 
            alloced, left);
    unsigned nerrors = 0;
    unsigned nblks = 0;

    // XXX
    hdr_t *h = (hdr_t *)heap_start;
    while ((uintptr_t)h < (uintptr_t)heap) {
        // we're screwed if the header is corrupted
        if (!check_hdr(h))
            return ++nerrors;            

        // if the block is free but the payload is corrupted, we have an error
        if (h->state == FREED && !check_mem(h, b_alloc_ptr(h), h->nbytes_alloc)) {
            trace("\tWrote block after free!\n"); 
            nerrors++;
        }

        // `check_block` makes a redundant call to `check_hdr`, but that's okay
        if (!check_block(h))
            nerrors++;
        

        h = (hdr_t *)((char *)b_rz2_ptr(h) + b_rz2_nbytes(h));
        nblks++;
    }

    if(nerrors)
        trace("checked %d blocks, detected %d errors\n", nblks, nerrors);
    else
        trace("SUCCESS: checked %d blocks, detected no errors\n", nblks);
    return nerrors;
}

