#pragma once

#include <openssl/aes.h>
#include "rsyncxx/node.hpp"

namespace rsync {
    class aesencrypt_t: public node_t {
    public:
        aesencrypt_t(const uint8_t *usrkey, unsigned bits, const uint8_t *ivec,
                     node_t *next): node_t(next)
        {   _icur = _ibuf; _ocur = _obuf + AES_BLOCK_SIZE;
            memcpy(_ivec, ivec, AES_BLOCK_SIZE);
            AES_set_encrypt_key(usrkey, bits, &_key);
            _genmark = false; }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        uint8_t _ivec[AES_BLOCK_SIZE];
        uint8_t _ibuf[AES_BLOCK_SIZE];
        uint8_t _obuf[AES_BLOCK_SIZE];
        uint8_t *_icur, *_ocur;
        AES_KEY _key;
        bool _genmark;
    };
    class aesdecrypt_t: public node_t {
    public:
        aesdecrypt_t(const uint8_t *usrkey, unsigned bits, const uint8_t *ivec,
                     node_t *next): node_t(next)
        {   _icur = _ibuf; _ocur = _obuf + AES_BLOCK_SIZE;
            memcpy(_ivec, ivec, AES_BLOCK_SIZE);
            AES_set_decrypt_key(usrkey, bits, &_key); }
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    private:
        uint8_t _ivec[AES_BLOCK_SIZE];
        uint8_t _ibuf[AES_BLOCK_SIZE];
        uint8_t _obuf[AES_BLOCK_SIZE];
        uint8_t *_icur, *_ocur;
        AES_KEY _key;
    };
}
