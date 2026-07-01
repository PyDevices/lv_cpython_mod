# Publishing to TestPyPI

How bindings changes in [lv_bindings](https://github.com/PyDevices/lv_bindings) become a versioned **`lvgl-cpython`** wheel on [TestPyPI](https://test.pypi.org/project/lvgl-cpython/).

You do **not** need a local clone of this repo for release — GitHub Actions can sync, tag, build, and upload from the web UI or `gh` CLI.

## Pipeline overview

```text
lv_bindings (your machine or CI)
  regenerate lvpy.c → commit → push main
           │
           ▼
lv_bindings: Trigger lv_cpython_mod release   (on push to generated/lvpy.c, lv_conf.h, lvgl)
           │
           ▼
lv_cpython_mod: Sync and release
  sync files from GitHub → commit main → push tag v0.<minor>.<N>
           │
           ▼
lv_cpython_mod: Publish TestPyPI             (on tag push v*.*.*)
  build → test → auditwheel → twine upload
```

## Version numbers

Tags and PyPI versions use **`0.<lvgl_minor>.<binding_release>`** while testing on TestPyPI:

| Part | Source |
|------|--------|
| `0` | TestPyPI override (use `LVCPYTHON_USE_REAL_LVGL_MAJOR=1` for real LVGL major, e.g. `9.1.N`) |
| `<lvgl_minor>` | `LVGL_VERSION_MINOR` in `lvgl/lvgl.h` |
| `<binding_release>` | Auto-incremented from existing `v0.<minor>.*` tags |

Preview the next version:

```bash
./scripts/next_release_version.sh --verbose
```

## One-time setup

### lv_cpython_mod secrets

| Secret | Required | Purpose |
|--------|----------|---------|
| `TESTPYPI_API_TOKEN` | yes | Upload wheels to TestPyPI |

Settings → Secrets and variables → Actions on [PyDevices/lv_cpython_mod](https://github.com/PyDevices/lv_cpython_mod).

Use the same TestPyPI token as [pydisplay](https://github.com/PyDevices/pydisplay) if you already have one.

### lv_bindings secret (automatic releases)

| Secret | Required | Purpose |
|--------|----------|---------|
| `LVCPYTHON_MOD_DISPATCH_TOKEN` | yes for auto-trigger | PAT with **`actions:write`** on `PyDevices/lv_cpython_mod` |

Settings → Secrets and variables → Actions on [PyDevices/lv_bindings](https://github.com/PyDevices/lv_bindings).

Fine-grained PAT scoped to `lv_cpython_mod`, or classic PAT with `repo` scope.

## Automatic release (recommended)

Work only in **lv_bindings**:

1. Update LVGL submodule and/or edit the generator.
2. Regenerate CPython bindings:
   ```bash
   ./regenerate_lvpy.sh
   ```
3. Commit and push to **`main`** (at least one of):
   - `generated/lvpy.c`
   - `lv_conf.h`
   - `lvgl` (submodule pin)

Pushing those paths starts [trigger-lv-cpython-mod-release.yml](https://github.com/PyDevices/lv_bindings/blob/main/.github/workflows/trigger-lv-cpython-mod-release.yml), which runs [sync-and-release.yml](.github/workflows/sync-and-release.yml) here with the lv_bindings commit SHA.

If the sync produces changes, this repo commits to `main`, pushes the next tag, and **Publish TestPyPI** runs on that tag.

Monitor:

- [lv_bindings Actions](https://github.com/PyDevices/lv_bindings/actions/workflows/trigger-lv-cpython-mod-release.yml)
- [lv_cpython_mod Actions](https://github.com/PyDevices/lv_cpython_mod/actions)

## Manual release (no lv_cpython_mod clone)

### GitHub web UI

1. Open [Actions → Sync and release](https://github.com/PyDevices/lv_cpython_mod/actions/workflows/sync-and-release.yml).
2. **Run workflow**.
3. Set **lv_bindings_ref** to `main` or a commit SHA.
4. Leave **skip_publish** unchecked to tag and publish.

To retry publish only (sync already done): [Actions → Publish TestPyPI](https://github.com/PyDevices/lv_cpython_mod/actions/workflows/publish-testpypi.yml) with version `X.Y.Z`.

### GitHub CLI (any machine)

```bash
# Full sync + tag + publish (reads lv_bindings from GitHub)
gh workflow run sync-and-release.yml --repo PyDevices/lv_cpython_mod

# Pin to a specific lv_bindings commit
gh workflow run sync-and-release.yml --repo PyDevices/lv_cpython_mod \
  -f lv_bindings_ref=abc1234567890

# Sync and commit only — no tag, no TestPyPI
gh workflow run sync-and-release.yml --repo PyDevices/lv_cpython_mod \
  -f skip_publish=true

# Watch progress
gh run list --repo PyDevices/lv_cpython_mod
gh run watch --repo PyDevices/lv_cpython_mod
```

Tag-only release (no lv_bindings sync) from a machine with a clone:

```bash
./scripts/publish_release_tag.sh --push
```

## Manual release (local clone)

```bash
# 1. Pull binding updates from GitHub (not your local sibling lv_bindings tree)
./scripts/sync_from_lv_bindings.sh
./scripts/sync_from_lv_bindings.sh --ref abc1234   # optional SHA/tag/branch

# 2. Commit sync (if the CI bot has not already)
git add generated/lvpy.c lv_conf.h lvgl
git commit -m "Sync bindings and LVGL from lv_bindings main."
git push origin main

# 3. Tag and publish
./scripts/publish_release_tag.sh --push
```

Preview without tagging:

```bash
./scripts/next_release_version.sh --verbose
./scripts/publish_release_tag.sh --dry-run
```

## GitHub Actions workflows

| Workflow | Trigger | What it does |
|----------|---------|--------------|
| [sync-and-release.yml](.github/workflows/sync-and-release.yml) | Manual; called from lv_bindings | Sync from GitHub → commit `main` → push next tag |
| [publish-testpypi.yml](.github/workflows/publish-testpypi.yml) | Tag push `v*.*.*`; manual | Build Linux wheel, `auditwheel repair`, upload TestPyPI |

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/sync_from_lv_bindings.sh` | Copy `generated/lvpy.c`, `lv_conf.h`; pin `lvgl` from **PyDevices/lv_bindings on GitHub** |
| `scripts/next_release_version.sh` | Print next `0.<minor>.<N>` version |
| `scripts/publish_release_tag.sh` | Create annotated tag `vX.Y.Z` and optionally push (triggers publish) |

## Install from TestPyPI

CI publishes a **manylinux x86_64, CPython 3.12** wheel:

```bash
pip install -i https://test.pypi.org/simple/ lvgl-cpython
pip install -i https://test.pypi.org/simple/ lvgl-cpython==0.1.3
```

TestPyPI rejects re-uploading the same version — each release needs a new tag (handled automatically by `publish_release_tag.sh`).

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| lv_bindings trigger workflow fails immediately | `LVCPYTHON_MOD_DISPATCH_TOKEN` missing or lacks `actions:write` on this repo |
| Sync workflow: “Already in sync” | lv_cpython_mod already matches that lv_bindings ref; no tag or publish |
| Sync workflow: `generated/lvpy.c not found` | lvpy.c not committed to lv_bindings at that ref |
| Publish fails: `linux_x86_64` unsupported | Old workflow without `auditwheel repair` (fixed in current `publish-testpypi.yml`) |
| Publish fails: 403 on TestPyPI | Bad or missing `TESTPYPI_API_TOKEN` |
| Publish fails: 400 duplicate version | Tag already uploaded; bump version with a new tag |

## Switching to real LVGL major in version tags

While testing, versions use major **`0`** instead of LVGL’s **`9`**:

```bash
LVCPYTHON_USE_REAL_LVGL_MAJOR=1 ./scripts/publish_release_tag.sh --push
# → e.g. v9.1.4 instead of v0.1.4
```

The CI auto-version step in `next_release_version.sh` uses the same rule.
