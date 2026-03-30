#include "helpers.h"
#include "assert.h"
#include "sfmm.h"
#include "debug.h"
#include "listhelpers.h"
#include <limits.h>

uint64_t aggregate_payload = 0;
uint64_t max_aggregate_payload = 0;
/**
 * The minimum size class that the size would fit into
 * @brief compute log2(size/32)
 */

int size_class(size_t size)
{
    size_t upper_b = 1;
    int idx = 0;
    while (size > upper_b * 32 && idx < (NUM_FREE_LISTS - 1))
    {
        upper_b <<= 1;
        idx++;
    }
    return idx;
}
/*DEBUG function: print number in binary*/
void print_binary(uint64_t num)
{
    int bits = sizeof(num) * CHAR_BIT; // Total bits in unsigned int
    int leading_zero = 0;              // Flag to skip leading zeros

    for (int i = bits - 1; i >= 0; i--)
    {
        uint64_t mask = 1ul << i;
        if (num & mask)
        {
            putchar('1');
            leading_zero = 0;
        }
        else if (!leading_zero)
        {
            putchar('0');
        }
    }

    // If the number is zero, print '0'
    if (leading_zero)
    {
        putchar('0');
    }
    printf("\n");
    fflush(stdout);
}

/** @brief determine whether a ptr is valid
 */
bool validptr(void *ptr)
{
    if (ptr == NULL || (uint64_t)ptr % 16 != 0)
        return false;
    sf_header *header_ptr = (sf_header *)(ptr - RSIZE);
    if ((ptr) < sf_mem_start() + 48) /*48 = 8 bytes unused, 32 byte prologue, 8 byte footer*/
    {
        return false;
    }
    header_t hdr = parse_header(*header_ptr);
    if ((hdr.block_size < 32) ||
        (hdr.block_size % 16 != 0) ||
        (hdr.block_size + ptr > sf_mem_end()) ||
        (hdr.alloc == 0) ||
        (hdr.in_qklst == 1))
    {

        return false;
    }
    return true;
}

header_t parse_header(sf_header header)
{
    header = header ^ MAGIC;
    header_t out = (header_t){
        .payload_size = (uint32_t)((header >> 32)),
        .block_size = (uint32_t)(0xFFFFFFF0 & header),
        .in_qklst = (header & 2),
        .alloc = (header & 1),
    };
    return out;
}

void write_hdr(void *ptr, header_t header)
{
    uint64_t hdr = 0x0;
    PUT_ALLOC(&hdr, header.alloc);
    PUT_BSIZE(&hdr, header.block_size);
    PUT_PSIZE(&hdr, header.payload_size);
    PUT_QLIST(&hdr, header.in_qklst);
    PUT(ptr, hdr ^ MAGIC);
}

sf_block *find_fit(size_t size)
{
    // check quick lists for fit
    if (((size - MIN_BLOCK_SIZE) % ALIGNMENT_SIZE == 0) &&
        (size < MIN_BLOCK_SIZE + NUM_QUICK_LISTS * ALIGNMENT_SIZE))
    {
        size_t ql_idx = ((size - MIN_BLOCK_SIZE) / ALIGNMENT_SIZE); /*quicklist index*/
        sf_block *block = ql_pop(ql_idx);
        if (block) /*case 1: found in quicklist*/
            return block;
    }
    /*case 2: search main free lists for first fit*/
    int idx = size_class(size); /*smallest possible segregated list*/
    int i = 0;
    while (idx + i < NUM_FREE_LISTS) /*look through freelists starting from idx*/
    {
        sf_block *sentinel = sf_free_list_heads + idx + i;
        bool isEmpty = (sentinel->body.links.next == sentinel) &&
                       (sentinel->body.links.prev == sentinel);
        if (!isEmpty)
        {
            sf_block *curr = sentinel->body.links.next;
            while (curr != sentinel)
            {
                header_t header = parse_header(curr->header);
                if (header.block_size >= size) /*check header block size*/
                    return curr;
                curr = curr->body.links.next;
            }
        }
        i++;
    }
    return NULL;
}
/**
 * @brief initialize a heap. with a prologue, one page of memory, and an epilogue.
 * @return pointer to the first allocated block in memory
 */
sf_block *mem_init()
{
    void *heap_start = sf_mem_grow();
    void *heap_end = sf_mem_end();
    if (!heap_start)
        return NULL;

    /*initialize the sentinel node links*/
    for (int i = 0; i < NUM_FREE_LISTS; i++)
    {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }

    sf_block *prologue = (sf_block *)(heap_start + RSIZE); /*ptr to prologue (1 row after heap)*/
    // initialize prologue header
    header_t prologue_hdr = (header_t){
        .alloc = 1,
        .payload_size = 0,
        .block_size = MIN_BLOCK_SIZE,
        .in_qklst = 0};
    write_hdr(&(prologue->header), prologue_hdr);                     /*write prologue header*/
    write_hdr(FTRP(prologue, prologue_hdr.block_size), prologue_hdr); /*write prologue footer*/

    // initialize first block
    sf_block *blockp = (sf_block *)((void *)FTRP(prologue, prologue_hdr.block_size) + RSIZE); /*ptr to start of empty block, not aligned*/
    header_t block_hdr = {
        .block_size = heap_end - (void *)blockp - RSIZE,
        .payload_size = 0,
        .in_qklst = 0,
        .alloc = 0};

    write_hdr((void *)blockp, block_hdr);                     /*write the empty block header*/
    write_hdr(FTRP(blockp, block_hdr.block_size), block_hdr); /* write*/
    /* add the block to the front of the free list*/
    free_list_push(blockp);

    // initialize epilogue
    void *epilogue = FTRP(blockp, block_hdr.block_size) + RSIZE; /*skip past footer of empty*/
    header_t epilogue_hdr = {
        .payload_size = 0,
        .block_size = 0,
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(epilogue, epilogue_hdr);
    return blockp;
}

sf_block *sf_mem_grow_safe()
{
    void *p = sf_mem_grow();
    if (!p)
        return NULL;
    sf_block *hdrp = (sf_block *)(p - RSIZE); //
                                              // header_t new_hdrp = parse_header(*(uint64_t *)(hdrp)); /*gets the header ptr of the current block, previously the epilogue*/
    header_t hdr = {
        .payload_size = 0,
        .block_size = PAGE_SZ,
        .in_qklst = 0,
        .alloc = 0};
    write_hdr(hdrp, hdr);
    void *epilogue = FTRP(hdrp, hdr.block_size) + RSIZE; /*skip past footer of empty*/
    header_t epilogue_hdr = {
        .payload_size = 0,
        .block_size = 0,
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(epilogue, epilogue_hdr);

    hdrp = coalesce(hdrp);
    free_list_push(hdrp);

    return hdrp;
}

sf_block *coalesce(sf_block *blockp)
{
    header_t hdr = parse_header(blockp->header);
    header_t prev_ftr = parse_header(*(uint64_t *)((char *)blockp - RSIZE));
    header_t prev_hdr = parse_header(*(uint64_t *)(PREV_BLKP(blockp, prev_ftr.block_size)));
    header_t next_hdr = parse_header(*(uint64_t *)NEXT_BLKP(blockp, hdr.block_size));
    if (prev_hdr.alloc && (next_hdr.alloc)) /* case 1*/
    {
        return blockp;
    }
    else if (prev_hdr.alloc && !next_hdr.alloc)
    { /*case 2*/
        sf_block *next = (sf_block *)NEXT_BLKP(blockp, hdr.block_size);
        free_list_remove(next);
        hdr.block_size += next_hdr.block_size;
        write_hdr(blockp, hdr); /*extends size past the og. footer*/
        write_hdr(FTRP(blockp, hdr.block_size), hdr);
    }

    else if (!prev_hdr.alloc && next_hdr.alloc)
    { /*case 3: prev is found*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp, prev_ftr.block_size));
        new_curr = free_list_remove(new_curr);

        void *ftrp = FTRP(blockp, hdr.block_size);
        hdr.block_size += prev_hdr.block_size;
        write_hdr(ftrp, hdr);              /*extends size of the prev. ptr*/
        write_hdr(&new_curr->header, hdr); /*new header */
        blockp = new_curr;
    }

    else
    {
        /*case 4*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp, prev_ftr.block_size));
        sf_block *next = (sf_block *)NEXT_BLKP(blockp, hdr.block_size);

        /*update links in free list*/
        free_list_remove(new_curr);
        free_list_remove(next);

        hdr.block_size += prev_hdr.block_size + next_hdr.block_size;
        write_hdr(new_curr, hdr);                       /* update previous header ptr*/
        write_hdr(FTRP(new_curr, hdr.block_size), hdr); /* update new footer pointer*/
        blockp = new_curr;
    }
    return blockp;
}

sf_block *place(void *ptr, size_t asize, size_t size)
{

    sf_block *blockp = (sf_block *)ptr;
    header_t hdr = parse_header(blockp->header);
    size_t bsize = hdr.block_size; /*size of the free block*/
    if (!hdr.in_qklst)
    {
        free_list_remove(blockp);
    }
    if (bsize - asize < MIN_BLOCK_SIZE)
    {
        header_t full = {
            .payload_size = size,
            .block_size = bsize,
            .alloc = 1,
            .in_qklst = 0};
        write_hdr(blockp, full);
        write_hdr(FTRP(blockp, bsize), full);
        return blockp;
    }
    split(blockp, asize, size);
    return blockp;
}

sf_block *split(sf_block *ptr, size_t asize, size_t size)
{
    sf_block *blockp = (sf_block *)ptr;
    header_t hdr = parse_header(blockp->header);
    size_t bsize = hdr.block_size; /*size of the free block*/
    header_t new_hdr = {
        .payload_size = size,
        .block_size = asize, /*account for splitting when bsize large*/
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(blockp, new_hdr);                           /* write hdr*/
    write_hdr(FTRP(blockp, new_hdr.block_size), new_hdr); /*write footer*/

    /*handle splitting*/
    sf_block *rem_block = (sf_block *)NEXT_BLKP(ptr, asize);
    header_t rem_hdr = {
        .payload_size = 0,
        .block_size = bsize - asize,
        .alloc = 0,
        .in_qklst = 0};
    write_hdr(rem_block, rem_hdr);
    write_hdr(FTRP(rem_block, rem_hdr.block_size), rem_hdr);

    /*write next*/
    rem_block = coalesce(rem_block);

    rem_hdr = parse_header(rem_block->header);
    free_list_push(rem_block);
    return blockp;
}
