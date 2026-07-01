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
lv_cpython_mod: Publish TestPyPI             (on tag push v*.*.* or workflow_dispatch)
  cibuildwheel → Linux manylinux + Windows amd64 → smoke tests → twine upload
```

## Version numbers

Format: **`X.Y.Z`**

| Part | Source |
|------|--------|
| **X.Y** (LVGL line) | lv_bindings release tag at `LV_BINDINGS_REF` (major.minor only), or `lvgl/lv_version.h` / `lvgl.h` |
| **Z** (release) | **This repo only** — highest existing `v<major>.<minor>.*` tag + 1, starting at **0** |

The release counter is **independent of lv_bindings’ binding patch**. Example: lv_bindings `v9.2.3` still maps to LVGL line **9.2** here; the first lv_cpython_mod release on that line is `v0.2.0`, then `v0.2.1` after a local-only change (e.g. `lv_conf.h`) without regenerating in lv_bindings.

| Mode | LVGL line | lv_cpython_mod tags |
|------|-----------|---------------------|
| TestPyPI (default) | 9.2 | `v0.2.0`, `v0.2.1`, … |
| Real LVGL major | 9.2 | `v9.2.0`, `v9.2.1`, … (`LVCPYTHON_USE_REAL_LVGL_MAJOR=1`) |

If `LV_BINDINGS_REF` has no release tag, major.minor comes from the local `lvgl` submodule headers.

Preview the next version:

```bash
./scripts/next_release_version.sh --verbose
```

## One-time setup

### lv_cpython_mod secrets

| Secret | Required | Purpose |
|--------|----------|---------|
| `TESTPYPI_API_TOKEN` | yes | Upload wheels to TestPyPI |
| `RELEASE_WORKFLOW_TOKEN` | yes for auto-publish from Sync and release | PAT with **`actions:write`** on this repo |

Settings → Secrets and variables → Actions on [PyDevices/lv_cpython_mod](https://github.com/PyDevices/lv_cpython_mod).

Use the same TestPyPI token as [pydisplay](https://github.com/PyDevices/pydisplay) if you already have one.

`RELEASE_WORKFLOW_TOKEN` can be the **same fine-grained PAT** as `LVCPYTHON_MOD_DISPATCH_TOKEN` on lv_bindings (add it to both repos). Sync and release pushes tags with `GITHUB_TOKEN`, which does **not** start Publish TestPyPI on its own; the script dispatches that workflow with this PAT after tagging.

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

If the sync produces changes, this repo commits to `main`, pushes the next tag, and dispatches **Publish TestPyPI** (tag pushes from `GITHUB_TOKEN` do not trigger other workflows).

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
| [sync-and-release.yml](.github/workflows/sync-and-release.yml) | Manual; called from lv_bindings | Sync from GitHub → commit `main` → push next tag → dispatch publish |
| [publish-testpypi.yml](.github/workflows/publish-testpypi.yml) | Tag push `v*.*.*` (local/manual tags); workflow_dispatch | **cibuildwheel**: Linux manylinux + Windows amd64 wheels, smoke tests, TestPyPI upload |

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/sync_from_lv_bindings.sh` | Copy `generated/lvpy.c`, `lv_conf.h`; pin `lvgl` from **PyDevices/lv_bindings on GitHub** |
| `scripts/next_release_version.sh` | Print next `0.<minor>.<N>` version |
| `scripts/publish_release_tag.sh` | Create annotated tag `vX.Y.Z` and optionally push (triggers publish) |

## Local wheel builds (cibuildwheel)

CI uses [cibuildwheel](https://cibuildwheel.pypa.io/) (config in `pyproject.toml`). To reproduce wheel builds locally **later**:

```bash
pipx install cibuildwheel   # one-time
echo "0.0.0.dev" > VERSION  # required; setuptools reads this file
pipx run cibuildwheel --platform linux    # or --platform windows
ls wheelhouse/
```

**Linux requires Docker.** cibuildwheel builds inside a manylinux container (`auditwheel repair` needs that environment). Without Docker you get:

```text
FileNotFoundError: [Errno 2] No such file or directory: 'docker'
```

Install [Docker Engine](https://docs.docker.com/engine/install/) (or Docker Desktop on WSL2), ensure your user can run `docker`, then retry. GitHub Actions runners already have Docker — you do not need it for releases, only for local Linux wheel reproduction.

**Windows** does not need Docker: run `pipx run cibuildwheel --platform windows` on a native Windows shell with MSVC Build Tools (same as a normal `pip install -e .` build).

**Without Docker (dev-only Linux wheel):** a non-manylinux wheel is enough to smoke-test the packaging path:

```bash
echo "0.0.0.dev" > VERSION
python -m pip install build
python -m build --wheel
python test_lvgl_cpython.py   # after pip install dist/*.whl or -e .
```

That wheel is not TestPyPI-ready (`linux_x86_64` tag, not `manylinux_*`); use cibuildwheel + Docker when you want to match CI.

## Install from TestPyPI

CI publishes **CPython 3.12** wheels for:

| Platform | Wheel tag |
|----------|-----------|
| Linux x86_64 | `manylinux_*` |
| Windows x64 | `win_amd64` |

```bash
pip install -i https://test.pypi.org/simple/ lvgl-cpython
pip install -i https://test.pypi.org/simple/ lvgl-cpython==0.3.0
```

Wheels are built with [cibuildwheel](https://cibuildwheel.pypa.io/) (`auditwheel` on Linux, `delvewheel` on Windows). Config lives in `pyproject.toml` under `[tool.cibuildwheel]`.

TestPyPI rejects re-uploading the same version — each release needs a new tag (handled automatically by `publish_release_tag.sh`).

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| lv_bindings trigger workflow fails immediately | `LVCPYTHON_MOD_DISPATCH_TOKEN` missing or lacks `actions:write` on this repo |
| Sync succeeded, tag pushed, but no Publish TestPyPI run | Tag was pushed by `GITHUB_TOKEN` in CI — add `RELEASE_WORKFLOW_TOKEN` (see above) or run Publish TestPyPI manually with the version |
| Sync workflow: “Already in sync” / “No release tag” | lv_cpython_mod already matches that lv_bindings ref; no commit, tag, or publish |
| Sync workflow: `generated/lvpy.c not found` | lvpy.c not committed to lv_bindings at that ref |
| Publish fails: `linux_x86_64` unsupported | Old hand-rolled workflow without wheel repair (use current cibuildwheel workflow) |
| Publish fails on Windows only | Check MSVC build logs in the `windows-latest` matrix job; local Windows builds need Visual Studio Build Tools |
| Publish fails: 403 on TestPyPI | Bad or missing `TESTPYPI_API_TOKEN` |
| Local cibuildwheel: `FileNotFoundError: 'docker'` | Linux manylinux builds need Docker locally; CI has it. See [Local wheel builds (cibuildwheel)](PUBLISHING.md#local-wheel-builds-cibuildwheel) |

## Switching to real LVGL major in version tags

While testing, versions use major **`0`** instead of LVGL’s **`9`**:

```bash
LVCPYTHON_USE_REAL_LVGL_MAJOR=1 ./scripts/publish_release_tag.sh --push
# → e.g. v9.1.4 instead of v0.1.4
```

The CI auto-version step in `next_release_version.sh` uses the same rule.
