/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include "helpers.h"

void *sf_malloc(size_t size)
{
    sf_set_magic(0);       // DEBUG
    size_t asize;          /* adjusted block size*/
    size_t extendsize;     /* adjust to extend heap if no fit*/
    size_t allocated_size; /*total allocated size of the heap*/
    sf_block *blockp = NULL;
    // ignore spurious requests
    if (size == 0)
        return NULL;

    /* adjust block size to include overhead and alignment reqs.*/
    if (size <= ALIGNMENT_SIZE) //
        asize = MIN_BLOCK_SIZE;
    else
        asize = ALIGNMENT_SIZE * ((size + ALIGNMENT_SIZE + (ALIGNMENT_SIZE - 1)) / ALIGNMENT_SIZE);

    /*initialize heap if not initialized already*/
    if (sf_mem_end() - sf_mem_start() < 32)
    {
        mem_init();
    }

    /*search the free list for a fit*/
    if ((blockp = find_fit(asize)) != NULL)
    {
        place(blockp, asize, size);
        return (void *)blockp;
    }
    size_t sizecurr = 0L;
    /*if no fit found, try to grow heap */
    extendsize = (asize >= PAGE_SZ) ? asize : PAGE_SZ;
    do
    {
        if ((blockp = sf_mem_grow_safe()) == NULL)
        {
            return NULL;
        }
        size += PAGE_SZ;
    } while (size < extendsize);
    place(blockp, asize, size);

    abort();
}

void sf_free(void *pp)
{
    // To be implemented
    abort();
}

void *sf_realloc(void *pp, size_t rsize)
{
    // To be implemented
    abort();
}

double sf_fragmentation()
{
    // To be implemented
    abort();
}

double sf_utilization()
{
    // To be implemented
    abort();
}
