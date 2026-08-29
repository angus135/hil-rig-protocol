# Python Transport native adapter

This directory is binding-private support for the future out-of-line Python CFFI extension. It is not part of the installed firmware-facing C API and does not add a second Transport abstraction or any protocol behaviour.

The core `hil_rig_protocol` Transport remains heap-free. Firmware and other normal C consumers continue to allocate `HIL_Transport_Context_T` and its workspace themselves. The host-only adapter uses heap allocation because a Python object will need dynamic lifetime management: one opaque `HIL_Python_Transport_T` owns exactly one public Transport context and exactly one workspace sized by `HIL_TRANSPORT_Required_Storage_Size()`.

`HIL_PY_TRANSPORT_Create()` first clears its output handle, asks the core for the exact workspace requirement, allocates zero-initialized handle and workspace storage, constructs a temporary public `HIL_Transport_Storage_T`, and calls the real `HIL_TRANSPORT_Init()`. The handle is published only after initialization succeeds. Core failures are returned as `HIL_PY_ADAPTER_STATUS_TRANSPORT_ERROR` with the exact `HIL_Transport_Status_T` in the output status. Adapter allocation failure is separate and leaves the core status as `HIL_TRANSPORT_STATUS_OK`. A null configuration is intentionally allowed to reach the core size query and is therefore reported as a Transport `INVALID_ARGUMENT` failure rather than an adapter-output argument failure.

The standard C allocator provides storage suitable for objects with fundamental alignment, which satisfies the current `HIL_TRANSPORT_WORKSPACE_ALIGNMENT` contract. Successful real Transport initialization in the adapter tests also exercises the core alignment validation on the active compiler.

`HIL_PY_TRANSPORT_Destroy()` only releases local lifetime state. It never calls `HIL_TRANSPORT_Reset()`, so destruction does not generate RESET traffic, events, retries, or other protocol work.

All stateful adapter functions perform only one adapter-level check: a null opaque handle returns `HIL_TRANSPORT_STATUS_INVALID_ARGUMENT`. Otherwise they call the corresponding public Transport facade exactly once and return its status unchanged. The adapter does not retry work, drain events or Application data, cache peeked output, commit output automatically, generate time or session seeds, or inspect frames or Application messages.

The adapter owns no external integration services. The future Python layer remains responsible for I/O, monotonic time, session-seed/entropy policy, scheduling, and Application workflow. Transport still exchanges complete opaque Application byte strings. Each adapter context remains single-owner and must not be called concurrently from multiple execution contexts.

Future CFFI code should depend only on this private adapter surface and stable public Transport value types. It must not access `HIL_Transport_Context_T`, `HIL_Transport_Storage_T`, workspace memory, or private Transport profile headers/state.

## Build boundary

The adapter is built only when `HIL_RIG_PROTOCOL_BUILD_PYTHON=ON`. This option currently builds only the native static adapter and, when `HIL_RIG_PROTOCOL_BUILD_TESTS=ON`, its C tests. It does not discover Python, CFFI, Python headers, packaging tools, serial libraries, or other Python dependencies.
