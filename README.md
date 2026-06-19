# lv_cpython_mod

Native CPython extension for LVGL, generated from `lv_bindings` (no MicroPython runtime).

## Build

```bash
# From workspace root (cmods/)
cd lv_bindings && ./regenerate_lvpy.sh

cd ../lv_cpython_mod
python3 -m venv .venv
.venv/bin/pip install -e .
.venv/bin/python test_lvgl_cpython.py
```

## Status

| Phase | Coverage |
|-------|----------|
| 1 | `init`/`deinit`, int constants, `LV_SYMBOL_*` strings |
| 2 | Enum namespaces (`lv.EVENT.CLICKED`, etc.) |
| 3–7 | Structs, widgets, module functions, callbacks — **not yet implemented** |

Set `max_phase` in `lv_bindings/binding/emit_cpython.py` as native emission expands.
