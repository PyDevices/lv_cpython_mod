# AGENTS.md — lv_cpython_mod

Native CPython extension module `lvgl` (no MicroPython runtime). See `README.md` for usage.

**Full multi-port build matrix:** [../AGENTS.md](../AGENTS.md) (“build them all”: parallel 1–4, then Windows CPython).

## Build / test

### Unix (WSL) — use `.venv`

```bash
git submodule update --init lvgl
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/pip install -e .    # rebuild after lvpy_runtime.c or generated/lvpy.c changes

.venv/bin/python test_lvgl_cpython.py
```

### Windows — **no venv**; use `pip.exe` / `python.exe` from WSL

```bash
pip.exe install -e "$(wslpath -w ~/github/cmods/lv_cpython_mod)"
cd ~/github/cmods/lv_cpython_mod
python.exe test_lvgl_cpython.py
```

Requires MSVC Build Tools (python.org CPython). Do not use MinGW for this extension.

Run **after** parallel phase 1 (steps 1–4) completes — not alongside step 4.

## Sync from lv_bindings

```bash
./scripts/sync_from_lv_bindings.sh              # lv_bindings main
./scripts/sync_from_lv_bindings.sh --ref v9.3.0  # tag or SHA
```

Release pipeline: [PUBLISHING.md](PUBLISHING.md)

## Gotchas

- **Unix + Windows editable installs** share one tree (`build/`, `*.egg-info`, in-repo `.so`/`.pyd`). Never run `.venv/bin/pip install -e .` and `pip.exe install -e .` at the same time.
- Editable install does **not** auto-recompile on import; rerun `pip install -e .` after C changes.
- Headless: no display backend — tests use a flush callback (see `test_lvgl_cpython.py`).
- Lint: none configured in this repo.
