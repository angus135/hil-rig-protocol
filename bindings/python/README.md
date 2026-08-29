# Python Transport binding support

This directory contains binding-private support for the HIL-RIG Python package. It is not part of the installed firmware-facing C API and does not add a second Transport implementation or protocol abstraction.

The core `hil_rig_protocol` Transport remains heap-free. Firmware and other normal C consumers continue to allocate `HIL_Transport_Context_T` and its workspace themselves. The host-only adapter uses heap allocation because a Python object requires dynamic lifetime management: one opaque `HIL_Python_Transport_T` owns exactly one public Transport context and exactly one workspace sized by `HIL_TRANSPORT_Required_Storage_Size()`.

`HIL_PY_TRANSPORT_Create()` is transactional. It clears its output handle, asks the core for the exact workspace requirement, allocates zero-initialized handle and workspace storage, constructs a temporary public `HIL_Transport_Storage_T`, and calls the real `HIL_TRANSPORT_Init()`. The handle is published only after initialization succeeds. Core failures are returned as `HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR` with the exact `HIL_Transport_Status_T`; adapter allocation failure is reported separately. `HIL_PY_TRANSPORT_Destroy()` only releases local lifetime state and never calls `HIL_TRANSPORT_Reset()`.

All stateful adapter functions perform only the opaque-handle null check before forwarding to the public Transport facade. The adapter does not retry work, drain events or Application data, cache or commit output, generate time or session seeds, perform I/O, or inspect Transport/Application messages. Each adapter context remains single-owner.

## CFFI boundary

`cdef.py` is the deliberately small declaration surface for the out-of-line CFFI API-mode module. It contains only public Transport value types and the adapter API. The generated C source includes `hil_rig_protocol_ffi.h`, allowing the C compiler to verify enum values, structure layout, field types, and function signatures against the real headers.

`build_ffi.py` only emits generated C source. CMake compiles and links that source into `hil_rig_protocol._native` together with the PR 1 adapter and the existing static C core. Generated C is written into the CMake build tree and must not be committed.

The installed Python package has two private binding details:

- `hil_rig_protocol._native`: compiled CFFI extension containing the adapter and C Transport core.
- `hil_rig_protocol._binding`: the only handwritten package module that imports `_native`; later Python modules should depend on this internal access point instead of importing `_native` directly.

Neither module is a supported public Transport API. Public Python code should import the supported value and lifetime layer from `hil_rig_protocol`; `_native` and `_binding` remain private implementation details. Future Application declarations should extend this same `_native` module so the C core is not compiled into a second Python extension.

## Public Transport value and lifetime layer

The package now exposes the public Transport enums, immutable configuration/result value types, Transport exceptions, and `Transport` native lifetime owner. Operational Transport methods remain intentionally deferred to the next binding PR, and Application bindings remain separate later work.

`TransportConfig.session_seed` is role-aware at construction time. A HOST with `session_seed=None` receives a cryptographically secure seed in the inclusive range `1..UINT64_MAX-1`; explicit valid HOST seeds are preserved for deterministic tests. A RIG resolves `None` to zero, accepts explicit zero, and rejects every nonzero seed. The resolved immutable configuration is available through `transport.config`.

One Python `Transport` owns one opaque native adapter handle. Call `close()` for deterministic cleanup, or use the object as a context manager:

```python
from hil_rig_protocol import Role, Transport, TransportConfig

with Transport(Role.HOST, TransportConfig()) as transport:
    effective_config = transport.config
```

`close()` is idempotent. CFFI garbage-collection cleanup remains a fallback when deterministic cleanup is missed; callers should not rely on GC timing. The native context is single-owner: construction records the creating thread, and live native use or deterministic release from another thread raises `TransportOwnershipError`. Transport objects cannot be copied, deep-copied, or pickled.

## Prerequisites

A Python package build requires:

- Python 3.12 or newer. Python 3.12 is the currently verified CI baseline.
- Python development/module headers for the active interpreter.
- CMake 3.17 or newer for the Python-enabled build path.
- A C11 compiler.
- CFFI.
- `scikit-build-core` when installing through `pip`.

Normal firmware builds with `HIL_RIG_PROTOCOL_BUILD_PYTHON=OFF` do not discover or execute Python and do not require CFFI or Python headers.

Sanitizer-enabled Linux builds link the sanitizer runtime into `_native`. Because
the Python executable itself is not normally AddressSanitizer-instrumented, load
the runtime first when checking the extension, for example:

```bash
LD_PRELOAD="$(gcc -print-file-name=libasan.so)" \
ASAN_OPTIONS=detect_leaks=0 \
python -c "import hil_rig_protocol._native"
```

The preload detail is specific to loading an ASan extension into a non-ASan
process; normal and coverage builds do not require it.

## Normal installation

From the repository root:

```sh
python -m pip install .
python -c "import hil_rig_protocol; import hil_rig_protocol._native"
```

`scikit-build-core` configures CMake with Python binding support enabled and C tests disabled for the package build. The resulting wheel contains the Python package and one platform-specific `_native` extension; it does not load or require a separately installed `hil_rig_protocol` shared library.

## Editable installation

```sh
python -m pip install -e .
python -c "import hil_rig_protocol; import hil_rig_protocol._native"
```

Editable and normal installations use the same native CMake build path.

## Python tests

Install the package first, then run:

```sh
python -m pytest tests/python
```

The complete Python suite retains the private native smoke tests and also verifies public enum parity, configuration validation, role-specific seed handling, creation error mapping, deterministic/context-manager/GC cleanup, single-thread ownership, and public package exports.

### Windows PowerShell

Run these commands from the repository root. A Visual Studio 2022 C/C++
toolchain and CMake must be installed.

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade pip
.\.venv\Scripts\python.exe -m pip install ".[test]"
```

Run the imports and tests from outside the checkout so the installed package
cannot be confused with the source tree:

```powershell
$Repository = (Resolve-Path .).Path
Set-Location $env:TEMP

& "$Repository\.venv\Scripts\python.exe" -c `
  "import hil_rig_protocol; import hil_rig_protocol._native as native; print(hil_rig_protocol.__file__); print(native.__file__)"

& "$Repository\.venv\Scripts\python.exe" -m pytest `
  "$Repository\tests\python"
```

The two import paths should point into `.venv\Lib\site-packages`, the native
module should end in `.pyd`, and the complete Python test suite should pass. If
CMake cannot find MSVC, run the commands from the Visual Studio 2022 Developer
PowerShell.

## Direct CMake build

For native binding development without building a wheel:

```sh
cmake -S . -B build-python \
  -DHIL_RIG_PROTOCOL_TRANSPORT_PROFILE=MVP \
  -DHIL_RIG_PROTOCOL_BUILD_TESTS=OFF \
  -DHIL_RIG_PROTOCOL_BUILD_PYTHON=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build build-python
```

CMake uses the discovered interpreter to execute `build_ffi.py`, generates the CFFI source under `build-python/bindings/python/`, builds the private adapter and `_native` module, and keeps warning relaxation limited to the generated CFFI source. The Python-enabled build requires the existing core target to be static so `_native` remains self-contained; the package build sets `BUILD_SHARED_LIBS=OFF` explicitly.
