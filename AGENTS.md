# AGENTS.md

This file defines the default expectations for AI coding agents working on this
C11 protocol library, its C++ tests, and its Python bindings.

The priorities are, in order:

1. Functional correctness.
2. Deterministic, bounded protocol behaviour.
3. Ownership, lifetime, and concurrency safety.
4. Minimal, targeted changes.
5. Clear, testable APIs and state transitions.
6. Maintainability without unnecessary abstraction.

Preserve established project architecture and conventions unless the task
specifically requires changing them.

Do not retrofit untouched code merely to satisfy this guide. Apply these
requirements to code added or materially changed by the task, and use them
when reviewing code.

## 1. Working Style

### Keep changes tightly scoped

- Implement the requested behaviour with the smallest sensible change set.
- Do not perform unrelated refactors, clean-ups, renames, formatting changes,
  or optimisations.
- Do not redesign an API or subsystem simply because another design is
  possible.
- Preserve existing public APIs, wire formats, ownership rules, and behaviour
  unless changing them is explicitly part of the task.
- Preserve unrelated local changes already present in the working tree.
- Do not modify generated code, vendor code, submodules, reference
  implementations, or compatibility copies unless explicitly instructed.
- Do not introduce new threads, callbacks, queues, buffers, or architectural
  layers merely to make an implementation easier.
- Prefer local fixes over repository-wide abstractions when the abstraction
  would only be used once.
- Only create a shared helper or constant when it removes genuine duplication
  or establishes an important invariant.

### Understand before editing

Before changing code:

- Read the relevant implementation, header, tests, and call sites.
- Trace the complete data and control flow involved in the requested
  behaviour.
- Identify ownership, lifetime, concurrency, buffer capacity, and state
  machine implications.
- Check whether similar functionality already exists elsewhere in the
  repository.
- Treat existing tests and externally visible behaviour as part of the
  contract.
- Do not infer protocol behaviour from names alone. Verify it in the
  implementation and relevant specifications.
- If behaviour, intent, or use is unclear or contradictory, investigate first,
  then ask one consolidated question only when unresolved ambiguity could
  materially alter behaviour, architecture, safety, or API compatibility.

### Do not hide uncertainty with code

- Do not add speculative checks, casts, retries, sleeps, state resets, or
  fallback behaviour without a concrete reason.
- Do not silently change semantics to make a test pass.
- Do not modify production code solely to make testing easier unless the new
  seam is also a valid production design.
- If a failure is unrelated to the requested work, report it rather than
  changing unrelated code.

## 2. C and C++ Style

Follow the repository formatter when one exists. Avoid formatting churn outside
edited lines.

### Avoid unnecessary casts

Do not add casts unless they are actually required for correctness,
compilation, API compatibility, or an intentional representation/type
conversion.

In particular:

- Do not cast merely to silence a warning.
- Do not cast a function to a compatible function-pointer type.
- Do not use casts as defensive decoration.
- Do not cast ignored return values to `void`.
- In C++, do not use C-style casts unless explicitly required.

Prefer allowing valid implicit C conversions when the source and destination
types are already compatible. A redundant cast can hide a real type mismatch
later.

Example:

```c
config->handler = handler_fn;
```

Prefer this over:

```c
config->handler = (handler_fn_t)handler_fn;
```

### Passing state

- Pass mutable state structures by pointer/reference.
- Do not make unnecessary copies of state or large records.
- Keep ownership explicit.
- Avoid arbitrary untyped pointers when a concrete typed pointer, ID, or
  statically owned object can express the relationship.

### Types and sizes

- Use types that represent the actual data being stored.
- Do not narrow sequence numbers, lengths, identifiers, counters, or protocol
  values without a defined reason.
- At wire-format and FFI boundaries, use explicit-width integer types.
- Keep length/capacity arithmetic in types capable of representing the full
  valid range.
- Be especially careful with signed/unsigned comparisons and arithmetic.
- Do not rely on casts to suppress those problems.

## 3. C++ Test and Support Code

C++ is used for GoogleTest tests and their support code. The production library
and its public firmware-facing API remain C11 unless a task explicitly changes
that boundary.

### Language and ownership

- Keep C++ code compatible with C++17; do not use newer language or standard
  library features.
- Do not expose C++ types, exceptions, templates, name mangling, or ownership
  conventions through the public C API.
- Prefer automatic storage and standard RAII containers for test-owned data.
  Do not add raw owning `new`/`delete` pairs.
- Keep C object ownership and lifetime visible when wrapping the public API.
  A C++ helper must not hide a required commit, consume, reset, or destruction
  transition.
- Use explicit casts only for intentional boundary conversions. Do not use a
  cast to bypass the type or const contract of the C API.

### Test boundaries and structure

- Unit tests may include private implementation headers when direct component
  coverage requires them. Integration tests must use only installed-style
  public headers and public APIs.
- Keep integration support as a caller model, not a second Transport model. It
  must not decode frames, synthesize protocol state, silently drain resources,
  or replace statuses returned by production code.
- Keep test time deterministic and caller-controlled. Do not use wall-clock
  waits or sleeps to drive retry, timeout, or recovery scenarios.
- Use fatal GoogleTest assertions for setup and prerequisites whose failure
  makes later statements unsafe; use non-fatal expectations for independent
  postconditions that can still be checked.
- Add every new integration test source to the explicit CMake source list. Do
  not bypass the repository check for unregistered integration tests.
- When changing public headers, preserve coverage from both C++ consumers and
  the standalone C11 consumer compile test.

## 4. Python Package and Bindings

Python code is a typed public facade over the shared C implementation. It must
not become an independent implementation of Transport or Application protocol
semantics.

### Python style and public values

- Keep package code compatible with Python 3.12, the minimum supported
  version. Do not require syntax or standard-library features introduced
  later.
- Follow the Ruff and mypy configuration in `pyproject.toml`. New or materially
  changed package functions and methods require complete type annotations.
- Continue using `from __future__ import annotations` in package modules that
  define annotated functions or values; import-only modules need not add it.
- Prefer immutable, `frozen=True`, slotted dataclasses for public value records,
  matching the existing API.
- Preserve exact-type validation where representation matters. In particular,
  do not accidentally accept `bool` as an integer, raw integers as enum values,
  mutable bytes-like objects as immutable records, or subclasses where an
  exact native representation is required.
- Keep representation validation in Python and semantic protocol validation in
  C. Do not duplicate native semantic rules or silently coerce an invalid value
  into an accepted one.
- Treat changes to package-level imports and `__all__` as public API changes.
  Keep export tests and the checked-in `py.typed` marker consistent.

### CFFI, buffers, and lifetime

- Handwritten package modules must access the native extension through
  `_binding`; do not import `_native` directly outside that internal access
  point.
- Keep `cdef.py` synchronized with the real public C declarations. Add or update
  parity tests for changed enum values, field names, array extents, signatures,
  and other exposed representation details.
- Generated CFFI source belongs in the build tree and must not be committed or
  edited by hand.
- Treat CFFI pointers and exported buffers as borrowed for the documented call
  only. Do not retain Python buffer pointers in native state, and release CFFI
  exports before returning to Python.
- For accepted Python buffers, validate the required contiguity and use byte
  length rather than element count. Return detached immutable `bytes` when the
  result must outlive native storage.
- Keep native creation and cleanup transactional. Publish a handle only after
  successful initialization, make explicit `close()` deterministic and
  idempotent, and retain garbage collection only as a fallback.
- Preserve creating-thread ownership checks for stateful native objects. Do not
  add locking or dispatch that makes unsupported cross-thread use appear safe.
- Keep normal native protocol outcomes as explicit status values. Raise binding
  exceptions for violated binding invariants, unknown native values, or
  impossible status/result combinations without discarding available native
  status context.

### Python tests

- Prefer pytest parameterization for representation boundaries and repeated
  protocol scenarios, with stable IDs when parity with named C++ cases matters.
- Exercise supported behavior through the public Python facade in integration
  tests. Do not reach through private CFFI state merely to duplicate a C-only
  test mechanism.
- Add focused tests for Python-specific risks such as exact type rejection,
  buffer contiguity and slicing, byte length, export release, detached copies,
  deterministic cleanup, thread ownership, and public exports.
- Keep tests deterministic: provide explicit seeds and caller-controlled times
  except when the behavior under test is secure seed generation itself.

## 5. API and Architecture Preferences

### Prefer simple, explicit APIs

- Keep APIs small and direct.
- Avoid unnecessary wrappers, indirection, class-like layers, registries, and
  generic frameworks in C.
- Do not introduce abstractions that obscure ownership or control flow.
- Separate policy from mechanism where it materially improves testability or
  portability.

### Preserve invariants at the correct layer

Do not push responsibility to callers when the module can enforce its own
invariant cheaply and reliably.

Examples include:

- bounds;
- valid configuration;
- legal state transitions;
- queue capacity;
- frame size;
- sequence advancement;
- ownership state;
- retry accounting.

Validate before mutating state whenever possible.

A failed operation should not partially consume ownership, sequence numbers,
retry counts, queue entries, or other state unless that partial transition is
explicitly part of the design.

### Programmer errors versus runtime errors

Use assertions for internal/programmer invariants such as:

- invalid static configuration;
- impossible internal states;
- required pointers supplied by trusted code;
- compile-time or startup assumptions that must always hold.

Use normal error handling for:

- malformed external input;
- queue/full conditions;
- timeouts;
- unavailable hardware reported through the application protocol;
- disconnected peers;
- protocol errors;
- other conditions expected during normal operation.

Do not fabricate or collapse meaningful status codes if the underlying layer
already exposes the distinction required by the caller.

## 6. Determinism and Memory

Core C protocol code should be deterministic by default.

Prefer:

- static storage;
- caller-owned buffers;
- caller-supplied workspaces;
- bounded queues;
- fixed-capacity structures;
- explicit ownership.

Do not introduce heap allocation into the core C protocol runtime. The Python
binding may use explicit allocation where its ownership and cleanup contract
requires it.

For every buffer or queue change, reason about:

- maximum payload;
- framing overhead;
- alignment/padding;
- delimiter ownership;
- producer/consumer concurrency;
- full/empty behaviour;
- what happens on failure after partial progress.

Do not reserve bytes for framing elements that are not actually stored inside
the bounded object.

Do not clear large buffers unnecessarily when resetting metadata or lengths is
sufficient and safe.

## 7. Ownership and Concurrency

Concurrency bugs are correctness bugs, not style issues.

For every state shared between execution contexts, establish:

- who owns it;
- who may read it;
- who may write it;
- what synchronisation protects it;
- whether an operation must be atomic;
- what lifetime guarantees apply.

Prefer a clear single owner plus explicit transfer of data or ownership where
that fits the architecture. Follow the public API contract when it requires
one owning execution context; do not add locking around an API that explicitly
does not support concurrent use.

Do not assume a shared or externally visible value is safe to update through a
non-atomic compound operation merely because it is declared `volatile`.

## 8. Protocol and Integration Boundaries

The library is hardware- and transport-medium-agnostic. Keep USB, UART,
serial, clocks, physical I/O, scheduling, and driver-specific decisions in the
caller or firmware integration layer.

When modifying protocol code:

- preserve the documented separation between Application semantics and
  Transport delivery;
- keep hardware-specific details out of public wire records unless the
  protocol explicitly defines them;
- preserve exact wire ordering, widths, endianness, framing, and delimiter
  rules;
- treat reset, reconnect, retry, and ownership transitions as part of the
  externally visible contract.

## 9. Comments and Documentation

Documentation should explain contracts and non-obvious reasoning, not narrate
obvious code. Keep it concise while relaying all required information. Use
visual aids such as Mermaid diagrams where they materially clarify a protocol
or state transition.

Use Doxygen-style documentation consistently for newly added or materially
changed:

- public functions;
- externally visible hooks;
- non-trivial private functions;
- structs and important fields;
- active configuration macros;
- state-machine or protocol contracts;
- unit tests, with only the documentation needed to explain their purpose.

Inline comments are useful for:

- subtle ordering requirements;
- concurrency reasoning;
- integration constraints;
- protocol invariants;
- why an apparently simpler implementation would be wrong.

Do not add comments that merely restate the next line of code.

Do not spend task scope on documentation-only clean-up unless documentation is
part of the request or is required to prevent misuse of changed behaviour.

Do not invent new jargon that does not exist outside the codebase unless it is
needed for a genuinely new concept. This also applies to names for functions,
variables, and files.

## 10. Code Review Priorities

When reviewing this protocol library, prioritise concrete functional defects
over stylistic commentary.

Review deeply for:

- race conditions;
- ownership/lifetime errors;
- use-after-reset or stale state;
- queue and buffer overflows;
- incorrect capacity calculations;
- partial state mutation on failure;
- sequence/retry accounting bugs;
- deadlocks and blocking behaviour;
- timing assumptions;
- reset/reconnect recovery;
- integer width/sign/overflow issues;
- packing/alignment/endianness;
- error-path correctness;
- mismatched assumptions between modules;
- invalid protocol state transitions.

De-prioritise the following (even if they contradict this style guide):

- naming preferences;
- speculative architecture improvements;
- broad refactors;
- documentation polish;
- minor style differences;
- test-count criticism without a concrete coverage gap;
- suggestions that cannot be tied to a plausible failure.

### Review findings must be demonstrable

For each significant finding, explain:

1. The relevant code path.
2. The exact preconditions.
3. The sequence of events that triggers the problem.
4. The resulting incorrect behaviour.
5. Why existing protection does not prevent it.
6. A minimal direction for fixing it.
7. A focused regression test that would reproduce it.

Do not label something as a race, overflow, deadlock, or protocol violation
without tracing how it can actually occur.
