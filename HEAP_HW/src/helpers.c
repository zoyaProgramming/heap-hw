#include "helpers.h"
#include "assert.h"
#include "sfmm.h"
#include "debug.h"

#include <limits.h>

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
/* a function to write the fields of a header/footer to a pointer*/
void write_hdr(void *ptr, header_t header)
{
    uint64_t hdr = 0x0;
    PUT_ALLOC(&hdr, header.alloc);
    PUT_BSIZE(&hdr, header.block_size);
    PUT_PSIZE(&hdr, header.payload_size);
    PUT_QLIST(&hdr, header.in_qklst);
    PUT(ptr, hdr ^ MAGIC);
}

sf_block *fetch_from_ql(size_t size)
{
    // sf_quick_lists
    int idx = ((size - MIN_BLOCK_SIZE) / ALIGNMENT_SIZE);
    int len = sf_quick_lists[idx].length;
    if (len == 0)
        return NULL;

    struct sf_block *first = sf_quick_lists[idx].first;
    return first;
}

sf_block *fetch_from_ql_deprecated(size_t size)
{
    // sf_quick_lists
    int idx = ((size - MIN_BLOCK_SIZE) / ALIGNMENT_SIZE);
    int len = sf_quick_lists[idx].length;
    if (len == 0)
        return NULL;
    else if (len == 1)
    {
        struct sf_block *first = sf_quick_lists[idx].first;
        first->body.links.next = NULL;
        first->body.links.prev = NULL;
        sf_quick_lists->length = 0;
        sf_quick_lists->first = NULL;
        return first;
    }
    // len >= 2, must set the
    struct sf_block *first = sf_quick_lists[idx].first;
    struct sf_block *next = first->body.links.next;
    next->body.links.prev = NULL;
    first->body.links.next = NULL;
    sf_quick_lists[idx].first = next;
    sf_quick_lists[idx].length -= 1;
    return first;
}

sf_block *find_fit(size_t size)
{
    int idx = 0;

    size_t upper_b = MIN_BLOCK_SIZE;

    // check quick lists for fit

    if (((size - MIN_BLOCK_SIZE) % ALIGNMENT_SIZE == 0) &&
        (size <= MIN_BLOCK_SIZE + NUM_QUICK_LISTS * ALIGNMENT_SIZE))
    {
        sf_block *block = fetch_from_ql_deprecated(size);
        if (block)
            return block;
    }
    size_t max = 32768; // 1024 * 32;
    // find smallest size group that size fits into
    //  (upper_b/2, upper_b]
    while (size > upper_b && upper_b <= max)
    {
        upper_b <<= 2;
        idx++;
    }

    // search main free lists for fit
    int i = 0;
    while (idx + i < NUM_FREE_LISTS)
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
                if (header.block_size >= size)
                    return curr;
                curr = curr->body.links.next;
            }
        }

        i++;
    }

    return NULL;
}

sf_block *sf_mem_grow_safe()
{
    void *p = sf_mem_grow();
    if (!p)
        return NULL;
    sf_block *hdrp = (sf_block *)p; //
                                    // header_t new_hdrp = parse_header(*(uint64_t *)(hdrp)); /*gets the header ptr of the current block, previously the epilogue*/
    header_t hdr = {
        .payload_size = 0,
        .block_size = PAGE_SZ,
        .in_qklst = 0,
        .alloc = 0};
    write_hdr(hdrp, hdr);

    return coalesce(hdrp);
}

/**
 * @brief: initialize a heap. with a prologue, one page of memory, and an epilogue.
 * @return: pointer to the first allocated block in memory
 */
sf_block *mem_init()
{
    void *heap_start = sf_mem_grow();
    void *heap_end = sf_mem_end();
    if (!heap_start)
        return NULL;

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
    blockp->body.links.next = &sf_free_list_heads[0];
    blockp->body.links.prev = &sf_free_list_heads[0];

    // initialize epilogue
    void *epilogue = FTRP(blockp, block_hdr.block_size) + RSIZE; /*skip past footer of empty*/
    header_t epilogue_hdr = {
        .payload_size = 0,
        .block_size = 0,
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(epilogue, epilogue_hdr);

    sf_free_list_heads[0].body.links.next = blockp;
    sf_free_list_heads[0].body.links.prev = blockp;
    return blockp;
}

sf_block *coalesce(sf_block *blockp)
{
    header_t hdr = parse_header(blockp->header);
    header_t prev_ftr = parse_header(*(uint64_t *)((char *)blockp - RSIZE));
    header_t prev_hdr = parse_header(*(uint64_t *)(PREV_BLKP(blockp, prev_ftr.block_size)));
    header_t next_hdr = parse_header(*(uint64_t *)NEXT_BLKP(blockp, hdr.block_size));

    if (prev_hdr.alloc && (next_hdr.alloc)) /* case 1*/
        return blockp;
    else if (prev_hdr.alloc && !next_hdr.alloc)
    { /*case 2*/
        blockp->body.links.next = ((sf_block *)NEXT_BLKP(blockp, hdr.block_size))->body.links.next;
        hdr.block_size += next_hdr.block_size;
        write_hdr(blockp, hdr); /*extends size past the og. footer*/
        write_hdr(FTRP(blockp, hdr.block_size), hdr);
    }

    else if (!prev_hdr.alloc && next_hdr.alloc)
    { /*case 3*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp, prev_ftr.block_size));
        new_curr->body.links.next = blockp->body.links.next;
        hdr.block_size += prev_hdr.block_size;
        write_hdr(FTRP(blockp, hdr.block_size), hdr); /*extends size of the prev. ptr*/
        write_hdr(&new_curr->header, hdr);            /*new header */
        blockp = new_curr;
    }

    else
    { /*case 4*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp, prev_ftr.block_size));
        new_curr->body.links.next = ((sf_block *)NEXT_BLKP(blockp, hdr.block_size))->body.links.next; /*update links*/
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
    sf_block *prev = NULL;
    sf_block *next = NULL;

    if (!hdr.in_qklst)
    {
        prev = blockp->body.links.prev;
        next = blockp->body.links.next;
        prev->body.links.next = next;
        next->body.links.prev = prev;
    }

    header_t new_hdr = {
        .payload_size = size,
        .block_size = bsize - asize >= 32 ? asize : bsize, /*account for splitting when bsize large*/
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(ptr, new_hdr);                           /* write hdr*/
    write_hdr(FTRP(ptr, new_hdr.block_size), new_hdr); /*write footer*/

    if (bsize - asize >= 32)
    { /*handle splitting*/
        sf_block *splitptr = (sf_block *)NEXT_BLKP(ptr, new_hdr.block_size);
        header_t split_hdr = {
            .payload_size = 0,
            .block_size = bsize - asize,
            .alloc = 0,
            .in_qklst = 0};
        void *ftrp = FTRP(splitptr, split_hdr.block_size);

        write_hdr(splitptr, split_hdr);
        if (!hdr.in_qklst)
        {
            splitptr->body.links.next = next;
            splitptr->body.links.prev = prev;
            prev->body.links.next = splitptr;
            next->body.links.prev = splitptr;
        }

        /*write next*/
        write_hdr(ftrp, split_hdr);
    }
    return blockp;
}

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