#include "lvpy_runtime.h"

PyObject *PyLvReferenceError = NULL;
PyObject *mp_lv_global_user_data = NULL;

PyTypeObject py_C_Pointer_type;
PyTypeObject py_lv_base_struct_type;
PyTypeObject py_blob_type;

static PyObject *py_get_dict_attr(PyObject *obj, const char *name)
{
    return PyObject_GetAttrString(obj, name);
}

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

PyObject *make_new_lv_struct(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    (void)kwargs;
    Py_ssize_t count = 1;
    PyObject *other = NULL;
    if (!PyArg_ParseTuple(args, "|O", &other)) return NULL;
    if (other && PyLong_Check(other)) {
        count = PyLong_AsSsize_t(other);
        other = NULL;
    }
    py_lv_struct_t *self = PyObject_New(py_lv_struct_t, type);
    if (!self) return NULL;
    size_t size = 64;
    if (other && PyObject_TypeCheck(other, type)) {
        self->data = ((py_lv_struct_t *)other)->data;
        self->owns_data = 0;
        return (PyObject *)self;
    }
    self->data = calloc((size_t)count, size);
    self->owns_data = self->data ? 1 : 0;
    return (PyObject *)self;
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
    if (!PyLvReferenceError) {
        PyLvReferenceError = PyErr_NewException("lvgl.LvReferenceError", NULL, NULL);
    }
    if (!mp_lv_global_user_data) {
        mp_lv_global_user_data = PyDict_New();
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
    .tp_new = make_new_lv_struct,
};

PyTypeObject py_blob_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl.Blob",
    .tp_basicsize = sizeof(py_lv_struct_t),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_base = &py_lv_base_struct_type,
};

PyTypeObject py_C_Pointer_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lvgl.C_Pointer",
    .tp_basicsize = sizeof(py_lv_struct_t),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_base = &py_lv_base_struct_type,
};
