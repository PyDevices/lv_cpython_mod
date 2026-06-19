"""Build the lvgl CPython extension (generated/lvpy.c + LVGL sources)."""

from __future__ import annotations

import os
from pathlib import Path

from setuptools import Extension, setup

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent
LV_BINDINGS = WORKSPACE / "lv_bindings"
LVGL_DIR = LV_BINDINGS / "lvgl"
GENERATED = LV_BINDINGS / "generated" / "lvpy.c"

if not GENERATED.is_file():
    raise SystemExit(
        f"{GENERATED} not found. Run: {LV_BINDINGS / 'regenerate_lvpy.sh'}"
    )

lvgl_sources = [
    os.path.relpath(p, ROOT)
    for p in (LVGL_DIR / "src").rglob("*.c")
    if p.name != "tjpgd.c"
]

runtime_sources = [
    "lvpy_runtime.c",
    os.path.relpath(GENERATED, ROOT),
]

include_dirs = [
    os.path.relpath(LV_BINDINGS, ROOT),
    os.path.relpath(LVGL_DIR, ROOT),
    ".",
]

define_macros = [("CMODS_CPYTHON_BUILD", "1")]

extra_compile_args = ["-Wno-unused-function", "-Wno-sign-compare"]

ext = Extension(
    "lvgl",
    sources=runtime_sources + lvgl_sources,
    include_dirs=include_dirs,
    define_macros=define_macros,
    extra_compile_args=extra_compile_args,
)

setup(
    name="lvgl-cpython",
    version="0.1.0",
    description="LVGL bindings for CPython (generated)",
    ext_modules=[ext],
    python_requires=">=3.9",
)
