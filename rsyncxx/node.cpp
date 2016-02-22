#include <algorithm>
#include <typeinfo>
#include "rsyncxx/utils.h"
#include "rsyncxx/node.hpp"

namespace rsync {
    buf_t *
    node_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        if (_next == NULL) {
            done = lastbuf && bufblank(bobj);
        } else if (_next == joint) {
            if (!bufblank(bobj)) bobj = _next->step(done, bobj, false, joint);
            done = lastbuf && bufblank(bobj);
        } else if (lastbuf || !bufblank(bobj)) {
            bobj = _next->step(done, bobj, lastbuf, joint);
        }
        return bobj;
    }
    node_t *
    node_t::append(node_t *next)
    {
        node_t *cur = this;
        while (cur->_next != NULL) cur = cur->_next;
        cur->_next = next;
        return this;
    }

    buf_t *
    memsrc_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t nbytes = std::min(bobj->buflast - bobj->buf1, _last - _cur);
        memcpy(bobj->buf1, _cur, nbytes);
        bobj->buf1 += nbytes; _cur += nbytes;
        bobj = node_t::step(done, bobj, _cur < _last && lastbuf, joint);
        done = _cur == _last;
        return bobj;
    }
    buf_t *
    memdest_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        _flush();
        ptrdiff_t nbytes = std::min(bobj->buf1 - bobj->buf0, _last - _cur);
        memcpy(_cur, bobj->buf0, nbytes);
        _cur += nbytes; bobj->buf0 += nbytes;
        if (bobj->buf0 == bobj->buf1) return bobj;
        push_back(bobj); return NULL;
    }
    void
    memdest_t::_flush(void)
    {
        ptrdiff_t nbytes;
        while (_cur < _last && !empty()) {
            nbytes = std::min(front()->buf1 - front()->buf0, _last - _cur);
            memcpy(_cur, front()->buf0, nbytes);
            _cur += nbytes; front()->buf0 += nbytes;
            if (front()->buf0 == front()->buf1) {
                front()->unref(); pop_front();
            }
        }
    }
    buf_t *
    membufsrc_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        if (!bufblank(_bobj))
            _bobj = node_t::step(done, _bobj, lastbuf && bufblank(bobj), joint);
        if (_bobj != NULL) {
            assert(_bobj->buf0 == _bobj->buf1);
            _bobj = _bobj->unref();
        }
        return node_t::step(done, bobj, lastbuf, joint);
    }
    buf_t *
    membufdest_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        done = lastbuf;
        if (bobj->buf0 == bobj->buf1) return bobj;
        push_back(bobj); return NULL;
    }
    uint8_t *
    membufdest_t::get(uint8_t *buf, uint8_t *buflast)
    {
        ptrdiff_t nbytes;
        while (size() > 0 && buf < buflast) {
            nbytes = std::min(front()->buf1 - front()->buf0, buflast - buf);
            memcpy(buf, front()->buf0, nbytes);
            buf += nbytes; front()->buf0 += nbytes;
            if (front()->buf0 == front()->buf1) {
                front()->unref(); pop_front();
            }
        }
        return buf;
    }
    bool
    membufdest_t::check(const uint8_t *data, ptrdiff_t szdata) const
    {
        ptrdiff_t nbytes;
        for (const_iterator iter = begin(); iter != end(); ++iter) {
            nbytes = (*iter)->buf1 - (*iter)->buf0;
            if (nbytes > szdata) return false;
            if (memcmp((*iter)->buf0, data, nbytes)) return false;
            data += nbytes; szdata -= nbytes;
        }
        return szdata == 0;
    }
    void
    membufdest_t::dump(const char *prefix, FILE *wfp) const
    {
        ptrdiff_t nbytes;
        unsigned off = 0;
        const_iterator iter;
        const uint8_t *buf0;
        uint8_t buf[BINLNWIDTH], *cur = buf, *last = buf + sizeof(buf);
        for (iter = begin(); iter != end(); ++iter) {
            buf0 = (*iter)->buf0;
            while (buf0 < (*iter)->buf1) {
                nbytes = std::min(last - cur, (*iter)->buf1 - buf0);
                if (nbytes == BINLNWIDTH) {
                    binlnshow(prefix, wfp, off, buf0, buf0 + nbytes);
                    buf0 += nbytes;
                    off += nbytes;
                } else {
                    memcpy(cur, buf0, nbytes);
                    cur += nbytes; buf0 += nbytes;
                    if (cur == last) {
                        binlnshow(prefix, wfp, off, buf, last);
                        cur = buf;
                        off += nbytes;
                    }
                }
            }
        }
        if (buf < cur) binlnshow(prefix, wfp, off, buf, cur);
    }

    combiner_t::~combiner_t()
    {
        std::list<node_t *>::iterator iter;
        for (iter = begin(); iter != end(); ++iter)
            if (*iter != NULL) (*iter)->unref();
    }
    buf_t *
    combiner_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        node_t *subnode; bool subdone;
        ptrdiff_t bufsz = bobj->buflast - bobj->bufstart;
        while (!empty()) {
            subnode = front(); subdone = false;
            // for the runner, just push the blank bobj.
#if 0
            fprintf(stderr, "subnode(%s) = %p, bobj(%s) = %p, _next(%s) = %p\n",
                    typeid(*subnode).name(), subnode,
                    typeid(*bobj).name(), bobj,
                    typeid(*_next).name(), _next);
#endif
            bobj = reuse(subnode->step(subdone, bobj, true, _next), bufsz);
            if (!subdone) return bobj;
            pop_front();
            subnode->unref();
        }
        done = _eopush && lastbuf;
#if 0
        fprintf(stderr, "%s: done = %s, _eopush = %s, lastbuf= %s\n",
                typeid(*this).name(), bool2str(done),
                bool2str(_eopush), bool2str(lastbuf));
#endif
        return node_t::step(done, bobj, _eopush && lastbuf, joint);
    }
    node_t *
    combiner_t::append(node_t *next)
    {
        if (_next == NULL && next != NULL)
            for (iterator iter = begin(); iter != end(); ++iter)
                (*iter)->append(next->nref());
        return node_t::append(next);
    }
}
