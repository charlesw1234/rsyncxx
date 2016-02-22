#pragma once

#include <unordered_map>
#include "rsyncxx/sums.hpp"
#include "rsyncxx/cmd.hpp"
#include "rsyncxx/signature.tab.hpp"

namespace rsync {
    class smvalue_t {
    public:
        smvalue_t(uint64_t idx, const strong_sum_t *strong)
        {   this->idx = idx; memcpy(&this->strong, strong, sizeof(*strong)); }
        uint64_t idx;
        strong_sum_t strong;
    };
#if (_MSC_VER == 1500) // v90
    typedef std::tr1::unordered_multimap<weak_sum_t, smvalue_t> smbase_t;
#else
    typedef std::unordered_multimap<weak_sum_t, smvalue_t> smbase_t;
#endif
    class DLLEXPORT signmap_t: public cmd_t, public smbase_t {
    public:
        signmap_t(void): node_t(NULL), cmd_t(&signature_suite), smbase_t()
        { _szblock = _numsigns = 0; _fpath = NULL; }
        virtual ~signmap_t() { if (_fpath) free(_fpath); }

        inline size_t szblock(void) const { return _szblock; }
        inline uint64_t szmin(void) const
        {   assert(!empty()); return (size() - 1) * _szblock; }
        inline uint64_t idx2start(uint64_t idx) const { return idx * _szblock; }
        inline const char *fpath(void) const { return _fpath; }
        inline ptrdiff_t fpathlen(void) const { return _fpathlast - _fpath; }

        virtual void dump(const char *prefix, FILE *wfp) const;
    protected:
        // from base.
        virtual buf_t *runstream(buf_t *bobj, bool lastbuf, node_t *joint);
        virtual buf_t *runcmd(buf_t *bobj, bool lastbuf, node_t *joint);
        // for the derives.
        virtual void sign1(node_t *joint) {}
        virtual void onpackend(void) {}
    private:
        size_t _szblock, _numsigns, _signidx;
        char *_fpath, *_fpathcur, *_fpathlast;
        sign_t _sign; uint8_t *_signcur;
    };
}
