
#include "rpmsystem-py.h"
#include <rpm/rpmstring.h>
#include "rpmfd-py.h"

struct rpmfdObject_s {
    PyObject_HEAD
    PyObject *md_dict;
    FD_t fd;
};

FD_t rpmfdGetFd(rpmfdObject *fdo)
{
    return fdo->fd;
}

int rpmfdFromPyObject(PyObject *obj, rpmfdObject **fdop)
{
    rpmfdObject *fdo = NULL;

    if (rpmfdObject_Check(obj)) {
	Py_INCREF(obj);
	fdo = (rpmfdObject *) obj;
    } else {
	fdo = (rpmfdObject *) PyObject_Call((PyObject *)&rpmfd_Type,
			    		    Py_BuildValue("(O)", obj), NULL);
    }
    if (fdo == NULL) return 0;

    if (Ferror(fdo->fd)) {
	Py_DECREF(fdo);
	PyErr_SetString(PyExc_IOError, Fstrerror(fdo->fd));
	return 0;
    }
    *fdop = fdo;
    return 1;
}

static PyObject *err_closed(void)
{
    PyErr_SetString(PyExc_ValueError, "I/O operation on closed file");
    return NULL;
}

static PyObject *rpmfd_new(PyTypeObject *subtype, 
			   PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "obj", "mode", "flags", NULL };
    char *mode = "r";
    char *flags = "ufdio";
    PyObject *fo = NULL;
    rpmfdObject *s = NULL;
    FD_t fd = NULL;
    int fdno;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ss", kwlist, 
				     &fo, &mode, &flags))
	return NULL;

    if (PyBytes_Check(fo)) {
	char *m = rstrscat(NULL, mode, ".", flags, NULL);
	Py_BEGIN_ALLOW_THREADS 
	fd = Fopen(PyBytes_AsString(fo), m);
	Py_END_ALLOW_THREADS 
	free(m);
    } else if ((fdno = PyObject_AsFileDescriptor(fo)) >= 0) {
	fd = fdDup(fdno);
    } else {
	PyErr_SetString(PyExc_TypeError, "path or file object expected");
	return NULL;
    }

    if (Ferror(fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(fd));
	return NULL;
    }

    if ((s = (rpmfdObject *)subtype->tp_alloc(subtype, 0)) == NULL) {
	Fclose(fd);
	return NULL;
    }
    /* TODO: remember our filename, mode & flags */
    s->fd = fd;
    return (PyObject*) s;
}

static PyObject *do_close(rpmfdObject *s)
{
    /* mimic python fileobject: close on closed file is not an error */
    if (s->fd) {
	Py_BEGIN_ALLOW_THREADS
	Fclose(s->fd);
	Py_END_ALLOW_THREADS
	s->fd = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_close(rpmfdObject *s)
{
    return do_close(s);
}

static void rpmfd_dealloc(rpmfdObject *s)
{
    PyObject *res = do_close(s);
    Py_XDECREF(res);
    Py_TYPE(s)->tp_free((PyObject *)s);
}

static PyObject *rpmfd_fileno(rpmfdObject *s)
{
    int fno;
    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    fno = Fileno(s->fd);
    Py_END_ALLOW_THREADS

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return Py_BuildValue("i", fno);
}

static PyObject *rpmfd_flush(rpmfdObject *s)
{
    int rc;

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    rc = Fflush(s->fd);
    Py_END_ALLOW_THREADS

    if (rc || Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_isatty(rpmfdObject *s)
{
    int fileno;
    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    fileno = Fileno(s->fd);
    Py_END_ALLOW_THREADS

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return PyBool_FromLong(isatty(fileno));
}

static PyObject *rpmfd_seek(rpmfdObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "offset", "whence", NULL };
    off_t offset;
    int whence = SEEK_SET;
    int rc = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "L|i", kwlist, 
				     &offset, &whence)) 
	return NULL;

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS
    rc = Fseek(s->fd, offset, whence);
    Py_END_ALLOW_THREADS
    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *rpmfd_tell(rpmfdObject *s)
{
    off_t offset;
    Py_BEGIN_ALLOW_THREADS
    offset = Ftell(s->fd);
    Py_END_ALLOW_THREADS
    return Py_BuildValue("L", offset);
    
}

static PyObject *rpmfd_read(rpmfdObject *s, PyObject *args, PyObject *kwds)
{
    char *kwlist[] = { "size", NULL };
    char *buf = NULL;
    ssize_t reqsize = -1;
    ssize_t bufsize = 0;
    ssize_t read = 0;
    PyObject *res = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|l", kwlist, &reqsize))
	return NULL;

    if (s->fd == NULL) return err_closed();

    /* XXX simple, stupid for now ... and broken for anything large */
    bufsize = (reqsize < 0) ? fdSize(s->fd) : reqsize;
    if ((buf = malloc(bufsize+1)) == NULL) {
	return PyErr_NoMemory();
    }
    
    Py_BEGIN_ALLOW_THREADS 
    read = Fread(buf, 1, bufsize, s->fd);
    Py_END_ALLOW_THREADS 

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	goto exit;
    }
    res = PyBytes_FromStringAndSize(buf, read);

exit:
    free(buf);
    return res;
}

static PyObject *rpmfd_write(rpmfdObject *s, PyObject *args, PyObject *kwds)
{

    const char *buf = NULL;
    ssize_t size = 0;
    char *kwlist[] = { "buffer", NULL };
    ssize_t rc = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#", kwlist, &buf, &size)) {
	return NULL;
    }

    if (s->fd == NULL) return err_closed();

    Py_BEGIN_ALLOW_THREADS 
    rc = Fwrite(buf, 1, size, s->fd);
    Py_END_ALLOW_THREADS 

    if (Ferror(s->fd)) {
	PyErr_SetString(PyExc_IOError, Fstrerror(s->fd));
	return NULL;
    }
    return Py_BuildValue("n", rc);
}

static char rpmfd_doc[] = "";

static struct PyMethodDef rpmfd_methods[] = {
    { "close",	(PyCFunction) rpmfd_close,	METH_NOARGS,
	NULL },
    { "fileno",	(PyCFunction) rpmfd_fileno,	METH_NOARGS,
	NULL },
    { "flush",	(PyCFunction) rpmfd_flush,	METH_NOARGS,
	NULL },
    { "isatty",	(PyCFunction) rpmfd_isatty,	METH_NOARGS,
	NULL },
    { "read",	(PyCFunction) rpmfd_read,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "seek",	(PyCFunction) rpmfd_seek,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { "tell",	(PyCFunction) rpmfd_tell,	METH_NOARGS,
	NULL },
    { "write",	(PyCFunction) rpmfd_write,	METH_VARARGS|METH_KEYWORDS,
	NULL },
    { NULL, NULL }
};

static PyObject *rpmfd_get_closed(rpmfdObject *s)
{
    return PyBool_FromLong((s->fd == NULL));
}

static PyGetSetDef rpmfd_getseters[] = {
    { "closed", (getter)rpmfd_get_closed, NULL, NULL },
    { NULL },
};

PyTypeObject rpmfd_Type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"rpm.fd",			/* tp_name */
	sizeof(rpmfdObject),		/* tp_size */
	0,				/* tp_itemsize */
	/* methods */
	(destructor) rpmfd_dealloc, 	/* tp_dealloc */
	0,				/* tp_print */
	(getattrfunc)0, 		/* tp_getattr */
	(setattrfunc)0,			/* tp_setattr */
	0,				/* tp_compare */
	(reprfunc)0,			/* tp_repr */
	0,				/* tp_as_number */
	0,				/* tp_as_sequence */
	0,				/* tp_as_mapping */
	(hashfunc)0,			/* tp_hash */
	(ternaryfunc)0,			/* tp_call */
	(reprfunc)0,			/* tp_str */
	PyObject_GenericGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,	/* tp_setattro */
	0,				/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,	/* tp_flags */
	rpmfd_doc,			/* tp_doc */
	0,				/* tp_traverse */
	0,				/* tp_clear */
	0,				/* tp_richcompare */
	0,				/* tp_weaklistoffset */
	0,				/* tp_iter */
	0,				/* tp_iternext */
	rpmfd_methods,			/* tp_methods */
	0,				/* tp_members */
	rpmfd_getseters,		/* tp_getset */
	0,				/* tp_base */
	0,				/* tp_dict */
	0,				/* tp_descr_get */
	0,				/* tp_descr_set */
	0,				/* tp_dictoffset */
	(initproc)0,			/* tp_init */
	(allocfunc)0,			/* tp_alloc */
	(newfunc) rpmfd_new,		/* tp_new */
	(freefunc)0,			/* tp_free */
	0,				/* tp_is_gc */
};
