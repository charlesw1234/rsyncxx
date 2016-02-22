#pragma once
#include "pylibdefs.hpp"
#include "_wrap.hpp"
#include "rsync/node.hpp"

namespace rsync {
    class pybuf_t: public buf_t {
    public:
        pybuf_t(PyObject_t *body): buf_t()
        {
            Py_INCREF(_body = body);
            if (PyString_Check(body)) {
                bufstart = buf0 = (uint8_t *)PyString_AsString(body);
                buflast = buf1 = bufstart + PyString_Size(body);
            }
        }
        virtual ~pybuf_t() { if (_body) Py_DECREF(_body); }
    private:
        PyObject_t *_body;
    };

    class pytypeobject_t;
    extern pytypeobject_t *pybuftype;
    Py_ssize_t pybuf_length(PyWrap_t *self);
}

extern "C" int init_buf(PyObject_t *mod);
