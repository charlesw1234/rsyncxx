#pragma once

#include "rsyncxx/node.hpp"
#include "rsyncxx/signmap.hpp"

namespace rsync {
    // delta_t: do delta for each input file and generate the packaged deltas.
    class DLLEXPORT delta_t: public combiner_t, public signmap_t {
    public:
        delta_t(ptrdiff_t szdeltabuf, ptrdiff_t szlitblock, node_t *next):
            node_t(next), combiner_t(next), signmap_t()
        {   _bobj = NULL; _szdeltabuf = szdeltabuf; _szlitblock = szlitblock;
#if 0
            fprintf(stderr, "%s(%u): %p, %p, %p\n", __FUNCTION__, __LINE__,
                    this, (combiner_t *)this, (signmap_t *)this);
#endif
        }
        virtual ~delta_t() { if (_bobj) _bobj->unref(); }

        inline delta_t *dref(void) { combiner_t::ref(); return this; }

        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
        virtual node_t *append(node_t *next)
        {   return combiner_t::append(next); }

        inline size_t szlitblock(void) const { return _szlitblock; }
    protected:
        // from base.
        virtual void sign1(node_t *joint);
        virtual void onpackend(void);
        // for derives.
        virtual node_t *_make_in(const char *fpath);
    private:
        buf_t *_bobj;
        ptrdiff_t _szdeltabuf, _szlitblock;
    };
}
