#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count)
{
	int cnt = 0;
	for (int i = 0; i < NUM_FREE_LISTS; i++)
	{
		sf_block *bp = sf_free_list_heads[i].body.links.next;
		while (bp != &sf_free_list_heads[i])
		{
			if (size == 0 || size == ((bp->header ^ sf_magic()) & ~0xffffffff0000000f))
				cnt++;
			bp = bp->body.links.next;
		}
	}
	if (size == 0)
	{
		cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
								 count, cnt);
	}
	else
	{
		cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
								 size, count, cnt);
	}
}

/*
 * Assert the total number of quick list blocks of a specified size.
 * If size == 0, then assert the total number of all quick list blocks.
 */
void assert_quick_list_block_count(size_t size, int count)
{
	int cnt = 0;
	for (int i = 0; i < NUM_QUICK_LISTS; i++)
	{
		sf_block *bp = sf_quick_lists[i].first;
		while (bp != NULL)
		{
			if (size == 0 || size == ((bp->header ^ sf_magic()) & ~0xffffffff0000000f))
				cnt++;
			bp = bp->body.links.next;
		}
	}
	if (size == 0)
	{
		cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
								 count, cnt);
	}
	else
	{
		cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
								 size, count, cnt);
	}
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);
	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;

	// We want to allocate up to exactly four pages, so there has to be space
	// for the header and the link pointers.
	void *x = sf_malloc(16316);
	cr_assert_not_null(x, "x is NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;
	void *x = sf_malloc(151505);

	cr_assert_null(x, "x is not NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(151504, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_quick, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 32, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/** [8 bits][hdr1 8byts][8 byts + 8 byts space][ftr 8 byts][hdr 8bytes][32 bytes][footer 8 bytes][header 8 bytes][16 bytes][footer 8 bytes][] */

	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(48, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(3936, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(224, 1);
	assert_free_block_count(3760, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);
	sf_free(y);

	sf_free(x);
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(544, 1);
	assert_free_block_count(3440, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT)
{
	size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200, sz_z = 700;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 4);
	assert_free_block_count(224, 3);
	assert_free_block_count(1808, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 8,
							 "Wrong first block in free list %d: (found=%p, exp=%p)",
							 i, bp, (char *)y - 8);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT)
{
	size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert((bp->header ^ sf_magic()) & 0x1, "Allocated bit is not set!");
	cr_assert(((bp->header ^ sf_magic()) & ~0xffffffff0000000f) == 96,
						"Realloc'ed block size (%ld) not what was expected (%ld)!",
						(bp->header ^ sf_magic()) & ~0xffffffff0000000f, 96);

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(3888, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT)
{
	size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert((bp->header ^ sf_magic()) & 0x1, "Allocated bit is not set!");
	cr_assert(((bp->header ^ sf_magic()) & ~0xffffffff0000000f) == 96,
						"Realloc'ed block size (%ld) not what was expected (%ld)!",
						(bp->header ^ sf_magic()) & ~0xffffffff0000000f, 96);

	// There should be only one free block.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(3952, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT)
{
	size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)x - 8);
	cr_assert((bp->header ^ sf_magic()) & 0x1, "Allocated bit is not set!");
	cr_assert(((bp->header ^ sf_magic()) & ~0xffffffff0000000f) == 32,
						"Realloc'ed block size (%ld) not what was expected (%ld)!",
						(bp->header ^ sf_magic()) & ~0xffffffff0000000f, 32);

	// After realloc'ing x, we can return a block of size ADJUSTED_BLOCK_SIZE(sz_x) - ADJUSTED_BLOCK_SIZE(sz_y)
	// to the freelist.  This block will go into the main freelist and be coalesced.
	// Note that we don't put split blocks into the quick lists because their sizes are not sizes
	// that were requested by the client, so they are not very likely to satisfy a new request.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(4016, 1);
}

// ############################################
// STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
// DO NOT DELETE OR MANGLE THESE COMMENTS
// ############################################

Test(sfmm_student_suite, test_fragmentation, .timeout = TEST_TIMEOUT)
{
	sf_malloc(16);
	double fragmentation = sf_fragmentation();
	cr_assert_eq(sf_fragmentation(), 0.5, "Error: expected fragmentation %lf got %lf.", 0.5, fragmentation);
}

Test(sfmm_student_suite, test_basic, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(16); // size32
	void *y = sf_malloc(16);
	/*void *z = */ sf_malloc(16); // [prologue: 40 bytes][x.hdr + x: 32 bytes][y.hdr + y: 32 bytes][y.hdr + y: 32 bytes][free: 3952]

	assert_free_block_count(3952, 1);
	/* free up a 64 byte block counting overhead*/
	sf_free(x);
	sf_free(y);

	cr_assert_eq((void *)(sf_quick_lists[0].first) + 8, y); /*check that start of the free list holds address of x */
	x = malloc(64);																					/*malloc*/

	assert_quick_list_block_count(32, 2);
	assert_quick_list_block_count(64, 0);
	assert_free_block_count(64, 0);
	assert_free_block_count(32, 0);
}
/*test coalescing behavior*/
Test(sfmm_student_suite, test_coalesce, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(208); /*too big for QL*/
	void *y = sf_malloc(208);
	/*void *z = */ sf_malloc(224);
	sf_free(x);
	sf_free(y);

	assert_free_block_count(448, 1);
	assert_free_block_count(224, 0);
}

/*test sf utilization*/
Test(sfmm_student_suite, test_peak_utilization, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(208); /*too big for QL*/
	void *y = sf_malloc(208);
	void *z = sf_malloc(224);
	sf_free(x);
	sf_free(y);

	assert_free_block_count(448, 1);
	assert_free_block_count(224, 0);
	/*test payload*/

	cr_assert_eq(sf_utilization(), (208.0 + 208.0 + 224.0) / 4096.0);

	void *v = sf_malloc(4096); // allocate a page, 8192

	sf_free(z);
	cr_assert_eq(sf_utilization(), (224.0 + 4096.0) / 8192.0);

	sf_realloc(v, 2000);

	assert_quick_list_block_count(224, 0);
}

/*Test large amounts of frees */
Test(sfmm_student_suite, test_sf_flush, .timeout = TEST_TIMEOUT)
{
	void *a = sf_malloc(16);
	void *b = sf_malloc(16);
	void *c = sf_malloc(16);

	void *d = sf_malloc(16);
	void *e = sf_malloc(16);
	void *f = sf_malloc(16);
	sf_free(a);
	sf_free(b);
	sf_free(c);
	sf_free(d);
	sf_free(e);

	assert_quick_list_block_count(32, 5); /*full capacity*/
	sf_free(f);														/* flush*/
	assert_quick_list_block_count(32, 1);
}
/*test min. split boundary*/
Test(sfmm_student_suite, split_test, .timeout = TEST_TIMEOUT)
{
	void *x = sf_malloc(4032); /*s*/
	sf_free(x);
	assert_free_block_count(4048, 1);
	sf_malloc(4032 - 32); /*allocate with 32 bytes remaining*/
	assert_free_block_count(4016, 0);
	assert_free_block_count(32, 1); /*shouldn't go in quick list*/
}
/* LIFO test*/
Test(sfmm_student_suite, quick_list_lifo_order, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;

	void *a = sf_malloc(1); // adjusted size 32
	void *b = sf_malloc(1); // adjusted size 32
	cr_assert_not_null(a);
	cr_assert_not_null(b);

	sf_free(a);
	sf_free(b);

	// Quick list should be LIFO: b then a
	void *x = sf_malloc(1);
	void *y = sf_malloc(1);

	cr_assert_eq(x, b, "Expected first quick-list reuse to return most recently freed block");
	cr_assert_eq(y, a, "Expected second quick-list reuse to return older freed block");
}

Test(sfmm_student_suite, quick_list_flush_on_sixth_insert, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;

	void *q[6];
	void *guard[6];

	// Separate quick-list candidates so flushed blocks cannot coalesce together.
	for (int i = 0; i < 6; i++)
	{
		q[i] = sf_malloc(1);			 // size class 32
		guard[i] = sf_malloc(200); // non-quick allocated spacer
		cr_assert_not_null(q[i]);
		cr_assert_not_null(guard[i]);
	}

	for (int i = 0; i < 6; i++)
	{
		sf_free(q[i]);
	}

	// With QUICK_LIST_MAX = 5:
	// freeing the 6th block flushes prior 5 to main free list, then inserts newest in quick list.
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(32, 5);
}

Test(sfmm_student_suite, realloc_splinter_does_not_split, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;

	void *x = sf_malloc(40); // adjusted size should be 64
	void *guard = sf_malloc(200);
	cr_assert_not_null(x);
	cr_assert_not_null(guard);

	void *y = sf_realloc(x, 24); // adjusted size 48, splinter 16 -> should NOT split
	cr_assert_not_null(y);
	cr_assert_eq(y, x, "Realloc shrinking with splinter should keep same payload pointer");

	sf_block *bp = (sf_block *)((char *)y - 8);
	size_t bsize = (bp->header ^ sf_magic()) & ~0xffffffff0000000f;
	cr_assert_eq(bsize, 64, "Block should remain 64 bytes when remainder would be splinter");
}

Test(sfmm_student_suite, realloc_exact_min_split_creates_free_32, .timeout = TEST_TIMEOUT)
{
	sf_errno = 0;

	void *x = sf_malloc(40); // adjusted size 64
	void *guard = sf_malloc(200);
	cr_assert_not_null(x);
	cr_assert_not_null(guard);

	void *y = sf_realloc(x, 8); // adjusted size 32, remainder exactly 32 -> split expected
	cr_assert_not_null(y);
	cr_assert_eq(y, x, "Realloc shrink should keep same payload pointer");

	sf_block *bp = (sf_block *)((char *)y - 8);
	size_t bsize = (bp->header ^ sf_magic()) & ~0xffffffff0000000f;
	cr_assert_eq(bsize, 32, "Realloc should shrink block to 32 when exact minimum split is possible");

	assert_free_block_count(32, 1);
}

Test(sfmm_student_suite, free_invalid_pointer_aborts, .signal = SIGABRT)
{
	void *x = sf_malloc(8);
	cr_assert_not_null(x);

	// Deliberately invalid (unaligned / not block payload start)
	sf_free((char *)x + 1);
}

Test(sfmm_student_suite, fragmentation_single_alloc, .timeout = TEST_TIMEOUT)
{
	void *p = sf_malloc(8); // block size 32
	cr_assert_not_null(p);
	cr_assert_eq(sf_fragmentation(), (8.0) / (32.0));
}

Test(sfmm_student_suite, fragmentation_multiple_allocs, .timeout = TEST_TIMEOUT)
{
	void *a = sf_malloc(8);		// bsize 32
	void *b = sf_malloc(200); // 224 block size, after rounding up
	void *c = sf_malloc(1);

	cr_assert_not_null(a);
	cr_assert_not_null(b);
	cr_assert_not_null(c);

	debug("%lf %lf", sf_fragmentation(), (8.0 + 200.0 + 1.0) / (32.0 + 224.0 + 32.0));
	cr_assert_eq(sf_fragmentation(), (8.0 + 200.0 + 1.0) / (32.0 + 224.0 + 32.0));
}
/*test whether the fragmentation fucntion ignores quicklist blocks*/
Test(sfmm_student_suite, fragmentation_ignores_freed_ql_blocks, .timeout = TEST_TIMEOUT)
{
	void *a = sf_malloc(8);		// blocksize 32
	void *b = sf_malloc(200); // blocksize 224
	void *c = sf_malloc(1);		// blocksize 32
	sf_free(c);//goes to ql
	
	cr_assert_not_null(a);
	cr_assert_not_null(b);
	cr_assert_not_null(c);
	cr_assert_eq(sf_fragmentation(), 208.0 / 256.0);
}