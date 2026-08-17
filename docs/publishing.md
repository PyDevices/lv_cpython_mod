# Publishing and releases

How bindings changes in [lvgl-bindings](https://github.com/PyDevices/lvgl-bindings) become a versioned **`pydevices-lvgl`** wheel on [TestPyPI](https://test.pypi.org/project/pydevices-lvgl/), and how to install those prebuilt wheels.

This repo is the publish path in the LVGL family. lvgl-bindings provides the binding tags and generated sources; lvgl-circuitpython and lvgl-micropython sync from those updates and rebuild their targets, but they do not publish separate packages.

You do **not** need a local clone of this repo for release — GitHub Actions can sync, publish a GitHub Release, build, and upload from the web UI or `gh` CLI.

## Pipeline overview

```text
lvgl-bindings (your machine or CI)
  regenerate lvgl_python.c → commit → push main
           │
           ▼
lvgl-bindings: Trigger lvgl-python release   (on push to generated/lvgl_python.c, lv_conf.h, lvgl)
           │
           ▼
lvgl-python: Sync and release
  sync files → write VERSION → commit main → publish GitHub Release vX.Y.Z
           │
           ▼
lvgl-python: Publish release packages     (on published Release or exact-tag retry)
  cibuildwheel → Linux manylinux + Windows amd64 + Android (PEP 738)
  + scripts/build_pyodide_wheel.sh → pyemscripten_2026_0 wasm32
  → smoke tests (native) → TestPyPI API-token upload
```

## Version numbers

Format: **`X.Y.Z`**

| Part | Source |
|------|--------|
| **X** (major) | `LVGL_VERSION_MAJOR` from the LVGL line (via lvgl-bindings tag or `lvgl` headers) |
| **Y** (minor) | `LVGL_VERSION_MINOR` — lvgl-bindings release tag major.minor, or `lvgl/lv_version.h` / `lvgl.h` |
| **Z** (release) | **This repo only** — highest existing `v<X>.<Y>.*` tag + 1, starting at **0** |

The release counter is **independent of lvgl-bindings’ binding patch**. Example: lvgl-bindings `v9.2.3` still maps to LVGL line **9.2** here; the first lvgl-python release on that line is `v9.2.0`, then `v9.2.1` after a local-only change (e.g. `lv_conf.h`) without regenerating in lvgl-bindings.

| LVGL line | lvgl-python tags |
|-----------|---------------------|
| 9.2 | `v9.2.0`, `v9.2.1`, … |
| 9.5 | `v9.5.0`, `v9.5.1`, … |

Early TestPyPI releases used major **`0`** instead of LVGL’s major (`v0.2.0`, `v0.5.0`, …). New releases use the real LVGL major.

If `LV_BINDINGS_REF` has no release tag, major.minor comes from the local `lvgl` submodule headers.

Preview the next version:

```bash
./scripts/next_release_version.sh --verbose
```

## One-time setup

### Repository Secrets

Requires repository authentication secrets for uploading wheels to TestPyPI and dispatching automatic release workflows across repositories.

- `TESTPYPI_API_TOKEN`: token owned by `bdbarnett` while the PyDevices
  TestPyPI organization request is pending.

Settings → Secrets and variables → Actions on repository.

## Automatic release (recommended)

Work only in **lvgl-bindings**:

1. Update LVGL submodule and/or edit the generator.
2. Regenerate CPython bindings:
   ```bash
   ./regenerate_lvpy.sh
   ```
3. Commit and push to **`main`** (at least one of):
   - `generated/lvgl_python.c`
   - `lv_conf.h`
   - `lvgl` (submodule pin)

Pushing those paths starts [trigger-lvgl-python-release.yml](https://github.com/PyDevices/lvgl-bindings/blob/main/.github/workflows/trigger-lvgl-python-release.yml), which runs [sync-and-release.yml](../.github/workflows/sync-and-release.yml) here with the lvgl-bindings commit SHA.

If the sync produces changes, this repo writes the next version into `VERSION`,
commits to `main`, and publishes the corresponding GitHub Release. The release
event starts **Publish release packages**.

Monitor:

- [lvgl-bindings Actions](https://github.com/PyDevices/lvgl-bindings/actions/workflows/trigger-lvgl-python-release.yml)
- [lvgl-python Actions](https://github.com/PyDevices/lvgl-python/actions)

## Manual release (no lvgl-python clone)

### GitHub web UI

1. Open [Actions → Sync and release](https://github.com/PyDevices/lvgl-python/actions/workflows/sync-and-release.yml).
2. **Run workflow**.
3. Set **lvgl_bindings_ref** to `main` or a commit SHA.
4. Leave **skip_publish** unchecked to tag and publish.

To retry publish only (sync already done): run **Publish release packages**
with the exact existing tag `vX.Y.Z`.

### GitHub CLI (any machine)

```bash
# Full sync + tag + publish (reads lvgl-bindings from GitHub)
gh workflow run sync-and-release.yml --repo PyDevices/lvgl-python

# Pin to a specific lvgl-bindings commit
gh workflow run sync-and-release.yml --repo PyDevices/lvgl-python \
  -f lvgl_bindings_ref=abc1234567890

# Sync and commit only — no tag, no TestPyPI
gh workflow run sync-and-release.yml --repo PyDevices/lvgl-python \
  -f skip_publish=true

# Watch progress
gh run list --repo PyDevices/lvgl-python
gh run watch --repo PyDevices/lvgl-python
```

Release without a bindings sync from a machine with a clone:

```bash
printf '%s\n' X.Y.Z > VERSION
git add VERSION
git commit -m "Prepare vX.Y.Z"
git push origin main
gh release create vX.Y.Z --target main --generate-notes
```

## Manual release (local clone)

```bash
# 1. Pull binding updates from GitHub (not your local sibling lvgl-bindings tree)
./scripts/sync_from_lvgl_bindings.sh
./scripts/sync_from_lvgl_bindings.sh --ref abc1234   # optional SHA/tag/branch

# 2. Commit sync (if the CI bot has not already)
git add generated/lvgl_python.c lv_conf.h lvgl
git commit -m "Sync bindings and LVGL from lvgl-bindings main."
git push origin main

# 3. Publish the committed version as a GitHub Release
gh release create vX.Y.Z --target main --generate-notes
```

Preview without tagging:

```bash
./scripts/next_release_version.sh --verbose
```

## GitHub Actions workflows

| Workflow | Trigger | What it does |
|----------|---------|--------------|
| [sync-and-release.yml](../.github/workflows/sync-and-release.yml) | Manual; called from lvgl-bindings | Sync from GitHub → commit versioned `main` → publish GitHub Release |
| [publish-release-packages.yml](../.github/workflows/publish-release-packages.yml) | Published GitHub Release; exact-tag manual retry | Shared **cibuildwheel** matrix: Linux manylinux + Windows amd64 + Android (`android_21_*`) + **Pyodide** `pyemscripten_2026_0_wasm32`; API-token upload to TestPyPI |

### Reading the Sync and release job in the Actions UI

The workflow defines three **mutually exclusive** release steps; GitHub lists **all** of them in the job graph, including steps that did not run:

| Step name | When it runs |
|-----------|----------------|
| **Publish GitHub Release** | Sync produced a commit (`changed=true`) and publish was not skipped |
| **No release tag** | Sync produced **no** commit (already in sync with lvgl-bindings) |
| **Skipped publish** | Workflow was started with **skip_publish** checked |

On a successful release, **Publish GitHub Release** shows **success** with logs
for `vX.Y.Z`. The other two steps appear as **skipped** — that is normal, not a
failure. Check **Publish release packages** for the wheel build and upload.

## Sync bindings from lvgl-bindings

Binding updates flow from [lvgl-bindings](https://github.com/PyDevices/lvgl-bindings) into this repo and onto TestPyPI. The [pipeline overview](#pipeline-overview) above covers automatic triggers, `gh` CLI without a clone, secrets, and versioning.

Quick manual sync from GitHub (with a local clone):

```bash
./scripts/sync_from_lvgl_bindings.sh          # lvgl-bindings main
./scripts/sync_from_lvgl_bindings.sh --ref SHA  # specific commit, tag, or branch
```

To reproduce CI wheels locally with cibuildwheel, see **[Local wheel builds (cibuildwheel)](#local-wheel-builds-cibuildwheel)** — Linux needs Docker; dev-only wheels can use `python -m build --wheel` without it.

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/sync_from_lvgl_bindings.sh` | Copy `generated/lvgl_python.c`, `lv_conf.h`, `display_driver.py`; pin `lvgl` from **PyDevices/lvgl-bindings on GitHub** |
| `scripts/next_release_version.sh` | Print next `<LVGL_major>.<minor>.<N>` version |

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

**Android (PEP 738):** needs the Android SDK on Linux or macOS (cibuildwheel installs packages via `sdkmanager`). API level and archs are set in `pyproject.toml` under `[tool.cibuildwheel.android]` (`ANDROID_API_LEVEL=21` → tags like `android_21_arm64_v8a`). Emulator tests are skipped in CI; validate on device with the [pydevices-android-template LVGL demo](https://github.com/PyDevices/pydevices-android-template/tree/main/android_demo).

```bash
echo "0.0.0.dev" > VERSION
pipx run cibuildwheel --platform android
ls wheelhouse/*android*.whl
```

**Without Docker (dev-only Linux wheel):** a non-manylinux wheel is enough to smoke-test the packaging path:

```bash
echo "0.0.0.dev" > VERSION
python -m pip install build
python -m build --wheel
python -c "import lvgl as lv; lv.init(); lv.deinit(); print('ok')"   # after pip install dist/*.whl or -e .
```

That wheel is not TestPyPI-ready (`linux_x86_64` tag, not `manylinux_*`); use cibuildwheel + Docker when you want to match CI.

## Install from TestPyPI

End-user install commands are in **[README.md](../README.md#install)**. CI publishes wheels for **CPython 3.10–3.14** (one wheel per minor × platform). Pip picks the tag that matches your interpreter (`cp312`, `cp314`, …).

| Platform | Wheel tag |
|----------|-----------|
| Linux x86_64 | `manylinux_*` |
| Windows x64 | `win_amd64` |
| Android arm64 (phones / TVs) | `android_21_arm64_v8a` (`cp313`, `cp314` only) |
| Android x86_64 (emulator) | `android_21_x86_64` (`cp313`, `cp314` only) |
| Pyodide / browser (WASM) | `pyemscripten_2026_0_wasm32` (`cp314` only) |

Pyodide / micropip (same TestPyPI project; micropip selects the wasm tag):

```python
import micropip
await micropip.install("pydevices-lvgl", index_urls="https://test.pypi.org/simple/")
import lvgl as lv
```

Local `web/wheels/` + Pages remain useful for smoke-testing a wheel before the next tag; **releases** ship the wasm wheel on TestPyPI alongside the native ones.

Releases before the multi-Python wheel matrix may only have `cp312` wheels; upgrade to the latest tag. To add a new Python line (e.g. 3.15), extend `build` in `pyproject.toml` `[tool.cibuildwheel]` and publish a new version.

Wheels are built with [cibuildwheel](https://cibuildwheel.pypa.io/) (`auditwheel` on Linux, `delvewheel` on Windows). Python versions and platforms are configured in `pyproject.toml` under `[tool.cibuildwheel]` (`build = "cp310-* … cp314-*"`). To support a newer CPython after a release, add its selector (e.g. `cp315-*`) and publish a **new** version — wheels cannot be added to an existing TestPyPI version.

TestPyPI rejects re-uploading the same filename. Normal releases use a new
version; exact-tag retries use `skip-existing` to finish an interrupted upload.

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| lvgl-bindings trigger workflow fails immediately | Dispatch secret missing or lacks required repository permissions |
| Sync committed but no GitHub Release appeared | `RELEASE_WORKFLOW_TOKEN` is missing or cannot create releases |
| Publish fails: 403 on TestPyPI | `TESTPYPI_API_TOKEN` is missing, expired, or lacks access to the project |
| Local cibuildwheel: `FileNotFoundError: 'docker'` | Linux manylinux builds need Docker locally; CI has it. See [Local wheel builds (cibuildwheel)](publishing.md#local-wheel-builds-cibuildwheel) |
| pip: `pydevices-lvgl==X.Y.Z (from versions: none)` | No wheel for your **CPython minor** on that platform — check files on TestPyPI; extend `[tool.cibuildwheel] build` and publish a new version |
| Publish fails: 400 duplicate version | Tag already uploaded; bump version with a new tag |
