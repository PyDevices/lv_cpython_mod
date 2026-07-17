# lvgl-cpython

Native CPython extension for [LVGL](https://lvgl.io/), generated from [`lv_bindings`](https://github.com/PyDevices/lv_bindings) with **no MicroPython runtime**.

> **Pip name:** `lvgl-cpython` · **Import:** `import lvgl as lv`

## Install

Prebuilt wheels are published as **`lvgl-cpython`** on [TestPyPI](https://test.pypi.org/project/lvgl-cpython/) (import as `lvgl`). CI builds a separate wheel for each CPython minor (3.10–3.14) on Linux x86_64 and Windows x64, **Android** wheels for **3.13–3.14** (`android_21_arm64_v8a`, `android_21_x86_64` per [PEP 738](https://peps.python.org/pep-0738/)), plus a **Pyodide** `pyemscripten_2026_0_wasm32` wheel (`cp314`) — pip/micropip select the tag that matches your interpreter.

**Android (python-for-android / pydisplay):** install the matching wheel from TestPyPI when building an APK, or let the `lvglcpython` p4a recipe fetch it (see [pydisplay_android](https://github.com/PyDevices/pydisplay_android)):

```bash
pip install -i https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ \
  --only-binary=:all: --platform android_21_arm64_v8a lvgl-cpython
```

```bash
pip install -i https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ lvgl-cpython
```

**Pyodide / micropip** (browser WASM; same project):

```python
import micropip
await micropip.install("lvgl-cpython", index_urls="https://test.pypi.org/simple/")
```

Pin a release:

```bash
pip install -i https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ lvgl-cpython==9.5.0
```

`--extra-index-url https://pypi.org/simple/` lets pip fetch dependencies (e.g. setuptools) from PyPI. Use the same `pip` / `python` you will run (e.g. `pip.exe` with Windows 3.14).

Quick check:

```bash
python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

**WSL + Windows Python** — `pip.exe` / `python.exe` on PATH:

```bash
pip.exe install -i https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ lvgl-cpython==9.5.0
python.exe -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
```

To build from source instead, see **[BUILDING.md](BUILDING.md)**. Release and packaging details: **[PUBLISHING.md](PUBLISHING.md)**.

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

## Type checking

Wheels install `lvgl.pyi` next to the `lvgl` extension module. For editable
builds, `pip install -e .` copies `generated/lvgl.pyi` at build time.

If your checker does not pick it up, set `python.analysis.stubPath` (Pylance) or
`stubPath` (pyright) to a directory containing `lvgl.pyi`, or use a
`typings/lvgl/__init__.pyi` that symlinks to the stub file from
[`lv_bindings`](https://github.com/PyDevices/lv_bindings) `generated/lvgl.pyi`.

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

### Limitations

- **Displays use module functions, not methods.** After `disp = lv.display_create(...)`, configure the display with calls like `lv.display_set_flush_cb(disp, cb)` — not `disp.set_flush_cb(cb)`.
- **Prefer module functions if a widget method acts wrong.** Some widgets share similar LVGL internals, so an occasional method on one widget type may not map to the function you expect. If something looks off, try the matching `lv.*` module function (same name as in the LVGL C API).
- **Low-level pointer helpers are incomplete.** A few advanced pointer utilities are not fully exposed in Python yet; the binding uses a small placeholder where needed.
- **Keep widgets alive while they have callbacks.** When you call `add_event_cb(handler, ...)` with `user_data=None`, the binding stores your handler on that widget. If Python garbage-collects the widget while LVGL still uses it, the callback can stop working. Hold a reference (e.g. keep it in a variable or list) for as long as the callback should run. Callbacks are removed when you delete the event or destroy the widget.

## Links

- [Source](https://github.com/PyDevices/lv_cpython_mod)
- [Issues](https://github.com/PyDevices/lv_cpython_mod/issues)
- [BUILDING.md](BUILDING.md) — build from source
- [PUBLISHING.md](PUBLISHING.md) — releases and TestPyPI CI
- Related: [lv_bindings](https://github.com/PyDevices/lv_bindings), [pydisplay](https://github.com/PyDevices/pydisplay)

## License

MIT — see [LICENSE](LICENSE).
