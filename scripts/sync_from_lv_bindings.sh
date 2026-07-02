#!/usr/bin/env bash
# Sync generated/lvgl_python.c, lv_conf.h, and the lvgl submodule pin from PyDevices/lv_bindings
# on GitHub (not the local workspace).
#
# Usage:
#   ./scripts/sync_from_lv_bindings.sh
#   ./scripts/sync_from_lv_bindings.sh --ref abc1234
#   LV_BINDINGS_REF=main ./scripts/sync_from_lv_bindings.sh
#
# After syncing, commit the updated files and lvgl submodule SHA in this repo.

set -euo pipefail

LV_BINDINGS_REPO="${LV_BINDINGS_REPO:-https://github.com/PyDevices/lv_bindings.git}"
LV_BINDINGS_REF="${LV_BINDINGS_REF:-main}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

REF="$LV_BINDINGS_REF"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --ref)
            REF=$2
            shift 2
            ;;
        --help | -h)
            sed -n '2,12p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Short-lived clone under /tmp so we never read the local sibling lv_bindings tree.
TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

echo "Fetching ${LV_BINDINGS_REPO} @ ${REF}..."
echo "(using temp clone ${TMP}/lv_bindings — removed on exit)"
git clone --filter=blob:none --no-checkout "${LV_BINDINGS_REPO}" "${TMP}/lv_bindings"

echo "Checking out generated/lvgl_python.c and lv_conf.h..."
git -C "${TMP}/lv_bindings" checkout "${REF}" -- generated/lvgl_python.c lv_conf.h

LVPY_SRC="${TMP}/lv_bindings/generated/lvgl_python.c"
LV_CONF_SRC="${TMP}/lv_bindings/lv_conf.h"
if [[ ! -f "$LVPY_SRC" ]]; then
    echo "Error: generated/lvgl_python.c not found on ${REF}." >&2
    echo "Regenerate and commit generated/lvgl_python.c in lv_bindings first." >&2
    exit 1
fi
if [[ ! -f "$LV_CONF_SRC" ]]; then
    echo "Error: lv_conf.h not found on ${REF}." >&2
    exit 1
fi

# Read the pinned lvgl commit from git metadata — no submodule clone (avoids SSH URLs).
echo "Reading lvgl submodule pin from lv_bindings..."
LVGL_SHA=$(git -C "${TMP}/lv_bindings" ls-tree "${REF}" lvgl | awk '{print $3}')
if [[ -z "$LVGL_SHA" || "$LVGL_SHA" == "lvgl" ]]; then
    echo "Error: could not read lvgl submodule commit from lv_bindings ${REF}." >&2
    exit 1
fi

mkdir -p "${SOURCE_REPO}/generated"
cp "$LVPY_SRC" "${SOURCE_REPO}/generated/lvgl_python.c"
cp "$LV_CONF_SRC" "${SOURCE_REPO}/lv_conf.h"

cd "${SOURCE_REPO}"
if [[ ! -f .gitmodules ]]; then
    echo "Error: lvgl submodule not configured in this repo." >&2
    exit 1
fi

echo "Updating local lvgl submodule to ${LVGL_SHA}..."
git submodule update --init lvgl
git -C lvgl fetch origin "${LVGL_SHA}" 2>/dev/null || git -C lvgl fetch origin
git -C lvgl checkout "${LVGL_SHA}"

echo
echo "Synced from lv_bindings ${REF}:"
echo "  generated/lvgl_python.c"
echo "  lv_conf.h"
echo "  lvgl @ ${LVGL_SHA}"
echo
echo "Commit when ready:"
echo "  git add generated/lvgl_python.c lv_conf.h lvgl"
echo "  git commit -m \"Sync bindings and LVGL from lv_bindings ${REF}.\""
