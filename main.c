#include <stdio.h>
#include <stdlib.h>
#include "mjp.h"

int main(int argc, char **argv)
{
    int dt;
    FILE *pfile;

    pfile = fopen(argv[1], "rb");
    if (pfile == NULL) {
        printf("can't open patch file\n");
        return -1;
    }

    /* parse */
    mjp_start();
    while ((dt = getc(pfile)) != EOF) {
        mjp_parse(dt);
    }
    mjp_parse_done();

    /* TODO: set call back */
    /* apply patch and restore */
    mjp_start();
    while ((dt = getc(pfile)) != EOF) {
        mjp_apply(dt);
    }
    mjp_apply_done();

    fclose(pfile);

    return 0;
}
