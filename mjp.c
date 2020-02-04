#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mjp.h"

#define BKT     MJP_BKT
#define EQL     MJP_EQL
#define DEL     MJP_DEL
#define INS     MJP_INS
#define MOD     MJP_MOD
#define ESC     MJP_ESC

static mjp_t mjp;

static bool mjp_is_cmd(int dt)
{
    switch (dt) {
    //case ESC:     // ESC is not command mode, is just for byte filling
    case MOD:
    case INS:
    case DEL:
    case EQL:
    case BKT:
        return true;
    default:
        return false;
    }
}

static mjp_dt_t mjp_get_oft(uint8_t *buf, int len)
{
    switch (len) {
    case 1:
        return buf[0] + 1;
    case 2:
        return buf[0] + buf[1] + 1;
    case 3:
        return ((uint32_t)buf[1] << 8) + ((uint32_t)buf[2] << 0);
    case 5:
        return ((uint32_t)buf[1] << 24) +
               ((uint32_t)buf[2] << 16) +
               ((uint32_t)buf[3] << 8) +
               ((uint32_t)buf[4] << 0);
    default:
        return -1;
    }
}

static mjp_dt_t mjp_parse_oft(mjp_oft_t *oft, int ch)
{
    /* already parsed */
    if ((oft->cnt != 0) && (oft->max == oft->cnt)) {
        return -1;
    }

    /* doesn't support */
    if ((oft->cnt >= 1) && (oft->buf[0] == 255)) {
        return -2;
    }

    oft->buf[oft->cnt++] = ch;

    if (oft->cnt == 1) {
        if (oft->buf[0] == 254) {
            oft->max = 5;
        } else if (oft->buf[0] == 253) {
            oft->max = 3;
        } else if (oft->buf[0] == 252) {
            oft->max = 2;
        } else {
            oft->max = 1;
        }
    }

    if (oft->max == oft->cnt) {
        return mjp_get_oft(oft->buf, oft->cnt);
    }
    return 0;
}

static void mjp_flush(void)
{
#ifdef MJP_WR_BUF_SIZE
    if (mjp.wr.cnt != 0) {
        if (mjp.des_wr_cb != NULL) {
            mjp.des_wr_cb(mjp.wr.addr + 1 - mjp.wr.cnt, mjp.wr.buf, mjp.wr.cnt);
        }
        mjp.wr.cnt = 0;
    }
#endif
}

static void mjp_write(mjp_dt_t addr, int dt)
{
#ifdef MJP_WR_BUF_SIZE
    /* set to buffer */
    mjp.wr.buf[mjp.wr.cnt++] = dt;
    mjp.wr.addr = addr;
    if (mjp.wr.cnt >= MJP_WR_BUF_SIZE) {
        mjp_flush();
    }
#else
    uint8_t buf[1];
    buf[0] = dt;
    mjp.des_wr_cb(addr, buf, 1);
#endif
}

static int mjp_read(mjp_dt_t addr)
{
    if (mjp.org_rd_cb != NULL) {
        return mjp.org_rd_cb(addr);
    }
    return 0;
}

int mjp_start(mjp_des_wr_t des_wr_cb, mjp_org_rd_t org_rd_cb, mjp_copy_t copy_cb)
{
    memset(&mjp, 0, sizeof(mjp_t));
    mjp.cmd = MJP_EOF;
    mjp.last_dt = MJP_EOF;
    mjp.des_wr_cb = des_wr_cb;
    mjp.org_rd_cb = org_rd_cb;
    mjp.copy_cb = copy_cb;
    return 0;
}

int mjp_apply_feed(int dt)
{
    mjp_dt_t val;
    switch (mjp.cmd) {
    case MOD:
        mjp_write(mjp.dfile_addr, dt);
        mjp.dfile_addr++;
        mjp.ofile_addr++;
        break;
    case INS:
        mjp_write(mjp.dfile_addr, dt);
        mjp.dfile_addr++;
        break;
    case DEL:
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_DEL_OFT;
        }
        if (val != 0) {
            mjp.ofile_addr += val;
            mjp.cmd = MJP_EOF;
        }
        break;
    case EQL:
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_EQL_OFT;
        }
        if (val != 0) {
            if (mjp.copy_cb != NULL) {
                mjp_flush();
                mjp.copy_cb(mjp.ofile_addr, mjp.dfile_addr, val);
                mjp.dfile_addr += val;
                mjp.ofile_addr += val;
            } else {
                int i;
                for (i = 0; i < val; i++) {
                    mjp_write(mjp.dfile_addr, mjp_read(mjp.ofile_addr));
                    mjp.dfile_addr++;
                    mjp.ofile_addr++;
                }
            }
            mjp.cmd = MJP_EOF;
        }
        break;
    case BKT:
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_BKT_OFT;
        }
        if (val != 0) {
            mjp.ofile_addr -= val;
            mjp.cmd = MJP_EOF;
        }
        break;
    default:
        return MJP_ERR_FORMAT;
    }
    mjp.cnt++;
    return 0;
}

int mjp_apply(int dt)
{
    int ret = 0;

    if ((mjp.cmd == BKT) || (mjp.cmd == EQL) || (mjp.cmd == DEL)) {
        ret = mjp_apply_feed(dt);
        mjp.last_dt = MJP_EOF;
    } else if (mjp.last_dt == ESC) {
        if (mjp_is_cmd(dt)) {
            mjp.cmd = dt;
            mjp.cnt = 0;
            mjp.oft.cnt = 0;
            mjp.oft.max = 0;
        } else {
            if (dt == ESC) {
                ret = mjp_apply_feed(dt);
            } else {
                ret = mjp_apply_feed(ESC);
                ret = mjp_apply_feed(dt);
            }
        }
        mjp.last_dt = MJP_EOF;
    } else {
        if (dt != ESC) {
            ret = mjp_apply_feed(dt);
        }
        mjp.last_dt = dt;
    }

    mjp.pfile_addr++;

    return ret;
}

int mjp_apply_done(void)
{
    mjp_flush();
    return mjp.dfile_addr;
}

/*-----------------------------------------------------------------------------*/
// mjp API with log
static const char *mjp_str_tab[6] = {
    "BKT",
    "EQL",
    "DEL",
    "INS",
    "MOD",
    "ESC",
};

static void mjp_log_cmd(int cmd, mjp_dt_t org, mjp_dt_t des, mjp_dt_t len)
{
    printf("\n");
    switch (mjp.cmd) {
    case MOD:
        printf("%s: Modify from %d(O) %d(D) for %d bytes",
               mjp_str_tab[cmd - BKT], org, des, len);
        break;
    case INS:
        printf("%s: Insert from %d(D) for %d bytes",
               mjp_str_tab[cmd - BKT], des, len);
        break;
    case DEL:
        printf("%s: Delete from %d(O) for %d bytes",
               mjp_str_tab[cmd - BKT], org, len);
        break;
    case EQL:
        printf("%s: Copy from %d(O) to %d(D) for %d bytes",
               mjp_str_tab[cmd - BKT], org, des, len);
        break;
    case BKT:
        printf("%s: Backtrace from %d(O) to %d(O), offset %d",
               mjp_str_tab[cmd - BKT], org, org - len, len);
        break;
    default:
        printf("ERROR");
        break;
    }
    printf("\n");
}

int mjp_parse_feed(int dt)
{
    mjp_dt_t val;
    switch (mjp.cmd) {
    case MOD:
        printf("%02X ", dt);
        mjp.dfile_addr++;
        mjp.ofile_addr++;
        break;
    case INS:
        printf("%02X ", dt);
        mjp.dfile_addr++;
        break;
    case DEL:
        printf("%02X ", dt);
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_DEL_OFT;
        }
        if (val != 0) {
            mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, val);
            mjp.ofile_addr += val;
            mjp.cmd = MJP_EOF;
        }
        break;
    case EQL:
        printf("%02X ", dt);
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_EQL_OFT;
        }
        if (val != 0) {
            mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, val);
            mjp.dfile_addr += val;
            mjp.ofile_addr += val;
            mjp.cmd = MJP_EOF;
        }
        break;
    case BKT:
        printf("%02X ", dt);
        val = mjp_parse_oft(&mjp.oft, dt);
        if (val < 0) {
            return MJP_ERR_BKT_OFT;
        }
        if (val != 0) {
            mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, val);
            mjp.ofile_addr -= val;
            mjp.cmd = MJP_EOF;
        }
        break;
    default:
        printf("ERROR\n");
        return MJP_ERR_FORMAT;
    }
    mjp.cnt++;
    return 0;
}

int mjp_parse(int dt)
{
    int ret = 0;

    if ((mjp.cmd == BKT) || (mjp.cmd == EQL) || (mjp.cmd == DEL)) {
        ret = mjp_parse_feed(dt);
        mjp.last_dt = MJP_EOF;
    } else if (mjp.last_dt == ESC) {
        if (mjp_is_cmd(dt)) {
            /* switch to new cmd mode */
            if ((mjp.cmd == MOD) || (mjp.cmd == INS)) {
                mjp_log_cmd(mjp.cmd, mjp.ofile_addr - mjp.cnt, mjp.dfile_addr - mjp.cnt, mjp.cnt);
            }
            mjp.cmd = dt;
            mjp.cnt = 0;
            mjp.oft.cnt = 0;
            mjp.oft.max = 0;
            printf("\n %6d | %s %02X %02X ",
               mjp.pfile_addr - 1,
               mjp_str_tab[dt - BKT],
               ESC,
               dt);
        } else {
            if (dt == ESC) {
                ret = mjp_parse_feed(dt);
                printf("(ESC)(ESC) ");
            } else {
                ret = mjp_parse_feed(ESC);
                printf("(ESC) ");
                ret = mjp_parse_feed(dt);
            }
        }
        mjp.last_dt = MJP_EOF;
    } else {
        if (dt != ESC) {
            ret = mjp_parse_feed(dt);
        }
        mjp.last_dt = dt;
    }

    mjp.pfile_addr++;

    return ret;
}

int mjp_parse_done(void)
{
    if ((mjp.cmd == MOD) || (mjp.cmd == INS)) {
        mjp_log_cmd(mjp.cmd, mjp.ofile_addr - mjp.cnt, mjp.dfile_addr - mjp.cnt, mjp.cnt);
    }
    printf("\nPatch file size: %d\n", mjp.pfile_addr);
    printf("Destination file size: %d\n", mjp.dfile_addr);
    printf("Original file size: %d (used)\n", mjp.ofile_addr);
    return 0;
}
