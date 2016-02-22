#pragma once

#include <zlib.h>
#include "rsyncxx/node.hpp"
#include "rsyncxx/utils.h"

namespace rsync {
    class DLLEXPORT infile_t: public node_t {
    public:
        infile_t(FILE *rfp, node_t *next, bool closef = true) :
            node_t(next), _rfp(rfp), _closef(closef) {}
        virtual ~infile_t() { if (_closef && _rfp) fclose(_rfp); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        FILE *_rfp;
        bool _closef;
    };
    class DLLEXPORT outfile_t: public node_t {
    public:
        outfile_t(FILE *wfp): node_t(NULL) { _wfp = wfp; }
        virtual ~outfile_t() { if (_wfp) fclose(_wfp); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        FILE *_wfp;
    };

    class DLLEXPORT gzinfile_t: public node_t {
    public:
        gzinfile_t(const char *fpath, node_t *next): node_t(next)
        {   _rfp = gzopen(fpath, "rb"); }
        virtual ~gzinfile_t() { if (_rfp) gzclose(_rfp); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        gzFile _rfp;
    };
    class DLLEXPORT gzoutfile_t: public node_t {
    public:
        gzoutfile_t(const char *fpath): node_t(NULL) { _wfp = gzopen(fpath, "wb9"); }
        virtual ~gzoutfile_t() { if (_wfp) gzclose(_wfp); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        gzFile _wfp;
    };

    class DLLEXPORT compresser_t: public node_t {
    public:
        compresser_t(buf_t *bobj, int level, node_t *next);
        compresser_t(ptrdiff_t bufsz, int level, node_t *next);
        virtual ~compresser_t() { deflateEnd(&_zs); if (_bobj) _bobj->unref(); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        ptrdiff_t _bufsz;
        buf_t *_bobj;
        z_stream _zs;
    };
    class DLLEXPORT uncompresser_t: public node_t {
    public:
        uncompresser_t(buf_t *bobj, node_t *next);
        uncompresser_t(ptrdiff_t bufsz, node_t *next);
        virtual ~uncompresser_t() { inflateEnd(&_zs); if (_bobj) _bobj->unref(); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        ptrdiff_t _bufsz;
        buf_t *_bobj;
        z_stream _zs;
    };

    class DLLEXPORT dump_t: public node_t {
    public:
        dump_t(const char *prefix, FILE *wfp, node_t *next): node_t(next)
        {   _prefix = prefix; _wfp = wfp; _off = 0;
            _lncur = _lnbuf; _lnlast = _lnbuf + sizeof(_lnbuf); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        const char *_prefix;
        FILE *_wfp;
        unsigned _off;
        uint8_t _lnbuf[BINLNWIDTH], *_lncur, *_lnlast;
    };
}
