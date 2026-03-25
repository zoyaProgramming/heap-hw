#include <stdio.h>
#include "sfmm.h"
#include "helpers.h"
#include "debug.h"

int main(int argc, char const *argv[])
{
    debug("SIZEOF: %lu", sizeof(sf_block *));
    sf_set_magic(0);
    double *ptr = (double *)sf_malloc(sizeof(double));

    *ptr = 320320320e-320;
    //*ptr = 16;
    sf_show_heap();
    debug("%lf", *ptr);
    // double *ptr2 = sf_malloc(sizeof(double));
    //  *ptr2 = 312;

    return EXIT_SUCCESS;
}
