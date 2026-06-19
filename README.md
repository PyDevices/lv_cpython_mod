# lv_cpython_mod

Native CPython extension for [LVGL](https://lvgl.io/), generated from [`lv_bindings`](https://github.com/PyDevices/lv_bindings) with **no MicroPython runtime**. The Python module is `lvgl`; most of the API surface is emitted into `lv_bindings/generated/lvpy.c`, with shared glue in `lvpy_runtime.c`.

## Requirements

- Python 3.9+
- A C compiler (GCC or Clang)
- [`lv_bindings`](https://github.com/PyDevices/lv_bindings) checked out as a **sibling** of this directory (see layout below)
- LVGL sources and headers inside that `lv_bindings` tree (via its submodule or checkout)

## Repository layout

`setup.py` expects this directory layout:

```text
workspace/
├── lv_bindings/          # code generator + LVGL tree + generated/lvpy.c
│   ├── lvgl/
│   ├── generated/lvpy.c  # produced by regenerate_lvpy.sh
│   └── regenerate_lvpy.sh
└── lv_cpython_mod/       # this package
    ├── lvpy_runtime.c
    ├── lvpy_runtime.h
    └── setup.py
```

Clone both repositories into the same parent folder before building.

## Build

```bash
# 1. Generate CPython bindings (from lv_bindings)
cd lv_bindings && ./regenerate_lvpy.sh

# 2. Build and install the extension
cd ../lv_cpython_mod
python3 -m venv .venv
.venv/bin/pip install -e .

# 3. Smoke tests
.venv/bin/python test_lvgl_cpython.py
```

`pip install` compiles `lvpy_runtime.c`, the generated `lvpy.c`, and LVGL sources from `lv_bindings/lvgl/src`. If `lv_bindings/generated/lvpy.c` is missing, the build stops with instructions to run `regenerate_lvpy.sh`.

To change how much of the API is emitted, edit `max_phase` in `lv_bindings/binding/emit_cpython.py` (currently **7**) and regenerate.

## Usage

```python
import lvgl as lv

lv.init()

disp = lv.display_create(240, 240)
lv.display_set_color_format(disp, lv.COLOR_FORMAT.RGB565)
buf = lv.draw_buf_create(240, 240, lv.COLOR_FORMAT.RGB565, 0)
lv.display_set_draw_buffers(disp, buf, None)
lv.display_set_render_mode(disp, lv.DISPLAY_RENDER_MODE.PARTIAL)

scr = lv.screen_active()
label = lv.label(scr)
label.set_text("Hello")
print(label.get_text())

def on_clicked(event):
    print("clicked", event.get_code())

scr.add_event_cb(on_clicked, lv.EVENT.CLICKED, None)
scr.send_event(lv.EVENT.CLICKED, None)

lv.deinit()
```

### API style

The binding follows the same naming as the MicroPython port where possible:

| Pattern | Example |
|---------|---------|
| Module constants | `lv.SYMBOL_OK`, `lv.EVENT.CLICKED` |
| Enum namespaces | `lv.COLOR_FORMAT.RGB565`, `lv.DISPLAY_RENDER_MODE.PARTIAL` |
| Widget types (callable) | `lv.obj()`, `lv.label(parent)`, `lv.button(parent)` |
| Object methods | `label.set_text("…")`, `scr.add_event_cb(cb, lv.EVENT.CLICKED, None)` |
| Module functions | `lv.display_create(240, 240)`, `lv.screen_active()` |
| Struct methods | `event.get_code()` on `lv_event_t` wrappers |
| Module-level struct helpers | `lv.event_get_code(event)` (same as MP module API) |

Display configuration uses module-level functions (`lv.display_set_flush_cb`, etc.), not methods on a display object.

## Architecture

| Component | Role |
|-----------|------|
| `lv_bindings/binding/emit_*.py` | Code generator (`emit_cpython.py` sets `max_phase`) |
| `lv_bindings/generated/lvpy.c` | Generated types, methods, module functions, callbacks |
| `lvpy_runtime.c` / `lvpy_runtime.h` | CPython glue: object/struct wrappers, convertors, callback dicts, GIL handling |
| `setup.py` | Builds the `lvgl` extension module |

**Object wrappers** (`py_lv_obj_t`) map `lv_obj_t *` to Python and keep per-object callback dicts. **Struct wrappers** (`py_lv_struct_t`) expose LVGL structs; pointers returned from LVGL are borrowed (`owns_data = 0`), while copies own their buffer.

**Event callbacks** (phase 7): pass a Python callable to `add_event_cb`. With `user_data=None`, the wrapper stores callbacks on the target object and passes the Python wrapper to LVGL as `user_data`.

## Emission phases

| Phase | Coverage |
|-------|----------|
| 1 | `lv.init()` / `lv.deinit()`, integer constants, `LV_SYMBOL_*` strings |
| 2 | Enum namespaces (`lv.EVENT.CLICKED`, `lv.COLOR_FORMAT.*`, …) |
| 3 | Struct types, field get/set |
| 4 | Struct methods (`event.get_code()`, …) |
| 5 | Widget / `lv_obj` types, constructors, methods |
| 6 | Module-level functions (`display_create`, `screen_active`, …) |
| 7 | Python callbacks (`add_event_cb`, flush/timer hooks, …) |

Phases 1–7 are enabled in the generator today. Smoke tests in `test_lvgl_cpython.py` cover init/deinit, widgets, module functions, and screen/button event callbacks.

## Known limitations

- **Sibling dependency**: this repo does not vendor `lv_bindings` or LVGL; both must be present at build time.
- **Display OO API**: no methods on `lv_display_t` wrappers; use `lv.display_*` module functions.
- **Prototype aliasing**: some widget `tp_methods` entries may point at the wrong C function when LVGL reuses prototypes; prefer module-level names when in doubt.
- **`C_Pointer` helper struct**: emitted late; a runtime stub is used until helper emission is complete.
- **Callback lifetime**: Python wrappers clear `lv_obj->user_data` on dealloc; long-running apps should keep references to objects that register callbacks.

## Development

Regenerate after changing `lv_bindings` or `max_phase`:

```bash
cd lv_bindings && ./regenerate_lvpy.sh
cd ../lv_cpython_mod && .venv/bin/pip install -e .
.venv/bin/python test_lvgl_cpython.py
```

Generator work lives in the [`lv_bindings`](https://github.com/PyDevices/lv_bindings) repository (`binding/emit_py_native.py`, `binding/emit_py_cp.py`, `binding/emit_cpython.py`). Runtime fixes and CPython-specific behavior belong here in `lvpy_runtime.c`.

## Related projects

- [PyDevices/lv_bindings](https://github.com/PyDevices/lv_bindings) — binding generator (MicroPython, CircuitPython, CPython targets)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — parent workspace for LVGL mod experiments
