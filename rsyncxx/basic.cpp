#include <algorithm>
#include "rsyncxx/basic.hpp"

namespace rsync {
    // infile_t.
    buf_t *
    infile_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t szmax = bobj->buflast - bobj->buf1;
        ptrdiff_t sz = (ptrdiff_t)fread(bobj->buf1, 1, szmax, _rfp);
        bobj->buf1 += sz;
        bobj = node_t::step(done, bobj, sz < szmax && lastbuf, joint);
        done = sz < szmax;
        return bobj;
    }

    // outfile_t.
    buf_t *
    outfile_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t szmax = bobj->buf1 - bobj->buf0;
        ptrdiff_t sz = (ptrdiff_t)fwrite(bobj->buf0, 1, szmax, _wfp);
        bobj->buf0 += sz;
#if 0
        fprintf(stderr, "%s: %u/%u, %s\n", __FUNCTION__,
                (unsigned)sz, (unsigned)szmax, bool2str(lastbuf));
#endif
        return node_t::step(done, bobj, lastbuf, joint);
    }

    // gzinfile_t.
    buf_t *
    gzinfile_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t szmax = bobj->buflast - bobj->buf1;
        int sz = gzread(_rfp, bobj->buf1, szmax);
        bobj->buf1 += sz;
        return node_t::step(done, bobj, sz < szmax && lastbuf, joint);
    }

    // gzoutfile_t.
    buf_t *
    gzoutfile_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t szmax = bobj->buf1 - bobj->buf0;
        int sz = gzwrite(_wfp, bobj->buf0, szmax);
        bobj->buf0 += sz;
        bobj = node_t::step(done, bobj, lastbuf, joint);
        return bobj;
    }

    // compresser_t.
    compresser_t::compresser_t(buf_t *bobj, int level, node_t *next): node_t(next)
    {
        _bufsz = bobj->buflast - bobj->bufstart;
        _bobj = bobj;
        memset(&_zs, 0, sizeof(_zs));
        deflateInit(&_zs, level);
    }
    compresser_t::compresser_t(ptrdiff_t bufsz, int level, node_t *next): node_t(next)
    {
        _bufsz = bufsz;
        _bobj = NULL;
        memset(&_zs, 0, sizeof(_zs));
        deflateInit(&_zs, level);
    }
    buf_t *
    compresser_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        int rc = Z_OK;
#if 0
        fprintf(stderr, "bobj: %u, lastbuf: %s, rc = %d\n",
                (unsigned)(bobj->buf1 - bobj->buf0), bool2str(lastbuf), rc);
#endif
        while (bobj->buf0 < bobj->buf1 || (lastbuf && rc == Z_OK)) {
            if (_bobj == NULL) _bobj = reuse(_bobj, _bufsz);
            _zs.next_in = bobj->buf0;
            _zs.avail_in = bobj->buf1 - bobj->buf0;
            _zs.next_out = _bobj->buf1;
            _zs.avail_out = _bobj->buflast - _bobj->buf1;
            rc = deflate(&_zs, lastbuf ? Z_FINISH: Z_NO_FLUSH);
            if (rc == Z_STREAM_END || rc == Z_OK) {
                bobj->buf0 = _zs.next_in; _bobj->buf1 = _zs.next_out;
            } else if (rc == Z_BUF_ERROR) {
            } else {
                fprintf(stderr, "%s(%u): deflate %d(%s)\n",
                        __FUNCTION__, __LINE__, rc, _zs.msg);
            }
            assert(_bobj != NULL);
            if (_bobj->buf1 == _bobj->buflast || rc == Z_STREAM_END) {
                _bobj = node_t::step(done, _bobj, rc == Z_STREAM_END, joint);
                _bobj = reuse(_bobj, _bufsz);
            }
        } 
        return bobj;
    }

    // uncompresser_t.
    uncompresser_t::uncompresser_t(buf_t *bobj, node_t *next): node_t(next)
    {
        _bufsz = bobj->buflast - bobj->bufstart;
        _bobj = bobj;
        memset(&_zs, 0, sizeof(_zs));
        inflateInit(&_zs);
    }
    uncompresser_t::uncompresser_t(ptrdiff_t bufsz, node_t *next): node_t(next)
    {
        _bufsz = bufsz;
        _bobj = NULL;
        memset(&_zs, 0, sizeof(_zs));
        inflateInit(&_zs);
    }
    buf_t *
    uncompresser_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        int rc = Z_OK;
#if 0
        fprintf(stderr, "bobj: %u, lastbuf: %s, rc = %d\n",
                (unsigned)(bobj->buf1 - bobj->buf0), bool2str(lastbuf), rc);
#endif
        while (bobj->buf0 < bobj->buf1 || (lastbuf && rc == Z_OK)) {
            if (_bobj == NULL) _bobj = reuse(_bobj, _bufsz);
            _zs.next_in = bobj->buf0;
            _zs.avail_in = bobj->buf1 - bobj->buf0;
            _zs.next_out = _bobj->buf1;
            _zs.avail_out = _bobj->buflast - _bobj->buf1;
            rc = inflate(&_zs, lastbuf ? Z_SYNC_FLUSH: Z_NO_FLUSH);
            if (rc == Z_STREAM_END || rc == Z_OK) {
                bobj->buf0 = _zs.next_in; _bobj->buf1 = _zs.next_out;
            } else if (rc == Z_BUF_ERROR) {
            } else {
                fprintf(stderr, "%s(%u): inflate %d(%s)\n",
                        __FUNCTION__, __LINE__, rc, _zs.msg);
            }
            assert(_bobj != NULL);
            if (_bobj->buf1 == _bobj->buflast || rc == Z_STREAM_END) {
                _bobj = node_t::step(done, _bobj, rc == Z_STREAM_END, joint);
                _bobj = reuse(_bobj, _bufsz);
            }
        }
        return bobj;
    }

    // dump_t.
    buf_t *
    dump_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        uint8_t *cur;
        ptrdiff_t nbytes;
        for (cur = bobj->buf0; cur < bobj->buf1; cur += nbytes) {
            nbytes = std::min(bobj->buf1 - cur, _lnlast - _lncur);
            memcpy(_lncur, cur, nbytes);
            _lncur += nbytes;
            if (_lncur == _lnlast) {
                binlnshow(_prefix, _wfp, _off, _lnbuf, _lncur);
                _off += _lncur - _lnbuf;
                _lncur = _lnbuf;
            }
        }
        if (lastbuf) binlnshow(_prefix, _wfp, _off, _lnbuf, _lncur);
        return node_t::step(done, bobj, lastbuf, joint);
    }
}
