#pragma once

#include <vector>
#include "rsyncxx/deltalist.hpp"

namespace rsync {
    class DLLEXPORT patch_t: public deltalist_t, public std::vector<refbuf_t *> {
    public:
        patch_t(ptrdiff_t patchbufsz): node_t(NULL), deltalist_t(), std::vector<refbuf_t *>()
        {   _patchbufsz = patchbufsz; _bobj = NULL; }
        virtual ~patch_t() { if (_bobj) _bobj->unref(); }
    protected:
        // from base.
        virtual void delta1(node_t *joint);
        virtual void unchanged(node_t *joint) {}
        virtual void onpackend(void) {}
        // for derives.
        virtual node_t *_make_in(const char *fpath);
        virtual node_t *_make_out(const char *fpath);
    private:
        ptrdiff_t _patchbufsz;
        buf_t *_bobj;
    };
}
