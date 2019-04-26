#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mjp.h"

FILE *ofile;
FILE *pfile;
FILE *dfile;



int dfile_wr(int addr, uint8_t *buf, int len)
{
    fwrite(buf, 1, len, dfile);
    return 0;
}

int ofile_rd(int addr)
{
    uint8_t buf[1];
    fseek(ofile, addr, SEEK_SET);
    fread(buf, 1, 1, ofile);
    return buf[0];
}

int o2d_copy(int desaddr, int srcaddr, int len)
{
    uint8_t *buf;
    buf = malloc(len + 1);
    //memset(buf, 0, len + 1);
    fseek(ofile, srcaddr, SEEK_SET);
    fread(buf, 1, len, ofile);
    fwrite(buf, 1, len, dfile);
    return 0;
}

typedef int (*mjp_copy_t)(int des, int src, int len);

int main(int argc, char **argv)
{
    int dt;

    ofile = fopen(argv[1], "rb");
    if (ofile == NULL) {
        printf("can't open original file\n");
        return -1;
    }

    pfile = fopen(argv[2], "rb");
    if (pfile == NULL) {
        printf("can't open patch file\n");
        return -1;
    }

    dfile = fopen(argv[3], "wb");
    if (dfile == NULL) {
        printf("can't open patch file\n");
        return -1;
    }

    printf("ftell %ld, %ld, %ld\n", ftell(ofile), ftell(pfile), ftell(dfile));

    printf("mjp_t size %d, (mjp_dt_t %d)", sizeof(mjp_t), sizeof(mjp_dt_t));

    /* parse */
    mjp_start(dfile_wr, ofile_rd, o2d_copy);
    while ((dt = getc(pfile)) != EOF) {
        mjp_parse(dt);
    }
    mjp_parse_done();

    /* apply patch and restore */
    fseek(pfile, 0, SEEK_SET);
    mjp_start(dfile_wr, ofile_rd, o2d_copy);    // set call back
    while ((dt = getc(pfile)) != EOF) {
        mjp_apply(dt);
    }
    mjp_apply_done();

    fclose(ofile);
    fclose(pfile);
    fclose(dfile);

    return 0;
}
