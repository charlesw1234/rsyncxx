#pragma once

#include "rsyncxx/node.hpp"

namespace rsync {
    class DLLEXPORT signature_t: public combiner_t {
    public:
        signature_t(size_t szblock, size_t nsigns, node_t *next);
        virtual ~signature_t() {}

        virtual void push_file(const char *fpath, node_t *node);
        virtual void push_end(void);
    private:
        size_t _szblock, _nsigns;
    };
}
