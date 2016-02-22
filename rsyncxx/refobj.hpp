#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>
#include "rsyncxx/defs.h"

namespace rsync {
    class refobj_t;
    class DLLEXPORT wrapobj_t {
    public:
        wrapobj_t(refobj_t *cxxobj);
        virtual ~wrapobj_t() {}

        virtual void ref(void) {}
        virtual void unref(void) {}
    };

    class DLLEXPORT refobj_t {
    public:
        refobj_t(void) { _refcnt = 1; _wrapobj = NULL; }
        virtual ~refobj_t() { assert(_refcnt == 0); assert(_wrapobj == NULL); }

        inline refobj_t *ref(void)
        {   if (_wrapobj) _wrapobj->ref();
            ++_refcnt;
            return this; }
        inline refobj_t *unref(void)
        {   assert(_refcnt > 0);
            if (_wrapobj) _wrapobj->unref();
            if (--_refcnt > 0) return NULL;
            if (_wrapobj == NULL) delete this;
            else _onref0();
            return NULL; }

        inline wrapobj_t *getwrap(void) { return _wrapobj; }
        // the caller of setwrap must have a refcnt, and this refcnt will
        // be consumed after the call of it.
        inline void setwrap(wrapobj_t *wrapobj)
        {   assert(_wrapobj == NULL); assert(_refcnt > 0);
            --_refcnt; _wrapobj = wrapobj; }
        inline void cleanwrap(void)
        {   if (_wrapobj) delete _wrapobj;
            _wrapobj = NULL;
            if (_refcnt == 0) delete this; }
    protected:
        virtual void _onref0(void) {}
    private:
        unsigned _refcnt;
        wrapobj_t *_wrapobj;
        void _dump(const char *func, unsigned ln)
        {   fprintf(stderr, "%s(%u): this = %p(%s), refcnt = %u, wrap = %p\n",
                    func, ln, this, typeid(*this).name(), _refcnt, _wrapobj); }
    };
    inline wrapobj_t::wrapobj_t(refobj_t *cxxobj) { cxxobj->setwrap(this); }
}
