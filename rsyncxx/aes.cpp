#include <algorithm>
#include <typeinfo>
#include "rsyncxx/aes.hpp"

// THE MIGRATE IS NOT FINISHED YET.
namespace rsync {
    buf_t *
    aesencrypt_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
#if 0
        ptrdiff_t nbytes;
        uint8_t *ilast = _ibuf + AES_BLOCK_SIZE;
        uint8_t *olast = _obuf + AES_BLOCK_SIZE;
#endif
#if 0
        fprintf(stderr, "%s(%p): status(%s), gm(%s), obuf(%u), ibuf(%u)\n",
                typeid(*this).name(), this, status_str(),
                _genmark ? "true": "false",
                (unsigned)(olast - _ocur), (unsigned)(_icur - _ibuf));
#endif
#if 0
        // writeout if pending output in _obuf.
        if (_ocur < olast) {
            assert(_ibuf == _icur);
            nbytes = std::min(olast - _ocur, nextwbytes());
            if (nbytes > 0) _ocur = writenext(_ocur, nbytes);
            if (_ocur == olast && _genmark) set_next_eos(join);
            return false;
        }
        assert(_ocur == olast);
        // generate the last mark block if required.
        if (status() == rss_eos && rbytes() == 0 && !_genmark) {
            assert(_icur < ilast);
            memset(_icur, (uint8_t)(_icur - _ibuf), ilast - _icur);
            _icur = ilast;
            _genmark = true;
        }
        // try to fill _ibuf if some pending data in _ibuf.
        if (_ibuf < _icur) {
            // consume input if possible.
            nbytes = std::min(ilast - _icur, rbytes());
            if (nbytes > 0) _icur = read(_icur, nbytes);
            if (_icur < ilast) return false;
            // encrypt into _obuf whenever _ibuf is full.
            _icur = _ibuf; _ocur = _obuf;
            AES_cbc_encrypt(_ibuf, _obuf, AES_BLOCK_SIZE,
                            &_key, _ivec, AES_ENCRYPT);
            return true;
        }
        assert(_ibuf == _icur);
        // encrypt data by block from self to next directly for speed up.
        nbytes = std::min(rbytes(), nextwbytes());
        nbytes -= nbytes % AES_BLOCK_SIZE;
        if (nbytes > 0) {
            AES_cbc_encrypt(space(), nextspace(), nbytes,
                            &_key, _ivec, AES_ENCRYPT);
            fwd(nbytes); fwdnext(nbytes);
        }
        // save the remaining bytes into _ibuf.
        if (status() != rss_eos && rbytes() == 0) return false;
        _icur = read(_icur, rbytes());
        return true;
#endif
        return node_t::step(done, bobj, lastbuf, joint);
    }
    buf_t *
    aesdecrypt_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
#if 0
        ptrdiff_t nbytes;
        uint8_t *ilast = _ibuf + AES_BLOCK_SIZE;
        uint8_t *olast0, *olast = _obuf + AES_BLOCK_SIZE;
#endif
#if 0
        fprintf(stderr, "%s(%p): status(%s), obuf(%u), ibuf(%u)\n",
                typeid(*this).name(), this, status_str(),
                (unsigned)(olast - _ocur), (unsigned)(_icur - _ibuf));
#endif
#if 0
        // writeout if pending output in _obuf;
        if (_ocur < olast) {
            assert(_ibuf == _icur);
            olast0 = status() == rss_eos && rbytes() == 0 ?
                _obuf + olast[-1]: olast;
            nbytes = std::min(olast0 - _ocur, nextwbytes());
            if (nbytes > 0) _ocur = writenext(_ocur, nbytes);
            if (status() == rss_eos && rbytes() == 0) {
                _ocur = olast;
                set_next_eos(join);
            }
            return false;
        }
        assert(_ocur == olast);
        // try to fill _ibuf if some pending data in _ibuf.
        if (_ibuf < _icur) {
            // consume input if possible.
            nbytes = std::min(ilast - _icur, rbytes());
            if (nbytes > 0) _icur = read(_icur, nbytes);
            if (_icur < ilast) {
                if (status() == rss_eos)
                    fprintf(stderr, "cipher size error.\n");
                return false;
            }
            // decrypt into _obuf whenever _ibuf is full.
            _icur = _ibuf; _ocur = _obuf;
            AES_cbc_encrypt(_ibuf, _obuf, AES_BLOCK_SIZE,
                            &_key, _ivec, AES_DECRYPT);
            return true;
        }
        assert(_ibuf == _icur);
        // decrypt data by block from self to next directly for speed up.
        nbytes = std::min(rbytes(), nextwbytes());
        nbytes -= nbytes % AES_BLOCK_SIZE;
        if (nbytes > 0) {
            AES_cbc_encrypt(space(), nextspace(), nbytes,
                            &_key, _ivec, AES_DECRYPT);
            fwd(nbytes);
            if (status() != rss_eos || rbytes() > 0) fwdnext(nbytes);
            else {
                fwdnext(nbytes + nextspace()[nbytes - 1] - AES_BLOCK_SIZE);
                set_next_eos(join);
                return false;
            }
        }
        if (rbytes() == 0) return false;
        // save the remaining bytes into _ibuf;
        _icur = read(_icur, rbytes());
        return true;
#endif
        return node_t::step(done, bobj, lastbuf, joint);
    }
}
