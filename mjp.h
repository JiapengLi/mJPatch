#ifndef __MJOJOPATCH_H
#define __MJOJOPATCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef EOF
#define MJP_EOF         (EOF)
#else
#define MJP_EOF         (-1)
#endif

//#define MJP_WR_BUF_SIZE         (32)

enum {
    MJP_ERR_FORMAT      = -1,
    MJP_ERR_BKT_OFT     = -2,
    MJP_ERR_EQL_OFT     = -3,
    MJP_ERR_DEL_OFT     = -4,
};

enum {
    MJP_BKT = 0xA2,
    MJP_EQL = 0xA3,
    MJP_DEL = 0xA4,
    MJP_INS = 0xA5,
    MJP_MOD = 0xA6,
    MJP_ESC = 0xA7,
};

typedef int mjp_dt_t;

typedef struct {
    uint8_t buf[5];
    uint8_t cnt;
    uint8_t max;
    uint8_t rfu;
} mjp_oft_t;

/* call back function: */
typedef int (*mjp_des_wr_t)(int addr, uint8_t *buf, int len);
typedef int (*mjp_org_rd_t)(int addr);
typedef int (*mjp_copy_t)(int des, int src, int len);

typedef struct {
    mjp_dt_t ofile_addr;
    mjp_dt_t pfile_addr;
    mjp_dt_t dfile_addr;
    mjp_dt_t cnt;

    int16_t last_dt;
    int16_t cmd;

    mjp_des_wr_t des_wr_cb;
    mjp_org_rd_t org_rd_cb;
    mjp_copy_t copy_cb;

    mjp_oft_t oft;

#ifdef MJP_WR_BUF_SIZE
    struct {
        mjp_dt_t addr;
        int cnt;
        uint8_t buf[MJP_WR_BUF_SIZE];
    } wr;
#endif
} mjp_t;

/* start and reset mjp global variables */
int mjp_start(mjp_des_wr_t des_wr_cb, mjp_org_rd_t org_rd_cb, mjp_copy_t copy_cb);

/* apply patch file to old file and generate new file */
int mjp_apply(int dt);
int mjp_apply_done(void);

/* parse and log the patch file */
int mjp_parse(int dt);
int mjp_parse_done(void);

#endif // __MJOJOPATCH_H
