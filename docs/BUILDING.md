# Building from source

Build and develop the native CPython extension locally. Day-to-day use should
prefer the TestPyPI wheel — see **[README.md](../README.md#install)**.

This repo is self-contained for a normal build: you only need **this clone**
(plus the `lvgl` submodule). No MicroPython, CircuitPython, or workspace
orchestrator is required.

## What is already vendored

These files are **committed in this repo** (they track the latest published
binding sync from [lv_bindings](https://github.com/PyDevices/lv_bindings)):

| Path | Role |
|------|------|
| `generated/lvgl_python.c` | Generated CPython binding |
| `generated/lvgl.pyi` | Type stubs (copied into the install) |
| `lv_conf.h` | LVGL config used for the build |
| `display_driver.py` | Optional helper (`import display_driver`) |
| `lvgl/` | LVGL C sources (git submodule) |

A normal `pip install -e .` compiles those vendored sources. You do **not** need
a local `lv_bindings` tree to build or test this package.

## When to clone `lv_bindings` (optional)

Clone [lv_bindings](https://github.com/PyDevices/lv_bindings) as a **sibling**
only when you are changing the **generator** or regenerating bindings:

```text
workspace/
  lv_bindings/      # optional — generator + regenerate_lvpy.sh
  lv_cpython_mod/   # this repo
```

Do that when you need to:

- Edit `lv_bindings/binding/emit_cpython.py` (e.g. `max_phase`) or other emitters
- Run `./regenerate_lvpy.sh` in lv_bindings, then sync the output into this repo
- Run the shared cross-runtime smoke script at `lv_bindings/tools/test_lvgl_smoke.py`

To refresh vendored files from GitHub **without** a sibling clone, use:

```bash
./scripts/sync_from_lv_bindings.sh          # lv_bindings main
./scripts/sync_from_lv_bindings.sh --ref SHA  # pin a commit/tag/branch
```

That script clones lv_bindings into a temp directory, copies the generated
files, and updates the `lvgl` submodule pin. Release flow: **[PUBLISHING.md](PUBLISHING.md)**.

## Requirements

### All platforms

- Python 3.9+ with `pip` and `setuptools`
- Vendored files above (already in the clone)
- `git submodule update --init lvgl`

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

`setup.py` selects MSVC warning flags on Windows and uses a linker response file
(LVGL compiles many `.c` files; the raw `link.exe` command line exceeds Windows
limits).

## Repository layout

```text
lv_cpython_mod/
├── generated/lvgl_python.c    # vendored binding (synced from lv_bindings)
├── generated/lvgl.pyi
├── lv_conf.h
├── display_driver.py
├── lvgl/                      # LVGL git submodule
├── src/lvpy_runtime.c
├── src/lvpy_runtime.h
├── tests/                     # unit tests
├── scripts/                   # sync / publish / pyodide wheel
└── setup.py
```

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/PyDevices/lv_cpython_mod.git
# or after a plain clone:
git submodule update --init lvgl
```

## Build and install

`pip install` compiles `src/lvpy_runtime.c`, `generated/lvgl_python.c`, and LVGL
sources under `lvgl/src`.

Use **editable** install (`-e`) while developing so the `.so` / `.pyd` beside
this directory stays in sync with rebuilds.

### WSL (Linux Python)

```bash
git clone --recurse-submodules https://github.com/PyDevices/lv_cpython_mod.git
cd lv_cpython_mod
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/pip install -e .
```

Quick import check:

```bash
.venv/bin/python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

Unit tests:

```bash
.venv/bin/python -m unittest discover -s tests
```

### Windows Python from WSL (no copy to `C:\`)

Keep the repo on the WSL filesystem and install into **Windows** Python with
`pip.exe`:

```bash
cd /path/to/lv_cpython_mod
pip.exe install -e "$(wslpath -w "$PWD")"
python.exe -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

The first build compiles every LVGL source and may take several minutes over
`\\wsl.localhost\...`.

### Windows (native shell)

```powershell
cd C:\path\to\lv_cpython_mod
git submodule update --init lvgl
py -m pip install -e .
py -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

Open a **new** terminal after installing Build Tools so `cl.exe` is on `PATH`.

### Changing API coverage (needs lv_bindings)

To change how much of the API is emitted, clone lv_bindings as a sibling, edit
`max_phase` in `lv_bindings/binding/emit_cpython.py`, regenerate, sync, then
reinstall:

```bash
cd ../lv_bindings && ./regenerate_lvpy.sh
cd ../lv_cpython_mod
./scripts/sync_from_lv_bindings.sh   # or copy generated files by hand
.venv/bin/pip install -e .           # or pip.exe on Windows
```

## Development

After the first full build, incremental rebuilds are much faster. With
`setuptools` and `wheel` in the target venv:

```bash
.venv/bin/python setup.py build_ext --inplace
```

Editable install does **not** recompile on import; rerun `pip install -e .` (or
`build_ext --inplace`) after C source changes (`src/lvpy_runtime.c` or
`generated/lvgl_python.c`).

- Generator work: [`lv_bindings`](https://github.com/PyDevices/lv_bindings)
- CPython runtime / packaging: this repo (`src/lvpy_runtime.c`, `setup.py`)

## Android (python-for-android)

Prefer a **prebuilt** `lvgl-cpython` wheel tagged `android_21_*` for your ABI
(`arm64_v8a` or `x86_64`) from TestPyPI. The
[pydisplay_android `lvglcpython` p4a recipe](https://github.com/PyDevices/pydisplay_android/tree/main/p4a_recipes/lvglcpython)
installs a matching wheel when `--extra-index-url` points at TestPyPI, otherwise
cross-compiles from this tree (`git submodule update --init lvgl`).

```bash
export P4A_lvgl_cpython_DIR=/path/to/lv_cpython_mod   # optional in-tree fallback
```

## Pyodide / WebAssembly

Native Linux/macOS/Windows wheels from TestPyPI are **not** loadable in Pyodide.
Each **Publish TestPyPI** release also builds a `pyemscripten_2026_0` wasm32
wheel. Preferred install in the browser:

```python
import micropip
await micropip.install("lvgl-cpython", index_urls="https://test.pypi.org/simple/")
```

### Local rebuild (optional)

- Host **Python 3.14** (match the xbuildenv CPython)
- Node.js (for Emscripten)
- Network on first run (downloads xbuildenv + emsdk)

```bash
./scripts/build_pyodide_wheel.sh
./scripts/build_pyodide_wheel.sh --no-copy   # dist/ only
PYODIDE_VERSION=314.0.0 ./scripts/build_pyodide_wheel.sh
```

## Architecture

| Component | Role |
|-----------|------|
| `generated/lvgl_python.c` (vendored) | Types, methods, module functions, callbacks |
| `src/lvpy_runtime.c` / `src/lvpy_runtime.h` | CPython glue: wrappers, convertors, GIL |
| `setup.py` | Builds the `lvgl` extension module |
| `lv_bindings` (optional sibling) | Generator only — not required to compile |

**Object wrappers** (`py_lv_obj_t`) map `lv_obj_t *` to Python and keep
per-object callback dicts. **Struct wrappers** (`py_lv_struct_t`) expose LVGL
structs.

**Event callbacks** (phase 7): pass a Python callable to `add_event_cb`. With
`user_data=None`, the wrapper stores callbacks on the target object.

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

Phases 1–7 are enabled in the generator today.

## Known limitations

- **Vendored build inputs**: you can compile from this repo alone; regenerate in
  lv_bindings only when changing the generator, then sync.
- **Windows toolchain**: python.org CPython on Windows requires MSVC Build Tools;
  MinGW cannot build this extension for that interpreter.

## Related projects

- [PyDevices/lv_bindings](https://github.com/PyDevices/lv_bindings) — binding generator
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay) — consumer of `import lvgl`
