#!/usr/bin/env bash
# Create and push the next release tag. Version is computed automatically as
#   <major>.<lvgl_minor>.<binding_release>
# where lvgl_minor comes from lv_bindings and binding_release increments from the
# latest matching git tag. Pushing the tag triggers publish-testpypi.
#
# Usage:
#   ./scripts/publish_release_tag.sh
#   ./scripts/publish_release_tag.sh --push
#   ./scripts/next_release_version.sh --verbose   # preview only

set -euo pipefail

DO_PUSH=0
DRY_RUN=0

usage() {
    cat <<'EOF'
Usage: ./scripts/publish_release_tag.sh [--push] [--dry-run]

Create an annotated git tag for the next auto-computed release version:

  <major>.<lvgl_minor>.<binding_release>

  lvgl_minor       — from the lv_bindings release tag when LV_BINDINGS_REF is set,
                     else from lvgl/lv_version.h or lvgl/lvgl.h
  binding_release  — patch from the lv_bindings tag, or highest v<major>.<minor>.* + 1
  major            — 0 for TestPyPI testing (set LVCPYTHON_USE_REAL_LVGL_MAJOR=1
                     to use LVGL_VERSION_MAJOR when ready for real releases)

Options:
  --push      Push the tag to origin (triggers the publish-testpypi workflow)
  --dry-run   Print the version that would be tagged; do not create a tag

Preview the next version:
  ./scripts/next_release_version.sh --verbose

Requires repository secret TESTPYPI_API_TOKEN (same token as PyDevices/pydisplay).
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --push)
            DO_PUSH=1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --help | -h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION="$("$SCRIPT_DIR/next_release_version.sh")"
TAG="v$VERSION"

cd "$SOURCE_REPO"

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Error: working tree has uncommitted changes; commit or stash before tagging." >&2
    exit 1
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Error: tag $TAG already exists ($(git rev-parse --short "$TAG^{commit}"))" >&2
    exit 1
fi

"$SCRIPT_DIR/next_release_version.sh" --verbose

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "Dry run — would create tag $TAG on $(git rev-parse --short HEAD)"
    exit 0
fi

git tag -a "$TAG" -m "Release $VERSION"
echo "Created annotated tag $TAG on $(git rev-parse --short HEAD)"

if [[ "$DO_PUSH" -eq 1 ]]; then
    git push origin "$TAG"
    echo "Pushed $TAG — publish-testpypi workflow should start shortly."
else
    echo "Push to publish: git push origin $TAG"
fi
