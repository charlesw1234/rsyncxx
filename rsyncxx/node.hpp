#pragma once

#include <list>
#include "rsyncxx/refobj.hpp"

namespace rsync {
    class DLLEXPORT buf_t: public refobj_t {
    private:
        inline void _init(ptrdiff_t bufsz, ptrdiff_t reserve)
        {   buf = (uint8_t *)malloc(bufsz);
            buf0 = buf1 = bufstart = buf + reserve;
            buflast = buf + bufsz; }
    public:
        buf_t(void): refobj_t()
        {   buf = bufstart = buf0 = buf1 = buflast = NULL; }
        buf_t(ptrdiff_t bufsz, ptrdiff_t reserve = 0): refobj_t()
        {   _init(bufsz, reserve); }
        buf_t(const uint8_t *data, ptrdiff_t datasz, ptrdiff_t reserve = 0): refobj_t()
        {   _init(reserve + datasz, reserve);
            memcpy(bufstart, data, datasz); buf1 = buflast; }
        buf_t(const uint8_t *mem0, const uint8_t *mem1, ptrdiff_t reserve = 0): refobj_t()
        {   _init(reserve + (mem1 - mem0), reserve);
            memcpy(bufstart, mem0, mem1 - mem0); buf1 = buflast; }

        virtual ~buf_t() { if (buf) free(buf); }

        inline buf_t *ref(void) { return (buf_t *)refobj_t::ref(); }
        inline buf_t *unref(void) { return (buf_t *)refobj_t::unref(); }

        uint8_t *buf, *bufstart, *buf0, *buf1, *buflast;

        inline void reuse(void) { assert(buf0 == buf1); buf0 = buf1 = bufstart; }

        // shortcuts
        inline uint8_t *read(uint8_t *outbuf, ptrdiff_t nbytes)
        {   memcpy(outbuf, buf0, nbytes); buf0 += nbytes;
            return outbuf + nbytes; }
        inline char *readstr(char *outstr, ptrdiff_t nbytes)
        {   memcpy(outstr, buf0, nbytes); buf0 += nbytes;
            return outstr + nbytes; }
    };
    inline buf_t *reuse(buf_t *bobj, ptrdiff_t bufsz, ptrdiff_t reserve = 0)
    {   if (bobj == NULL) bobj = new buf_t(bufsz, reserve);
        else if (bobj->buflast - bobj->bufstart >= bufsz) bobj->reuse();
        else { bobj->unref(); bobj = new buf_t(bufsz, reserve); }
        return bobj; }
    inline bool bufblank(const buf_t *bobj)
    {   return bobj == NULL || bobj->buf0 == bobj->buf1; }

    class DLLEXPORT refbufspace_t: public refobj_t {
    public:
        refbufspace_t(ptrdiff_t bufsz): refobj_t()
        {   bufstart = bufwrite = (uint8_t *)malloc(bufsz);
            buflast = bufstart + bufsz; }
        ~refbufspace_t() { free(bufstart); }

        inline refbufspace_t *ref(void) { return (refbufspace_t *)refobj_t::ref(); }
        inline refbufspace_t *unref(void) { return (refbufspace_t *)refobj_t::unref(); }

        uint8_t *bufstart, *bufwrite, *buflast;
    };
    class DLLEXPORT refbuf_t: public buf_t {
    public:
        refbuf_t(refbufspace_t *space): buf_t()
        {   _space = space->ref();
            buf = NULL;
            bufstart = buf0 = space->bufstart;
            buflast = buf1 = space->bufwrite; }
        virtual ~refbuf_t() { _space->unref(); }
    private:
        refbufspace_t *_space;
    };

    class DLLEXPORT node_t: public refobj_t {
    public:
        friend class combiner_t;
        node_t(node_t *next) : refobj_t() { _next = next; }
        virtual ~node_t() { if (_next) _next->unref(); }

        inline node_t *nref(void) { refobj_t::ref(); return this; }
        inline node_t *nunref(void) { refobj_t::unref(); return NULL; }

        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
        inline buf_t *stepnext(bool &done, buf_t *bobj,
                               bool lastbuf = true, node_t *joint = NULL)
        {   return _next ? _next->step(done, bobj, lastbuf, joint): bobj; }

        virtual node_t *append(node_t *next);
    private:
        node_t *_next;
    };
#define NODE_REF(NODE)   ((NODE) ? (NODE)->nref(): NULL)

    class DLLEXPORT memsrc_t: public node_t {
    public:
        memsrc_t(const void *data, size_t szdata, node_t *next): node_t(next)
        {   _cur = (const uint8_t *)data; _last = _cur + szdata; }
        memsrc_t(const void *data0, const void *data1, node_t *next):
            node_t(next)
        {   _cur = (const uint8_t *)data0; _last = (const uint8_t *)data1; }
        virtual ~memsrc_t() {}

        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        const uint8_t *_cur, *_last;
    };
    class DLLEXPORT memdest_t: public node_t, public std::list<buf_t *> {
    public:
        memdest_t(void *data, size_t szdata): node_t(NULL), std::list<buf_t *>()
        {   _cur = (uint8_t *)data; _last = _cur + szdata; }
        memdest_t(void *data0, void *data1): node_t(NULL), std::list<buf_t *>()
        {   _cur = (uint8_t *)data0; _last = (uint8_t *)data1; }
        virtual ~memdest_t()
        {   for (iterator iter = begin(); iter != end(); ++iter)
                (*iter)->unref(); }

        inline void reset(void *data, size_t szdata)
        {   _cur = (uint8_t *)data; _last = _cur + szdata; _flush(); }
        inline void reset(void *data0, void *data1)
        {   _cur = (uint8_t *)data0; _last = (uint8_t *)data1; _flush(); }

        inline bool full(void) const { return _cur == _last; }

        virtual buf_t *step(bool &done, buf_t *bobj,
                            bool lastbuf = true, node_t *joint = NULL);
    private:
        uint8_t *_cur, *_last;
        void _flush(void);
    };
    class DLLEXPORT membufsrc_t: public node_t {
    public:
        membufsrc_t(buf_t *bobj, node_t *next): node_t(next) { _bobj = bobj; }
        membufsrc_t(const uint8_t *mem, ptrdiff_t szmem, node_t *next): node_t(next)
        {   _bobj = new buf_t(mem, szmem); }
        membufsrc_t(const uint8_t *mem0, const uint8_t *mem1, node_t *next): node_t(next)
        {   _bobj = new buf_t(mem0, mem1); }

        virtual ~membufsrc_t() { if (_bobj) _bobj->unref(); }
        virtual buf_t *step(bool &done, buf_t *bobj,
                            bool lastbuf = true, node_t *joint = NULL);
    private:
        buf_t *_bobj;
    };
    class DLLEXPORT membufdest_t: public node_t, public std::list<buf_t *> {
    public:
        membufdest_t(void): node_t(NULL), std::list<buf_t *>() {}
        virtual ~membufdest_t()
        {   for (iterator iter = begin(); iter != end(); ++iter)
                (*iter)->unref(); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
        uint8_t *get(uint8_t *buf, uint8_t *buflast);
        inline uint8_t *get(uint8_t *buf, ptrdiff_t szdata)
        {   return get(buf, buf + szdata); }
        bool check(const uint8_t *data, ptrdiff_t szdata) const;
        inline bool check(const uint8_t *mem0, const uint8_t *mem1) const
        {   return check(mem0, mem1 - mem0); }
        void dump(const char *prefix, FILE *wfp) const;
    };

    class DLLEXPORT combiner_t: virtual public node_t, public std::list<node_t *> {
    public:
        combiner_t(node_t *next): node_t(next) { _eopush = false; }
        virtual ~combiner_t();

        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);

        virtual node_t *append(node_t *next);

        inline void push_node(node_t *node)
        {   push_back(node); if (_next) node->append(_next->nref()); }

        inline void push_buf(buf_t *bobj)
        {   push_back(new membufsrc_t(bobj, _next ? _next->nref(): NULL)); }

        inline void push_buf(const uint8_t *mem, ptrdiff_t szmem)
        {   buf_t *bobj = new buf_t(szmem);
            bobj->buf0 = bobj->bufstart; bobj->buf1 = bobj->buflast;
            memcpy(bobj->bufstart, mem, szmem);
            push_buf(bobj); }
        inline void push_buf(const uint8_t *mem0, const uint8_t *mem1)
        {   buf_t *bobj = new buf_t(mem1 - mem0);
            bobj->buf0 = bobj->bufstart; bobj->buf1 = bobj->buflast;
            memcpy(bobj->bufstart, mem0, mem1 - mem0);
            push_buf(bobj); }
        inline void push_byte(uint8_t byte) { push_buf(&byte, 1); }
        inline void push_mem(const void *data0, const void *data1)
        {   push_back(new memsrc_t(data0, data1, _next ? _next->nref(): NULL));}
        inline void push_mem(const void *data, ptrdiff_t szdata)
        {   push_mem(data, ((const uint8_t *)data) + szdata); }

        virtual void push_end(void) { _eopush = true; }
    private:
        bool _eopush;
    };
}
