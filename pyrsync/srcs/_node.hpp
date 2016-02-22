#pragma once
#include "pylibdefs.hpp"
#include "_wrap.hpp"
#include "rsync/node.hpp"

namespace rsync {
    class pynodetype_t: public pytypeobject_t {
    public:
        pynodetype_t(pytypeobject_t *base,
                     const char *name, PyMethodDef_t *methods):
            pytypeobject_t(base, name, methods) {}
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<node_t *>(cxxobj); }

        bool getnext(PyObject_t *pynext, node_t **nextobj);
    };
    extern pytypeobject_t *pynodetype;
    extern pytypeobject_t *pysourcetype;
    extern pytypeobject_t *pysinktype;

    inline bool
    pynodetype_t::getnext(PyObject_t *pynext, node_t **nextobj)
    {
        *nextobj = NULL;
        if (pynext == NULL || pynext == Py_None) return true;
        else if (!pynodetype->pycheck0(pynext)) return false;
        *nextobj = NODE_REF(PY2CXX(node_t, pynext));
        return true;
    }
}

extern "C" int init_node(PyObject_t *mod);
