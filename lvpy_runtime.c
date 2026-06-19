#include "lvpy_runtime.h"
#include <stdint.h>

PyObject *PyLvReferenceError = NULL;
PyObject *mp_lv_global_user_data = NULL;
PyObject *lvpy_nesting_obj = NULL;

PyTypeObject py_C_Pointer_type;
PyTypeObject py_lv_base_struct_type;
PyTypeObject py_blob_type;

static PyThread_type_lock lvgl_lock = NULL;
static long lvgl_lock_owner = -1;
static int lvgl_lock_depth = 0;
static int lvpy_nesting_count = 0;
static int lvpy_lock_save_key = -1;

typedef struct {
    PyTypeObject *type;
    size_t size;
} lv_struct_size_entry;

static lv_struct_size_entry *struct_size_entries = NULL;
static size_t struct_size_entry_count = 0;
static size_t struct_size_entry_cap = 0;

void lvpy_lock(void)
{
    if (!lvgl_lock) {
        return;
    }
    long tid = PyThread_get_thread_ident();
    if (lvgl_lock_depth > 0 && lvgl_lock_owner == tid) {
        lvgl_lock_depth++;
        return;
    }
    PyThread_acquire_lock(lvgl_lock, WAIT_LOCK);
    lvgl_lock_owner = tid;
    lvgl_lock_depth = 1;
}

void lvpy_unlock(void)
{
    if (!lvgl_lock || lvgl_lock_depth == 0) {
        return;
    }
    long tid = PyThread_get_thread_ident();
    if (lvgl_lock_owner != tid) {
        return;
    }
    lvgl_lock_depth--;
    if (lvgl_lock_depth == 0) {
        lvgl_lock_owner = -1;
        PyThread_release_lock(lvgl_lock);
    }
}

void lvpy_release_lock_for_python(void)
{
    long tid = PyThread_get_thread_ident();
    if (lvgl_lock_owner != tid || lvgl_lock_depth == 0) {
        if (lvpy_lock_save_key >= 0) {
            PyThread_set_key_value(lvpy_lock_save_key, (void *)(intptr_t)0);
        }
        return;
    }
    if (lvpy_lock_save_key >= 0) {
        PyThread_set_key_value(lvpy_lock_save_key, (void *)(intptr_t)lvgl_lock_depth);
    }
    while (lvgl_lock_depth > 0) {
        lvpy_unlock();
    }
}

void lvpy_reacquire_lock_after_python(void)
{
    if (lvpy_lock_save_key < 0) {
        return;
    }
    int saved = (int)(intptr_t)PyThread_get_key_value(lvpy_lock_save_key);
    for (int i = 0; i < saved; i++) {
        lvpy_lock();
    }
    PyThread_set_key_value(lvpy_lock_save_key, (void *)(intptr_t)0);
}

void lvpy_nesting_inc(void)
{
    lvpy_nesting_count++;
    if (lvpy_nesting_obj) {
        PyObject *val = PyLong_FromLong(lvpy_nesting_count);
        if (val) {
            PyObject_SetAttrString(lvpy_nesting_obj, "value", val);
            Py_DECREF(val);
        } else {
            PyErr_Clear();
        }
    }
}

void lvpy_nesting_dec(void)
{
    if (lvpy_nesting_count > 0) {
        lvpy_nesting_count--;
    }
    if (lvpy_nesting_obj) {
        PyObject *val = PyLong_FromLong(lvpy_nesting_count);
        if (val) {
            PyObject_SetAttrString(lvpy_nesting_obj, "value", val);
            Py_DECREF(val);
        } else {
            PyErr_Clear();
        }
    }
}

void lv_struct_register_size(PyTypeObject *type, size_t size)
{
    if (!type || size == 0) return;
    for (size_t i = 0; i < struct_size_entry_count; i++) {
        if (struct_size_entries[i].type == type) {
            struct_size_entries[i].size = size;
            return;
        }
    }
    if (struct_size_entry_count >= struct_size_entry_cap) {
        size_t new_cap = struct_size_entry_cap ? struct_size_entry_cap * 2 : 32;
        lv_struct_size_entry *grown = realloc(struct_size_entries, new_cap * sizeof(lv_struct_size_entry));
        if (!grown) return;
        struct_size_entries = grown;
        struct_size_entry_cap = new_cap;
    }
    struct_size_entries[struct_size_entry_count].type = type;
    struct_size_entries[struct_size_entry_count].size = size;
    struct_size_entry_count++;
}

size_t lv_struct_get_size(PyTypeObject *type)
{
    if (!type) return 0;
    for (size_t i = 0; i < struct_size_entry_count; i++) {
        if (struct_size_entries[i].type == type) {
            return struct_size_entries[i].size;
        }
    }
    return 0;
}

static PyObject *py_nesting_get_value(PyObject *self, void *closure)
{
    (void)self;
    (void)closure;
    return PyLong_FromLong(lvpy_nesting_count);
}

static int py_nesting_set_value(PyObject *self, PyObject *value, void *closure)
{
    (void)self;
    (void)closure;
    if (!value) {
        PyErr_SetString(PyExc_TypeError, "cannot delete nesting value");
        return -1;
    }
    lvpy_nesting_count = (int)PyLong_AsLong(value);
    if (PyErr_Occurred()) return -1;
    return 0;
}

static PyGetSetDef py_nesting_getset[] = {
    {"value", py_nesting_get_value, py_nesting_set_value, NULL},
    {NULL}
};

static PyTypeObject py_nesting_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl._Nesting",
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_getset = py_nesting_getset,
};

PyObject *lvpy_create_nesting_obj(void)
{
    if (PyType_Ready(&py_nesting_type) < 0) return NULL;
    return PyType_GenericNew(&py_nesting_type, NULL, NULL);
}

static PyObject *py_lv_dereference(PyObject *self, PyObject *args)
{
    py_lv_struct_t *inst = (py_lv_struct_t *)self;
    Py_ssize_t size = 0;
    if (!PyArg_ParseTuple(args, "|n", &size)) return NULL;
    if (!inst->data) {
        PyErr_SetString(PyLvReferenceError, "struct data is NULL");
        return NULL;
    }
    if (size <= 0) {
        size = (Py_ssize_t)lv_struct_get_size(Py_TYPE(self));
    }
    if (size <= 0) {
        PyErr_SetString(PyExc_ValueError, "__dereference__ requires a byte size for this object");
        return NULL;
    }
    return PyMemoryView_FromMemory((char *)inst->data, size, PyBUF_WRITE);
}

static PyObject *py_blob_cast(PyObject *self, PyObject *args)
{
    py_lv_struct_t *inst = (py_lv_struct_t *)self;
    PyObject *type_obj = NULL;
    if (!PyArg_ParseTuple(args, "|O", &type_obj)) return NULL;
    if (!type_obj) {
        return PyLong_FromVoidPtr(inst->data);
    }
    if (!PyType_Check(type_obj)) {
        PyErr_SetString(PyExc_TypeError, "cast argument must be a type");
        return NULL;
    }
    py_lv_struct_t *out = PyObject_New(py_lv_struct_t, (PyTypeObject *)type_obj);
    if (!out) return NULL;
    out->data = inst->data;
    out->owns_data = 0;
    return (PyObject *)out;
}

static PyMethodDef py_blob_methods[] = {
    {"__dereference__", py_lv_dereference, METH_VARARGS, NULL},
    {"__cast__", py_blob_cast, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

bool mp_obj_is_true(PyObject *obj)
{
    return obj && obj != Py_None && PyObject_IsTrue(obj);
}

PyObject *convert_to_bool(bool b)
{
    return PyBool_FromLong(b ? 1 : 0);
}

PyObject *convert_to_str(const char *str)
{
    if (!str) Py_RETURN_NONE;
    return PyUnicode_FromString(str);
}

const char *convert_from_str(PyObject *obj)
{
    if (!obj || obj == Py_None) return NULL;
    return PyUnicode_AsUTF8(obj);
}

PyObject *mp_obj_new_int(long v) { return PyLong_FromLong(v); }
PyObject *mp_obj_new_int_from_uint(unsigned long v) { return PyLong_FromUnsignedLong(v); }
PyObject *mp_obj_new_int_from_ll(long long v) { return PyLong_FromLongLong(v); }
PyObject *mp_obj_new_int_from_ull(unsigned long long v) { return PyLong_FromUnsignedLongLong(v); }
PyObject *mp_obj_new_float_from_f(float v) { return PyFloat_FromDouble(v); }

long mp_obj_get_int(PyObject *obj) { return PyLong_AsLong(obj); }

unsigned long long mp_obj_get_ull(PyObject *obj)
{
    return (unsigned long long)PyLong_AsUnsignedLongLong(obj);
}

double mp_obj_get_float(PyObject *obj) { return PyFloat_AsDouble(obj); }

void *copy_buffer(const void *buffer, size_t size)
{
    void *copy = malloc(size);
    if (copy) memcpy(copy, buffer, size);
    return copy;
}

static void *py_array_to_ptr(PyObject *arr, size_t element_size)
{
    if (!arr || arr == Py_None) return NULL;
    if (PyUnicode_Check(arr) || PyBytes_Check(arr) || PyByteArray_Check(arr) ||
        PyObject_CheckBuffer(arr)) {
        return mp_to_ptr(arr);
    }
    if (!PySequence_Check(arr)) {
        PyErr_Format(PyExc_TypeError, "Expected sequence or buffer, got '%s'",
            Py_TYPE(arr)->tp_name);
        return NULL;
    }
    Py_ssize_t len = PySequence_Size(arr);
    if (len < 0) return NULL;
    void *buf = malloc((size_t)len * element_size);
    if (!buf) {
        PyErr_NoMemory();
        return NULL;
    }
    unsigned char *element_addr = (unsigned char *)buf;
    for (Py_ssize_t i = 0; i < len; i++) {
        PyObject *item = PySequence_GetItem(arr, i);
        if (!item) {
            free(buf);
            return NULL;
        }
        unsigned long long val = PyLong_AsUnsignedLongLong(item);
        Py_DECREF(item);
        if (PyErr_Occurred()) {
            free(buf);
            return NULL;
        }
        memcpy(element_addr, &val, element_size);
        element_addr += element_size;
    }
    return buf;
}

static PyObject *py_array_from_ptr(void *lv_arr, size_t element_size)
{
    (void)element_size;
    if (!lv_arr) Py_RETURN_NONE;
    return ptr_to_mp(lv_arr);
}

void *mp_array_to_u8ptr(PyObject *arr) { return py_array_to_ptr(arr, 1); }
void *mp_array_to_i32ptr(PyObject *arr) { return py_array_to_ptr(arr, 4); }
void *mp_array_to_u16ptr(PyObject *arr) { return py_array_to_ptr(arr, 2); }
void *mp_array_to_u32ptr(PyObject *arr) { return py_array_to_ptr(arr, 4); }

PyObject *mp_array_from_u8ptr(void *arr) { return py_array_from_ptr(arr, 1); }
PyObject *mp_array_from_u16ptr(void *arr) { return py_array_from_ptr(arr, 2); }
PyObject *mp_array_from_i32ptr(void *arr) { return py_array_from_ptr(arr, 4); }
PyObject *mp_array_from_u32ptr(void *arr) { return py_array_from_ptr(arr, 4); }

PyObject *mp_read_ptr_C_Pointer(void *field)
{
    return ptr_to_mp(field);
}

PyObject *lv_to_mp_struct(PyTypeObject *type, void *lv_struct)
{
    if (!lv_struct) Py_RETURN_NONE;
    py_lv_struct_t *self = PyObject_New(py_lv_struct_t, type);
    if (!self) return NULL;
    self->data = lv_struct;
    self->owns_data = 0;
    return (PyObject *)self;
}

PyObject *lv_to_mp_struct_own(PyTypeObject *type, void *lv_struct)
{
    if (!lv_struct) Py_RETURN_NONE;
    py_lv_struct_t *self = PyObject_New(py_lv_struct_t, type);
    if (!self) {
        free(lv_struct);
        return NULL;
    }
    self->data = lv_struct;
    self->owns_data = 1;
    return (PyObject *)self;
}

void py_lv_struct_dealloc(py_lv_struct_t *self)
{
    if (self->data && self->owns_data) {
        free(self->data);
        self->data = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

py_lv_struct_t *mp_to_lv_struct(PyObject *obj)
{
    if (!obj || obj == Py_None) return NULL;
    if (!PyObject_TypeCheck(obj, &py_lv_base_struct_type) &&
        !PyObject_TypeCheck(obj, &py_blob_type)) {
        PyErr_SetString(PyExc_TypeError, "Expected struct object");
        return NULL;
    }
    return (py_lv_struct_t *)obj;
}

static int py_lv_struct_apply_dict(py_lv_struct_t *self, PyObject *dict)
{
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(dict, &pos, &key, &value)) {
        if (PyObject_SetAttr((PyObject *)self, key, value) < 0) {
            return -1;
        }
    }
    return 0;
}

PyObject *make_new_lv_struct(PyTypeObject *type, PyObject *args, PyObject *kwargs, size_t elem_size)
{
    (void)kwargs;
    Py_ssize_t count = 1;
    PyObject *other = NULL;
    if (!PyArg_ParseTuple(args, "|O", &other)) return NULL;
    if (other && PyLong_Check(other)) {
        count = PyLong_AsSsize_t(other);
        other = NULL;
    }
    if (elem_size == 0) {
        elem_size = lv_struct_get_size(type);
    }
    py_lv_struct_t *self = PyObject_New(py_lv_struct_t, type);
    if (!self) return NULL;
    if (other && PyObject_TypeCheck(other, type)) {
        self->data = ((py_lv_struct_t *)other)->data;
        self->owns_data = 0;
        return (PyObject *)self;
    }
    if (elem_size == 0) {
        self->data = NULL;
        self->owns_data = 0;
    } else {
        self->data = calloc((size_t)count, elem_size);
        self->owns_data = self->data ? 1 : 0;
    }
    if (other && PyDict_Check(other)) {
        if (!self->data) {
            Py_DECREF(self);
            PyErr_SetString(PyExc_TypeError, "cannot construct struct from dict without a known size");
            return NULL;
        }
        if (py_lv_struct_apply_dict(self, other) < 0) {
            Py_DECREF(self);
            return NULL;
        }
    }
    return (PyObject *)self;
}

static PyObject *py_C_Pointer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    return make_new_lv_struct(type, args, kwds, 0);
}

static void py_lv_delete_cb(lv_event_t *e)
{
    lv_obj_t *lv_obj = lv_event_get_target(e);
    if (!lv_obj) return;
    py_lv_obj_t *self = (py_lv_obj_t *)lv_obj->user_data;
    if (self) self->lv_obj = NULL;
}

PyTypeObject *py_get_base_obj_type(void)
{
    return py_lv_obj_types[0] ? py_lv_obj_types[0]->py_type : NULL;
}

PyObject *lv_to_mp(lv_obj_t *lv_obj)
{
    if (!lv_obj) Py_RETURN_NONE;
    py_lv_obj_t *self = (py_lv_obj_t *)lv_obj->user_data;
    if (!self) {
        PyTypeObject *py_type = py_get_base_obj_type();
        py_lv_obj_type_t **iter = &py_lv_obj_types[0];
        const lv_obj_class_t *cls = lv_obj_get_class(lv_obj);
        for (; *iter; iter++) {
            if ((*iter)->lv_obj_class == cls) {
                py_type = (*iter)->py_type;
                break;
            }
        }
        self = PyObject_New(py_lv_obj_t, py_type);
        if (!self) return NULL;
        self->lv_obj = lv_obj;
        self->callbacks = NULL;
        lv_obj->user_data = self;
        lv_obj_add_event_cb(lv_obj, py_lv_delete_cb, LV_EVENT_DELETE, NULL);
    }
    return (PyObject *)self;
}

lv_obj_t *mp_to_lv(PyObject *obj)
{
    if (!obj || obj == Py_None) return NULL;
    py_lv_obj_t *self = (py_lv_obj_t *)obj;
    PyTypeObject *base = py_get_base_obj_type();
    if (base && !PyObject_TypeCheck(obj, base) &&
        !PyType_IsSubtype(Py_TYPE(obj), base)) {
        PyErr_SetString(PyExc_TypeError, "Expected LVGL object");
        return NULL;
    }
    if (!self->lv_obj) {
        PyErr_SetString(PyLvReferenceError, "Referenced object was deleted!");
        return NULL;
    }
    return self->lv_obj;
}

PyObject *mp_get_callbacks(PyObject *obj)
{
    py_lv_obj_t *self = (py_lv_obj_t *)obj;
    if (!self->callbacks) {
        self->callbacks = PyDict_New();
        if (!self->callbacks) return NULL;
    }
    return self->callbacks;
}

void lvpy_dealloc_obj(py_lv_obj_t *self)
{
    if (self->lv_obj) {
        self->lv_obj->user_data = NULL;
    }
    Py_XDECREF(self->callbacks);
    self->callbacks = NULL;
    Py_TYPE(self)->tp_free((PyObject *)self);
}

void *mp_to_ptr(PyObject *obj)
{
    if (!obj || obj == Py_None) return NULL;
    if (PyDict_Check(obj)) return (void *)obj;
    if (PyObject_TypeCheck(obj, &py_lv_base_struct_type) ||
        PyObject_TypeCheck(obj, &py_blob_type)) {
        return ((py_lv_struct_t *)obj)->data;
    }
    if (PyObject_CheckBuffer(obj)) {
        Py_buffer view;
        if (PyObject_GetBuffer(obj, &view, PyBUF_SIMPLE) == 0) {
            void *p = view.buf;
            PyBuffer_Release(&view);
            return p;
        }
    }
    if (PyUnicode_Check(obj)) {
        return (void *)PyUnicode_AsUTF8(obj);
    }
    PyErr_Format(PyExc_TypeError, "Cannot convert '%s' to pointer", Py_TYPE(obj)->tp_name);
    return NULL;
}

PyObject *ptr_to_mp(void *data)
{
    return lv_to_mp_struct(&py_blob_type, data);
}

PyObject *get_callback_dict_from_user_data(void *user_data)
{
    if (!user_data) return NULL;
    PyObject *obj = (PyObject *)user_data;
    if (PyDict_Check(obj)) return obj;
    return mp_get_callbacks(obj);
}

void *mp_lv_callback(PyObject *py_callback, void *lv_callback, const char *callback_name,
    void **user_data_ptr, void *containing_struct,
    void *(*get_user_data)(void *), void (*set_user_data)(void *, void *))
{
    (void)containing_struct;
    if (lv_callback && py_callback && PyCallable_Check(py_callback)) {
        void *user_data = NULL;
        if (user_data_ptr) {
            if (!*user_data_ptr) {
                *user_data_ptr = PyDict_New();
                if (!*user_data_ptr) return NULL;
                Py_INCREF((PyObject *)*user_data_ptr);
            } else if (PyDict_Check((PyObject *)*user_data_ptr)) {
                Py_INCREF((PyObject *)*user_data_ptr);
            }
            user_data = *user_data_ptr;
        } else if (get_user_data && set_user_data) {
            user_data = get_user_data(containing_struct);
            if (!user_data) {
                user_data = PyDict_New();
                set_user_data(containing_struct, user_data);
            }
        }
        if (user_data) {
            PyObject *callbacks = get_callback_dict_from_user_data(user_data);
            if (callbacks) {
                PyDict_SetItemString(callbacks, callback_name, py_callback);
                Py_INCREF(py_callback);
            }
        }
        return lv_callback;
    }
    return mp_to_ptr(py_callback);
}

PyObject *mp_lv_funcptr(PyObject *wrapper, void *lv_fun, void *lv_callback,
    const char *func_name, void *user_data)
{
    (void)wrapper;
    if (!lv_fun) Py_RETURN_NONE;
    if (lv_fun == lv_callback) {
        PyObject *callbacks = get_callback_dict_from_user_data(user_data);
        if (callbacks) return PyDict_GetItemString(callbacks, func_name);
    }
    Py_RETURN_NONE;
}

void py_lv_runtime_init_types(void)
{
    if (!lvgl_lock) {
        lvgl_lock = PyThread_allocate_lock();
    }
    if (lvpy_lock_save_key < 0) {
        lvpy_lock_save_key = PyThread_create_key();
    }
    if (!PyLvReferenceError) {
        PyLvReferenceError = PyErr_NewException("lvgl.LvReferenceError", NULL, NULL);
    }
    if (!mp_lv_global_user_data) {
        mp_lv_global_user_data = PyDict_New();
    }
    if (!lvpy_nesting_obj) {
        lvpy_nesting_obj = lvpy_create_nesting_obj();
    }
    if (PyType_Ready(&py_lv_base_struct_type) < 0) return;
    if (PyType_Ready(&py_blob_type) < 0) return;
    if (PyType_Ready(&py_C_Pointer_type) < 0) return;
}

PyTypeObject py_lv_base_struct_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl.Struct",
    .tp_basicsize = sizeof(py_lv_struct_t),
    .tp_dealloc = (destructor)py_lv_struct_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_methods = py_blob_methods,
};

PyTypeObject py_blob_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl.Blob",
    .tp_basicsize = sizeof(py_lv_struct_t),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_base = &py_lv_base_struct_type,
    .tp_methods = py_blob_methods,
};

PyTypeObject py_C_Pointer_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl.C_Pointer",
    .tp_basicsize = sizeof(py_lv_struct_t),
    .tp_dealloc = (destructor)py_lv_struct_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_base = &py_lv_base_struct_type,
    .tp_new = py_C_Pointer_new,
};
