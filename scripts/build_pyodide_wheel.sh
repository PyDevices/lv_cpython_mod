#!/usr/bin/env bash
# Build an Emscripten / Pyodide wheel for import lvgl (micropip-installable).
#
# Target ABI: pyemscripten_2026_0 (Python 3.14 / Pyodide 314.x) — matches the
# Pyodide vendored by pydisplay (web/pyscript/vendor/pyodide).
#
# Usage (from repo root):
#   ./scripts/build_pyodide_wheel.sh
#   ./scripts/build_pyodide_wheel.sh --no-copy   # leave wheel in dist/ only
#   PYODIDE_VERSION=314.0.0 ./scripts/build_pyodide_wheel.sh
#
# Requires: Linux, Python 3.14 (host must match xbuildenv Python), Node.js,
# network on first run (downloads xbuildenv + emsdk). Does not need Docker.
#
# Tip: install a matching host Python with uv if needed:
#   curl -LsSf https://astral.sh/uv/install.sh | sh
#   uv python install 3.14
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PYODIDE_VERSION="${PYODIDE_VERSION:-314.0.0}"
VENV="${PYODIDE_BUILD_VENV:-$ROOT/.venv-pyodide-build}"
OUT_WHEELS="${OUT_WHEELS:-$ROOT/web/wheels}"
COPY_TO_WEB=1

for arg in "$@"; do
  case "$arg" in
    --no-copy) COPY_TO_WEB=0 ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 2
      ;;
  esac
done

if [[ ! -f generated/lvgl_python.c ]]; then
  echo "error: generated/lvgl_python.c missing — run scripts/sync_from_lv_bindings.sh" >&2
  exit 1
fi
if [[ ! -f lvgl/lvgl.h ]]; then
  echo "error: lvgl submodule missing — run: git submodule update --init lvgl" >&2
  exit 1
fi

# Host Python must match the xbuildenv CPython major.minor (3.14 for 314.x).
pick_python() {
  if command -v uv >/dev/null 2>&1; then
    uv python find 3.14 2>/dev/null && return 0
  fi
  for cand in python3.14 python3; do
    if command -v "$cand" >/dev/null 2>&1; then
      ver="$("$cand" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
      if [[ "$ver" == "3.14" ]]; then
        command -v "$cand"
        return 0
      fi
    fi
  done
  return 1
}

if ! PY="$(pick_python)"; then
  echo "error: need host Python 3.14 to build Pyodide ${PYODIDE_VERSION} wheels" >&2
  echo "  Install with: uv python install 3.14" >&2
  echo "  (or pyenv / deadsnakes / python.org)" >&2
  exit 1
fi

if [[ ! -d "$VENV" ]] || ! "$VENV/bin/python" -c 'import sys; raise SystemExit(0 if sys.version_info[:2]==(3,14) else 1)' 2>/dev/null; then
  echo "Creating build venv with $($PY --version) …"
  rm -rf "$VENV"
  if command -v uv >/dev/null 2>&1; then
    # --seed installs pip so subsequent `python -m pip` works.
    uv venv --seed --python "$PY" "$VENV"
  else
    "$PY" -m venv "$VENV"
  fi
fi

# shellcheck disable=SC1091
source "$VENV/bin/activate"
if command -v uv >/dev/null 2>&1; then
  uv pip install -q -U "pyodide-build>=0.30"
else
  python -m pip install -q -U pip
  python -m pip install -q -U "pyodide-build>=0.30"
fi

echo "Installing / selecting xbuildenv ${PYODIDE_VERSION} …"
pyodide xbuildenv install "${PYODIDE_VERSION}"
pyodide xbuildenv use "${PYODIDE_VERSION}"

echo "Building lvgl-cpython for Pyodide ${PYODIDE_VERSION} (emscripten $(pyodide config get emscripten_version)) …"
rm -rf dist
mkdir -p dist

# pyodide build wraps PEP 517 and cross-compiles setuptools Extension("lvgl").
pyodide build . -o dist/ -v

shopt -s nullglob
wheels=(
  dist/lvgl_cpython-*-pyemscripten_*_wasm32.whl
  dist/lvgl_cpython-*-pyodide_*_wasm32.whl
  dist/lvgl_cpython-*-emscripten_*_wasm32.whl
)
if ((${#wheels[@]} == 0)); then
  echo "error: no Pyodide/wasm wheel produced in dist/" >&2
  ls -la dist/ >&2 || true
  exit 1
fi

echo "Built:"
printf '  %s\n' "${wheels[@]}"

if [[ "$COPY_TO_WEB" -eq 1 ]]; then
  mkdir -p "$OUT_WHEELS"
  # Keep only current wasm wheels in the Pages-served folder.
  rm -f "$OUT_WHEELS"/lvgl_cpython-*-pyemscripten_*_wasm32.whl \
        "$OUT_WHEELS"/lvgl_cpython-*-pyodide_*_wasm32.whl \
        "$OUT_WHEELS"/lvgl_cpython-*-emscripten_*_wasm32.whl \
        "$OUT_WHEELS"/lvgl_cpython-pyemscripten_2026_0_wasm32.whl
  cp -f "${wheels[@]}" "$OUT_WHEELS/"
  # Index for micropip consumers (pydisplay pyodide.html): must name a real wheel file.
  primary="${wheels[0]}"
  for w in "${wheels[@]}"; do
    if [[ "$w" == *pyemscripten_2026_0_wasm32.whl ]]; then
      primary="$w"
      break
    fi
  done
  primary_base="$(basename "$primary")"
  printf '%s\n' "{\"wheel\": \"${primary_base}\"}" > "$OUT_WHEELS/lvgl.json"
  echo "Copied to $OUT_WHEELS/"
  printf '  %s\n' "$OUT_WHEELS"/lvgl_cpython-*.whl
  echo "  $OUT_WHEELS/lvgl.json → ${primary_base}"
fi

cat <<EOF

Next:
  # Local micropip (needs COI-friendly server if testing from a page):
  #   await micropip.install('./wheels/<wheel-name>')
  #
  # Or with pyodide venv:
  #   pyodide venv .venv-pyodide && source .venv-pyodide/bin/activate
  #   pip install ${wheels[0]}
  #   python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"
EOF
