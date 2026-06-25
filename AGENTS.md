# AGENTS.md

## Cursor Cloud specific instructions

This repo (`lv_cpython_mod`) builds a single native CPython C-extension module named
`lvgl` (LVGL bindings, no MicroPython runtime). It is a library, not a long-running
service — there is no dev server, database, or daemon. "Running" it means importing
the compiled module and exercising the API. See `README.md` for the full build/usage
reference.

### Environment layout (set up by the update script)
- Workspace venv: `/workspace/.venv` (editable install of the `lvgl` extension).
- External sibling dependency: `/lv_bindings` (cloned from
  `https://github.com/PyDevices/lv_bindings`, with the `lvgl` git submodule).
  `setup.py` hardcodes this path as `<parent of repo>/lv_bindings` (here `/lv_bindings`).
  It is **not vendored** in this repo and lives outside `/workspace`.
- Generator venv: `/lv_bindings/.venv` (has `pycparser==2.21`, used only to emit bindings).
- Generated bindings: `/lv_bindings/generated/lvpy.c` (gitignored, ~3 MB). `setup.py`
  hard-fails if this file is missing.

### Build / test / run (use the workspace venv)
- Build (after any change to `lvpy_runtime.c`): `/workspace/.venv/bin/pip install -e .`
  (or incremental `/workspace/.venv/bin/python setup.py build_ext --inplace`).
- Tests (smoke suite): `/workspace/.venv/bin/python test_lvgl_cpython.py`.
- Quick check: `/workspace/.venv/bin/python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"`.
- Lint: none configured (no flake8/ruff/black/CI). Nothing to run.

### Non-obvious gotchas
- Editable install does **not** auto-recompile C sources on import. After editing
  `lvpy_runtime.c`, you must rebuild (`pip install -e .` or `build_ext --inplace`).
- After editing anything that changes the generated surface, regenerate first:
  `cd /lv_bindings && ./regenerate_lvpy.sh` (requires `gcc -E` and the
  `/lv_bindings/.venv` with `pycparser==2.21`), then rebuild the extension.
- The LVGL submodule clones via an `ssh://git@github.com` URL in `.gitmodules`; on this
  VM that resolves over HTTPS and succeeds. If a fresh clone ever fails on the submodule,
  re-run `git -C /lv_bindings submodule update --init` after rewriting the URL to HTTPS.
- Headless: there is no display backend. Rendering is verified by registering a flush
  callback (`disp.set_flush_cb(...)`) and reading pixels via `color_p.__dereference__(n)`,
  exactly as `test_lvgl_cpython.py` does.
