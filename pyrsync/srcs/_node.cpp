#include "_node.hpp"
#include <vector>
#include "_buf.hpp"
#include "rsync/basic.hpp"
#include "rsync/node.hpp"

namespace rsync {
    static inline int
    PyFile_CheckEx(PyObject_t *pyobj, int order)
    {
        if (PyFile_Check(pyobj)) return 1;
        PyErr_Format(PyExc_TypeError, "wrong type %s #%d, expect PyFile.\n",
                     Py_TYPE(pyobj)->tp_name, order);
        return 0;
    }

    /*--- node ---*/
    static PyObject_t *
    pynode_run(PyObject_t *self, PyObject_t *args)
    {
        unsigned szbuf;
        bool done = false;
        buf_t *bobj = NULL;
        node_t *node = PY2CXX(node_t, self);
        if (!PyArg_ParseTuple(args, "I", &szbuf)) return NULL;
        while (!done)
            bobj = node->step(done, reuse(bobj, szbuf));
        if (bobj) bobj->unref();
        Py_INCREF(Py_None);
        return Py_None;
    }
    static PyMethodDef_t pynode_methods[] = {
        { "run", (PyCFunction_t)pynode_run, METH_VARARGS },
        { NULL, NULL, 0, NULL }
    };
    static pynodetype_t pynodetypeobj(NULL, "_rsync.node", pynode_methods);
    pytypeobject_t *pynodetype = &pynodetypeobj;

    /*--- source ---*/
    class pysource_t: public node_t {
    public:
        pysource_t(PyObject_t *body, node_t *next): node_t(next)
        {   Py_INCREF(_body = body); }
        virtual ~pysource_t() { if (_body) Py_DECREF(_body); }

        virtual buf_t *
        step(bool &done, buf_t *bobj,
             bool lastbuf = true, node_t *joint = NULL);
    private:
        PyObject_t *_body;
    };
    buf_t *
    pysource_t::step(bool &done, buf_t *bobj, bool lastbuf, node_t *joint)
    {
        buf_t *_bobj;
        Py_ssize_t pos;
        bool done0 = false;
        if (_body) {
            if (PyString_Check(_body)) {
                _bobj = new pybuf_t(_body);
                _bobj = node_t::step(done0, _bobj, false, joint);
                if (_bobj) _bobj->unref();
                Py_DECREF(_body); _body = NULL;
            } else if (PyTuple_Check(_body)) {
                for (pos = 0; pos < PyTuple_Size(_body); ++pos) {
                    _bobj = new pybuf_t(PyTuple_GetItem(_body, pos));
                    _bobj = node_t::step(done0, _bobj, false, joint);
                    if (_bobj) _bobj->unref();
                }
                Py_DECREF(_body); _body = NULL;
            } else if (PyList_Check(_body)) {
                for (pos = 0; pos < PyList_Size(_body); ++pos) {
                    _bobj = new pybuf_t(PyList_GetItem(_body, pos));
                    _bobj = node_t::step(done0, _bobj, false, joint);
                    if (_bobj) _bobj->unref();
                }
                Py_DECREF(_body); _body = NULL;
            }
        }
        bobj = node_t::step(done, bobj, lastbuf, joint);
        done = lastbuf;
        return bobj;
    }
    class pysourcetype_t: public pynodetype_t {
    public:
        pysourcetype_t(void):
            pynodetype_t(pynodetype, "_rsync.source", NULL) {}
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<pysource_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   node_t *nextobj; PyObject_t *pybody, *pynext = NULL;
            if (!PyArg_ParseTuple(args, "O|O", &pybody, &pynext)) return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new pysource_t(pybody, nextobj); }
    };
    static pysourcetype_t pysourcetypeobj;
    pytypeobject_t *pysourcetype = &pysourcetypeobj;

    /*--- sink ---*/
    class pysink_t: public node_t {
    public:
        pysink_t(ptrdiff_t szpystr): node_t(NULL) { _szpystr = szpystr; }
        virtual ~pysink_t()
        {   for (Py_ssize_t idx = 0; idx < npyobjs(); ++idx)
                Py_DECREF((PyObject_t *)_pyobjs[idx]); }
        virtual buf_t *
        step(bool &done, buf_t *bobj,
             bool lastbuf = true, node_t *joint = NULL)
        {   ptrdiff_t nbytes; PyStringObject_t *pystr;
            done = lastbuf;
            while (bobj->buf0 < bobj->buf1) {
                if (_pyobjs.empty() || _pyobjs.back()->ob_size == _szpystr) {
                    pystr = (PyStringObject_t *)
                        PyString_FromStringAndSize(NULL, _szpystr);
                    pystr->ob_size = 0;
                    _pyobjs.push_back(pystr);
                }
                pystr = _pyobjs.back();
                nbytes = std::min(bobj->buf1 - bobj->buf0,
                                  _szpystr - pystr->ob_size);
                memcpy(pystr->ob_sval + pystr->ob_size, bobj->buf0, nbytes);
                pystr->ob_size += nbytes;
                bobj->buf0 += nbytes;
            }
            return bobj; }

        inline Py_ssize_t npyobjs(void) const
        {   return (Py_ssize_t)_pyobjs.size(); }
        inline PyObject_t *get(Py_ssize_t idx)
        { return (PyObject_t *)_pyobjs[idx]; }
    private:
        ptrdiff_t _szpystr;
        std::vector<PyStringObject_t *> _pyobjs;
    };
    static Py_ssize_t
    pysink_length(PyWrap_t *self)
    {
        pysink_t *ptself = PY2CXX(pysink_t, self);
        assert(ptself != NULL);
        return ptself->npyobjs();
    }
    static PyObject_t *
    pysink_concat(PyWrap_t *self, PyObject_t *other)
    {
        PyErr_SetString(PyExc_NotImplementedError,
                        "concat is not implemented yet.");
        return NULL;
    }
    static PyObject_t *
    pysink_repeat(PyWrap_t *self, Py_ssize_t count)
    {
        PyErr_SetString(PyExc_NotImplementedError,
                        "repeat is not implemented yet.");
        return NULL;
    }
    static PyObject_t *
    pysink_item(PyWrap_t *self, Py_ssize_t idx)
    {
        PyObject_t *pybuf;
        pysink_t *ptself = PY2CXX(pysink_t, self);
        assert(ptself != NULL);
        if (idx < 0 || idx >= ptself->npyobjs()) {
            PyErr_SetString(PyExc_IndexError, "buffer index out of range");
            return NULL;
        }
        Py_INCREF(pybuf = ptself->get(idx));
        return pybuf;
    }
    static PyObject_t *
    pysink_slice(PyWrap_t *self, Py_ssize_t left, Py_ssize_t right)
    {
        PyObject_t *pybuf, *pylist;
        pysink_t *ptself = PY2CXX(pysink_t, self);
        assert(ptself != NULL);
        Py_ssize_t idx, npyobjs = ptself->npyobjs();
        if (left < 0) left = 0;
        else if (left > npyobjs) left = npyobjs;
        if (right < 0) right = 0;
        else if (right > npyobjs) right = npyobjs;
        if (right < left) right = left;
        pylist = PyList_New(right - left);
        for (idx = left; idx < right; ++idx) {
            Py_INCREF(pybuf = ptself->get(idx - left));
            PyList_SetItem(pylist, idx, pybuf);
        }
        return pylist;
    }
    static int
    pysink_ass_item(PyWrap_t *self, Py_ssize_t idx, PyObject_t *other)
    {
        PyErr_SetString(PyExc_TypeError, "sink is read-only.");
        return -1;
    }
    static int
    pysink_ass_slice(PyWrap_t *self, Py_ssize_t left, Py_ssize_t right,
                     PyObject_t *other)
    {
        PyErr_SetString(PyExc_TypeError, "sink is read-only.");
        return -1;
    }
#if 0
    static PyObject_t *
    pysink_subscript(PyWrap_t *self, PyObject_t *item)
    {
        pysink_t *ptself = dynamic_cast<pysink_t *>(self->cxxobj);
        assert(ptself != NULL);
        if (PyIndex_Check(item)) {
            Py_ssize_t idx = PyNumber_AsSsize_t(item, PyExc_IndexError);
            if (idx == -1 && PyErr_Occurred()) return NULL;
            if (idx < 0) idx += ptself->npyobjs();
            return pysink_item(self, idx);
        } else if (PySlice_Check(item)) {
            Py_ssize_t start, stop, step, slicelength;
            if (PySlice_GetIndicesEx((PySliceObject *)item, ptself->npyobjs(),
                                     &start, &stop, &step, &slicelength) < 0)
                return NULL;
            if (slicelength <= 0) return PyList_New(0);
            else {
                assert(step > 0);
                Py_ssize_t idx, idxmax = (stop - start + step - 1) / step;
                PyObject_t *pybuf, *pylist = PyList_New(idxmax);
                for (idx = 0; idx < idxmax; ++idx) {
                    assert(start + idx * step < stop);
                    Py_INCREF(pybuf = ptself->get(start + idx * step));
                    PyList_SetItem(pylist, idx, pybuf);
                }
                return pylist;
            }
        }
        PyErr_SetString(PyExc_TypeError, "sequence index must be integer.");
        return NULL;
    }
    static int
    pysink_ass_subscript(PyWrap_t *self, PyObject_t *item, PyObject_t *value)
    {
        PyErr_SetString(PyExc_TypeError, "sink is read-only.");
        return -1;
    }
#endif
    static PySequenceMethods pysink_as_sequence = {
        (lenfunc)pysink_length,
        (binaryfunc)pysink_concat,
        (ssizeargfunc)pysink_repeat,
        (ssizeargfunc)pysink_item,
        (ssizessizeargfunc)pysink_slice,
        (ssizeobjargproc)pysink_ass_item,
        (ssizessizeobjargproc)pysink_ass_slice
    };
#if 0
    static PyMappingMethods pysink_as_mapping = {
        (lenfunc)pysink_length,
        (binaryfunc)pysink_subscript,
        (objobjargproc)pysink_ass_subscript
    };
#endif
    static PyObject_t *
    pysink_get_len(PyWrap_t *self)
    {
        PyStringObject_t *str0;
        pysink_t *ptself = PY2CXX(pysink_t, self);
        Py_ssize_t total = 0, idx, maxidx = ptself->npyobjs();
        for (idx = 0; idx < maxidx; ++idx) {
            str0 = (PyStringObject_t *)ptself->get(idx);
            total += str0->ob_size;
        }
        return PyInt_FromLong(total);
    }
    static PyObject_t *
    pysink_get_string(PyWrap_t *self)
    {
        char *cur;
        PyStringObject_t *str, *str0;
        pysink_t *ptself = PY2CXX(pysink_t, self);
        Py_ssize_t total = 0, idx, maxidx = ptself->npyobjs();
        for (idx = 0; idx < maxidx; ++idx) {
            str0 = (PyStringObject_t *)ptself->get(idx);
            total += str0->ob_size;
        }
        str = (PyStringObject_t *)PyString_FromStringAndSize(NULL, total);
        cur = str->ob_sval;
        for (idx = 0; idx < maxidx; ++idx) {
            str0 = (PyStringObject_t *)ptself->get(idx);
            memcpy(cur, str0->ob_sval, str0->ob_size);
            cur += str0->ob_size;
        }
        return (PyObject_t *)str;
    }
    static PyObject_t *
    pysink_get_strlist(PyWrap_t *self)
    {
        pysink_t *ptself = PY2CXX(pysink_t, self);
        Py_ssize_t idx, maxidx = ptself->npyobjs();
        PyObject_t *str, *strlist = PyList_New(maxidx);
        for (idx = 0; idx < maxidx; ++idx) {
            str = ptself->get(idx);
            Py_INCREF(str);
            PyList_SetItem(strlist, idx, str);
        }
        return strlist;
    }
    static PyMethodDef_t pysink_methods[] = {
        { "get_len", (PyCFunction_t)pysink_get_len, METH_NOARGS },
        { "get_string", (PyCFunction_t)pysink_get_string, METH_NOARGS },
        { "get_strlist", (PyCFunction_t)pysink_get_strlist, METH_NOARGS },
        { NULL, NULL, 0, NULL }
    };
    class pysinktype_t: public pynodetype_t {
    public:
        pysinktype_t(void):
            pynodetype_t(pynodetype, "_rsync.sink", pysink_methods)
        {   set_as_sequence(&pysink_as_sequence); }
        //set_as_mapping(&pysink_as_mapping); }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<pysink_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   unsigned szpystr;
            if (!PyArg_ParseTuple(args, "I", &szpystr)) return NULL;
            return new pysink_t(szpystr); }
    };
    static pysinktype_t pysinktypeobj;
    pytypeobject_t *pysinktype = &pysinktypeobj;

    /* infile */
    class pyinfile_t: public infile_t {
    public:
        pyinfile_t(PyObject_t *rfobj, node_t *next):
            infile_t(PyFile_AsFile(rfobj), next, false)
        {   _rfobj = rfobj; Py_INCREF(_rfobj); }

        virtual ~pyinfile_t() { Py_DECREF(_rfobj); _rfobj = NULL; }
    private:
        PyObject_t *_rfobj;
    };

    class pyinfiletype_t: public pynodetype_t {
    public:
        pyinfiletype_t(void):
            pynodetype_t(pynodetype, "_rsync.infile", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<pyinfile_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   node_t *nextobj; PyObject_t *rfp_obj, *pynext = NULL;
            if (!PyArg_ParseTuple(args, "O|O", &rfp_obj, &pynext) ||
                !PyFile_CheckEx(rfp_obj, 1)) return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new pyinfile_t(rfp_obj, nextobj); }
    };
    static pyinfiletype_t pyinfiletypeobj;
    pytypeobject_t *pyinfiletype = &pyinfiletypeobj;

    /* outfile */
    class pyoutfile_t: public outfile_t {
    public:
        pyoutfile_t(PyObject_t *wfobj): outfile_t(PyFile_AsFile(wfobj))
        {   _wfobj = wfobj; Py_INCREF(_wfobj); }
    protected:
        virtual void closef(void) { Py_DECREF(_wfobj); _wfobj = NULL; }
    private:
        PyObject_t *_wfobj;
    };

    class pyoutfiletype_t: public pynodetype_t {
    public:
        pyoutfiletype_t(void):
            pynodetype_t(pynodetype, "_rsync.outfile", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<outfile_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   PyObject_t *wfpobj;
            if (!PyArg_ParseTuple(args, "O", &wfpobj) ||
                !PyFile_CheckEx(wfpobj, 1)) return NULL;
            return new pyoutfile_t(wfpobj); }
    };
    static pyoutfiletype_t pyoutfiletypeobj;
    pytypeobject_t *pyoutfiletype = &pyoutfiletypeobj;

    /* gzinfile */
    class pygzinfiletype_t: public pynodetype_t {
    public:
        pygzinfiletype_t(void):
            pynodetype_t(pynodetype, "_rsync.gzinfile", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<gzinfile_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   char *fpath; node_t *nextobj; PyObject_t *pynext = NULL;
            if (!PyArg_ParseTuple(args, "s|O", &fpath, &pynext)) return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new gzinfile_t(fpath, nextobj); }
    };
    static pygzinfiletype_t pygzinfiletypeobj;
    pytypeobject_t *pygzinfiletype = &pygzinfiletypeobj;

    /* gzoutfile */
    class pygzoutfiletype_t: public pynodetype_t {
    public:
        pygzoutfiletype_t(void):
            pynodetype_t(pynodetype, "_rsync.gzoutfile", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<gzoutfile_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   char *fpath;
            if (!PyArg_ParseTuple(args, "s", &fpath)) return NULL;
            return new gzoutfile_t(fpath); }
    };
    static pygzoutfiletype_t pygzoutfiletypeobj;
    pytypeobject_t *pygzoutfiletype = &pygzoutfiletypeobj;

    /* compresser */
    class pycompressertype_t: public pynodetype_t {
    public:
        pycompressertype_t(void):
            pynodetype_t(pynodetype, "_rsync.compresser", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<compresser_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   ptrdiff_t bufsz; int level; node_t *nextobj;
            PyObject_t *pynext = NULL;
            if (!PyArg_ParseTuple(args, "ni|O", &bufsz, &level, &pynext))
                return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new compresser_t(bufsz, level, nextobj); }
    };
    static pycompressertype_t pycompressertypeobj;
    pytypeobject_t *pycompressertype = &pycompressertypeobj;

    /* uncompresser */
    class pyuncompressertype_t: public pynodetype_t {
    public:
        pyuncompressertype_t(void):
            pynodetype_t(pynodetype, "_rsync.uncompresser", NULL) { }
        virtual refobj_t *cxxcheck(refobj_t *cxxobj) const
        {   return dynamic_cast<uncompresser_t *>(cxxobj); }
        virtual refobj_t *
        cxxnew(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
        {   ptrdiff_t bufsz; node_t *nextobj; PyObject_t *pynext = NULL;
            if (!PyArg_ParseTuple(args, "n|O", &bufsz, &pynext)) return NULL;
            if (!getnext(pynext, &nextobj)) return NULL;
            return new uncompresser_t(bufsz, nextobj); }
    };
    static pyuncompressertype_t pyuncompressertypeobj;
    pytypeobject_t *pyuncompressertype = &pyuncompressertypeobj;
}

extern "C" int
init_node(PyObject_t *mod)
{
    if (rsync::pynodetypeobj.install(mod) < 0) return -1;
    if (rsync::pysourcetypeobj.install(mod) < 0) return -1;
    if (rsync::pysinktypeobj.install(mod) < 0) return -1;
    if (rsync::pyinfiletypeobj.install(mod) < 0) return -1;
    if (rsync::pyoutfiletypeobj.install(mod) < 0) return -1;
    if (rsync::pygzinfiletypeobj.install(mod) < 0) return -1;
    if (rsync::pygzoutfiletypeobj.install(mod) < 0) return -1;
    if (rsync::pycompressertypeobj.install(mod) < 0) return -1;
    if (rsync::pyuncompressertypeobj.install(mod) < 0) return -1;
    return 0;
}
