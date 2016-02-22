#include "_rsyncnode.hpp"
#include "_node.hpp"
#include "rsync/signature.hpp"
#include "rsync/delta.hpp"
#include "rsync/patch.hpp"

namespace rsync {
    /* pycb_t */
    class pycb_t {
    public:
        pycb_t(PyObject_t *self) { _self = self; }
        ~pycb_t() {}
    protected:
        inline PyObject_t *_call(const char *member, const char *fmt, ...)
        {   va_list va;
            PyObject_t *args, *retval, *func = NULL;
            if (!(func = PyObject_GetAttrString(_self, member))) return NULL;
            if (fmt == NULL || !*fmt) args = PyTuple_New(0);
            else {
                va_start(va, fmt);
                args = Py_VaBuildValue(fmt, va);
                va_end(va);
            }
            retval = PyObject_Call(func, args, NULL);
            Py_DECREF(args);
            Py_DECREF(func);
            return retval;
        }
        inline node_t *_py2cxx(PyObject_t *pynode)
        {
            node_t *node = PY2CXXN(node_t, pynode);
            NODE_REF(node);
            Py_DECREF(pynode);
            return node;
        }
    private:
        PyObject_t *_self;
    };

    /* signature */
    static PyObject_t *
    pysign_push_file(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
    {
        char *fpath;
        PyObject_t *nodeobj;
        signature_t *sign = PY2CXX(signature_t, self);
        if (!PyArg_ParseTuple(args, "sO", &fpath, &nodeobj) ||
            !pynodetype->pycheck(nodeobj)) return NULL;
        sign->push_file(fpath, NODE_REF(PY2CXX(node_t, nodeobj)));
        Py_RETURN_NONE;
    }
    static PyObject_t *
    pysign_push_end(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
    {
        signature_t *sign = PY2CXX(signature_t, self);
        sign->push_end();
        Py_RETURN_NONE;
    }
    static PyMethodDef_t pysign_methods[] = {
        { "push_file", (PyCFunction_t)pysign_push_file, METH_VARARGS },
        { "push_end", (PyCFunction_t)pysign_push_end, METH_VARARGS },
        { NULL, NULL, 0, NULL }
    };
    class pysigntype_t: public pynodetype_t {
    public:
        pysigntype_t(void):
            pynodetype_t(pynodetype, "_rsync.signature", pysign_methods) {}
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<signature_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   size_t szblock, nsigns; node_t *nextobj; PyObject_t *pynext = NULL;
            if (!PyArg_ParseTuple(args, "nn|O", &szblock, &nsigns, &pynext))
                return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new signature_t(szblock, nsigns, nextobj); }
    };
    static pysigntype_t pysigntypeobj;
    pytypeobject_t *pysigntype = &pysigntypeobj;

    /* delta */
    class pydelta_t: public delta_t, public pycb_t {
    public:
        pydelta_t(PyObject_t *self, ptrdiff_t szdeltabuf,
                  ptrdiff_t szlitblock, node_t *next):
            node_t(next), delta_t(szdeltabuf, szlitblock, next), pycb_t(self)
        {}
    protected:
        virtual node_t *_make_in(const char *fpath)
        {
            PyObject_t *pynode = _call("make_in", "(s)", fpath);
            if (pynode != NULL) return _py2cxx(pynode);
            else if (!PyErr_Occurred()) return delta_t::_make_in(fpath);
            PyErr_Print();
            return NULL;
        }
    };
    class pydeltatype_t: public pynodetype_t {
    public:
        pydeltatype_t(void):
            pynodetype_t(pynodetype, "_rsync.delta", NULL) {}
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<delta_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   ptrdiff_t szdeltabuf, szlitblock; node_t *nextobj;
            PyObject_t *pynext = NULL;
            if (!PyArg_ParseTuple(args, "nn|O",
                                  &szdeltabuf, &szlitblock, &pynext))
                return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new pydelta_t(self, szdeltabuf, szlitblock, nextobj); }
    };
    static pydeltatype_t pydeltatypeobj;
    pytypeobject_t *pydeltatype = &pydeltatypeobj;

    /* patch */
    class pypatch_t: public patch_t, public pycb_t {
    public:
        pypatch_t(PyObject_t *self, ptrdiff_t patchbufsz):
            node_t(NULL), patch_t(patchbufsz), pycb_t(self) {}
    protected:
        virtual void unchanged(node_t *joint)
        {
            PyObject_t *pynode = _call("unchanged", "(s)", fpath());
            if (pynode != NULL) Py_DECREF(pynode);
            else if (!PyErr_Occurred()) return patch_t::unchanged(joint);
            PyErr_Print();
        }
        virtual node_t *_make_in(const char *fpath)
        {
            PyObject_t *pynode = _call("make_in", "(s)", fpath);
            if (pynode != NULL) return _py2cxx(pynode);
            else if (!PyErr_Occurred()) return patch_t::_make_in(fpath);
            PyErr_Print();
            return NULL;
        }
        virtual node_t *_make_out(const char *fpath)
        {
            PyObject_t *pynode = _call("make_out", "(s)", fpath);
            if (pynode != NULL) return _py2cxx(pynode);
            else if (!PyErr_Occurred()) return patch_t::_make_out(fpath);
            PyErr_Print();
            return NULL;
        }
    };
    class pypatchtype_t: public pynodetype_t {
    public:
        pypatchtype_t(void):
            pynodetype_t(pynodetype, "_rsync.patch", NULL) {}
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<patch_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   ptrdiff_t patchbufsz;
            if (!PyArg_ParseTuple(args, "n", &patchbufsz)) return NULL;
            return new pypatch_t(self, patchbufsz); }
    };
    static pypatchtype_t pypatchtypeobj;
    pytypeobject_t *pypatchtype = &pypatchtypeobj;
}

extern "C" int
init_rsyncnode(PyObject_t *mod)
{
    if (rsync::pysigntypeobj.install(mod) < 0) return -1;
    if (rsync::pydeltatypeobj.install(mod) < 0) return -1;
    if (rsync::pypatchtypeobj.install(mod) < 0) return -1;
    return 0;
}
