#ifndef __MJOJOPATCH_H
#define __MJOJOPATCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef EOF
#define MJP_EOF         (EOF)
#else
#define MJP_EOF         (-1)
#endif

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
    uint8_t buf[9];
    uint8_t cnt;
    uint8_t max;
    mjp_dt_t val;
} mjp_oft_t;

typedef struct {
    mjp_dt_t ofile_addr;
    mjp_dt_t pfile_addr;
    mjp_dt_t dfile_addr;
    mjp_oft_t oft;
    int last_dt;
    int cmd;
    int cnt;
    int sta;
} mjp_t;

int mjp_start(void);
int mjp_restore(int dt);
int mjp_done(void);

int mjp_parse(int dt);
int mjp_parse_done(void);

#endif // __MJOJOPATCH_H
