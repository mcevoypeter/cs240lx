// trivial program to start: allocate a single block, free it, check the
// heap
//
// i'd start by making sure you get the identical values for:
// ckalloc, then ckfree, then ck_heap_errors.
//
// you can do by running
//      "make testall"
// or adding testall to the makefile "all:" target.
#include "rpi.h"
#include "ckalloc.h"

void notmain(void) {
    printk("test1\n");

    // start heap allocating after the first mb.   give it 1mb to use.
    kmalloc_init_set_start(0x100000);
    unsigned n = 1024*1024;
    ck_init(kmalloc(n),n);

    char *p = (char *)0x100000 + ALLOC_OFFSET;

    printk("%p=0xff\n", 0xff);

    ck_heap_errors();
}
