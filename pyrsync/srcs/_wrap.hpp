#pragma once
#include "pylibdefs.hpp"
#include "rsync/refobj.hpp"

namespace rsync {
    class buf_t;
    // use class xxx_t: public cxxdbg_t, public xxxbase_t {...};
    // so the call of constructor, destructor of xxx_t can be dumped.
    class cxxdbg_t {
    public:
        cxxdbg_t(void) SHOWCALL(this);
        virtual ~cxxdbg_t() SHOWCALL(this);
    };

    typedef struct {
        PyObject_HEAD
        refobj_t *cxxobj;
    }  PyWrap_t;

#define PY2CXX(TYPE, PYOBJ)                                     \
    (dynamic_cast<TYPE *>(((PyWrap_t *)(PYOBJ))->cxxobj))

#define PY2CXXN(TYPE, PYOBJ)                                    \
    ((PYOBJ) == NULL ? NULL : (PYOBJ) == Py_None ? NULL :       \
     (dynamic_cast<TYPE *>(((PyWrap_t *)(PYOBJ))->cxxobj)))

    class pytypeobject_t {
    public:
        pytypeobject_t(pytypeobject_t *base,
                       const char *name, PyMethodDef_t *methods);

        inline void set_as_sequence(PySequenceMethods *as_sequence)
        {   _body.tp_as_sequence = as_sequence; }
        inline void set_as_mapping(PyMappingMethods *as_mapping)
        {   _body.tp_as_mapping = as_mapping; }
        inline void set_str(reprfunc strfunc)
        {   _body.tp_str = strfunc; }
        inline void set_as_buffer(PyBufferProcs *as_buffer)
        {   _body.tp_as_buffer = as_buffer; }

        // check whether the provided c++ object is the current type.
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return cxxobj; }

        // override to create c++ object for the current type.
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   return new refobj_t(); }

        inline PyObject_t *pynew(void)
        {   return PyObject_New(PyObject_t, &_body); }
        inline int pycheckn(PyObject_t *self)
        {   if (self == NULL || self == Py_None) return 1;
            return PyObject_IsInstance(self, (PyObject_t *)&_body); }
        inline int pycheck0n(PyObject_t *self)
        {   if (self == NULL || self == Py_None) return 1;
            if (!PyObject_IsInstance(self, (PyObject_t *)&_body))
            {   PyErr_Format(PyExc_TypeError, "must be %s, not %s.",
                             _body.tp_name, self->ob_type->tp_name);
                return 0; }
            return 1; }
        inline int pycheck(PyObject_t *self)
        {   return PyObject_IsInstance(self, (PyObject_t *)&_body); }
        inline int pycheck0(PyObject_t *self)
        {   if (!PyObject_IsInstance(self, (PyObject_t *)&_body)) {
                PyErr_Format(PyExc_TypeError, "must be %s, not %s.",
                             _body.tp_name, self->ob_type->tp_name);
                return 0; }
            return 1; }

        // Called by PyXXX_Init to initialize with new created c++ object.
        int pyinit(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs);

        int install(PyObject_t *mod);
    private:
        PyTypeObject_t _body;
    };

    // return the wrap python object for the provided c++ object.
    // if the c++ object has wrapper, return it.
    // otherwise, create a new python wrapper for c++ object.
    PyObject_t *cxx2py(refobj_t *cxxobj);
}

extern "C" int init_wrap(PyObject_t *mod);
