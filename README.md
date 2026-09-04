# **sfmm — Segregated Free-List Memory Allocator**
A custom malloc / realloc / free implementation in C for the x86-64 architecture, written for CSE 320 (Dynamic Memory Allocation). Full assignment spec is in ASSIGNMENT.md.

## **Build**
`make`        # builds bin/sfmm and bin/sfmm_tests  
`make debug`  # adds -DDEBUG, colored logging, and print statements  
`make clean`
## **Design**
Segregated free lists: 12 circular, doubly linked free lists (sf_free_list_heads), bucketed by power-of-two size class starting at the 32-byte minimum block size. Each list uses a dummy header node to avoid edge cases on insert/remove.
Quick lists: 12 singly linked LIFO lists (sf_quick_lists) that cache recently freed small blocks (capped at QUICK_LIST_MAX per list) to avoid searching/splitting on the common case. Lists are flushed to the main free lists once they overflow.
Boundary tags: every block has a header and footer so adjacent blocks can be coalesced in O(1). Free blocks are coalesced immediately, while small blocks freed into quick lists are coalesced on flush.
Header/footer obfuscation:  Prevents corruption. Header and footer words are XOR'ed with a runtime magic number (sf_magic()) so a corrupted or forged pointer can't trivially be freed. validptr() checks alignment, size, and heap bounds before trusting a pointer.
Alignment: payloads are aligned to 16-byte (two-row) boundaries, and blocks are only split when the remainder is large enough to avoid splinters.
Prologue and epilogue blocks bound the heap so that we never need to consider the ends while coalescing.
## **Layout**
`include/sfmm.h`       Provided API and block/heap layout (do not modify)  
`include/helpers.h`      Header packing/parsing, block macros, header_t type  
`include/listhelpers.h`  Free list / quick list operations  
`src/sfmm.c`             sf_malloc, sf_free, sf_realloc, sf_fragmentation, sf_utilization  
`src/helpers.c`          find_fit, place, split, coalesce, mem_init, validptr  
`src/listhelpers.c`       Free list and quick list push/pop/flush  
`tests/sfmm_tests.c`      Provided Criterion tests  
`tests/my_tests.c`        Scratch tests for header bit-packing macros
## **API**
| Function |Purpose|
| ---|---|
| sf_malloc(size) |Allocate size bytes, returns 16-byte-aligned payload pointer |
| sf_realloc(ptr, size) |	Resize a block in place when possible, else malloc+copy+free|
| sf_free(ptr)	| Free a block; abort()s on an invalid pointer |
| sf_fragmentation() |	Ratio of total payload to total allocated block size |
| sf_utilization() |	Ratio of peak aggregate payload to current heap size |
|sf_show_heap(), sf_show_blocks(), sf_show_free_lists(), and sf_show_quick_lists()| (in helpers.c) print the heap state for debugging.|

## **Run**
`./bin/sfmm`         # sample driver in src/main.c  
`./bin/sfmm_tests`   # Criterion test suite
