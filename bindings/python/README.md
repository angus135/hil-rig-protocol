# Python native binding support

This directory contains binding-private support for the HIL-RIG Python package. It is not part of the installed firmware-facing C API and does not add a second Transport implementation or protocol abstraction.

The core `hil_rig_protocol` Transport remains heap-free. Firmware and other normal C consumers continue to allocate `HIL_Transport_Context_T` and its workspace themselves. The host-only adapter uses heap allocation because a Python object requires dynamic lifetime management: one opaque `HIL_Python_Transport_T` owns exactly one public Transport context and exactly one workspace sized by `HIL_TRANSPORT_Required_Storage_Size()`.

`HIL_PY_TRANSPORT_Create()` is transactional. It clears its output handle, asks the core for the exact workspace requirement, allocates zero-initialized handle and workspace storage, constructs a temporary public `HIL_Transport_Storage_T`, and calls the real `HIL_TRANSPORT_Init()`. The handle is published only after initialization succeeds. Core failures are returned as `HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR` with the exact `HIL_Transport_Status_T`; adapter allocation failure is reported separately. `HIL_PY_TRANSPORT_Destroy()` only releases local lifetime state and never calls `HIL_TRANSPORT_Reset()`.

All stateful adapter functions perform only the opaque-handle null check before forwarding to the public Transport facade. The adapter does not retry work, drain events or Application data, cache or commit output, generate time or session seeds, perform I/O, or inspect Transport/Application messages. Each adapter context remains single-owner.

## CFFI boundary

`cdef.py` is the deliberately small declaration surface for the out-of-line CFFI API-mode module. It contains public Transport value types, the Transport ownership adapter API, and the public Application types and functions. The generated C source includes `hil_rig_protocol_ffi.h`, allowing the C compiler to verify enum values, structure layout, field types, and function signatures against the real headers.

`build_ffi.py` only emits generated C source. CMake compiles and links that source into `hil_rig_protocol._native` together with the Transport adapter and the existing static C core. Generated C is written into the CMake build tree and must not be committed.

The installed Python package has two private binding details:

- `hil_rig_protocol._native`: compiled CFFI extension containing the adapter and shared C Transport/Application core.
- `hil_rig_protocol._binding`: the only handwritten package module that imports `_native`; later Python modules should depend on this internal access point instead of importing `_native` directly.

Neither module is a supported public Transport API. Public Python code should import the supported value and lifetime layer from `hil_rig_protocol`; `_native` and `_binding` remain private implementation details. Transport and Application share this one `_native` module and one linked protocol core.

## Private Application foundation

Application calls the eight public `HIL_APPLICATION_*` codec functions directly.
Its lightweight public context needs no dynamic workspace or ownership adapter.
CFFI verifies value layouts against the C compiler; context and outer message
layouts are partial. CFFI 1.17 cannot nest partial value structs in the public
unnamed body union, so those records declare complete fields and checked array
extents without explicit padding. All encoding, decoding and validation remain
in C.

This surface is private native infrastructure for Test Configuration and fixed
Digital, Analogue and PWM Test Instruction/Result messages. Public Python codec
objects and integration with the Transport wrapper are deferred to the next PR.
The public package exports are unchanged.

Decoded spans borrow caller-owned decode storage and must not outlive it. Query
`HIL_APPLICATION_Decode_Storage_Size`, use NULL for zero bytes, otherwise allocate
enough `max_align_t` elements and cast the allocation to `uint8_t *`. Pass the
exact requested byte capacity, retaining the allocation owner while accessing
spans. The native tests verify its alignment against
`HIL_APPLICATION_DECODE_STORAGE_ALIGNMENT`, including on Windows/MSVC.
MSVC's C headers omit `max_align_t`, although the public Application alignment
macro names it. The shared private header supplies an MSVC-C-only `long double`
typedef for this allocation owner (8-byte alignment, matching MSVC's C++
`max_align_t` and the existing Transport fallback). This isolates the public
macro's C portability limitation without changing the core headers or codec.
Encoding borrows source spans only during the call; decoded spans never borrow
wire input.

## Public Transport facade

The package exposes the public Transport enums, immutable configuration/result value types, Transport exceptions, and the complete caller-driven `Transport` facade. The Python layer forwards the existing C Transport contract; it does not add protocol scheduling, external I/O, retries, output caching, or Application decoding. Public Application objects and Transport integration are deferred to the next PR.

`TransportConfig.session_seed` is role-aware at construction time. A HOST with `session_seed=None` receives a cryptographically secure seed in the inclusive range `1..UINT64_MAX-1`; explicit valid HOST seeds are preserved for deterministic tests. A RIG resolves `None` to zero, accepts explicit zero, and rejects every nonzero seed. The resolved immutable configuration is available through `transport.config`.

One Python `Transport` owns one opaque native adapter handle. Call `close()` for deterministic cleanup, or use the object as a context manager:

```python
from hil_rig_protocol import Role, Transport, TransportConfig

with Transport(Role.HOST, TransportConfig()) as transport:
    effective_config = transport.config
```

`close()` is idempotent. CFFI garbage-collection cleanup remains a fallback when deterministic cleanup is missed; callers should not rely on GC timing. The native context is single-owner: construction records the creating thread, and live native use or deterministic release from another thread raises `TransportOwnershipError`. Transport objects cannot be copied, deep-copied, or pickled.

### Caller-driven servicing contract

Every live `Transport` operation must run on the creating thread. The caller owns the monotonic millisecond domain supplied to `notify_link_state()`, `process()`, and `commit_output()`; the binding never reads a clock, clamps time, or synthesizes timestamps. Normal protocol outcomes such as `NOT_READY`, `CAPACITY_EXHAUSTED`, `MESSAGE_TOO_LARGE`, and `DELIVERY_FAILED` remain explicit `TransportStatus` values. Native invariant failures raise `TransportInternalError`, while impossible binding/status combinations raise `TransportBindingError`.

`receive_bytes()` borrows one C-contiguous buffer only for the native call and reports the exact accepted prefix in `ReceiveResult.bytes_consumed`. On `CAPACITY_EXHAUSTED`, the caller retries only the unconsumed suffix. A zero-length input is still forwarded because it can resume a completed frame retained by native Transport. Mutable buffers must not be modified concurrently with the call.

`peek_output()` copies one complete opaque encoded item into immutable Python `bytes`. A successful peek pins that native item; repeated peeks continue to ask native Transport for the same pinned output. The caller owns any partial external-write offset and must call `commit_output(now_ms)` only after the complete peeked byte string has been accepted by the external interface. The binding never caches output, commits automatically, or interprets Transport frames.

`read_event()` consumes one event at a time. Event draining is caller service work because unread events occupy bounded native capacity. `read_application_data()` returns one complete opaque Application byte string and consumes it only after the full native copy succeeds. Reliable Transport delivery and `DELIVERY_CONFIRMED` report byte delivery, not semantic Application acceptance; Application responses remain separate opaque messages.

The complete caller contract, verified wheel matrix, and installation guidance
are in the [public Python Transport guide](../../docs/python/transport.md). A
tested [caller-owned servicing example](../../examples/python/transport_servicing.py)
demonstrates receive suffix ownership and partial external writes. Production
serial/USB integration and higher-level recovery policy remain outside this
package.

## Prerequisites

A Python package build requires:

- Python 3.12 or newer. Binary wheels are verified for CPython 3.12, 3.13, and
  3.14 on `manylinux_2_28` x86-64 and Windows AMD64.
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

The complete Python suite retains the private native smoke tests and also verifies public enum parity, configuration validation, role-specific seed handling, creation error mapping, deterministic/context-manager/GC cleanup, single-thread ownership, buffer borrowing, exact receive consumption, output peek/commit separation, Application reads, event conversion, status snapshots, and public package exports.

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
