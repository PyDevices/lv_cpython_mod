#ifndef LVPY_RUNTIME_H
#define LVPY_RUNTIME_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared LVGL / Python glue (no MicroPython runtime). */

extern PyObject *PyLvReferenceError;
extern PyObject *mp_lv_global_user_data;

typedef struct {
    PyObject_HEAD
    void *data;
    int owns_data;
} py_lv_struct_t;

typedef struct {
    PyObject_HEAD
    lv_obj_t *lv_obj;
    PyObject *callbacks;
} py_lv_obj_t;

typedef struct {
    const lv_obj_class_t *lv_obj_class;
    PyTypeObject *py_type;
} py_lv_obj_type_t;

extern py_lv_obj_type_t *py_lv_obj_types[];

PyTypeObject *py_get_base_obj_type(void);

/* Primitive convertors (names match generated binding code). */
bool mp_obj_is_true(PyObject *obj);
PyObject *convert_to_bool(bool b);
PyObject *convert_to_str(const char *str);
const char *convert_from_str(PyObject *obj);
PyObject *mp_obj_new_int(long v);
PyObject *mp_obj_new_int_from_uint(unsigned long v);
PyObject *mp_obj_new_int_from_ll(long long v);
PyObject *mp_obj_new_int_from_ull(unsigned long long v);
PyObject *mp_obj_new_float_from_f(float v);
long mp_obj_get_int(PyObject *obj);
unsigned long long mp_obj_get_ull(PyObject *obj);
double mp_obj_get_float(PyObject *obj);

void *mp_to_ptr(PyObject *obj);
PyObject *ptr_to_mp(void *data);

PyObject *lv_to_mp_struct(PyTypeObject *type, void *lv_struct);
PyObject *lv_to_mp_struct_own(PyTypeObject *type, void *lv_struct);
void py_lv_struct_dealloc(py_lv_struct_t *self);
py_lv_struct_t *mp_to_lv_struct(PyObject *obj);
PyObject *make_new_lv_struct(PyTypeObject *type, PyObject *args, PyObject *kwargs);
void *copy_buffer(const void *buffer, size_t size);

void *mp_array_to_u8ptr(PyObject *arr);
void *mp_array_to_u16ptr(PyObject *arr);
void *mp_array_to_u32ptr(PyObject *arr);
void *mp_array_to_i32ptr(PyObject *arr);
PyObject *mp_array_from_u8ptr(void *arr);
PyObject *mp_array_from_u16ptr(void *arr);
PyObject *mp_array_from_i32ptr(void *arr);
PyObject *mp_array_from_u32ptr(void *arr);
PyObject *mp_read_ptr_C_Pointer(void *field);

PyObject *lv_to_mp(lv_obj_t *lv_obj);
lv_obj_t *mp_to_lv(PyObject *obj);
PyObject *mp_get_callbacks(PyObject *obj);

void lvpy_dealloc_obj(py_lv_obj_t *self);
PyObject *get_callback_dict_from_user_data(void *user_data);

void *mp_lv_callback(PyObject *py_callback, void *lv_callback, const char *callback_name,
    void **user_data_ptr, void *containing_struct,
    void *(*get_user_data)(void *), void (*set_user_data)(void *, void *));

PyObject *mp_lv_funcptr(PyObject *wrapper, void *lv_fun, void *lv_callback,
    const char *func_name, void *user_data);

#define mp_write_ptr_C_Pointer(obj) (((py_lv_struct_t *)obj)->data)

extern PyTypeObject py_C_Pointer_type;
extern PyTypeObject py_lv_base_struct_type;
extern PyTypeObject py_blob_type;
extern PyTypeObject py_lv_obj_type;  /* defined in generated lvpy.c (phase 5+) */

void py_lv_runtime_init_types(void);

#ifdef __cplusplus
}
#endif

#endif /* LVPY_RUNTIME_H */
