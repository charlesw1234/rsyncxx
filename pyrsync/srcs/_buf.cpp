#include "_buf.hpp"

namespace rsync {
    Py_ssize_t
    pybuf_length(PyWrap_t *self)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        return ptself->buf1 - ptself->buf0;
    }
    static PyObject_t *
    pybuf_concat(PyWrap_t *self, PyObject_t *other)
    {
        PyErr_SetString(PyExc_NotImplementedError,
                        "concat is not implemented yet.");
        return NULL;
    }
    static PyObject_t *
    pybuf_repeat(PyWrap_t *self, Py_ssize_t count)
    {
        PyErr_SetString(PyExc_NotImplementedError,
                        "repeat is not implemented yet.");
        return NULL;
    }
    static PyObject_t *
    pybuf_item(PyWrap_t *self, Py_ssize_t idx)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        if (idx < 0 || idx >= ptself->buf1 - ptself->buf0) {
            PyErr_SetString(PyExc_IndexError, "buffer index out of range");
            return NULL;
        }
        return PyString_FromStringAndSize((char *)ptself->buf0 + idx, 1);
    }
    static PyObject_t *
    pybuf_slice(PyWrap_t *self, Py_ssize_t left, Py_ssize_t right)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        ptrdiff_t szbuf = ptself->buf1 - ptself->buf0;
        if (left < 0) left = 0;
        else if (left > szbuf) left = szbuf;
        if (right < 0) right = 0;
        else if (right > szbuf) right = szbuf;
        if (right < left) right = left;
        return PyString_FromStringAndSize((char *)ptself->buf0 + left,
                                          right - left);
    }
    static int
    pybuf_ass_item(PyWrap_t *self, Py_ssize_t idx, PyObject_t *other)
    {
        PyErr_SetString(PyExc_TypeError, "buf is read-only.");
        return -1;
    }
    static int
    pybuf_ass_slice(PyWrap_t *self, Py_ssize_t left, Py_ssize_t right,
                    PyObject_t *other)
    {
        PyErr_SetString(PyExc_TypeError, "buf is read-only.");
        return -1;
    }
#if 0
    static PyObject_t *
    pybuf_subscript(PyWrap_t *self, PyObject_t *item)
    {
        buf_t *ptself = dynamic_cast<buf_t *>(self->cxxobj);
        assert(ptself != NULL);
        ptrdiff_t szbuf = ptself->buf1 - ptself->buf0;
        if (PyIndex_Check(item)) {
            Py_ssize_t idx = PyNumber_AsSsize_t(item, PyExc_IndexError);
            if (idx == -1 && PyErr_Occurred()) return NULL;
            if (idx < 0) idx += szbuf;
            return pybuf_item(self, idx);
        } else if (PySlice_Check(item)) {
            Py_ssize_t start, stop, step, slicelength;
            if (PySlice_GetIndicesEx((PySliceObject *)item, szbuf,
                                     &start, &stop, &step, &slicelength) < 0)
                return NULL;
            if (slicelength <= 0) return PyString_FromStringAndSize("", 0);
            else if (step == 1)
                return PyString_FromStringAndSize((char *)ptself->buf0 + start,
                                                  stop - start);
            else {
                PyErr_SetString(PyExc_IndexError, "step must be 1.");
                return NULL;
            }
        }
        PyErr_SetString(PyExc_TypeError, "sequence index must be integer.");
        return NULL;
    }
    static int
    pybuf_ass_subscript(PyWrap_t *self, PyObject_t *item, PyObject_t *value)
    {
        PyErr_SetString(PyExc_TypeError, "buf is read-only.");
        return -1;
    }
#endif
    static PyObject_t *
    pybuf_str(PyWrap_t *self)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        return PyString_FromStringAndSize((const char *)ptself->buf0,
                                          ptself->buf1 - ptself->buf0);
    }
    static Py_ssize_t
    pybuf_buffer_getreadbuf(PyWrap_t *self, Py_ssize_t index, const void **ptr)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        if (index != 0) {
            PyErr_SetString(PyExc_SystemError,
                            "accessing non-existent buf seg");
            return -1;
        }
        *ptr = (void *)ptself->buf0;
        return ptself->buf1 - ptself->buf0;
    }
    static Py_ssize_t
    pybuf_buffer_getwritebuf(PyWrap_t *self,
                             Py_ssize_t index, const void **ptr)
    {
        PyErr_SetString(PyExc_TypeError,
                        "Cannot use buf as modifiable buffer");
        return -1;
    }
    static Py_ssize_t
    pybuf_buffer_getsegcount(PyWrap_t *self, Py_ssize_t *lenp)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        if (lenp) *lenp = ptself->buf1 - ptself->buf0;
        return 1;
    }
    static Py_ssize_t
    pybuf_buffer_getcharbuf(PyWrap_t *self, Py_ssize_t index, const char **ptr)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        if (index != 0) {
            PyErr_SetString(PyExc_SystemError,
                            "accessing non-existent buf seg");
            return -1;
        }
        *ptr = (const char *)ptself->buf0;
        return ptself->buf1 - ptself->buf0;
    }
    static int
    pybuf_buffer_getbuffer(PyWrap_t *self, Py_buffer *view, int flags)
    {
        buf_t *ptself = PY2CXX(buf_t, self);
        assert(ptself != NULL);
        return PyBuffer_FillInfo(view, (PyObject_t *)self,
                                 (void *)ptself->buf0,
                                 ptself->buf1 - ptself->buf0, 1, flags);
    }

    static PySequenceMethods pybuf_as_sequence = {
        (lenfunc)pybuf_length,
        (binaryfunc)pybuf_concat,
        (ssizeargfunc)pybuf_repeat,
        (ssizeargfunc)pybuf_item,
        (ssizessizeargfunc)pybuf_slice,
        (ssizeobjargproc)pybuf_ass_item,
        (ssizessizeobjargproc)pybuf_ass_slice
    };
#if 0
    static PyMappingMethods pybuf_as_mapping = {
        (lenfunc)pybuf_length,
        (binaryfunc)pybuf_subscript,
        (objobjargproc)pybuf_ass_subscript
    };
#endif
    static PyBufferProcs pybuf_as_buffer = {
        (readbufferproc)pybuf_buffer_getreadbuf,
        (writebufferproc)pybuf_buffer_getwritebuf,
        (segcountproc)pybuf_buffer_getsegcount,
        (charbufferproc)pybuf_buffer_getcharbuf,
        (getbufferproc)pybuf_buffer_getbuffer,
        0,
    };
    static PyMethodDef_t pybuf_methods[] = {
        { NULL, NULL, 0, NULL }
    };
    class pybuftype_t: public pytypeobject_t {
    public:
        pybuftype_t(void): pytypeobject_t(NULL, "_rsync.buf", pybuf_methods)
        {   set_as_sequence(&pybuf_as_sequence);
            //set_as_mapping(&pybuf_as_mapping);
            set_str((reprfunc)pybuf_str);
            set_as_buffer(&pybuf_as_buffer); }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<buf_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   PyObject_t *pyinobj = NULL;
            if (!PyArg_ParseTuple(args, "O", &pyinobj)) return NULL;
            return new pybuf_t(pyinobj); }
    };
    static pybuftype_t pybuftypeobj;
    pytypeobject_t *pybuftype = &pybuftypeobj;
}

int
init_buf(PyObject_t *mod)
{
    if (rsync::pybuftypeobj.install(mod) < 0) return -1;
    return 0;
}
