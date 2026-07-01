#!/usr/bin/env bash
# Compute the next lvgl-cpython release version: <major>.<lvgl_minor>.<binding_release>
#
#   major           — 0 while testing on TestPyPI (override); LVGL_VERSION_MAJOR when
#                     LVCPYTHON_USE_REAL_LVGL_MAJOR=1
#   lvgl_minor      — from lvgl/lvgl.h (LVGL_VERSION_MINOR)
#   binding_release — one greater than the highest existing v<major>.<minor>.* tag
#
# Usage (from repo root or anywhere):
#   ./scripts/next_release_version.sh
#   ./scripts/next_release_version.sh --verbose
#
# Environment:
#   LVCPYTHON_USE_REAL_LVGL_MAJOR=1  Use LVGL major from lvgl.h instead of test major 0

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

VERBOSE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose | -v)
            VERBOSE=1
            shift
            ;;
        --help | -h)
            sed -n '2,18p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

LVGL_H="$SOURCE_REPO/lvgl/lvgl.h"
if [[ ! -f "$LVGL_H" ]]; then
    echo "Error: $LVGL_H not found (run: git submodule update --init lvgl)." >&2
    exit 1
fi

read_lvgl_define() {
    local name=$1
    sed -n "s/^#define ${name}[[:space:]]\\+\\([0-9]\\+\\).*/\\1/p" "$LVGL_H" | head -1
}

LVGL_MAJOR=$(read_lvgl_define LVGL_VERSION_MAJOR)
LVGL_MINOR=$(read_lvgl_define LVGL_VERSION_MINOR)
LVGL_PATCH=$(read_lvgl_define LVGL_VERSION_PATCH)

if [[ -z "$LVGL_MAJOR" || -z "$LVGL_MINOR" ]]; then
    echo "Error: could not parse LVGL version from $LVGL_H" >&2
    exit 1
fi

if [[ "${LVCPYTHON_USE_REAL_LVGL_MAJOR:-0}" == "1" ]]; then
    VERSION_MAJOR=$LVGL_MAJOR
else
    VERSION_MAJOR=0
fi
VERSION_MINOR=$LVGL_MINOR

cd "$SOURCE_REPO"

LAST_BINDING=-1
while IFS= read -r tag; do
    [[ -z "$tag" ]] && continue
    tag="${tag#v}"
    if [[ "$tag" =~ ^${VERSION_MAJOR}\.${VERSION_MINOR}\.([0-9]+)$ ]]; then
        patch="${BASH_REMATCH[1]}"
        if (( patch > LAST_BINDING )); then
            LAST_BINDING=$patch
        fi
    fi
done < <(git tag -l "v${VERSION_MAJOR}.${VERSION_MINOR}.*")

NEXT_BINDING=$((LAST_BINDING + 1))
VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${NEXT_BINDING}"

if [[ "$VERBOSE" -eq 1 ]]; then
    echo "LVGL source: ${LVGL_MAJOR}.${LVGL_MINOR}.${LVGL_PATCH} ($LVGL_H)"
    if [[ "$VERSION_MAJOR" == "$LVGL_MAJOR" ]]; then
        echo "Published major: ${VERSION_MAJOR} (from LVGL)"
    else
        echo "Published major: ${VERSION_MAJOR} (TestPyPI test override; LVGL major is ${LVGL_MAJOR})"
    fi
    echo "Published minor: ${VERSION_MINOR} (LVGL minor)"
    if (( LAST_BINDING >= 0 )); then
        echo "Last binding release: v${VERSION_MAJOR}.${VERSION_MINOR}.${LAST_BINDING}"
    else
        echo "Last binding release: (none for v${VERSION_MAJOR}.${VERSION_MINOR}.*)"
    fi
    echo "Next binding release: ${NEXT_BINDING}"
    echo "Next version: ${VERSION}"
else
    echo "$VERSION"
fi
