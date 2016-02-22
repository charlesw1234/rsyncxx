#pragma once

#include <list>
#include "rapidjson/document.h"
#include "rsyncxx/cmd.hpp"
#include "rsyncxx/delta.tab.hpp"

namespace rsync {
    class deltalist_t;
    struct deltacmd_spot_t {
        uint64_t *off;
        buf_t *bobj;
        deltalist_t *dlobj;
        node_t *patch1;
        node_t *joint;
        inline buf_t *step(buf_t *bobj)
        {   bool done = false;
            return patch1->stepnext(done, bobj, false, joint); }
    };

    class DLLEXPORT deltacmd_t {
    public:
        deltacmd_t(void) {}
        virtual ~deltacmd_t() {}
        // return true when no data can be dumped through this cmd.
        virtual bool exec(deltacmd_spot_t &spot) = 0;
        virtual rapidjson::Value jdump(rapidjson::Document &doc) const = 0;

        virtual void dump(const char *prefix, FILE *wfp) const = 0;
    };
    class DLLEXPORT deltacmdlist_t: public std::list<deltacmd_t *> {
    public:
        virtual ~deltacmdlist_t() { reset(); }
        virtual void reset(void)
        {  for (iterator iter = begin(); iter != end(); ++iter) delete *iter;
            std::list<deltacmd_t *>::clear(); }
    };
    class DLLEXPORT deltalist_t: public cmd_t, public deltacmdlist_t {
    public:
        deltalist_t(void): node_t(NULL), cmd_t(&delta_suite), deltacmdlist_t()
        {   _fpath = _fpathcur = _fpathlast = NULL; }
        virtual ~deltalist_t() { reset(); }

        virtual void reset(void)
        {   _lastcopy = 0; deltacmdlist_t::reset(); _cache.reset(); }

        void pop_head(void);
        deltacmdlist_t::iterator get_cache(uint64_t off);
        void dec_cache_ref(deltacmdlist_t::iterator iter);

        inline const char *fpath(void) const { return _fpath; }

        virtual void dump(const char *prefix, FILE *wfp) const
        {   for (const_iterator iter = begin(); iter != end(); ++iter)
                (*iter)->dump(prefix, wfp); }
        void jload(const rapidjson::Value &jarray)
        {   for (unsigned idx = 0; idx < jarray.Size(); ++idx) _jload(jarray[idx]); }
        rapidjson::Value jdump(rapidjson::Document &doc) const
        {   rapidjson::Value jarray(rapidjson::kArrayType);
            for (const_iterator iter = begin(); iter != end(); ++iter)
                jarray.PushBack((*iter)->jdump(doc), doc.GetAllocator());
            return jarray; }
    protected:
        // from base.
        virtual buf_t *runstream(buf_t *bobj, bool lastbuf, node_t *joint);
        virtual buf_t *runcmd(buf_t *bobj, bool lastbuf, node_t *joint);
        // for derives.
        virtual void delta1(node_t *joint) {}
        virtual void unchanged(node_t *joint) {}
        virtual void onpackend(void) {}
        // for derives to do debug check.
        virtual void _issue_copy(uint64_t start, uint64_t end);
        virtual void _issue_literal(uint64_t size);
    private:
        char *_fpath, *_fpathcur, *_fpathlast;
        uint64_t _lastcopy;
        deltacmdlist_t _cache;
        void _calldelta1(node_t *joint);
        void _jload(const rapidjson::Value &jdesc);
    };
}
