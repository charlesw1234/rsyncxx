#include <algorithm>
#include <typeinfo>
#include "rsyncxx/signature.hpp"
#include "rsyncxx/signature.tab.hpp"
#include "rsyncxx/sums.hpp"

namespace rsync {
    class sign1_t: public node_t {
    public:
        sign1_t(size_t szblock, size_t nsigns, node_t *next): node_t(next)
        {   _szblock = szblock; _nbytes = 0;
            _sign1bufsz = sizeof(sign_t) + nsigns * sizeof(sign_t);
            _rollsum.init(); _strongsigner.init(sizeof(strong_sum_t));
            _bobj = new buf_t(_sign1bufsz, sizeof(sign_t)); }
        virtual ~sign1_t() { if (_bobj) _bobj->unref(); }

        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
#ifdef _MSC_VER
        void *operator new(size_t sz) { return _aligned_malloc(sz, 64); }
        void operator delete(void *ptr) { _aligned_free(ptr); }
#endif
    private:
        size_t _szblock, _sign1bufsz, _nbytes;
        rollsum_t _rollsum;
        strong_signer_t _strongsigner;
        buf_t *_bobj;
        void _issue_signs(bool lastissue, node_t *joint);
    };
    buf_t *
    sign1_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        bool lastissue;
        ptrdiff_t nbytes;
        sign_t *signs = (sign_t *)_bobj->buf0;
        sign_t *signsnext = (sign_t *)_bobj->buf1;
        sign_t *signslast = (sign_t *)_bobj->buflast;
        while (bobj->buf0 < bobj->buf1) {
            nbytes = std::min(bobj->buf1 - bobj->buf0, (ptrdiff_t)(_szblock - _nbytes));
            assert(nbytes > 0);
            // consume input as long as possible.
            _rollsum.update(bobj->buf0, nbytes);
            _strongsigner.update(bobj->buf0, nbytes);
            _nbytes += nbytes;
            bobj->buf0 += nbytes;
            if (_nbytes < _szblock && !lastbuf) return bobj; // wait more data.
            assert(_nbytes > 0);
            // issue a block.
            signsnext->weak = _rollsum.digest();
            _strongsigner.final(signsnext->strong, sizeof(strong_sum_t));
            ++signsnext;
            _bobj->buf1 = (uint8_t *)signsnext;
            // restore the status.
            _nbytes = 0;
            _rollsum.init();
            _strongsigner.init(sizeof(strong_sum_t));
            // issue if required.
            lastissue = lastbuf && bobj->buf0 == bobj->buf1;
            if (lastissue || signsnext == signslast) {
                _issue_signs(lastissue, joint);
                signs = (sign_t *)_bobj->buf0;
                signsnext = (sign_t *)_bobj->buf1;
                signslast = (sign_t *)_bobj->buflast;
            }
        }
        if (lastbuf) {
            if (signs < signsnext) _issue_signs(true, joint);
            done = true;
        }
        return bobj;
    }
    void
    sign1_t::_issue_signs(bool lastbuf, node_t *joint)
    {
        bool done = false;
        uint8_t buf[sizeof(sign_t)];
        ptrdiff_t bodysz = _bobj->buf1 - _bobj->buf0;
        ptrdiff_t nsigns = bodysz / sizeof(sign_t);
        ptrdiff_t cmdsz = signature_make_signature(buf, nsigns) - buf;
        assert(cmdsz <= (ptrdiff_t)sizeof(sign_t));
        assert(_bobj->buf + sizeof(sign_t) == _bobj->buf0);
        _bobj->buf0 -= cmdsz;
        memcpy(_bobj->buf0, buf, cmdsz);
        _bobj = node_t::step(done, _bobj, lastbuf, joint);
        _bobj = reuse(_bobj, _sign1bufsz, sizeof(sign_t));
    }

    signature_t::signature_t(size_t szblock, size_t nsigns, node_t *next):
        node_t(next), combiner_t(next)
    {
        uint8_t *buf = (uint8_t *)alloca(signature_suite.maxcmdlen);
        _szblock = szblock; _nsigns = nsigns;
        push_buf(buf, signature_make_block_len(buf, szblock));
    }
    void
    signature_t::push_file(const char *fpath, node_t *node)
    {
        size_t fpathlen = strlen(fpath);
        buf_t *bobj = new buf_t(signature_suite.maxcmdlen + fpathlen);
        // push filepath command.
        bobj->buf1 = signature_make_file_path(bobj->buf1, fpathlen);
        memcpy(bobj->buf1, fpath, fpathlen);
        bobj->buf1 += fpathlen;
        push_buf(bobj);
        // push filepath body.
        push_node(node->append(new sign1_t(_szblock, _nsigns, NULL)));
    }
    void signature_t::push_end(void)
    {
        push_byte(signature_cmd_pack_end);
        combiner_t::push_end();
    }
}
