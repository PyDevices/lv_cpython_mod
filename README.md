# lv_cpython_mod

Native CPython extension for [LVGL](https://lvgl.io/), generated from [`lv_bindings`](https://github.com/PyDevices/lv_bindings) with **no MicroPython runtime**. The Python module is `lvgl`; the API surface is in `generated/lvpy.c`, with shared glue in `lvpy_runtime.c`. LVGL sources live in the `lvgl` git submodule.

## Requirements

### All platforms

- Python 3.9+ with `pip` and `setuptools`
- `generated/lvpy.c` and `lv_conf.h` (committed in this repo; sync from lv_bindings with `./scripts/sync_from_lv_bindings.sh`)
- LVGL sources: `lvgl/` git submodule (`git submodule update --init lvgl`)

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

```text
lv_cpython_mod/
├── generated/lvpy.c    # CPython binding (synced from lv_bindings)
├── lv_conf.h           # LVGL config (synced from lv_bindings)
├── lvgl/               # LVGL git submodule
├── lvpy_runtime.c
├── lvpy_runtime.h
└── setup.py
```

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/PyDevices/lv_cpython_mod.git
# or after a plain clone:
git submodule update --init lvgl
```

## Sync and publish

Binding updates flow from [lv_bindings](https://github.com/PyDevices/lv_bindings) into this repo and onto TestPyPI. See **[PUBLISHING.md](PUBLISHING.md)** for the full pipeline (automatic triggers, `gh` CLI without a clone, secrets, and versioning).

Quick manual sync from GitHub:

```bash
./scripts/sync_from_lv_bindings.sh          # lv_bindings main
./scripts/sync_from_lv_bindings.sh --ref SHA  # specific commit
```

## Build and install

`pip install` compiles `lvpy_runtime.c`, `generated/lvpy.c`, and all LVGL sources under `lvgl/src`.

Use **editable** install (`-e`) while developing so the `.so` / `.pyd` in this directory stays in sync with rebuilds.

### WSL (Linux Python)

From WSL, using Linux Python in a venv:

```bash
git clone --recurse-submodules https://github.com/PyDevices/lv_cpython_mod.git
cd lv_cpython_mod
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
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

- **Build inputs are vendored here**: `generated/lvpy.c`, `lv_conf.h`, and the `lvgl` submodule are synced from lv_bindings (see [PUBLISHING.md](PUBLISHING.md)); generator work still happens in lv_bindings.
- **Windows toolchain**: python.org CPython on Windows requires MSVC Build Tools; MinGW cannot build this extension for that interpreter.
- **Regenerate in lv_bindings first**: after generator changes, run `regenerate_lvpy.sh` there, commit `generated/lvpy.c`, then sync or let CI sync (see [PUBLISHING.md](PUBLISHING.md)).
- **Display OO API**: no methods on `lv_display_t` wrappers; use `lv.display_*` module functions.
- **Prototype aliasing**: some widget `tp_methods` entries may point at the wrong C function when LVGL reuses prototypes; prefer module-level names when in doubt.
- **`C_Pointer` helper struct**: emitted late; a runtime stub is used until helper emission is complete.
- **Callback lifetime**: Per-registration callback dicts (`add_event_cb` with `user_data=None`) are released when the event is removed or the object is deleted. Python wrappers clear `lv_obj->user_data` on dealloc; long-running apps should still keep references to objects that register callbacks.

## Development

Regenerate after changing `lv_bindings` or `max_phase`, then reinstall (see [Build and install](#build-and-install)):

```bash
cd lv_bindings && ./regenerate_lvpy.sh
cd ../lv_cpython_mod && .venv/bin/pip install -e .
.venv/bin/python test_lvgl_cpython.py
```

After the first full build, incremental rebuilds are much faster (only changed sources such as `generated/lvpy.c` are recompiled). From `lv_cpython_mod`, with `setuptools` and `wheel` installed in the target venv:

```bash
.venv/bin/python setup.py build_ext --inplace
```

With an editable install (`pip install -e .`), the updated `.so` / `.pyd` beside this repo is picked up immediately — no reinstall step needed.

Generator work lives in the [`lv_bindings`](https://github.com/PyDevices/lv_bindings) repository (`binding/emit_py_native.py`, `binding/emit_py_cp.py`, `binding/emit_cpython.py`). Runtime fixes and CPython-specific behavior belong here in `lvpy_runtime.c`.

## Related projects

- [PyDevices/lv_bindings](https://github.com/PyDevices/lv_bindings) — binding generator (MicroPython, CircuitPython, CPython targets)
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — parent workspace for LVGL mod experiments
