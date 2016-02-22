#pragma once

#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "rsyncxx/node.hpp"

namespace rsync {
    struct cmd_spec_t {
        uint8_t kind, immed, szparam1, szparam2;
    };
    struct cmd_suite_t {
        uint8_t maxcmd;
        uint8_t maxcmdlen;
        unsigned entrance;
        const cmd_spec_t *specs;
        const char **graph;
        inline uint8_t paramsize(uint8_t cmd) const
        {   return specs[cmd].szparam1 + specs[cmd].szparam2; }
    };

    inline uint8_t getparamtype(uint64_t param)
    {   return ((param & 0xFFUL) == param) ? 0:
            ((param &0xFFFFUL) == param) ? 1:
            ((param & 0xFFFFFFFFUL) == param) ? 3: 4; }
    uint8_t *make_param(uint8_t *cur, uint64_t param, uint8_t paramtype);

    extern const uint8_t cstatus_cmd;
    extern const uint8_t cstatus_cmd_params;
    extern const uint8_t cstatus_cmd_body;
    class DLLEXPORT cmd_t: virtual public node_t {
    public:
        cmd_t(const cmd_suite_t *suite): node_t(NULL)
        {   _suite = suite; _pbufcur = _pbuf;
            _status = 0; _cmd = suite->entrance; }
        virtual ~cmd_t() {}
        virtual buf_t *step(bool &done, buf_t *bobj, bool lastbuf = true, node_t *joint = NULL);
    protected:
        uint8_t _status; // status == 0, 1 for command, > 1, for derivde class.
        uint8_t _cmd, _prevcmd;
        uint64_t _param1, _param2;

        // status in _status.
        virtual buf_t *runstream(buf_t *bobj, bool lastbuf, node_t *joint) = 0;

        // cmd in _cmd, _param1, _param2.
        virtual buf_t *runcmd(buf_t *bobj, bool lastbuf, node_t *joint) = 0;

        inline uint8_t _kind(uint8_t cmd) const { return _suite->specs[cmd].kind; }
    private:
        const cmd_suite_t *_suite;
        uint8_t _pbuf[16], *_pbufcur;
        buf_t *_parse_cmd(buf_t *bobj, bool lastbuf, node_t *joint);
        buf_t *_parse_cmd_params(buf_t *bobj, bool lastbuf, node_t *joint);
        const uint8_t *_parse_param(const uint8_t *cur, uint64_t &param, uint8_t szparam) const;
        inline bool _kc_next(void) const
        {   return _suite->graph[_kind(_prevcmd)][_kind(_cmd)] == 'X'; }
        inline bool _kc_exit(void) const
        {   return strchr(_suite->graph[_kind(_cmd)], 'X') == NULL; }
    };
}
