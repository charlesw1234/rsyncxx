#include "rsyncxx/sums.hpp"

#define DO1(buf, idx)  { s1 += buf[idx]; s2 += s1; }
#define DO2(buf, idx)  DO1(buf, idx); DO1(buf, idx + 1);
#define DO4(buf, idx)  DO2(buf, idx); DO2(buf, idx + 2);
#define DO8(buf, idx)  DO4(buf, idx); DO4(buf, idx + 4);
#define DO16(buf)      DO8(buf, 0); DO8(buf, 8);
#define OF16(off)      { s1 += 16 * off; s2 += 136 * off; }

namespace rsync {
    void
    rollsum_t::update(const uint8_t *buf, size_t szbuf)
    {
        count += szbuf;
        while (szbuf >= 16) {
            DO16(buf);
            OF16(ROLLSUM_CHAR_OFFSET);
            buf += 16;
            szbuf -= 16;
        }
        while (szbuf > 0) {
            s1 += (*buf++ + ROLLSUM_CHAR_OFFSET);
            s2 += s1;
            --szbuf;
        }
    }

    bool
    rolledbuf_t::commit(const uint8_t *data, ptrdiff_t szdata)
    {
        ptrdiff_t nbytes;
        const uint8_t *datacur = data, *datalast = data + szdata;
        _rollsum.update(data, szdata);
        while (!_full && datacur < datalast) {
            if (buf0 < buf1) nbytes = buflast - buf1;
            else if (buf0 > buf1) nbytes = buf0 - buf1;
            else { nbytes = buflast - bufstart; buf0 = buf1 = bufstart; }
            nbytes = std::min(datalast - datacur, nbytes);
            memcpy(buf1, datacur, nbytes);
            buf1 += nbytes; datacur += nbytes;
            if (buf1 == buflast) buf1 = bufstart;
            if (buf0 == buf1) _full = true;
        }
        assert(datacur == datalast);
        return _full;
    }
    uint8_t
    rolledbuf_t::rotate(uint8_t data)
    {
        uint8_t retdata;
        assert(_full);
        assert(buf0 == buf1);
        retdata = *buf0;
        _rollsum.rotate(*buf0, data);
        *buf0++ = data;
        if (buf0 == buflast) buf0 = bufstart;
        buf1 = buf0;
        return retdata;
    }
    void
    rolledbuf_t::strong(strong_sum_t &sum) const
    {
        strong_signer_t signer;
        signer.init(sizeof(sum));
        if (buf0 < buf1) signer.update(buf0, buf1);
        else if (buf0 > buf1 || _full) {
            signer.update(buf0, buflast); signer.update(bufstart, buf1);
        }
        signer.final(sum, sizeof(sum));
    }
    void
    rolledbuf_t::_rollback(void)
    {
        ptrdiff_t sz0 = buflast - buf0;
        ptrdiff_t sz1 = buf1 - bufstart;
        void *tmp = malloc(sz1);
        memcpy(tmp, bufstart, sz1);
        memmove(bufstart, buf0, sz0);
        memcpy(bufstart + sz0, tmp, sz1);
        buf0 = bufstart;
        buf1 = bufstart + sz0 + sz1;
        free(tmp);
    }
}
