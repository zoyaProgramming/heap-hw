#include "listhelpers.h"
#include "sfmm.h"
#include "helpers.h"
sf_block *ql_pop(int idx)
{
  // sf_quick_lists
  int len = sf_quick_lists[idx].length;
  if (len == 0)
    return NULL;
  else if (len == 1)
  {
    struct sf_block *first = sf_quick_lists[idx].first;
    first->body.links.next = NULL;
    sf_quick_lists[idx].length = 0;
    sf_quick_lists[idx].first = NULL;
    return first;
  }
  // len >= 2, must set the
  struct sf_block *first = sf_quick_lists[idx].first;
  struct sf_block *next = first->body.links.next;
  sf_quick_lists[idx].first = next;
  sf_quick_lists[idx].length -= 1;
  return first;
}
int ql_flush(int qlist_idx)
{
  while (sf_quick_lists[qlist_idx].length > 0)
  {
    sf_block *block = ql_pop(qlist_idx);
    block = coalesce(block);
    header_t hdr = parse_header(block->header);
    hdr.alloc = 0;
    hdr.in_qklst = 0;
    write_hdr(block, hdr); /*set new updated hdr*/
    free_list_push(block); /*insert block into front of free list*/
  }
  return 1;
}
void *ql_push(sf_block *b)
{
  header_t header = parse_header(b->header);
  int ql_idx = (header.block_size - 32) / 16;
  if (sf_quick_lists[ql_idx].length == MAX_QLIST_SIZE)
    ql_flush(ql_idx);

  b->body.links.next = sf_quick_lists[ql_idx].first; /*update links*/
  sf_quick_lists[ql_idx].first = b;
  sf_quick_lists[ql_idx].length += 1;
  return b;
}
sf_block *free_list_remove(sf_block *block)
{
  sf_block *prev = block->body.links.prev;
  sf_block *next = block->body.links.next;
  header_t head = parse_header(prev->header);
  prev->body.links.next = next;
  next->body.links.prev = prev;
  block->body.links.next = NULL;
  block->body.links.prev = NULL;
  return block;
}
void free_list_push(sf_block *block)
{

  /** compute the sentinel node based on the block size */
  header_t hdr = parse_header(block->header);
  int idx = size_class(hdr.block_size);
  sf_block *sentinel = sf_free_list_heads + idx;
  sf_block *old_first = sentinel->body.links.next;
  int was_empty = (old_first == sentinel);
  block->body.links.prev = sentinel;
  block->body.links.next = old_first;
  sentinel->body.links.next = block;
  old_first->body.links.prev = block;
}