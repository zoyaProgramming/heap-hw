/**
 * All helper functions
 */
#include "sfmm.h"
#ifndef HELPERS_H
#define HELPERS_H
#define ALIGNMENT_SIZE 16 /*1 row: alignment of payload*/
#define RSIZE 8           /* row size in bytes = 2 * 4*/
#define WSIZE 2           /*1 word in bytes*/
#define MIN_BLOCK_SIZE 32 /* 1 row header, 1 row */
#define MAX_QLIST_SIZE 208

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/*macro to pack a allocation bit into a header size*/
#define PACK(size, alloc) ((size) | (alloc))

/*read and write a header at address p*/
#define GET(p) (*(uint64_t *)(p))
#define PUT(p, val) (*(uint64_t *)(p) = (val))

/**Create a bitmask with LEN 1s, starting OFF bits from the right */
#define BITMASK(off, len) ((((0x1UL << len) - 1) << off))

/*helpers to get payload size or block size*/
#define GET_PSIZE(p) ((GET(p)) & BITMASK(32, 32))
#define GET_BSIZE(p) ((GET(p)) & 0xFFFFFFF0UL)
#define GET_ALLOC(p) ((GET(p)) & 0x1)

/**helpers to put byte / payload size */
/**put the payload size into 64-bit value at ptr  */
#define PUT_PSIZE(ptr, psize) (PUT(ptr, ((GET(ptr) & (BITMASK(0, 32))) | ((uint64_t)(psize) << 32))))
/**put the byte size into the 64-bit value at ptr */
#define PUT_BSIZE(ptr, bsize) (PUT(ptr, ((GET(ptr) & ~(BITMASK(4, 28))) | (bsize))))

#define PUT_QLIST(ptr, bit) (PUT(ptr, ((GET(ptr) & ~(0x2LU)) | (bit << 1))))
#define PUT_ALLOC(ptr, bit) (PUT(ptr, ((GET(ptr) & ~(0x1)) | (bit))))

/*given a block ptr blockp, compute address of next and prev blocks*/
#define NEXT_BLKP(blockp, blk_size) ((void *)(blockp) + blk_size)
#define PREV_BLKP(blockp, blk_size) ((void *)(blockp) - (blk_size))

/*A variable to record max payload*/
extern uint64_t aggregate_payload;
extern uint64_t max_aggregate_payload;
/* Type with information about a block header
 */
typedef struct header_t
{
    uint32_t payload_size;
    uint32_t block_size;
    bool in_qklst;
    bool alloc;
} header_t;
/*given a block ptr blockp, compute address of hdr and footr*/
#define HDRP(blockp) ((char *)(blockp))
#define FTRP(blockp, blk_size) ((void *)(blockp) + blk_size - RSIZE)
/**
 * Return the first free sf_block of a certain size
 * @param size: size of the block to access
 **/
sf_block *sf_mem_grow_safe();

/**
 * Parse the header's raw data, including block size, payload, and whether or not it is allocated/in quicklist.
 * Unobfuscates data by XOR with MAGIC.
 * @param header the 64-bit unsigned raw header, obfuscated by MAGIC
 */
header_t parse_header(sf_header header);

/**
 * @brief Write 64-bit header data to a location. After computing the 64-bit data from the input,
 * obfuscates and sets the ptr to the new 64-bit header.
 */
void write_hdr(void *ptr, header_t header);

/**
 * Find the first block with enough space to allocate a block of a certain size
 * Uses a first-fit policy
 * @param size: the size of the block
 */
sf_block *find_fit(size_t size);

/**
 * Initialize the memory with one page. Create 32-bit prologue and 8-bit epilogue
 * @return: pointer to the first block of the allocated memory */
sf_block *mem_init();

/**
 * coalesce block with any blocks left or right of it
 *
 * @note DOES NOT remove block from free list
 * @note left and right blocks are removed from free list
 * @return ptr to the coalesced block, still in free list*/
sf_block *coalesce(sf_block *blockp);

/**
 * @brief given a pointer to valid memory of the desired size,
 *          set the hdr and footer of the memory block, and return a ptr to the block.
 *          If there is enough space to split the block into 2 blocks, do so and add padding.
 *          Also remove the block from the free list, and quick list if applicable.
 * @param ptr ptr to the location in memory
 * @param asize size of the block, including padding, header, footer, size >= 32 bytes
 * @param size  size of the payload (user input provided)
 */
sf_block *place(void *ptr, size_t asize, size_t size);

/*Helper function to print a number's binary value*/
void print_binary(uint64_t num);

/**
 * @brief find the idx of the size class that a certain size fits into (in the main free list)
 * @param size: size of the free block for which you are determining the size class
 * @return the index of the proper free list for the size
 */
int size_class(size_t size);

/** @brief determine whether a ptr is valid
 * false if :
 * The pointer is NULL.
 * The pointer is not 16-byte aligned
 * After XOR'ing the stored header with MAGIC
 *  The block size is less than the minimum block size of 32.
 *  The block size is not a multiple of 16
 *  The header of the block is before the start of the first block of the heap, or the footer of the block is after the end of the last block in the heap.
 *  The allocated bit in the header is 0, or the in quick list bit in the header is 1.
 */
bool validptr(void *ptr);

/** */
sf_block *split(sf_block *ptr, size_t asize, size_t size);

#endif