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
        blockp = place(blockp, asize, size);

        aggregate_payload += size;
        max_aggregate_payload = MAX(aggregate_payload, max_aggregate_payload);
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
    aggregate_payload += size;
    max_aggregate_payload = MAX(aggregate_payload, max_aggregate_payload);
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
    uint32_t old_payload_size = header.payload_size; /*preserve*/

    if (((header.block_size - MIN_BLOCK_SIZE) % ALIGNMENT_SIZE == 0) &&
        (header.block_size < MIN_BLOCK_SIZE + NUM_QUICK_LISTS * ALIGNMENT_SIZE) /*case 1*/
    )
    {
        header = (header_t){
            .alloc = 0,
            .block_size = header.block_size,
            .in_qklst = 1,
            .payload_size = 0};
        write_hdr(block, header);
        write_hdr(FTRP(block, header.block_size), header);
        ql_push(block);
    }
    else
    { /*case 2: add to main lists*/
        header = (header_t){
            .alloc = 0,
            .block_size = header.block_size,
            .in_qklst = 0,
            .payload_size = 0};
        write_hdr(block, header);
        write_hdr(FTRP(block, header.block_size), header);
        sf_block *new_ptr = coalesce(block);
        header_t new_hdr = parse_header(new_ptr->header);

        new_hdr.alloc = 0;
        new_hdr.in_qklst = 0;
        new_hdr.payload_size = 0;

        write_hdr(new_ptr, new_hdr);
        write_hdr(FTRP(new_ptr, new_hdr.block_size), new_hdr);

        free_list_push(new_ptr);
    }
    aggregate_payload -= old_payload_size;
}

void *sf_realloc(void *pp, size_t rsize)
{
    // To be implemented
    if (!validptr(pp))
    {
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block *block = (sf_block *)(pp - RSIZE);
    header_t hdr = parse_header(block->header);
    if (hdr.block_size <= 0) /*free ptr*/
    {
        sf_free(pp);
        return NULL;
    }

    /*Case 1 : Reallocating to a larger size block: call malloc and memcpy*/
    if (rsize > hdr.block_size)
    {
        void *new_block = sf_malloc(rsize);
        if (!new_block)
            return NULL;

        new_block = memcpy(new_block, pp, hdr.payload_size); /*copy payload to the new block*/
        sf_free(pp);
        return new_block;
    }
    else /**case 2: relocating to a smaller size block */
    {
        /*adjusted size of the block : smallest 16-byte aligned block that can fit the payload */
        size_t asize = rsize <= ALIGNMENT_SIZE ? MIN_BLOCK_SIZE : ALIGNMENT_SIZE * ((rsize + ALIGNMENT_SIZE + (ALIGNMENT_SIZE - 1)) / ALIGNMENT_SIZE);

        if (hdr.block_size - asize >= 32) /*original block big enough to split into 2 blocks*/
        {
            split(block, asize, rsize);

            aggregate_payload -= hdr.payload_size;
            aggregate_payload += rsize;
            return pp;
        }
        else /*block too small to split: only update the payload size */
        {
            hdr.payload_size = asize;
            write_hdr(block, hdr);
            write_hdr(FTRP(block, hdr.block_size), hdr);

            aggregate_payload -= hdr.payload_size;
            aggregate_payload += rsize;
            return pp;
        }
    }
}
/**
 *
 * @brief Get the current amount of internal fragmentation of the heap.
 *
 * @return  the current amount of internal fragmentation, defined to be the
 * ratio of the total amount of payload to the total size of allocated blocks.
 * If there are no allocated blocks, then the returned value should be 0.0.
 */
double sf_fragmentation()
{
    void *mem_start = sf_mem_start();
    void *epilogue = sf_mem_end() - 8;
    sf_block *curr = (sf_block *)(mem_start + 40);
    double total_payload = 0.0;
    double total_size = 0.0;

    if (epilogue - mem_start <= 48 + 32)
        return 0.0;


    while ((void *)curr < epilogue)
    {
        header_t header = parse_header(curr->header);
        if (header.alloc && !header.in_qklst)
        {
            total_payload += (double)(header.payload_size);
            total_size += (double)(header.block_size);
        }
        curr = NEXT_BLKP(curr, header.block_size);
    }
    debug("%lf %lf ", total_payload, total_size);
    return total_payload / total_size;
}

/*
 * Get the peak memory utilization for the heap.
 *
 * @return  the peak memory utilization over the interval starting from the
 * time the heap was initialized, up to the current time.  The peak memory
 * utilization at a given time, as defined in the lecture and textbook,
 * is the ratio of the maximum aggregate payload up to that time, divided
 * by the current heap size.  If the heap has not yet been initialized,
 * this function should return 0.0.
 */
double sf_utilization()
{
    double heap_size = (double)(sf_mem_end() - sf_mem_start());
    if (heap_size == 0.0)
    {
        return 0.0;
    }
    else
    {
        return ((double)max_aggregate_payload) / heap_size;
    }
}
