#include <algorithm>
#include <typeinfo>
#include "rsyncxx/deltalist.hpp"

namespace rsync {
    // various commands.
    // copy data from delta into dest.
    class deltacmd_literal_t: public deltacmd_t {
    public:
        deltacmd_literal_t(uint64_t szliteral): deltacmd_t() { _bobj = new buf_t(szliteral); }
        virtual ~deltacmd_literal_t() { if (_bobj) _bobj->unref(); }

        inline bool done(void) const { return _bobj->buf1 == _bobj->buflast; }
        inline ptrdiff_t nbytes(void) const { return _bobj->buflast - _bobj->buf1; }
        inline const uint8_t *save(const uint8_t *data, ptrdiff_t nbytes)
        {   memcpy(_bobj->buf1, data, nbytes); _bobj->buf1 += nbytes;
            return data + nbytes; }

        virtual bool exec(deltacmd_spot_t &spot) { _bobj = spot.step(_bobj); return true; }
        virtual void dump(const char *prefix, FILE *wfp) const
        {   fprintf(wfp, "%sliteral(%u)\n", prefix, (unsigned)(_bobj->buf1 - _bobj->buf0)); }
        virtual rapidjson::Value jdump(rapidjson::Document &doc) const
        {   rapidjson::Value jarray(rapidjson::kArrayType);
            ptrdiff_t szliteral = _bobj->buflast - _bobj->bufstart;
            jarray.PushBack("literal", doc.GetAllocator());
            jarray.PushBack((uint64_t)szliteral, doc.GetAllocator());
            return jarray; }
    private:
        buf_t *_bobj;
    };

    // copy data from src into dest.
    class deltacmd_source_t;
    class source_use_t {
    public:
        uint64_t start, end;
        deltacmd_t *src1, *src2;
        inline void init(uint64_t start, uint64_t end)
        {   assert(src1 == NULL); assert(src2 == NULL);
            this->start = start; this->end = end; }
        inline void append(deltacmd_t *src)
        {   if (src1 == NULL) src1 = src;
            else if (src2 == NULL) src2 = src;
            else assert(0); }
        inline void
        insert(deltacmdlist_t *dlbase, deltacmdlist_t::iterator &iter)
        {   if (src1) { dlbase->insert(iter, src1); src1 = NULL; }
            if (src2) { dlbase->insert(iter, src2); src2 = NULL; } }
    };
    class deltacmd_source_t: public deltacmd_t {
    public:
        deltacmd_source_t(uint64_t start, uint64_t end,
                          bool copy_ornot = true, uint16_t use_count = 0): deltacmd_t()
        {   this->copy_ornot = copy_ornot; this->use_count = use_count;
            this->start = start; this->end = end;
            space = NULL; }
        virtual ~deltacmd_source_t() { if (space) space->unref(); }

        void merge(source_use_t &srcuse);

        inline const uint8_t *get(uint64_t off) const
        {   assert(space != NULL); assert(use_count > 0);
            assert(start <= off); assert(off < end);
            return space->bufstart + (off - start); }
        inline uint64_t get_end(void) const { return end; }
        inline bool check(uint64_t off) const { return start <= off && off < end; }

        inline uint16_t get_use_count(void) const { return use_count; }
        inline bool dec_use_count(void) { return --use_count == 0; }
        inline void alloc(void)
        {   assert(space == NULL);
            if (use_count == 0) return;
            space = new refbufspace_t((ptrdiff_t)(end - start)); }

        inline buf_t *getbuf(void) const { assert(space != NULL); return new refbuf_t(space); }

        virtual bool exec(deltacmd_spot_t &spot);

        virtual void dump(const char *prefix, FILE *wfp) const
        {   fprintf(wfp, "%ssource([%u, %u), %s, use count(%u))\n", prefix,
                    (unsigned)start, (unsigned)end,
                    copy_ornot ? "copy": "no-copy",
                    (unsigned)use_count); }
        virtual rapidjson::Value jdump(rapidjson::Document &doc) const
        {   rapidjson::Value jarray(rapidjson::kArrayType);
            jarray.PushBack("source", doc.GetAllocator());
            jarray.PushBack(start, doc.GetAllocator());
            jarray.PushBack(end, doc.GetAllocator());
            jarray.PushBack(copy_ornot, doc.GetAllocator());
            jarray.PushBack(use_count, doc.GetAllocator());
            return jarray; }
    private:
        bool copy_ornot;
        uint16_t use_count;
        uint64_t start, end;
        refbufspace_t *space;
    };
    static int u64cmp(const void *ptr0, const void *ptr1)
    {   uint64_t val0 = *(const uint64_t *)ptr0;
        uint64_t val1 = *(const uint64_t *)ptr1;
        return val0 < val1 ? -1: val0 > val1 ? 1: 0; }
    void
    deltacmd_source_t::merge(source_use_t &su)
    {
        uint64_t pos[4];
        bool insu, inself, copy_ornot0;
        uint16_t use_count0;
        pos[0] = su.start; pos[1] = su.end; pos[2] = start; pos[3] = end;
        qsort(pos, 4, sizeof(uint64_t), u64cmp);
        for (unsigned idx = 0; idx < 3; ++idx) {
            if (pos[idx + 1] > end) break;
            if (pos[idx] == pos[idx + 1]) continue;
            insu = pos[idx] >= su.start && pos[idx + 1] <= su.end;
            inself = pos[idx] >= start; // && pos[idx + 1] <= end
            if (!insu && !inself) continue;
            copy_ornot0 = inself ? copy_ornot: 0;
            use_count0 = (insu ? 1: 0) + (inself ? use_count: 0);
            if (pos[idx + 1] < end) {
                su.append(new deltacmd_source_t(pos[idx], pos[idx + 1],
                                                copy_ornot0, use_count0));
            } else { // pos[idx + 1] == end.
                start = pos[idx];
                assert(copy_ornot == copy_ornot0);
                use_count = use_count0;
            }
        }
        if (su.end <= end) su.start = su.end;
        else su.start = std::max(su.start, end);
    }
    bool
    deltacmd_source_t::exec(deltacmd_spot_t &spot)
    {
        ptrdiff_t nbytes;
        buf_t *bobj;
        if (*spot.off == end) return true;
        assert(*spot.off < end);
        // do the required skip.
        if (*spot.off < start) {
            nbytes = std::min((ptrdiff_t)(start - *spot.off),
                              spot.bobj->buf1 - spot.bobj->buf0);
            if (nbytes == 0) return false;
            *spot.off += nbytes;
            spot.bobj->buf0 += nbytes;
            if (*spot.off < start) return false;
        }
        nbytes = std::min(spot.bobj->buf1 - spot.bobj->buf0, (ptrdiff_t)(end - *spot.off));
        if (nbytes > 0) {
            if (use_count > 0) {
                assert(space->bufstart != NULL);
                assert(space->bufwrite + nbytes <= space->buflast);
                memcpy(space->bufwrite, spot.bobj->buf0, nbytes);
                space->bufwrite += nbytes;
            }
            if (!copy_ornot) {
                spot.bobj->buf0 += nbytes; // do not copy, just consume it.
            } else {
                if (spot.bobj->buf0 + nbytes == spot.bobj->buf1) {
                    // last part of spot.bobj, step it directly.
                    spot.bobj = spot.step(spot.bobj);
                } else if (use_count == 0) {
                    // one pass piece, new a buf_t to pass it.
                    bobj = new buf_t(spot.bobj->buf0, nbytes);
                    bobj = spot.step(bobj);
                    if (bobj) bobj->unref();
                    spot.bobj->buf0 += nbytes;
                } else if (space->bufwrite == space->buflast) {
                    // space is full now, step it by refbuf_t.
                    bobj = new refbuf_t(space);
                    bobj = spot.step(bobj);
                    if (bobj) bobj->unref();
                    spot.bobj->buf0 += nbytes;
                } else {
                    // space is not full yet, just consume the input.
                    spot.bobj->buf0 += nbytes;
                }
            }
            *spot.off += nbytes;
        }
        return *spot.off == end;
    }

    // copy data from cache into src.
    class deltacmd_use_t: public deltacmd_t {
    public:
        deltacmd_use_t(uint64_t start, uint64_t end): deltacmd_t()
        {   this->start = this->cur = start; this->end = end; }
        virtual bool exec(deltacmd_spot_t &spot);
        virtual void dump(const char *prefix, FILE *wfp) const
        {   fprintf(wfp, "%suse([%u, %u))\n", prefix, (unsigned)start, (unsigned)end); }
        virtual rapidjson::Value jdump(rapidjson::Document &doc) const
        {   rapidjson::Value jarray(rapidjson::kArrayType);
            jarray.PushBack("use", doc.GetAllocator());
            jarray.PushBack(start, doc.GetAllocator());
            jarray.PushBack(end, doc.GetAllocator());
            return jarray; }
    private:
        uint64_t start, cur, end;
    };
    bool
    deltacmd_use_t::exec(deltacmd_spot_t &spot)
    {
        deltacmdlist_t::iterator iter;
        deltacmd_source_t *srccmd;
        buf_t *bobj;
        while (cur < end) {
            iter = spot.dlobj->get_cache(cur);
            srccmd = dynamic_cast<deltacmd_source_t *>(*iter);
            assert(srccmd != NULL);
            bobj = srccmd->getbuf();
            cur += bobj->buf1 - bobj->buf0;
            bobj = spot.step(bobj);
            if (bobj) bobj->unref();
            if (cur == srccmd->get_end()) spot.dlobj->dec_cache_ref(iter);
        }
        return cur == end;
    }

    // deltalist_t.
    static const uint8_t cstatus_file_path = cstatus_cmd_body;
    static const uint8_t cstatus_literal = cstatus_cmd_body + 1;
    void
    deltalist_t::pop_head(void)
    {
        deltacmd_t *cmd = front();
        deltacmd_source_t *srccmd = dynamic_cast<deltacmd_source_t *>(cmd);
        pop_front();
        if (srccmd == NULL || srccmd->get_use_count() == 0) delete cmd;
        else _cache.push_back(srccmd);
    }
    deltacmdlist_t::iterator
    deltalist_t::get_cache(uint64_t off)
    {
        deltacmdlist_t::iterator iter;
        deltacmd_source_t *srccmd;
        for (iter = _cache.begin(); iter != _cache.end(); ++iter) {
            srccmd = dynamic_cast<deltacmd_source_t *>(*iter);
            assert(srccmd != NULL);
            if (srccmd->check(off)) return iter;
        }
        return _cache.end();
    }
    void
    deltalist_t::dec_cache_ref(deltacmdlist_t::iterator iter)
    {
        deltacmd_source_t *srccmd = dynamic_cast<deltacmd_source_t *>(*iter);
        if (!srccmd->dec_use_count()) return;
        delete srccmd;
        _cache.erase(iter);
    }
    buf_t *
    deltalist_t::runstream(buf_t *bobj, bool lastbuf, node_t *joint)
    {
        ptrdiff_t nbytes;
        if (_status == cstatus_file_path) {
            nbytes = std::min(bobj->buf1 - bobj->buf0, _fpathlast - _fpathcur);
            if (nbytes > 0) _fpathcur = bobj->readstr(_fpathcur, nbytes);
            _status = _fpathcur == _fpathlast ? cstatus_cmd: cstatus_file_path;
            return bobj;
        } else if (_status == cstatus_literal) {
            assert(!empty());
            deltacmd_literal_t *literal = dynamic_cast<deltacmd_literal_t *>(back());
            assert(literal != NULL);
            nbytes = std::min(bobj->buf1 - bobj->buf0, literal->nbytes());
            if (nbytes > 0) {
                literal->save(bobj->buf0, nbytes);
                bobj->buf0 += nbytes;
            }
            if (!literal->done()) { _status = cstatus_literal; return bobj; }
            //literal->reset();
            _status = cstatus_cmd;
            return bobj;
        }
        assert(0);
        return NULL; // just for cut off warning.
    }
    buf_t *
    deltalist_t::runcmd(buf_t *bobj, bool lastbuf, node_t *joint)
    {
        switch ((delta_cmd_kind_t)_kind(_cmd)) {
        case kdelta_file_path:
            if (_fpath != NULL) {
                _calldelta1(joint);
                free(_fpath); _fpath = NULL;
            }
            _fpath = _fpathcur = (char *)malloc((size_t)_param1 + 1);
            _fpathlast = _fpath + _param1; *_fpathlast = 0;
            reset();
            _status = cstatus_file_path;
            return bobj;
        case kdelta_file_unchanged_end:
            if (_fpath != NULL) {
                unchanged(joint);
                free(_fpath); _fpath = NULL;
            }
            reset();
            _status = cstatus_cmd;
            return bobj;
        case kdelta_pack_end:
            if (_fpath != NULL) {
                _calldelta1(joint);
                free(_fpath); _fpath = NULL;
            }
            reset();
            onpackend();
            _status = cstatus_cmd;
            return bobj;
        case kdelta_pack_delta: assert(0);
        case kdelta_copy:
            _issue_copy(_param1, _param1 + _param2);
            _status = cstatus_cmd;
            return bobj;
        case kdelta_literal:
            _issue_literal(_param1);
            _status = cstatus_literal;
            return bobj;
        default: break;
        }
        assert(0);
        return NULL; // just for cut off warning.
    }
    void
    deltalist_t::_issue_copy(uint64_t start, uint64_t end)
    {
        source_use_t su;
        deltacmd_source_t *srccmd;
        su.start = start; su.end = end; su.src1 = su.src2 = NULL;
        if (_lastcopy > start) {
            for (iterator iter = begin(); iter != this->end(); ++iter) {
                srccmd = dynamic_cast<deltacmd_source_t *>(*iter);
                if (srccmd == NULL) continue;
                srccmd->merge(su);
                su.insert(this, iter);
                if (su.start == su.end) break;
            }
        }
        assert(end == su.end);
        if (start < su.start) push_back(new deltacmd_use_t(start, su.start));
        if (su.start < su.end) {
            assert(_lastcopy <= su.start);
            push_back(new deltacmd_source_t(su.start, su.end));
            _lastcopy = su.end;
        }
    }
    void
    deltalist_t::_issue_literal(uint64_t size)
    {   push_back(new deltacmd_literal_t(_param1)); }
    void
    deltalist_t::_calldelta1(node_t *joint)
    {
        deltacmd_source_t *srccmd;
        for (iterator iter = begin(); iter != end(); ++iter) {
            srccmd = dynamic_cast<deltacmd_source_t *>(*iter);
            if (srccmd) srccmd->alloc();
        }
        delta1(joint);
    }
    void
    deltalist_t::_jload(const rapidjson::Value &jdesc)
    {
        const char *ctype = jdesc[0].GetString();
        uint64_t param1 = jdesc[1].GetUint64();
        if (!strcmp(ctype, "literal")) push_back(new deltacmd_literal_t(param1));
        else if (!strcmp(ctype, "copy")) _issue_copy(param1, param1 + jdesc[2].GetUint64());
        else assert(0); // others are not supported now.
    }
}
