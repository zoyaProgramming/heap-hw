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
 * @brief remove a specific block from its free list
 * @note post: links to the current node in the previous and next node have been removed and current block links are NULL
 */
sf_block *free_list_remove(sf_block *block);

/**
 * @brief append block to the start of the free list
 *
 * @param block An sf_block with an updated, accurate header.
 * @note postcondition: the links in the free-list have been updated so block is the next after the head
 */
void free_list_push(sf_block *block);

#endif