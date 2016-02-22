#pragma once

#include <stdlib.h>
#include "defs.h"
#include "rsyncxx/blake2.h"
#include "rsyncxx/node.hpp"

#define ROLLSUM_CHAR_OFFSET 31
namespace rsync {
    typedef uint32_t weak_sum_t;
    typedef uint8_t strong_sum_t[32];
    class strong_signer_t {
    public:
        inline void init(size_t sz) { blake2b_init(&_ctx, sz); }
        inline void update(const uint8_t *buf, size_t szbuf)
        {   blake2b_update(&_ctx, buf, szbuf); }
        inline void update(const uint8_t *buf0, const uint8_t *buf1)
        {   blake2b_update(&_ctx, buf0, buf1 - buf0); }
        inline void final(uint8_t *buf, size_t szbuf)
        {   blake2b_final(&_ctx, buf, szbuf); }
    private:
        blake2b_state _ctx;
    };
    struct sign_t {
        weak_sum_t weak;
        strong_sum_t strong;
    };

    class rollsum_t {
    public:
        uint64_t count;
        uint64_t s1, s2;

        inline void init(void) { count = s1 = s2 = 0; }
        void update(const uint8_t *buf, size_t szbuf);
        inline void rotate(uint8_t out, uint8_t in)
        {   s1 += in - out;
            s2 += s1 - count * (out + ROLLSUM_CHAR_OFFSET); }
        inline void rollin(uint8_t in)
        {   s1 += in + ROLLSUM_CHAR_OFFSET; s2 += s1; ++count; }
        inline void rollout(uint8_t out)
        {   s1 -= out + ROLLSUM_CHAR_OFFSET;
            s2 -= count * (out + ROLLSUM_CHAR_OFFSET); --count; }
        inline weak_sum_t digest(void) const
        {   return (uint32_t)((s2 << 16) | (s1 & 0xFFFF)); }
    };

    class rolledbuf_t: public buf_t {
    public:
        rolledbuf_t(size_t blocksz): buf_t(blocksz) { reset(); }
        virtual ~rolledbuf_t() {}

        inline rolledbuf_t *ref(void) { return (rolledbuf_t *)buf_t::ref(); }
        inline rolledbuf_t *unref(void) { return (rolledbuf_t *)buf_t::unref(); }

        // reset it, so it can be reused.
        inline void reset(void) { _full = false; _rollsum.init(); }

        inline bool full(void) const { return _full; }
        inline ptrdiff_t szfree(void) const
        {   return buf0 > buf1 ? buf0 - buf1:
                buf0 == buf1 && _full ? 0: (buflast - buf1) + (buf0 - buf); }
        inline ptrdiff_t szused(void) const
        {   return buf0 < buf1 ? buf1 - buf0:
                buf0 == buf1 && !_full ? 0: (buflast - buf0) + (buf1 - buf); }

        bool commit(const uint8_t *data, ptrdiff_t szdata);
        uint8_t rotate(uint8_t data);
        inline weak_sum_t weak(void) const { return _rollsum.digest(); }
        void strong(strong_sum_t &sum) const;

        // trim data in this, so it can be used as a buf_t to output data.
        inline rolledbuf_t *output(void)
        {   if (buf0 < buf1); // done.
            else if (buf0 == buf1 && !_full) { unref(); return NULL; }
            else if (buf == buf1) buf1 = buflast;
            else _rollback();
            return this; }
    private:
        bool _full;
        rollsum_t _rollsum;
        void _rollback(void);
    };
}
