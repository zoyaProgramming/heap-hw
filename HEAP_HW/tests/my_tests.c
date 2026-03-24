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
void print_binary(uint64_t num) {
    int bits = sizeof(num) * CHAR_BIT; // Total bits in unsigned int
    int leading_zero = 0; // Flag to skip leading zeros

    for (int i = bits - 1; i >= 0; i--) {
        uint64_t mask = 1ul << i;
        if (num & mask) {
            putchar('1');
            leading_zero = 0;
        } else if (!leading_zero) {
            putchar('0');
        }
    }

    // If the number is zero, print '0'
    if (leading_zero) {
        putchar('0');
    }
    printf("\n");
    fflush(stdout);
}
Test(custom, bitwrite){
    uint64_t byte = 0x0UL;
    uint64_t * p = &byte;
    PUT_ALLOC(p, 1);

    fflush(stdout);
    cr_assert_eq(byte, 1, "error");


    print_binary(byte);
    PUT_QLIST(p, 1);
    print_binary(byte);
    //cr_assert_eq(byte, 3, "q list not set properly");
    


    printf("sdiufjoafdoifds\n");
    PUT_BSIZE(p, 1);
    
    print_binary(byte);
    PUT_PSIZE(p, 0xFFFFFFFF);
    print_binary(byte);

    PUT_ALLOC(p, 0);
    print_binary(byte);


}
