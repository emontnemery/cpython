#ifndef Py_INTERNAL_INTERP_H
#define Py_INTERNAL_INTERP_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_atomic.h"        // _Py_atomic_address
#include "pycore_ast_state.h"     // struct ast_state
#include "pycore_gil.h"           // struct _gil_runtime_state
#include "pycore_gc.h"            // struct _gc_runtime_state
#include "pycore_warnings.h"      // struct _warnings_runtime_state

struct _pending_calls {
    PyThread_type_lock lock;
    /* Request for running pending calls. */
    _Py_atomic_int calls_to_do;
    /* Request for looking at the `async_exc` field of the current
       thread state.
       Guarded by the GIL. */
    int async_exc;
#define NPENDINGCALLS 32
    struct {
        int (*func)(void *);
        void *arg;
    } calls[NPENDINGCALLS];
    int first;
    int last;
};

struct _ceval_state {
    int recursion_limit;
    /* This single variable consolidates all requests to break out of
       the fast path in the eval loop. */
    _Py_atomic_int eval_breaker;
    /* Request for dropping the GIL */
    _Py_atomic_int gil_drop_request;
    struct _pending_calls pending;
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    struct _gil_runtime_state gil;
#endif
};

/* fs_codec.encoding is initialized to NULL.
   Later, it is set to a non-NULL string by _PyUnicode_InitEncodings(). */
struct _Py_unicode_fs_codec {
    char *encoding;   // Filesystem encoding (encoded to UTF-8)
    int utf8;         // encoding=="utf-8"?
    char *errors;     // Filesystem errors (encoded to UTF-8)
    _Py_error_handler error_handler;
};

struct _Py_bytes_state {
    PyObject *empty_string;
    PyBytesObject *characters[256];
};

struct _Py_unicode_ids {
    Py_ssize_t size;
    PyObject **array;
};

struct _Py_unicode_state {
    // The empty Unicode object is a singleton to improve performance.
    PyObject *empty_string;
    /* Single character Unicode strings in the Latin-1 range are being
       shared as well. */
    PyObject *latin1[256];
    struct _Py_unicode_fs_codec fs_codec;

    /* This dictionary holds all interned unicode strings.  Note that references
       to strings in this dictionary are *not* counted in the string's ob_refcnt.
       When the interned string reaches a refcnt of 0 the string deallocation
       function will delete the reference from this dictionary.

       Another way to look at this is that to say that the actual reference
       count of a string is:  s->ob_refcnt + (s->state ? 2 : 0)
    */
    PyObject *interned;

    // Unicode identifiers (_Py_Identifier): see _PyUnicode_FromId()
    struct _Py_unicode_ids ids;
};

#ifndef WITH_FREELISTS
// without freelists
#  define PyFloat_MAXFREELIST 0
// for tuples only store empty tuple singleton
#  define PyTuple_MAXSAVESIZE 1
#  define PyTuple_MAXFREELIST 1
#  define PyList_MAXFREELIST 0
#  define PyDict_MAXFREELIST 0
#  define _PyAsyncGen_MAXFREELIST 0
#  define PyContext_MAXFREELIST 0
#endif

#ifndef PyFloat_MAXFREELIST
#  define PyFloat_MAXFREELIST   100
#endif

struct _Py_float_state {
#if PyFloat_MAXFREELIST > 0
    /* Special free list
       free_list is a singly-linked list of available PyFloatObjects,
       linked via abuse of their ob_type members. */
    int numfree;
    PyFloatObject *free_list;
#endif
};

/* Speed optimization to avoid frequent malloc/free of small tuples */
#ifndef PyTuple_MAXSAVESIZE
   // Largest tuple to save on free list
#  define PyTuple_MAXSAVESIZE 20
#endif
#ifndef PyTuple_MAXFREELIST
   // Maximum number of tuples of each size to save
#  define PyTuple_MAXFREELIST 2000
#endif

struct _Py_tuple_state {
#if PyTuple_MAXSAVESIZE > 0
    /* Entries 1 up to PyTuple_MAXSAVESIZE are free lists,
       entry 0 is the empty tuple () of which at most one instance
       will be allocated. */
    PyTupleObject *free_list[PyTuple_MAXSAVESIZE];
    int numfree[PyTuple_MAXSAVESIZE];
#endif
};

/* Empty list reuse scheme to save calls to malloc and free */
#ifndef PyList_MAXFREELIST
#  define PyList_MAXFREELIST 80
#endif

struct _Py_list_state {
#if PyList_MAXFREELIST > 0
    PyListObject *free_list[PyList_MAXFREELIST];
    int numfree;
#endif
};

#ifndef PyDict_MAXFREELIST
#  define PyDict_MAXFREELIST 80
#endif

struct _Py_dict_state {
#if PyDict_MAXFREELIST > 0
    /* Dictionary reuse scheme to save calls to malloc and free */
    PyDictObject *free_list[PyDict_MAXFREELIST];
    int numfree;
    PyDictKeysObject *keys_free_list[PyDict_MAXFREELIST];
    int keys_numfree;
#endif
};

#ifndef _PyAsyncGen_MAXFREELIST
#  define _PyAsyncGen_MAXFREELIST 80
#endif

struct _Py_async_gen_state {
#if _PyAsyncGen_MAXFREELIST > 0
    /* Freelists boost performance 6-10%; they also reduce memory
       fragmentation, as _PyAsyncGenWrappedValue and PyAsyncGenASend
       are short-living objects that are instantiated for every
       __anext__() call. */
    struct _PyAsyncGenWrappedValue* value_freelist[_PyAsyncGen_MAXFREELIST];
    int value_numfree;

    struct PyAsyncGenASend* asend_freelist[_PyAsyncGen_MAXFREELIST];
    int asend_numfree;
#endif
};

#ifndef PyContext_MAXFREELIST
#  define PyContext_MAXFREELIST 255
#endif

struct _Py_context_state {
#if PyContext_MAXFREELIST > 0
    // List of free PyContext objects
    PyContext *freelist;
    int numfree;
#endif
};

struct _Py_exc_state {
    // The dict mapping from errno codes to OSError subclasses
    PyObject *errnomap;
    PyBaseExceptionObject *memerrors_freelist;
    int memerrors_numfree;
    // The ExceptionGroup type
    PyObject *PyExc_ExceptionGroup;
};


// atexit state
typedef struct {
    PyObject *func;
    PyObject *args;
    PyObject *kwargs;
} atexit_callback;

struct atexit_state {
    atexit_callback **callbacks;
    int ncallbacks;
    int callback_len;
};


// Type attribute lookup cache: speed up attribute and method lookups,
// see _PyType_Lookup().
struct type_cache_entry {
    unsigned int version;  // initialized from type->tp_version_tag
    PyObject *name;        // reference to exactly a str or None
    PyObject *value;       // borrowed reference or NULL
};

#define MCACHE_SIZE_EXP 12
#define MCACHE_STATS 0

struct type_cache {
    struct type_cache_entry hashtable[1 << MCACHE_SIZE_EXP];
#if MCACHE_STATS
    size_t hits;
    size_t misses;
    size_t collisions;
#endif
};


/* interpreter state */

// The PyInterpreterState typedef is in Include/pystate.h.
struct _is {

    struct _is *next;
    struct _ts *tstate_head;

    /* Reference to the _PyRuntime global variable. This field exists
       to not have to pass runtime in addition to tstate to a function.
       Get runtime from tstate: tstate->interp->runtime. */
    struct pyruntimestate *runtime;

    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;

    int finalizing;

    struct _ceval_state ceval;
    struct _gc_runtime_state gc;

    // sys.modules dictionary
    PyObject *modules;
    PyObject *modules_by_index;
    // Dictionary of the sys module
    PyObject *sysdict;
    // Dictionary of the builtins module
    PyObject *builtins;
    // importlib module
    PyObject *importlib;
    // override for config->use_frozen_modules (for tests)
    // (-1: "off", 1: "on", 0: no override)
    int override_frozen_modules;

    /* Used in Modules/_threadmodule.c. */
    long num_threads;
    /* Support for runtime thread stack size tuning.
       A value of 0 means using the platform's default stack size
       or the size specified by the THREAD_STACK_SIZE macro. */
    /* Used in Python/thread.c. */
    size_t pythread_stacksize;

    PyObject *codec_search_path;
    PyObject *codec_search_cache;
    PyObject *codec_error_registry;
    int codecs_initialized;

    PyConfig config;
#ifdef HAVE_DLOPEN
    int dlopenflags;
#endif

    PyObject *dict;  /* Stores per-interpreter state */

    PyObject *builtins_copy;
    PyObject *import_func;
    // Initialized to _PyEval_EvalFrameDefault().
    _PyFrameEvalFunction eval_frame;

    Py_ssize_t co_extra_user_count;
    freefunc co_extra_freefuncs[MAX_CO_EXTRA_USERS];

#ifdef HAVE_FORK
    PyObject *before_forkers;
    PyObject *after_forkers_parent;
    PyObject *after_forkers_child;
#endif

    uint64_t tstate_next_unique_id;

    struct _warnings_runtime_state warnings;
    struct atexit_state atexit;

    PyObject *audit_hooks;

    struct _Py_bytes_state bytes;
    struct _Py_unicode_state unicode;
    struct _Py_float_state float_state;
    /* Using a cache is very effective since typically only a single slice is
       created and then deleted again. */
    PySliceObject *slice_cache;

    struct _Py_tuple_state tuple;
    struct _Py_list_state list;
    struct _Py_dict_state dict_state;
    struct _Py_async_gen_state async_gen;
    struct _Py_context_state context;
    struct _Py_exc_state exc_state;

    struct ast_state ast;
    struct type_cache type_cache;
};

extern void _PyInterpreterState_ClearModules(PyInterpreterState *interp);
extern void _PyInterpreterState_Clear(PyThreadState *tstate);


/* cross-interpreter data registry */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient. */

struct _xidregitem;

struct _xidregitem {
    PyTypeObject *cls;
    crossinterpdatafunc getdata;
    struct _xidregitem *next;
};

PyAPI_FUNC(struct _is*) _PyInterpreterState_LookUpID(int64_t);

PyAPI_FUNC(int) _PyInterpreterState_IDInitref(struct _is *);
PyAPI_FUNC(int) _PyInterpreterState_IDIncref(struct _is *);
PyAPI_FUNC(void) _PyInterpreterState_IDDecref(struct _is *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_INTERP_H */
