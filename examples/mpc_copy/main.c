#include <stdio.h>
#include "mpc.h"

int
main(int argc, char ** argv)
{
    fprintf(stdout, "Hello, World\n");

    mpc_parser_t* Adjective = mpc_new("adjective");
    mpc_cleanup(1, Adjective);

    fprintf(stdout, "Called an mpc function.\n");

    return 0;
}