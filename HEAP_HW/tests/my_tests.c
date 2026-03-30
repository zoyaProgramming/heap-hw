/**
 * In the tests/sfmm_tests.c file,
 *  there are ten unit test examples. These tests check for the correctness of sf_malloc, sf_realloc, and sf_free.
 *  We provide some basic assertions, but by no means are they exhaustive.
 * It is your job to ensure that your header/footer bits are set correctly and that blocks are allocated/freed as specified.
 */

#include <criterion/criterion.h>
#include <limits.h>
#include "helpers.h"
#include "stdio.h"

// Function to print binary representation of an unsigned integer
Test(custom, bitwrite)
{
    uint64_t byte = 0x0UL;
    uint64_t *p = &byte;
    PUT_ALLOC(p, 1);

    fflush(stdout);
    cr_assert_eq(byte, 1, "error");

    print_binary(byte);
    PUT_QLIST(p, 1);
    print_binary(byte);
    // cr_assert_eq(byte, 3, "q list not set properly");

    PUT_BSIZE(p, 1232);
    print_binary(byte);
    cr_assert_eq((int)(GET_BSIZE(p)), 1232, "p %d ", GET_BSIZE(p));

    PUT_BSIZE(p, (0U - 1U));
    cr_assert_eq((uint64_t)(GET_BSIZE(p)), (0U - 1U) & ((0LL - 1) << 4), "got: %lu expected : %lu", GET_BSIZE(p), (0U - 1U) & ((0LL - 1) << 4));

    print_binary(byte);
    PUT_BSIZE(p, 0);

    print_binary(byte);
    PUT_PSIZE(p, 0xFFFFFFFF);
    print_binary(byte);

    PUT_ALLOC(p, 0);
    print_binary(byte);
}
