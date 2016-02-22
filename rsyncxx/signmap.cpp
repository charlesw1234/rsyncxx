#include <algorithm>
#include "rsyncxx/signmap.hpp"

namespace rsync {
    static const uint8_t cstatus_file_path = cstatus_cmd_body;
    static const uint8_t cstatus_signs = cstatus_cmd_body + 1;
    buf_t *
    signmap_t::runstream(buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t nbytes;
        const sign_t *sobj;
        uint8_t *signlast = (uint8_t *)(&_sign + 1);
        if (_status == cstatus_file_path) {
            nbytes = std::min(bobj->buf1 - bobj->buf0, _fpathlast - _fpathcur);
            if (nbytes > 0) _fpathcur = bobj->readstr(_fpathcur, nbytes);
            _status = _fpathcur == _fpathlast ? cstatus_cmd: cstatus_file_path;
            return bobj;
        } else if (_status == cstatus_signs) {
            nbytes = std::min(bobj->buf1 - bobj->buf0, signlast - _signcur);
            assert(nbytes > 0);
            if (nbytes == sizeof(_sign)) {
                assert(_signcur == (uint8_t *)&_sign);
                sobj = (const sign_t *)bobj->buf0;
                bobj->buf0 += nbytes;
            } else {
                _signcur = bobj->read(_signcur, nbytes);
                if (_signcur < signlast) {
                    _status = cstatus_signs;
                    return bobj;
                }
                _signcur = (uint8_t *)&_sign;
                sobj = &_sign;
            }
            insert(value_type(sobj->weak,
                              smvalue_t(_signidx++, &sobj->strong)));
#if 0
            fprintf(stderr, "%s: _numsigns = %u, nbytes = %d\n",
                    _fpath, (unsigned)_numsigns, (int)nbytes);
#endif
            _status = --_numsigns > 0 ? cstatus_signs: cstatus_cmd;
            return bobj;
        }
        assert(0);
        return NULL; // just for cut off warning.
    }
    buf_t *
    signmap_t::runcmd(buf_t *bobj, bool lastbuf, node_t *joint)
    {
        switch ((signature_cmd_kind_t)_kind(_cmd)) {
        case ksignature_block_len:
            _szblock = (size_t)_param1;
            _status = cstatus_cmd;
            return bobj;
        case ksignature_file_path:
            if (_fpath != NULL) { sign1(joint); free(_fpath); _fpath = NULL; }
            _fpath = _fpathcur = (char *)malloc((size_t)_param1 + 1);
            _fpathlast = _fpath + _param1; *_fpathlast = 0;
            _signidx = 0;
            clear();
            _status = cstatus_file_path;
            return bobj;
        case ksignature_pack_end:
            if (_fpath != NULL) { sign1(joint); free(_fpath); _fpath = NULL; }
            onpackend();
            _status = cstatus_cmd;
            return bobj;
        case ksignature_pack_signature: assert(0);
        case ksignature_signature:
            _numsigns = (size_t)_param1;
            _signcur = (uint8_t *)&_sign;
            _status = cstatus_signs;
            return bobj;
        default: break;
        }
        assert(0);
        return NULL; // just for cut off warning.
    }

    void
    signmap_t::dump(const char *prefix, FILE *wfp) const
    {
        const_iterator iter;
        const uint8_t *first, *cur, *last;
        for (iter = begin(); iter != end(); ++iter) {
            fprintf(wfp, "%s%08X: %03u ", prefix,
                    (unsigned)iter->first, (unsigned)iter->second.idx);
            first = (const uint8_t *)&iter->second.strong;
            last = (const uint8_t *)(&iter->second.strong + 1);
            for (cur = first; cur < last; ++cur)
                fprintf(wfp, "%c%02X", cur == first ? '[': ',', (unsigned)*cur);
            fprintf(wfp, "]\n");
        }
    }
}
