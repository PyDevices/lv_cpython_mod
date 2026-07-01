# AGENTS.md

## Cursor Cloud specific instructions

This repo (`lv_cpython_mod`) builds a single native CPython C-extension module named
`lvgl` (LVGL bindings, no MicroPython runtime). It is a library, not a long-running
service — there is no dev server, database, or daemon. "Running" it means importing
the compiled module and exercising the API. See `README.md` for the full build/usage
reference.

### Environment layout (set up by the update script)
- Workspace venv: `/workspace/.venv` (editable install of the `lvgl` extension).
- **In-repo build inputs:** `generated/lvpy.c`, `lv_conf.h`, and the `lvgl` git submodule.
- **Sync from upstream:** `./scripts/sync_from_lv_bindings.sh` fetches `generated/lvpy.c`,
  `lv_conf.h`, and the `lvgl` pin from `PyDevices/lv_bindings` on GitHub (after
  bindings are regenerated and committed there).

### Build / test / run (use the workspace venv)
- Initialize submodules: `git submodule update --init lvgl`
- Build (after any change to `lvpy_runtime.c`): `/workspace/.venv/bin/pip install -e .`
  (or incremental `/workspace/.venv/bin/python setup.py build_ext --inplace`).
- Tests (smoke suite): `/workspace/.venv/bin/python test_lvgl_cpython.py`.
- Quick check: `/workspace/.venv/bin/python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"`.
- Lint: none configured (no flake8/ruff/black/CI). Nothing to run.

### Non-obvious gotchas
- Editable install does **not** auto-recompile C sources on import. After editing
  `lvpy_runtime.c`, you must rebuild (`pip install -e .` or `build_ext --inplace`).
- After LVGL or generator changes, regenerate in `lv_bindings`, commit `generated/lvpy.c`
  there, then run `./scripts/sync_from_lv_bindings.sh` in this repo.
- Headless: there is no display backend. Rendering is verified by registering a flush
  callback (`disp.set_flush_cb(...)`) and reading pixels via `color_p.__dereference__(n)`,
  exactly as `test_lvgl_cpython.py` does.
