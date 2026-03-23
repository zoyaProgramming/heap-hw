/**
 * All helper functions
 */
#ifndef HELPERS.h
#define HELPERS.h
#define ALIGNMENT_SIZE 16   /*1 row: alignment of payload*/
#define RSIZE 8             /* row size in bytes = 2 * 4*/
#define WSIZE 2             /*1 word in bytes*/
#define MIN_BLOCK_SIZE 32   /* 1 row header, 1 row */
#include "sfmm.h"

#define MAX(x, y) ((x) > (y)? (x) : (y))

/*macro to pack a allocation bit into a header size*/
#define PACK(size, alloc) ((size) | (alloc))

/*read and write a header at address p*/
#define GET(p) (*(uint64_t * )(p))
#define PUT(p, val) (*(uint64_t *)(p) = (val))

/*helpers to get payload size or block size*/
#define GET_PSIZE(p) (GET(p) & ~0xFFFFFFFF)
#define GET_BSIZE(p) (GET(p) &  0xFFFFFFF0)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*given a block ptr blockp, compute address of hdr and footr*/
#define HDRP(blockp)    ((char*)(blockp) - RSIZE)
#define FTRP(blockp)    ((char*)(blockp) + GET_BSIZE(HDRP(blockp)) - ALIGNMENT_SIZE)

/*given a block ptr blockp, compute address of next and prev blocks*/
#define NEXT_BLKP(blockp)((uint64_t*)(blockp) + GET_BSIZE((char*)(blockp) - RSIZE))
#define PREV_BLKP(blockp)((uint64_t*)(blockp) - GET_BSIZE((char*)(blockp) - ALIGNMENT_SIZE))


/*
 * "Quick lists":  These are used to hold recently freed blocks of small sizes, so that they
 * can be used to satisfy allocations without searching lists or splitting blocks.
 * Blocks on a quick list are marked as allocated, so they are not available for coalescing.
 * The number of blocks in any individual quick list is limited to QUICK_LIST_MAX.
 * If adding a block to a quick list would cause it to exceed QUICK_LIST_MAX, then the
 * list is flushed, returning the existing blocks in the list to the main pool, and then
 * the block being freed is added to the now-empty list, leaving that list containing one block.
 *
 * The quick lists are indexed by (size-MIN_BLOCK_SIZE)/ALIGN_SIZE, starting with blocks of the
 * minimum block size at index 0, blocks of size MIN_BLOCK_SIZE+ALIGN_SIZE at index 1, and so on.
 * They are maintained as singly linked lists, using a LIFO discipline.
 */




/* Type with information about a block
*/
typedef struct header_t{
    uint32_t payload_size;
    uint32_t block_size;
    bool in_qklst;
    bool alloc;
} header_t;


/** 
 * Return the first free sf_block of a certain size
 * @param size: size of the block to access
**/
sf_block * fetch_from_ql(size_t size);

/**
 * Parse the header.
 * @param 
 */
header_t parse_header(sf_header header);

/**
 * Find the first block with enough space to allocate a block of a certain size
 * @param size: the size of the block
 */
sf_block * find_fit(size_t size);


/**
 * Initialize the memory with one page 
 */
void * mem_init();

/**
 * grow memory and coalesce with the preceding block
 * 
 */
void * coalesce();


#endif