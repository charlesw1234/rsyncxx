#include "pylibdefs.hpp"
#include "_wrap.hpp"
#include "_buf.hpp"
#include "_node.hpp"
#include "_rsyncnode.hpp"
#include "rsync/utils.h"

static PyObject_t *
pybinshow(PyObject_t *self, PyObject_t *args)
{
    char *prefix;
    FILE *wfp;
    char *buf0;
    ssize_t binlen;
    Py_ssize_t str_len;
    PyObject_t *wfp_obj, *str_obj;

    if (!PyArg_ParseTuple(args, "sOOn", &prefix, &wfp_obj, &str_obj, &binlen) ||
        !PyFile_Check(wfp_obj) || !PyString_Check(str_obj)) return NULL;

    wfp = PyFile_AsFile(wfp_obj);
    PyString_AsStringAndSize(str_obj, &buf0, &str_len);
    if (binlen > str_len) {
        PyErr_Format(PyExc_TypeError,
                     "size(%ld) too large than string length %ld.",
                     binlen, str_len); return NULL;
    }
    binshow(prefix, wfp, (const uint8_t *)buf0,
            (const uint8_t *)(buf0 + binlen));
    Py_RETURN_NONE;
}

static PyMethodDef_t methods[] = {
    { "binshow", pybinshow, METH_VARARGS, NULL },
    { NULL, NULL, 0, NULL }
};

extern "C"
MODFUNC(_rsync)
{
    PyObject_t *mod;
    if ((mod = Py_InitModule("_rsync", methods)) == NULL) MODFUNCRET(NULL);
    PyModule_AddObject(mod, "VERSION", PyInt_FromLong(PYMOD_VERSION));
    if (init_wrap(mod) < 0) MODFUNCRET(NULL);
    if (init_buf(mod) < 0) MODFUNCRET(NULL);
    if (init_node(mod) < 0) MODFUNCRET(NULL);
    if (init_rsyncnode(mod) < 0) MODFUNCRET(NULL);
    MODFUNCRET(mod);
}
