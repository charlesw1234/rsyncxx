#include "rsyncxx/patch.hpp"
#include "rsyncxx/basic.hpp"

namespace rsync {
    // patch1_t: deal with a single file, generate it's patch.
    class patch1_t: public node_t {
    public:
        patch1_t(deltalist_t *dlobj, ptrdiff_t patchbufsz, node_t *next): node_t(next)
        {   _dlobj = dlobj; _patchbufsz = patchbufsz; _off = 0; }
        virtual ~patch1_t() {}
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        deltalist_t *_dlobj;
        ptrdiff_t _patchbufsz;
        uint64_t _off;
    };
    buf_t *
    patch1_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        deltacmd_spot_t spot;
        spot.off = &_off;
        spot.bobj = bobj;
        spot.dlobj = _dlobj;
        spot.patch1 = this;
        spot.joint = joint;
        while (!_dlobj->empty()) {
            if (!_dlobj->front()->exec(spot)) return spot.bobj;
            _dlobj->pop_head();
        }
        // the bytes remained in spot.bobj is useless, cut them off.
        if (spot.bobj && spot.bobj->buf0 < spot.bobj->buf1)
            spot.bobj->buf0 = spot.bobj->buf1 = spot.bobj->bufstart;
        spot.bobj = reuse(spot.bobj, _patchbufsz);
        return node_t::step(done, spot.bobj, lastbuf, joint);
    }

    // patch_t.
    void
    patch_t::delta1(node_t *joint)
    {
        bool done = false;
        node_t *innode = _make_in(fpath());
        node_t *outnode = _make_out(fpath());
        if (innode == NULL || outnode == NULL) {
            if (innode != NULL) innode->unref();
            if (outnode != NULL) outnode->unref();
            return;
        }
        innode->append(new patch1_t(this, _patchbufsz, outnode));
        while (!done) _bobj = innode->step(done, reuse(_bobj, _patchbufsz));
        innode->unref();
    }
    node_t *
    patch_t::_make_in(const char *fpath)
    {
        FILE *rfp = fopen(fpath, "rb");
        if (rfp == NULL) return NULL;
        return new infile_t(rfp, NULL);
    }
    node_t *
    patch_t::_make_out(const char *fpath)
    {
        char wfpath[256];
        snprintf(wfpath, sizeof(wfpath), "%s.new", fpath);
        FILE *wfp = fopen(wfpath, "wb");
        if (wfp == NULL) return NULL;
        return new outfile_t(wfp);
    }
}
