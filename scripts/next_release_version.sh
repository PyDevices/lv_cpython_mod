#!/usr/bin/env bash
# Compute the next lvgl-cpython release version: <major>.<lvgl_minor>.<release>
#
#   major, lvgl_minor — from the lv_bindings tag at LV_BINDINGS_REF when present,
#                       else lvgl/lv_version.h or lvgl/lvgl.h
#   release           — lv_cpython_mod-only counter: highest v<major>.<minor>.* + 1
#                       (starts at 0; independent of lv_bindings binding patch)
#
# Usage:
#   ./scripts/next_release_version.sh
#   ./scripts/next_release_version.sh --verbose
#   LV_BINDINGS_REF=a1971cd ./scripts/next_release_version.sh --verbose
#
# Environment:
#   LV_BINDINGS_REF     lv_bindings commit, tag, or branch (from sync CI)
#   LV_BINDINGS_REPO    default https://github.com/PyDevices/lv_bindings.git

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

next_release_patch() {
    local version_major=$1 version_minor=$2
    local last=-1 patch tag

    cd "$SOURCE_REPO"
    while IFS= read -r tag; do
        [[ -z "$tag" ]] && continue
        tag="${tag#v}"
        if [[ "$tag" =~ ^${version_major}\.${version_minor}\.([0-9]+)$ ]]; then
            patch="${BASH_REMATCH[1]}"
            if (( patch > last )); then
                last=$patch
            fi
        fi
    done < <(git tag -l "v${version_major}.${version_minor}.*")

    echo $((last + 1))
}

VERSION_SOURCE=""
BINDINGS_TAG_VERSION=""
LVGL_MAJOR=""
LVGL_MINOR=""
LVGL_PATCH=""

if BINDINGS_TAG_VERSION=$(resolve_lv_bindings_tag_version); then
    IFS=. read -r LVGL_MAJOR LVGL_MINOR _BINDINGS_PATCH <<< "$BINDINGS_TAG_VERSION"
    VERSION_SOURCE="lv_bindings tag v${BINDINGS_TAG_VERSION} (major.minor only)"
    LVGL_PATCH="${_BINDINGS_PATCH}"
elif read_lvgl_version_from_tree; then
    VERSION_SOURCE="lvgl tree (${LVGL_MAJOR}.${LVGL_MINOR}.${LVGL_PATCH})"
else
    echo "Error: could not determine LVGL major.minor." >&2
    echo "Set LV_BINDINGS_REF to an lv_bindings release tag, or init the lvgl submodule." >&2
    exit 1
fi

VERSION_MAJOR=$LVGL_MAJOR
VERSION_MINOR=$LVGL_MINOR
NEXT_RELEASE=$(next_release_patch "$VERSION_MAJOR" "$VERSION_MINOR")
VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${NEXT_RELEASE}"

if [[ "$VERBOSE" -eq 1 ]]; then
    echo "Version source: ${VERSION_SOURCE}"
    if [[ -n "${BINDINGS_TAG_VERSION:-}" ]]; then
        echo "lv_bindings tag: v${BINDINGS_TAG_VERSION}"
    fi
    if [[ -n "${LVGL_MAJOR:-}" ]]; then
        echo "LVGL API line: ${LVGL_MAJOR}.${LVGL_MINOR}"
        echo "Published major: ${VERSION_MAJOR} (from LVGL)"
    fi
    echo "Published minor: ${VERSION_MINOR} (LVGL minor)"
    if (( NEXT_RELEASE > 0 )); then
        echo "Last lv_cpython_mod release: v${VERSION_MAJOR}.${VERSION_MINOR}.$((NEXT_RELEASE - 1))"
    else
        echo "Last lv_cpython_mod release: (none for v${VERSION_MAJOR}.${VERSION_MINOR}.*)"
    fi
    echo "Next lv_cpython_mod release: ${NEXT_RELEASE}"
    echo "Next version: ${VERSION}"
else
    echo "$VERSION"
fi
