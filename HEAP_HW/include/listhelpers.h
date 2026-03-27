/**
 * @zoyaProgramming
 * Helper functions to work with quick lists and main free lists
 */
#ifndef LISTHELPERS_H
#define LISTHELPERS_H
#include "sfmm.h"
#include "debug.h"
#include "helpers.h"

sf_block *fetch_from_ql(size_t size);

/**
 * @brief Helpers related to quicklists*/

int ql_flush(int qlist_idx);
sf_block *ql_pop(int idx);
void *ql_push(sf_block *b);

/**
 * @brief helpers related to main free lists
 * @def free_list_remove: remove a specific block from its free list
 * @def free_list_push: push a block to the start of the idx free list
 */
sf_block *free_list_remove(sf_block *block);
void free_list_push(sf_block *block);

#endif