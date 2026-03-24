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
void write_hdr(void* ptr, header_t header){
    uint32_t lower = (header.block_size & 0xFFFFFFF0) | (header.in_qklst << 1)&2  | (header.alloc & 1);
    PUT(ptr, ((uint64_t)(header.payload_size) << 32) | lower);
}

sf_block * fetch_from_ql(size_t size){
   // sf_quick_lists
    int idx = ((size - MIN_BLOCK_SIZE ) / ALIGNMENT_SIZE);
    int len =  sf_quick_lists[idx].length;
    if(len == 0) return NULL;

    struct sf_block * first = sf_quick_lists[idx].first;
    return first;
}


sf_block * fetch_from_ql_deprecated(size_t size){
   // sf_quick_lists
    int idx = ((size - MIN_BLOCK_SIZE ) / ALIGNMENT_SIZE);
    int len =  sf_quick_lists[idx].length;
    if(len == 0) return NULL;
    else if (len==1)
    {
        struct sf_block * first = sf_quick_lists[idx].first;
        first->body.links.next=NULL;
        first->body.links.prev=NULL;
        sf_quick_lists->length = 0;
        sf_quick_lists->first = NULL;
        return first;
    }
    // len >= 2, must set the 
    struct sf_block * first = sf_quick_lists[idx].first;
    struct sf_block * next = first->body.links.next;
    next->body.links.prev = NULL;
    first->body.links.next = NULL;
    sf_quick_lists[idx].first=next;
    sf_quick_lists[idx].length -= 1;
    return first;
}

sf_block * find_fit(size_t size){
    int idx = 0;
    size_t upper_b = MIN_BLOCK_SIZE;
    
    // check quick lists for fit
    if(((size - MIN_BLOCK_SIZE ) % ALIGNMENT_SIZE == 0) && 
    (size <= MIN_BLOCK_SIZE + NUM_QUICK_LISTS * ALIGNMENT_SIZE)){
        sf_block * block = fetch_from_ql_deprecated(size);
        return block;
    }
    
    size_t max = 32768; // 1024 * 32;
    // find smallest size group that size fits into
    //  (upper_b/2, upper_b]
    while(size > upper_b && upper_b <= max){
        upper_b <<=2;
        idx++;
    }

    // search main free lists for fit
    int i = 0;
    while (idx + i <= NUM_FREE_LISTS){
        sf_block * head = sf_free_list_heads[idx + i].body.links.next;
        bool isEmpty = head->body.links.next == &sf_free_list_heads[idx] && head->body.links.prev == &sf_free_list_heads;

        if(!isEmpty){
            header_t header = parse_header(head->header);
            sf_block * sentinel = head;
            // search the free list, 
            while( header.block_size < size && head->body.links.next != sentinel){
                head = head->body.links.next;
                header = parse_header(head->header);
            }
            // check if found block
            if(header.block_size >= size){
                return head;
            }
        }
        
        i++;
    }

    return NULL;
}

void * sf_mem_grow_safe(){
    void * p = sf_mem_grow();
    if(!p) return NULL;
    char * hdrp = HDRP(p);
    header_t new_hdrp = parse_header(*(uint64_t*)(hdrp)); /*gets the header ptr of the current block, previously the epilogue*/
    new_hdrp.block_size = PAGE_SZ; /*block size*/    
    new_hdrp.payload_size=PAGE_SZ - ALIGNMENT_SIZE; /* page size - 16byte*/
    new_hdrp.alloc = 0; new_hdrp.in_qklst = 0;
    return coalesce(hdrp);
}

void * mem_init(){
    void * heap_start = sf_mem_grow();
    void * heap_end = sf_mem_end();
    if(!heap_start ) return NULL;

    /*DEBUG*/
    assert((sf_mem_end() - heap_start ) >= 32);
    /*END DEBUG*/

    void * prologue = heap_start + RSIZE; /*increase by 8 :*/
    // initialize prologue header
    header_t prologue_hdr = parse_header(*(uint64_t * )(prologue + RSIZE));
    prologue_hdr.alloc =0;
    prologue_hdr.in_qklst = 0;
    prologue_hdr.block_size = MIN_BLOCK_SIZE;
    write_hdr(prologue + RSIZE, prologue_hdr);         /*write prologue header*/
    write_hdr(FTRP(prologue), prologue_hdr);           /*write prologue footer*/

    // initialize empty block
    
    header_t block_hdr = parse_header(*(uint64_t * )(prologue + prologue_hdr.block_size));
    void * blockp = FTRP(prologue) + ALIGNMENT_SIZE; /*ptr to start of empty block*/
    block_hdr.block_size = (void*)(HDRP(heap_end)) - blockp;       /*subtract prologue and extra space*/
    block_hdr.payload_size = block_hdr.block_size - ALIGNMENT_SIZE; /*subtract alignment size */
    block_hdr.in_qklst = 0;
    block_hdr.alloc = 0;
    write_hdr(blockp, block_hdr);      /*write the empty block header*/
    write_hdr(FTRP(blockp), block_hdr);

}

void * coalesce(sf_block * blockp){
    header_t prev_hdr = parse_header(*(uint64_t *)(PREV_BLKP(blockp)));
    header_t next_hdr = parse_header(*(uint64_t *)(NEXT_BLKP(blockp)));
    header_t hdr = parse_header(*(uint64_t*)HDRP(blockp));
    
    if(prev_hdr.alloc && (next_hdr.alloc ))         /* case 1*/
        return blockp;
    else if (prev_hdr.alloc && !next_hdr.alloc ){   /*case 2*/
        hdr.block_size += next_hdr.block_size;
        hdr.payload_size += next_hdr.payload_size;
        write_hdr(HDRP(blockp), hdr); /*extends size past the og. footer*/
        write_hdr(FTRP(blockp), hdr);
    }

    else if (!prev_hdr.alloc && next_hdr.alloc){    /*case 3*/
        hdr.block_size += prev_hdr.block_size;
        hdr.payload_size += prev_hdr.payload_size;
        write_hdr(FTRP(blockp), hdr); /*extends size of the prev. ptr*/
        write_hdr(HDRP(PREV_BLKP(blockp)), hdr);
        blockp = PREV_BLKP(blockp);
    }

    else { /*case 4*/
        hdr.block_size += prev_hdr.block_size + next_hdr.block_size;
        hdr.payload_size += prev_hdr.payload_size + next_hdr.payload_size;
        write_hdr(HDRP(PREV_BLKP(blockp)), hdr); /* update previous header ptr*/
        write_hdr(FTRP(NEXT_BLKP(blockp)), hdr); /* update new footer pointer*/
        blockp = PREV_BLKP(blockp);
    }
    return blockp;
}

