#include "_wrap.hpp"

namespace rsync {
    #define SZTYPEHASH  127
    class typehash_t {
    private:
        pytypeobject_t *arr0[SZTYPEHASH];
        PyTypeObject_t *arr1[SZTYPEHASH];
        inline unsigned _hash(PyTypeObject_t *pytype)
        {   return (unsigned)(((uint64_t)pytype) % SZTYPEHASH); }
        inline pytypeobject_t *_search(PyTypeObject_t *pytype)
        {   unsigned pos0 = _hash(pytype), pos1 = pos0;
            do {
                if (arr1[pos0] == pytype) return arr0[pos0];
                if (arr0[pos0] == NULL) break;
                if (++pos0 == SZTYPEHASH) pos0 = 0;
            } while (pos0 != pos1);
            return NULL; }
    public:
        typehash_t(void) {}
        ~typehash_t() {}

        void add(pytypeobject_t *cxxpytype, PyTypeObject_t *pytype);
        inline pytypeobject_t *get(PyTypeObject_t *pytype)
        {
            pytypeobject_t *cxxpytype;
            while (pytype != &PyType_Type) {
                if ((cxxpytype = _search(pytype))) return cxxpytype;
                pytype = pytype->tp_base;
            }
            return NULL;
        }
        inline pytypeobject_t *get(refobj_t *cxxobj)
        {
            for (unsigned pos = 0; pos < SZTYPEHASH; ++pos) {
                if (arr0[pos] == NULL) continue;
                if (arr0[pos]->cxxcheck(cxxobj) == NULL) continue;
                return arr0[pos];
            }
            return NULL;
        }
    };
    void
    typehash_t::add(pytypeobject_t *cxxpytype, PyTypeObject_t *pytype)
    {
        unsigned pos0 = _hash(pytype), pos1 = pos0;
        for (;;) {
            if (arr0[pos0] == NULL) break;
            if (pos0++ == SZTYPEHASH) pos0 = 0;
            if (!(pos0 != pos1)) assert(pos0 != pos1); // full.
        }
        arr0[pos0] = cxxpytype;
        arr1[pos0] = pytype;
    }
    static typehash_t typehash;

    static int
    PyWrap_Init(PyObject_t *self, PyObject_t *args, PyObject_t *kwargs)
    {
        pytypeobject_t *cxxpytype = typehash.get(self->ob_type);
#if 0
        fprintf(stderr, "%s(%u): %s(%p), refcnt = %u\n",
                __FUNCTION__, __LINE__,
                self->ob_type->tp_name, self, (unsigned)self->ob_refcnt);
#endif
        return cxxpytype->pyinit(self, args, kwargs);
    }
    static void PyWrap_Dealloc(PyObject_t *self)
    {
#if 0
        fprintf(stderr, "%s(%u): %s(%p), refcnt = %u\n",
                __FUNCTION__, __LINE__,
                self->ob_type->tp_name, self, (unsigned)self->ob_refcnt);
#endif
        refobj_t *refobj;
        if (self && (refobj = PY2CXX(refobj_t, self)))
            refobj->cleanwrap();
    }

    pytypeobject_t::pytypeobject_t(pytypeobject_t *base,
                                   const char *name, PyMethodDef_t *methods)
    {
        typehash.add(this, &_body);
        memset(&_body, 0, sizeof(_body));
        _body.tp_name = name;
        _body.tp_basicsize = sizeof(PyWrap_t);
        _body.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
        _body.tp_base = base ? &base->_body: NULL;
        _body.tp_init = PyWrap_Init;
        _body.tp_dealloc = PyWrap_Dealloc;
        _body.tp_methods = methods;
        _body.tp_alloc = PyType_GenericAlloc;
        _body.tp_new = PyType_GenericNew;
        _body.tp_free = PyObject_Del;
    }
    // see pytypeobject_t::pyinit at tail.
    int
    pytypeobject_t::install(PyObject_t *mod)
    {
        const char *name = strrchr(_body.tp_name, '.') + 1;
        if (PyType_Ready(&_body) < 0) return -1;
        Py_INCREF(&_body);
        PyModule_AddObject(mod, name, (PyObject_t *)&_body);
        return 0;
    }

    class pywrapobj_t: public wrapobj_t {
    public:
        pywrapobj_t(PyObject_t *pyobj, refobj_t *cxxobj): wrapobj_t(cxxobj)
        {
            _pyobj = (PyWrap_t *)pyobj;
            _pyobj->cxxobj = cxxobj;
        }
        virtual ~pywrapobj_t() {}

        virtual void ref(void)
        {
#if 0
            fprintf(stderr, "%s(%u): %s(%p) refcnt = %u\n",
                    __FUNCTION__, __LINE__,
                    _pyobj->ob_type->tp_name, _pyobj,
                    (unsigned)_pyobj->ob_refcnt);
#endif
            Py_INCREF(_pyobj);
        }
        virtual void unref(void) {
#if 0
            fprintf(stderr, "%s(%u): %s(%p) refcnt = %u\n",
                    __FUNCTION__, __LINE__,
                    _pyobj->ob_type->tp_name, _pyobj,
                    (unsigned)_pyobj->ob_refcnt);
#endif
            Py_DECREF(_pyobj);
        }

        inline PyObject_t *get(void) { return (PyObject_t *)_pyobj; }
    private:
        PyWrap_t *_pyobj;
    };
    int
    pytypeobject_t::pyinit(PyObject_t *self,
                           PyObject_t *args, PyObject_t *kwargs)
    {
        refobj_t *cxxobj = cxxnew(self, args, kwargs);
        if (cxxobj == NULL) return -1;
        new pywrapobj_t(self, cxxobj);
        return 0;
    }

    PyObject_t *
    cxx2py(refobj_t *cxxobj)
    {
        wrapobj_t *wrap = cxxobj->getwrap();
        if (wrap != NULL) { // have wrap object already.
            pywrapobj_t *pywrap = dynamic_cast<pywrapobj_t *>(wrap);
            assert(pywrap != NULL);
            return pywrap->get();
        }
        pytypeobject_t *cxxpytype = typehash.get(cxxobj);
        assert(cxxpytype != NULL);
        return (new pywrapobj_t(cxxpytype->pynew(), cxxobj))->get();
    }
}

int
init_wrap(PyObject_t *mod)
{
    return 0;
}
