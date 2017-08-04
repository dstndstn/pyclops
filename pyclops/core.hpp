#ifndef _PYCLOPS_CORE_HPP
#define _PYCLOPS_CORE_HPP

#include <iostream>
#include <stdexcept>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <numpy/arrayobject.h>

namespace pyclops {
#if 0
}  // emacs pacifier
#endif


// See pyclops/converters.hpp.
template<typename T> struct converter;

// Forward declarations needed below.
struct py_tuple;
struct py_dict;


// -------------------------------------------------------------------------------------------------
//
// struct py_object


struct py_object {
    // Holds reference, can never be NULL.  (If there is an attempt to construct a py_object
    // with a null pointer, then the pyerr_occurred exception will be thrown, see below.)
    PyObject *ptr = nullptr;

    py_object();   // default constructor produces Py_None
    ~py_object();

    py_object(py_object &&x);
    py_object(const py_object &x);
    py_object &operator=(const py_object &x);
    py_object &operator=(py_object &&x);
    
    // Note: instead of using this constructor...
    py_object(PyObject *x, bool increment_refcount);
    
    // ...I prefer to use these constructor-like functions.
    static py_object borrowed_reference(PyObject *x) { return py_object(x, true); }  // increment refcount
    static py_object new_reference(PyObject *x) { return py_object(x, false); }      // don't increment refcount

    inline bool is_none() const { return ptr == Py_None; }
    inline bool is_tuple() const { return PyTuple_Check(ptr); }
    inline bool is_dict() const { return PyDict_Check(ptr); }
    inline bool is_array() const { return PyArray_Check(ptr); }
    inline bool is_callable() const { return PyCallable_Check(ptr); }
    inline ssize_t get_refcount() const { return Py_REFCNT(ptr); }

    // These are safe to call without checking is_callable().
    py_object call(const py_tuple &args);
    py_object call(const py_tuple &args, const py_dict &kwds);

    // Note: to further convert to a C++ string, wrap return value in "from_python<std::string> ()".
    py_object str() const  { return py_object::new_reference(PyObject_Str(ptr)); }
    py_object repr() const { return py_object::new_reference(PyObject_Repr(ptr)); }
};


// -------------------------------------------------------------------------------------------------
//
// struct py_tuple
//
// Reference: https://docs.python.org/2/c-api/tuple.html


struct py_tuple : public py_object {
    // Note: no default constructor.  To create a length-zero tuple, use make() or make_empty(0).
    py_tuple(const py_object &x, const char *where=NULL);
    py_tuple(py_object &&x, const char *where=NULL);
    py_tuple &operator=(const py_object &x);
    py_tuple &operator=(py_object &&x);

    ssize_t size() const { return PyTuple_Size(ptr); }

    inline py_object get_item(ssize_t pos) const;
    inline void set_item(ssize_t pos, const py_object &x);

    // make_empty(): constructor-like function which makes "empty" tuple containing None values.
    static inline py_tuple make_empty(ssize_t n);

    // make(): constructor-like function which makes python tuple from arbitrary C++ args.
    template<typename... Args>
    static inline py_tuple make(Args... args);

    inline void _check(const char *where=NULL);
    static void _throw(const char *where);   // non-inline, defined in exceptions.cpp
};


// -------------------------------------------------------------------------------------------------
//
// struct py_dict
//
// Reference: https://docs.python.org/2/c-api/dict.html


struct py_dict : public py_object {
    py_dict();
    py_dict(const py_object &x, const char *where=NULL);
    py_dict(py_object &&x, const char *where=NULL);
    py_dict &operator=(const py_object &x);
    py_dict &operator=(py_object &&x);

    ssize_t size() const { return PyDict_Size(ptr); }

    inline void _check(const char *where=NULL);
    static void _throw(const char *where);   // non-inline, defined in exceptions.cpp
};


// -------------------------------------------------------------------------------------------------
//
// Exceptions.


// This exception is thrown whenever we "discover" in C++ code that a python exception
// has occurred (either because PyErr_Occurred() returned true, or (PyObject *) is NULL).
struct pyerr_occurred : std::exception
{
    const char *where = nullptr;
    pyerr_occurred(const char *where=nullptr);
    virtual char const *what() const noexcept;
};


// -------------------------------------------------------------------------------------------------
//
// py_object implementation.


inline py_object::py_object() : 
    py_object(Py_None, true)
{ }

inline py_object::~py_object()
{
    Py_XDECREF(ptr);
    this->ptr = nullptr;
}

inline py_object::py_object(const py_object &x) : 
    py_object(x.ptr, true)
{ }

inline py_object::py_object(py_object &&x) :
    ptr(x.ptr)
{
    x.ptr = nullptr;
}

inline py_object::py_object(PyObject *x, bool increment_refcount) :
    ptr(x)
{
    if (!x)
	throw pyerr_occurred();
    if (increment_refcount)
	Py_INCREF(x);
}

inline py_object &py_object::operator=(const py_object &x)
{ 
    // this ordering handles the self-assignment case correctly
    Py_XINCREF(x.ptr);
    Py_XDECREF(this->ptr);
    this->ptr = x.ptr;
    return *this; 
}

inline py_object &py_object::operator=(py_object &&x)
{ 
    this->ptr = x.ptr;
    x.ptr = NULL;
    return *this; 
}

// Note: PyObject_Call() is always fastest, if args/kwds are known to be a tuple/dict.
inline py_object py_object::call(const py_tuple &args)
{
    PyObject *p = PyObject_Call(ptr, args.ptr, NULL);
    return new_reference(p);
}

inline py_object py_object::call(const py_tuple &args, const py_dict &kwds)
{
    PyObject *p = PyObject_Call(ptr, args.ptr, kwds.ptr);
    return new_reference(p);
}

inline std::ostream &operator<<(std::ostream &os, const py_object &x)
{
    py_object s = x.str();
    char *p = PyString_AsString(s.ptr);
    if (!p)
	throw pyerr_occurred();
    os << p;
    return os;
}


// -------------------------------------------------------------------------------------------------
//
// py_tuple implementation



inline py_tuple::py_tuple(const py_object &x, const char *where) :
    py_object(x) 
{ 
    this->_check();
}
    
inline py_tuple::py_tuple(py_object &&x, const char *where) :
    py_object(x) 
{ 
    this->_check();
}

inline py_tuple &py_tuple::operator=(const py_object &x)
{
    // this ordering handles the self-assignment case correctly
    Py_XINCREF(x.ptr);
    Py_XDECREF(this->ptr);
    this->ptr = x.ptr;
    this->_check();
    return *this;
}

inline py_tuple &py_tuple::operator=(py_object &&x)
{
    this->ptr = x.ptr;
    x.ptr = NULL;
    this->_check();
    return *this;
}

inline void py_tuple::_check(const char *where)
{
    if (!PyTuple_Check(this->ptr))
	_throw(where);
}

inline py_object py_tuple::get_item(ssize_t pos) const
{
    return py_object::borrowed_reference(PyTuple_GetItem(this->ptr, pos));
}

inline void py_tuple::set_item(ssize_t pos, const py_object &x)
{
    int err = PyTuple_SetItem(this->ptr, pos, x.ptr);

    // TODO: check that my understanding of refcounting and error indicator is correct here

    if (!err)
	Py_INCREF(x.ptr);   // success
    else
	throw pyerr_occurred();  // failure
}


// Static constructor-like member function 
inline py_tuple py_tuple::make_empty(ssize_t n)
{
    // Note: if n < 0, then PyTuple_New() sets the python global error appropriately.
    return py_tuple::new_reference(PyTuple_New(n));
}


// _set_tuple(): helper for py_tuple::make() below.
template<typename... Args>
inline void _set_tuple(py_tuple &t, int pos, Args... args);

template<> inline void _set_tuple(py_tuple &t, int pos) { }

template<typename A, typename... Ap>
inline void _set_tuple(py_tuple &t, int pos, A a, Ap... ap)
{
    t.set_item(pos, converter<A>::to_python(a));
    _set_tuple(t, pos+1, ap...);
}

// Static constructor-like member function.
template<typename... Args>
inline py_tuple py_tuple::make(Args... args)
{
    py_tuple ret = make_empty(sizeof...(Args));
    _set_tuple(ret, 0, args...);
    return ret;
}


// -------------------------------------------------------------------------------------------------
//
// py_dict implementation.


inline py_dict::py_dict() :
    py_object(PyDict_New(), false)
{ }

inline py_dict::py_dict(const py_object &x, const char *where) :
    py_object(x) 
{ 
    this->_check();
}
    
inline py_dict::py_dict(py_object &&x, const char *where) :
    py_object(x) 
{ 
    this->_check();
}

inline py_dict &py_dict::operator=(const py_object &x)
{
    // this ordering handles the self-assignment case correctly
    Py_XINCREF(x.ptr);
    Py_XDECREF(this->ptr);
    this->ptr = x.ptr;
    this->_check();
    return *this;
}

inline py_dict &py_dict::operator=(py_object &&x)
{
    this->ptr = x.ptr;
    x.ptr = NULL;
    this->_check();
    return *this;
}

inline void py_dict::_check(const char *where)
{
    if (!PyDict_Check(this->ptr))
	_throw(where);
}


}  // namespace pyclops

#endif  // _PYCLOPS_OBJECT_HPP