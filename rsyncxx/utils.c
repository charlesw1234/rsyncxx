#include <assert.h>
#include "rsyncxx/utils.h"

void
binlnshow(const char *prefix, FILE *wfp, unsigned off,
          const uint8_t *buf0, const uint8_t *buf1)
{
    const uint8_t *cur;
    char hexbuf[80], *hbcur = hexbuf, *hblast = hexbuf + sizeof(hexbuf);
    char chbuf[40], *cbcur = chbuf, *cblast = chbuf + sizeof(chbuf);
    assert(buf1 - buf0 <= BINLNWIDTH);
    hexbuf[0] = 0; chbuf[0] = 0;
    for (cur = buf0; cur < buf1; ++cur) {
        if (cur > buf0) {
            if ((cur - buf0) % 5 > 0) {
                hbcur += snprintf(hbcur, hblast - hbcur, " ");
            } else {
                hbcur += snprintf(hbcur, hblast - hbcur, "  ");
                cbcur += snprintf(cbcur, cblast - cbcur, " ");
            }
        }
        hbcur += snprintf(hbcur, hblast - hbcur, "%02X", *cur);
        cbcur += snprintf(cbcur, cblast - cbcur, "%c",
                          *cur >= 32 && *cur < 127 ? *cur: '.');
    }
    fprintf(wfp, "|%s|%4u|%4X|%-62s|%-23s|\n", prefix, off, off, hexbuf, chbuf);
}
void
binshow(const char *prefix, FILE *wfp, const uint8_t *buf0, const uint8_t *buf1)
{
    const uint8_t *cur, *cur1;
    for (cur = buf0; cur < buf1; cur += BINLNWIDTH) {
        cur1 = cur + BINLNWIDTH;
        if (buf1 < cur1) cur1 = buf1;
        binlnshow(prefix, wfp, cur - buf0, cur, cur1);
    }
}
