#include <malloc.h>
#include <list>
#include <algorithm>
#include "rsyncxx/delta.hpp"
#include "rsyncxx/delta.tab.hpp"
#include "rsyncxx/basic.hpp"
#include "rsyncxx/sums.hpp"

namespace rsync {
    // delta1new_t: generate a patch for the new created file.
    class delta1new_t: public node_t {
    public:
        delta1new_t(node_t *next): node_t(next) {}
        virtual ~delta1new_t();

        buf_t *step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint);
    private:
        std::list<buf_t *> _body;
    };
    delta1new_t::~delta1new_t()
    {
        std::list<buf_t *>::iterator iter;
        for (iter = _body.begin(); iter != _body.end(); ++iter)
            (*iter)->unref();
    }
    buf_t *
    delta1new_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        buf_t *cmdbobj;
        uint8_t *buf = (uint8_t *)alloca(delta_suite.maxcmdlen);
        uint64_t len = 0;
        std::list<buf_t *>::iterator iter;
        if (!bufblank(bobj)) { _body.push_back(bobj); bobj = NULL; }
        if (!lastbuf) return bobj;
        for (iter = _body.begin(); iter != _body.end(); ++iter)
            len += (*iter)->buf1 - (*iter)->buf0;
        if (len > 0) {
            cmdbobj = new buf_t(buf, delta_make_literal(buf, len));
            cmdbobj = node_t::step(done, cmdbobj, false, joint);
            if (cmdbobj) cmdbobj->unref();
            // delete the blank bobj from parameter.
            // non-blank bobj already push_back to _body.
            if (bobj != NULL && bobj->buf0 == bobj->buf1) bobj->unref();
            for (;;) {
                bobj = _body.front();
                _body.pop_front();
                if (_body.empty()) break; // leave the last bobj to return.
                bobj = node_t::step(done, bobj, false, joint);
                if (bobj) bobj->unref();
            }
        }
        done = true;
        return node_t::step(done, bobj, lastbuf, joint);
    }

    // copycmd_t: copy command encapsulate.
    class copycmd_t {
    public:
        copycmd_t(void) { _start = _length = 0; }

        inline bool empty(void) const { return _length == 0; }
        inline uint64_t end(void) const { return _start + _length; }

        inline void reset(void) { _length = 0; }

        inline uint8_t *gencmd(uint64_t *cnt, uint8_t *cur) const
        {   ++*cnt; return delta_make_copy(cur, _start, _length); }

        inline uint8_t *
        commit(uint64_t *cnt, uint8_t *cur, uint64_t start, uint64_t length)
        {   if (_length == 0) { _start = start; _length = length; }
            else if (_start + _length == start) _length += length;
            else { cur = gencmd(cnt, cur); _start = start; _length = length; }
            return cur; }

        bool whole(uint64_t rtotal) const { return _start == 0 && _length == rtotal; }
    private:
        uint64_t _start, _length;
    };
    // literalcmd_t: literal command encapsulate.
    class literalcmd_t: public std::list<buf_t *> {
    public:
        literalcmd_t(ptrdiff_t szlitblock): std::list<buf_t *>() { _szlitblock = szlitblock; }
        virtual ~literalcmd_t();

        inline uint64_t len(void) const
        {   uint64_t len = 0;
            for (const_iterator iter = begin(); iter != end(); ++iter)
                len += (*iter)->buf1 - (*iter)->buf0;
            return len; }
        inline void push_byte(uint8_t byte)
        {   if (empty() || back()->buf1 == back()->buflast)
                push_back(new buf_t(_szlitblock));
            *back()->buf1++ = byte; }

        inline uint8_t *gencmd(uint64_t *cnt, uint8_t *cur) const
        {   ++*cnt; return delta_make_literal(cur, len()); }
        void flush(node_t *node, node_t *joint);
    private:
        ptrdiff_t _szlitblock;
    };
    literalcmd_t::~literalcmd_t()
    {   for (iterator iter = begin(); iter != end(); ++iter) (*iter)->unref(); }
    void
    literalcmd_t::flush(node_t *node, node_t *joint)
    {
        buf_t *bobj;
        bool done = false;
        while (!empty()) {
            bobj = node->stepnext(done, front(), false, joint);
            if (bobj) bobj->unref();
            pop_front();
        }
    }

    // delta1_t: generate patch for a single file.
    class delta1_t: public node_t {
    public:
        delta1_t(delta_t *dobj, node_t *next): node_t(next), _literal(dobj->szlitblock())
        {   _dobj = dobj; _ncmds = _rtotal = 0; _cmdbufcur = _cmdbuf;
            _rolledbuf = new rolledbuf_t(_dobj->szblock()); }
        virtual ~delta1_t()
        {   if (_rolledbuf) _rolledbuf->unref();
            _dobj->unref(); }

        buf_t *step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint);
    private:
        delta_t *_dobj;
        uint64_t _ncmds, _rtotal;
        uint8_t _cmdbuf[1024], *_cmdbufcur;
        copycmd_t _copy;
        literalcmd_t _literal;
        rolledbuf_t *_rolledbuf;
        bool _match(uint64_t &idx, weak_sum_t sum) const;
        inline void _ensure(ptrdiff_t reqsz, node_t *joint)
        {   if (_cmdbuf + sizeof(_cmdbuf) - _cmdbufcur < reqsz)
                _issue_cmdbuf(joint); }
        void _issue_cmdbuf(node_t *joint);

        inline void _push_byte_cmd(uint8_t cmd, node_t *joint)
        {   _ensure(sizeof(cmd), joint); *_cmdbufcur++ = cmd; ++_ncmds; }

        void _prepare_copy(node_t *joint)
        {   if (_literal.empty()) return;
            _prepare_literal(joint);
            _commit_literal(joint); }
        inline void
        _commit_copy(uint64_t idx, uint64_t length, node_t *joint)
        {   uint64_t start = _dobj->idx2start(idx);
            _ensure(delta_suite.maxcmdlen, joint);
            _cmdbufcur = _copy.commit(&_ncmds, _cmdbufcur, start, length); }

        void _prepare_literal(node_t *joint)
        {   if (_copy.empty()) return;
            _ensure(delta_suite.maxcmdlen, joint);
            _cmdbufcur = _copy.gencmd(&_ncmds, _cmdbufcur);
            _copy.reset(); }
        inline void _commit_literal(node_t *joint)
        {   _ensure(delta_suite.maxcmdlen, joint);
            _cmdbufcur = _literal.gencmd(&_ncmds, _cmdbufcur);
            _issue_cmdbuf(joint);
            _literal.flush(this, joint); }
    };
    buf_t *
    delta1_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        uint64_t idx;
        ptrdiff_t nbytes;
        // loop on every input bytes.
        while (bobj->buf0 < bobj->buf1) {
            nbytes = std::min(bobj->buf1 - bobj->buf0, _rolledbuf->szfree());
            if (nbytes > 0) {
                // commit input into _rolledbuf as more as possible.
                _rolledbuf->commit(bobj->buf0, nbytes);
                bobj->buf0 += nbytes;
                _rtotal += nbytes;
            } else {
                // roll a byte in _rolledbuf and a byte out.
                assert(_rolledbuf->full());
                _literal.push_byte(_rolledbuf->rotate(*bobj->buf0));
                ++bobj->buf0; ++_rtotal;
            }
            if (_rolledbuf->full() && _match(idx, _rolledbuf->weak())) {
                // a match found, commit a copy command.
                _prepare_copy(joint);
                _commit_copy(idx, _rolledbuf->szused(), joint);
                _rolledbuf->reset();
            }
        }
        if (lastbuf) { // wind up process.
            // check the match of the last block.
            if (_rolledbuf && _match(idx, _rolledbuf->weak())) {
                // a match found, commit a copy command.
                _prepare_copy(joint);
                _commit_copy(idx, _rolledbuf->szused(), joint);
                _rolledbuf = _rolledbuf->unref();
            } else if (_rolledbuf && _rolledbuf->szused() > 0) {
                // not matched, commit a literal command.
                _prepare_literal(joint);
                _literal.push_back(_rolledbuf->output());
                _rolledbuf = NULL;
            }
            if (!_literal.empty()) {
                _prepare_literal(joint);
                _commit_literal(joint);
            } else if (!_copy.empty()) {
                // check unchanged, optimize if true.
                if (_ncmds == 0 && _copy.whole(_rtotal) &&
                    _rtotal > _dobj->szmin()) { // unchanged.
                    _push_byte_cmd(delta_cmd_file_unchanged_end, joint);
                } else { // changed.
                    _ensure(delta_suite.maxcmdlen, joint);
                    _cmdbufcur = _copy.gencmd(&_ncmds, _cmdbufcur);
                }
                _copy.reset();
                _issue_cmdbuf(joint);
            }
            done = true;
        }
        return node_t::step(done, bobj, lastbuf, joint);
    }
    typedef std::pair<signmap_t::iterator, signmap_t::iterator> sumrange_t;
    bool
    delta1_t::_match(uint64_t &idx, weak_sum_t sum) const
    {
        bool matched = false;
        strong_sum_t strong;
        signmap_t::iterator iter;
        signmap_t *smobj = (signmap_t *)_dobj;
        sumrange_t range = smobj->equal_range(sum);
        uint64_t idx0, start0, diff0, start = _copy.end(), diff;
        if (range.first == range.second) return false;
        _rolledbuf->strong(strong);
        for (iter = range.first; iter != range.second; ++iter) {
            if (memcmp(&strong, &iter->second.strong, sizeof(strong))) continue;
            idx0 = iter->second.idx;
            start0 = smobj->idx2start(idx0);
            if (start0 == start) { idx = idx0; return true; }
            diff0 = start0 < start ? start - start0: start0 - start;
            if (!matched) {
                matched = true; idx = idx0; diff = diff0;
            } else if (diff0 < diff || (diff0 == diff && idx < idx0)) {
                idx = idx0; diff = diff0;
            }
        }
        return matched;
    }
    void
    delta1_t::_issue_cmdbuf(node_t *joint)
    {
        buf_t *bobj;
        bool done = false;
        if (_cmdbuf == _cmdbufcur) return;
        bobj = new buf_t(_cmdbuf, _cmdbufcur);
        if ((bobj = node_t::step(done, bobj, false, joint))) bobj->unref();
        _cmdbufcur = _cmdbuf;
    }

    buf_t *
    delta_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        while (!bufblank(bobj))
            bobj = signmap_t::step(done, bobj, lastbuf, NULL);
        return combiner_t::step(done, bobj, lastbuf, joint);
    }

    void
    delta_t::sign1(node_t *joint)
    {
        bool done = false;
        buf_t *cmdbobj;
        node_t *bodynode, *deltanode;
        // filter the invalid fpath.
        if (!(bodynode = _make_in(fpath()))) return;
        // append delta body.
        deltanode = signmap_t::empty() ?
            (node_t *)new delta1new_t(NULL):
            (node_t *)new delta1_t(dref(), NULL);
        bodynode->append(deltanode);
        // generate filepath command and body in cmdbobj.
        cmdbobj = new buf_t(delta_suite.maxcmdlen + fpathlen());
        cmdbobj->buf1 = delta_make_file_path(cmdbobj->buf1, fpathlen());
        memcpy(cmdbobj->buf1, fpath(), fpathlen());
        cmdbobj->buf1 += fpathlen();
        // push filepath command buf and body node.
        push_buf(cmdbobj);
        push_node(bodynode);
        // run it.
        while (!combiner_t::empty()) {
            _bobj = reuse(_bobj, _szdeltabuf);
            _bobj = combiner_t::step(done, _bobj, false, joint);
        }
    }
    void
    delta_t::onpackend(void)
    {
        push_byte(delta_cmd_pack_end);
        push_end();
    }

    node_t *
    delta_t::_make_in(const char *fpath)
    {
        FILE *rfp = fopen(fpath, "rb");
        if (rfp == NULL) return NULL;
        return new infile_t(rfp, NULL);
    }
}
