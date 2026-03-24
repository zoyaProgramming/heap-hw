#include "helpers.h"
#include "assert.h"
#include "sfmm.h"

header_t parse_header(sf_header header)
{
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
    uint32_t lower = (header.block_size & 0xFFFFFFF0) | ((header.in_qklst << 1) & 2) | (header.alloc & 1);
    PUT(ptr, ((uint64_t)(header.payload_size) << 32) | lower);
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
    while (idx + i <= NUM_FREE_LISTS)
    {
        sf_block *head = sf_free_list_heads[idx + i].body.links.next;
        bool isEmpty = (head->body.links.next == &sf_free_list_heads[idx]) &&
                       (head->body.links.prev == &sf_free_list_heads[idx]);
        if (!isEmpty)
        {
            header_t header = parse_header(head->header);
            sf_block *sentinel = head;
            while (header.block_size < size && head->body.links.next != sentinel)
            {
                head = head->body.links.next;
                header = parse_header(head->header);
            }
            // check if found block
            if (header.block_size >= size)
            {
                return head;
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
    PUT_BSIZE(&hdrp->header, PAGE_SZ);
    PUT_QLIST(&hdrp->header, 0);
    PUT_ALLOC(&hdrp->header, 0);
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

    /*DEBUG*/
    assert((sf_mem_end() - heap_start) >= 32);
    /*END DEBUG*/

    sf_block *prologue = (sf_block *)(heap_start + RSIZE); /*ptr to prologue (1 row after heap)*/
    // initialize prologue header
    header_t prologue_hdr = parse_header(prologue->header);
    prologue_hdr.alloc = 0;
    prologue_hdr.in_qklst = 0;
    prologue_hdr.block_size = MIN_BLOCK_SIZE;
    write_hdr(&prologue->header, prologue_hdr); /*write prologue header*/
    write_hdr(FTRP(prologue), prologue_hdr);    /*write prologue footer*/

    // initialize first block
    sf_block *blockp = (sf_block *)(FTRP(prologue) + RSIZE); /*ptr to start of empty block, not aligned*/
    header_t block_hdr = parse_header(blockp->header);
    block_hdr.block_size = heap_end - (void *)blockp; /*subtract prologue and extra space*/
    block_hdr.in_qklst = 0;
    block_hdr.alloc = 0;

    write_hdr(blockp, block_hdr);       /*write the empty block header*/
    write_hdr(FTRP(blockp), block_hdr); /* write*/
    blockp->body.links.next = NULL;
    blockp->body.links.prev = NULL;

    // initialize epilogue
    void *epilogue = FTRP(blockp) + RSIZE; /*skip past footer of empty*/
    PUT_ALLOC(epilogue, 1);
    PUT_QLIST(epilogue, 0);
    return blockp;
}

sf_block *coalesce(sf_block *blockp)
{
    header_t prev_hdr = parse_header(*(uint64_t *)(PREV_BLKP(blockp)));
    header_t next_hdr = parse_header(*(uint64_t *)(NEXT_BLKP(blockp)));
    header_t hdr = parse_header(blockp->header);

    if (prev_hdr.alloc && (next_hdr.alloc)) /* case 1*/
        return blockp;
    else if (prev_hdr.alloc && !next_hdr.alloc)
    { /*case 2*/
        blockp->body.links.next = ((sf_block *)NEXT_BLKP(blockp))->body.links.next;
        hdr.block_size += next_hdr.block_size;
        write_hdr(blockp, hdr); /*extends size past the og. footer*/
        write_hdr(FTRP(blockp), hdr);
    }

    else if (!prev_hdr.alloc && next_hdr.alloc)
    { /*case 3*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp));
        new_curr->body.links.next = blockp->body.links.next;
        hdr.block_size += prev_hdr.block_size;
        write_hdr(FTRP(blockp), hdr);      /*extends size of the prev. ptr*/
        write_hdr(&new_curr->header, hdr); /*new header */
        blockp = new_curr;
    }

    else
    { /*case 4*/
        sf_block *new_curr = (sf_block *)(PREV_BLKP(blockp));
        new_curr->body.links.next = ((sf_block *)NEXT_BLKP(blockp))->body.links.next; /*update links*/
        hdr.block_size += prev_hdr.block_size + next_hdr.block_size;
        write_hdr(HDRP(PREV_BLKP(blockp)), hdr); /* update previous header ptr*/
        write_hdr(FTRP(NEXT_BLKP(blockp)), hdr); /* update new footer pointer*/
        blockp = PREV_BLKP(blockp);
    }
    return blockp;
}

sf_block *place(void *ptr, size_t asize, size_t size)
{
    size_t bsize = GET_BSIZE(ptr); /*size of the free block*/
    sf_block *blockp = (sf_block *)ptr;

    header_t hdr = {
        .payload_size = size,
        .block_size = bsize - asize >= 32 ? asize : bsize, /*account for splitting when bsize large*/
        .alloc = 1,
        .in_qklst = 0};
    write_hdr(ptr, hdr);       /* write hdr*/
    write_hdr(FTRP(ptr), hdr); /*write footer*/

    if (bsize - asize >= 32)
    { /*handle splitting*/
        sf_block *splitptr = (sf_block *)NEXT_BLKP(ptr);
        void *ftrp = FTRP(splitptr);
        PUT_BSIZE(splitptr, bsize - asize);
        PUT_ALLOC(splitptr, 0);
        PUT_QLIST(splitptr, 0);
        splitptr->body.links.next = blockp->body.links.next;
        splitptr->body.links.prev = blockp;
        blockp->body.links.next = splitptr;

        /*write next*/
        PUT_BSIZE(ftrp, bsize - asize);
        PUT_ALLOC(ftrp, 0);
        PUT_QLIST(ftrp, 0);
    }
    return blockp;
}