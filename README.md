# lv_cpython_mod

Native CPython extension for [LVGL](https://lvgl.io/), generated from [`lv_bindings`](https://github.com/PyDevices/lv_bindings) with **no MicroPython runtime**. The Python module is `lvgl`; most of the API surface is emitted into `lv_bindings/generated/lvpy.c`, with shared glue in `lvpy_runtime.c`.

## Requirements

### All platforms

- Python 3.9+ with `pip` and `setuptools`
- [`lv_bindings`](https://github.com/PyDevices/lv_bindings) checked out as a **sibling** of this directory (see layout below)
- LVGL sources inside that `lv_bindings` tree (`lv_bindings/lvgl/`, usually via submodule)
- Generated bindings: `lv_bindings/generated/lvpy.c` (from `regenerate_lvpy.sh`)

### WSL / Linux / macOS

- GCC or Clang
- `python3-dev` (or equivalent) matching your Python version

On Debian/Ubuntu:

```bash
sudo apt install python3-dev build-essential
```

### Windows (native or via WSL + `pip.exe`)

- [python.org](https://www.python.org/) CPython (or another MSVC-built Python 3.9+)
- **Microsoft C++ Build Tools** with the **Desktop development with C++** workload  
  ([Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/))
- MinGW is **not** supported for python.org Windows Python; use MSVC.

`setup.py` selects MSVC warning flags on Windows and uses a linker response file (LVGL compiles ~285 `.c` files; the raw `link.exe` command line exceeds Windows limits).

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

## Generate bindings

Regenerate whenever `lv_bindings` or `max_phase` changes, or if `generated/lvpy.c` is missing or out of date:

```bash
cd lv_bindings
./regenerate_lvpy.sh
```

Requires GCC for preprocessing (`lv_bindings/regenerate_lvpy.sh` uses `gcc -E`). Run this step from WSL or Linux even when the final install targets Windows Python.

## Build and install

`pip install` compiles `lvpy_runtime.c`, the generated `lvpy.c`, and all LVGL sources under `lv_bindings/lvgl/src`. If `lv_bindings/generated/lvpy.c` is missing, the build exits with instructions to run `regenerate_lvpy.sh`.

Use **editable** install (`-e`) while developing so the `.so` / `.pyd` in this directory stays in sync with rebuilds.

### WSL (Linux Python)

From WSL, using Linux Python in a venv:

```bash
# 1. Generate bindings (if not done already)
cd ~/github/cmods/lv_bindings && ./regenerate_lvpy.sh

# 2. Build and install
cd ../lv_cpython_mod
python3 -m venv .venv
.venv/bin/pip install -e .

# 3. Smoke test
.venv/bin/python test_lvgl_cpython.py
```

Quick import check:

```bash
.venv/bin/python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

### Windows Python from WSL (no copy to `C:\`)

You can keep the repo on the WSL filesystem and install into **Windows** Python using `pip.exe`. `setup.py` resolves the sibling `lv_bindings` tree via relative paths.

From a WSL bash shell (use a Linux cwd such as `~`, not a UNC path):

```bash
cd ~/github/cmods/lv_bindings && ./regenerate_lvpy.sh

pip.exe install -e "$(wslpath -w ~/github/cmods/lv_cpython_mod)"
```

The first build compiles every LVGL source file and may take several minutes over `\\wsl.localhost\...`. Install completes into Windows `site-packages`; the extension module is also written beside this repo (e.g. `lvgl.cp314-win_amd64.pyd`).

Verify:

```bash
python.exe -c "import lvgl as lv; lv.init(); print('ok'); lv.deinit()"
```

### Windows (native shell)

With the repo on a Windows drive (or accessed via `\\wsl.localhost\...` in PowerShell/cmd):

```powershell
cd C:\path\to\cmods\lv_bindings
# regenerate from WSL if gcc is not available on Windows:
#   wsl ./regenerate_lvpy.sh

cd ..\lv_cpython_mod
py -m pip install -e .
py -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

Open a **new** terminal after installing Build Tools so `cl.exe` is on `PATH`.

### Changing API coverage

To change how much of the API is emitted, edit `max_phase` in `lv_bindings/binding/emit_cpython.py` (currently **7**), run `regenerate_lvpy.sh`, then reinstall:

```bash
cd lv_bindings && ./regenerate_lvpy.sh
cd ../lv_cpython_mod && pip install -e .    # or pip.exe on Windows
```

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
- **Windows toolchain**: python.org CPython on Windows requires MSVC Build Tools; MinGW cannot build this extension for that interpreter.
- **Regenerate before build**: `generated/lvpy.c` must match the current `emit_py_native.py` generator (run `regenerate_lvpy.sh` after pulling `lv_bindings` changes).
- **Display OO API**: no methods on `lv_display_t` wrappers; use `lv.display_*` module functions.
- **Prototype aliasing**: some widget `tp_methods` entries may point at the wrong C function when LVGL reuses prototypes; prefer module-level names when in doubt.
- **`C_Pointer` helper struct**: emitted late; a runtime stub is used until helper emission is complete.
- **Callback lifetime**: Python wrappers clear `lv_obj->user_data` on dealloc; long-running apps should keep references to objects that register callbacks.

## Development

Regenerate after changing `lv_bindings` or `max_phase`, then reinstall (see [Build and install](#build-and-install)):

```bash
cd lv_bindings && ./regenerate_lvpy.sh
cd ../lv_cpython_mod && .venv/bin/pip install -e .
.venv/bin/python test_lvgl_cpython.py
```

Generator work lives in the [`lv_bindings`](https://github.com/PyDevices/lv_bindings) repository (`binding/emit_py_native.py`, `binding/emit_py_cp.py`, `binding/emit_cpython.py`). Runtime fixes and CPython-specific behavior belong here in `lvpy_runtime.c`.

## Related projects

- [PyDevices/lv_bindings](https://github.com/PyDevices/lv_bindings) — binding generator (MicroPython, CircuitPython, CPython targets)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — parent workspace for LVGL mod experiments
