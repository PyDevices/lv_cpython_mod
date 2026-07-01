#!/usr/bin/env bash
# Compute the next lvgl-cpython release version: <major>.<lvgl_minor>.<binding_release>
#
# Preferred: resolve X.Y.Z from the lv_bindings tag at LV_BINDINGS_REF (same scheme as
# lv_bindings regenerate_all.sh: LVGL major.minor + binding patch).
#
# Fallback: read LVGL version from lvgl/lv_version.h or lvgl/lvgl.h and increment the
# highest existing v<major>.<minor>.* tag in this repo.
#
# Usage:
#   ./scripts/next_release_version.sh
#   ./scripts/next_release_version.sh --verbose
#   LV_BINDINGS_REF=a1971cd ./scripts/next_release_version.sh --verbose
#
# Environment:
#   LV_BINDINGS_REF                 lv_bindings commit, tag, or branch (from sync CI)
#   LV_BINDINGS_REPO                default https://github.com/PyDevices/lv_bindings.git
#   LVCPYTHON_USE_REAL_LVGL_MAJOR=1 Use LVGL major from version (else 0 for TestPyPI)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
LV_BINDINGS_REPO="${LV_BINDINGS_REPO:-https://github.com/PyDevices/lv_bindings.git}"

VERBOSE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose | -v)
            VERBOSE=1
            shift
            ;;
        --help | -h)
            sed -n '2,22p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

read_lvgl_version_from_tree() {
    local major minor patch version_file
    for version_file in "$SOURCE_REPO/lvgl/lv_version.h" "$SOURCE_REPO/lvgl/lvgl.h"; do
        if [[ ! -f "$version_file" ]]; then
            continue
        fi
        major=$(grep -E '^#define LVGL_VERSION_MAJOR' "$version_file" | awk '{print $3}')
        minor=$(grep -E '^#define LVGL_VERSION_MINOR' "$version_file" | awk '{print $3}')
        patch=$(grep -E '^#define LVGL_VERSION_PATCH' "$version_file" | awk '{print $3}')
        if [[ -n "$major" && -n "$minor" && -n "$patch" ]]; then
            LVGL_MAJOR=$major
            LVGL_MINOR=$minor
            LVGL_PATCH=$patch
            return 0
        fi
    done
    return 1
}

# Echo X.Y.Z when ref is or resolves to an lv_bindings release tag.
resolve_lv_bindings_tag_version() {
    local ref="${LV_BINDINGS_REF:-}"
    [[ -z "$ref" ]] && return 1

    ref="${ref#v}"
    if [[ "$ref" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "$ref"
        return 0
    fi

    local obj refname tag tag_sha
    while read -r obj refname; do
        [[ "$refname" != *'^{}' ]] && continue
        tag="${refname#refs/tags/}"
        tag="${tag%\^{}}"
        tag="${tag#v}"
        if [[ ! "$tag" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            continue
        fi
        tag_sha="$obj"
        if [[ "$tag_sha" == "$ref" ]]; then
            echo "$tag"
            return 0
        fi
    done < <(git ls-remote --tags "$LV_BINDINGS_REPO")
    return 1
}

apply_major_override() {
    local major=$1 minor=$2 patch=$3
    if [[ "${LVCPYTHON_USE_REAL_LVGL_MAJOR:-0}" == "1" ]]; then
        echo "${major}.${minor}.${patch}"
    else
        echo "0.${minor}.${patch}"
    fi
}

VERSION_SOURCE=""
BINDINGS_TAG_VERSION=""
if BINDINGS_TAG_VERSION=$(resolve_lv_bindings_tag_version); then
    VERSION_SOURCE="lv_bindings tag v${BINDINGS_TAG_VERSION}"
    IFS=. read -r TAG_MAJOR TAG_MINOR TAG_PATCH <<< "$BINDINGS_TAG_VERSION"
    VERSION=$(apply_major_override "$TAG_MAJOR" "$TAG_MINOR" "$TAG_PATCH")
    read_lvgl_version_from_tree || true
else
    if ! read_lvgl_version_from_tree; then
        echo "Error: could not read LVGL version from lvgl/lv_version.h or lvgl/lvgl.h" >&2
        echo "Set LV_BINDINGS_REF to an lv_bindings release tag (e.g. v9.2.0)." >&2
        exit 1
    fi
    VERSION_SOURCE="lvgl tree (${LVGL_MAJOR}.${LVGL_MINOR}.${LVGL_PATCH})"
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
fi

if [[ "$VERBOSE" -eq 1 ]]; then
    echo "Version source: ${VERSION_SOURCE}"
    if [[ -n "${BINDINGS_TAG_VERSION:-}" ]]; then
        echo "lv_bindings release: v${BINDINGS_TAG_VERSION}"
        if [[ "${LVCPYTHON_USE_REAL_LVGL_MAJOR:-0}" == "1" ]]; then
            echo "Published version: ${VERSION} (LVGL major from lv_bindings tag)"
        else
            echo "Published version: ${VERSION} (major 0 TestPyPI override; lv_bindings v${BINDINGS_TAG_VERSION})"
        fi
    else
        echo "LVGL source: ${LVGL_MAJOR}.${LVGL_MINOR}.${LVGL_PATCH}"
        if [[ "${LVCPYTHON_USE_REAL_LVGL_MAJOR:-0}" == "1" ]]; then
            echo "Published major: ${VERSION_MAJOR} (from LVGL)"
        else
            echo "Published major: 0 (TestPyPI test override; LVGL major is ${LVGL_MAJOR})"
        fi
        echo "Published minor: ${VERSION_MINOR} (LVGL minor)"
        echo "Next binding release: ${VERSION##*.}"
    fi
    echo "Next version: ${VERSION}"
else
    echo "$VERSION"
fi
