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

static const char *mjp_str_tab[6] = {
    "BKT",
    "EQL",
    "DEL",
    "INS",
    "MOD",
    "ESC",
};

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
    if (oft->val != 0) {
        return -1;
    }
    if (oft->buf[0] == 255) {
        return -2;
    }
    oft->buf[oft->cnt++] = ch;
    if (oft->cnt == 1) {
        if (oft->buf[0] == 255) {
            oft->max = 9;
        } else if (oft->buf[0] == 254) {
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
        oft->val = mjp_get_oft(oft->buf, oft->cnt);
        return oft->val;
    }
    return 0;
}

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

int mjp_start(void)
{
    memset(&mjp, 0, sizeof(mjp_t));
    mjp.cmd = MJP_EOF;
    mjp.last_dt = MJP_EOF;
    return 0;
}

int mjp_parse(int dt)
{
    if ((mjp.last_dt == ESC) && (mjp_is_cmd(dt))) {
        /* switch to new cmd mode */
        if ((mjp.cmd == MOD) ||(mjp.cmd == INS)) {
            mjp_log_cmd(mjp.cmd, mjp.ofile_addr - mjp.cnt, mjp.dfile_addr - mjp.cnt, mjp.cnt);
        }
        mjp.cmd = dt;
        mjp.last_dt = MJP_EOF;
        mjp.cnt = 0;
        mjp.oft.cnt = 0;
        mjp.oft.max = 0;
        mjp.oft.val = 0;
        printf("\n %6d | %s %02X %02X ",
               mjp.pfile_addr - 1,
               mjp_str_tab[dt - BKT],
               ESC,
               dt);
    } else {
        if ((dt != ESC) || (mjp.last_dt == ESC)) {
            mjp.cnt++;
            switch (mjp.cmd) {
            case MJP_EOF:
                printf("UNKNONWN PATCH FILE\n");
                return -1;
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
                if (mjp_parse_oft(&mjp.oft, dt) < 0) {
                    printf("UNSUPPORTED DEL LENGTH\n");
                }
                if (mjp.oft.val != 0) {
                    mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, mjp.oft.val);
//                    printf("(DEL ORGADDR %d, %dB)", mjp.ofile_addr, mjp.oft.val);
                    mjp.ofile_addr += mjp.oft.val;
                }
                break;
            case EQL:
                printf("%02X ", dt);
                if (mjp_parse_oft(&mjp.oft, dt) < 0) {
                    printf("UNSUPPORTED EQL LENGTH\n");
                }
                if (mjp.oft.val != 0) {
                    mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, mjp.oft.val);
                    mjp.dfile_addr += mjp.oft.val;
                    mjp.ofile_addr += mjp.oft.val;
                }
                break;
            case BKT:
                printf("%02X ", dt);
                if (mjp_parse_oft(&mjp.oft, dt) < 0) {
                    printf("UNSUPPORTED BKT LENGTH\n");
                }
                if (mjp.oft.val != 0) {
                    mjp_log_cmd(mjp.cmd, mjp.ofile_addr, mjp.dfile_addr, mjp.oft.val);
                    mjp.ofile_addr -= mjp.oft.val;
                }
                break;
            default:
                printf("ERROR\n");
                return -1;
            }
            /* set ch to EOF after ch is processed */
            dt = MJP_EOF;
        }
        mjp.last_dt = dt;
    }
    mjp.pfile_addr++;
    return 0;
}

int mjp_parse_done(void)
{
    printf("\nPatch file size: %d\n", mjp.pfile_addr);
    printf("Destination file size: %d\n", mjp.dfile_addr);
    printf("Original file size: %d (used)\n", mjp.ofile_addr);
    return 0;
}



