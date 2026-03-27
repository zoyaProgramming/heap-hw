/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "helpers.h"
#include "listhelpers.h"

void *sf_malloc(size_t size)
{
    size_t asize;      /* adjusted block size*/
    size_t extendsize; /* adjust to extend heap if no fit*/
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
        if (mem_init() == NULL)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
    }

    /*search the free list for a fit*/
    if ((blockp = find_fit(asize)) != NULL)
    {
        header_t h = parse_header(blockp->header);
        blockp = place(blockp, asize, size);
        h = parse_header(blockp->header);
        return (void *)(blockp) + 8;
    }

    size_t sizecurr = 0L;

    /*if no fit found, try to grow heap */
    extendsize = (asize >= PAGE_SZ) ? asize : PAGE_SZ;
    do
    {
        if ((blockp = sf_mem_grow_safe()) == NULL)
        {
            sf_errno = ENOMEM;
            return NULL;
        }
        header_t hdr = parse_header(blockp->header);
        sizecurr = hdr.block_size;
    } while (sizecurr < extendsize);

    void *out = (void *)place(blockp, asize, size) + 8;
    return out;
}

void sf_free(void *pp)
{

    /** ignore out of bounds requests */
    if (!validptr(pp))
    {
        abort();
    }
    /*include the header in the block ptr*/
    sf_block *block = (sf_block *)(pp - RSIZE);
    header_t header = parse_header(block->header);
    header_t prev_hdr = parse_header(*(uint64_t *)(pp - 8));
    if (((header.block_size - MIN_BLOCK_SIZE) % ALIGNMENT_SIZE == 0) &&
        (header.block_size < MIN_BLOCK_SIZE + NUM_QUICK_LISTS * ALIGNMENT_SIZE) /*case 1*/
    )
    {
        header = (header_t){
            .alloc = 1,
            .block_size = header.block_size,
            .in_qklst = 1,
            .payload_size = 0};
        write_hdr(block, header);
        write_hdr(FTRP(block, header.block_size), header);
        ql_push(block);
        // int ql_idx = (header.block_size - 32) / 16;
        // if (sf_quick_lists[ql_idx].length == MAX_QLIST_SIZE)
        //     ql_flush(ql_idx);

        // block->body.links.next = sf_quick_lists[ql_idx].first; /*update links*/
        // sf_quick_lists[ql_idx].first = block;
        // sf_quick_lists[ql_idx].length += 1;
        return;
    }
    else
    { /*case 2: add to main lists*/
        header = (header_t){
            .alloc = 0,
            .block_size = header.block_size,
            .in_qklst = 0,
            .payload_size = 0};
        write_hdr(block, header);
        sf_block *new_ptr = coalesce(block);
        header_t new_hdr = parse_header(new_ptr->header);
        new_hdr.alloc = 0;
        new_hdr.in_qklst = 0;
        new_hdr.payload_size = 0;

        write_hdr(FTRP(new_ptr, new_hdr.block_size), new_hdr);
        header_t ftr_size = parse_header(((sf_block *)FTRP(new_ptr, new_hdr.block_size))->header);
        int idx = size_class(new_hdr.block_size);
        free_list_push(new_ptr);
        return;
    }
}

void *sf_realloc(void *pp, size_t rsize)
{
    // To be implemented
    if (!validptr(pp))
        abort();
    sf_block *block = (sf_block *)(pp - RSIZE);
    header_t hdr = parse_header(block->header);
    if (hdr.block_size <= 0) /*free ptr*/
    {
        sf_free(pp);
        return NULL;
    }
    /**
     * When reallocating to a larger size, always follow these three steps:
     Call sf_malloc to obtain a larger block.
     Call memcpy to copy the data in the block given by the client to the block returned by sf_malloc. Be sure to copy the entire payload area, but no more.

     Call sf_free on the block given by the client (inserting into a freelist and coalescing if required).

     Return the block given to you by sf_malloc to the client.

     If sf_malloc returns NULL, sf_realloc must also return NULL. Note that you do not need to set sf_errno in sf_realloc because sf_malloc should take care of this.
     */
    /*Case 1 : Reallocating to a larger size*/
    if (rsize > hdr.block_size) /**/
    {
        void *new_block = sf_malloc(rsize);
        if (!new_block)
            return NULL;
        memcpy(new_block, pp, hdr.payload_size); /*copy payload to the new block*/
        sf_free(pp);
        return new_block;
    }
    else /**case 2: try to split */
    {
        size_t asize = 0;
        if (rsize <= ALIGNMENT_SIZE) //
            asize = MIN_BLOCK_SIZE;
        else
            asize = ALIGNMENT_SIZE * ((rsize + ALIGNMENT_SIZE + (ALIGNMENT_SIZE - 1)) / ALIGNMENT_SIZE);
        if (hdr.block_size - asize >= 32)
        {
            void *new_block = (void *)split(block, asize, rsize);
            return pp;
        }
        else
        {
            hdr.block_size = asize;
            write_hdr(block, hdr);
            write_hdr(FTRP(block, hdr.block_size), hdr);
            return pp;
        }
    }
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
