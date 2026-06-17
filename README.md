# Psi.Functionoid

Header-only, policy-driven type erasure for callables. The library descends from
[Boost.Functionoid](https://github.com/psiha/functionoid) (Domagoj Šarić’s
alternate `boost::function`); it is modernized for C++14+ and lives under the
`psi::functionoid` namespace with CMake target `Psi::Functionoid`.

Two public surfaces:

| Type | Role |
|------|------|
| **`callable`** | Owning wrapper — configurable `std::function` replacement |
| **`function_ref`** | Non-owning view — configurable `std::function_ref` replacement |

## Why not `std::function`?

`std::function` fixed the ergonomics of type-erased callbacks, but the design is
largely frozen: one size fits all (always copyable, fixed SBO, RTTI-based
`target_type`/`target`, empty handler throws). Hot paths pay for features they
never use.

**Psi.Functionoid `callable`** keeps the familiar “assign anything callable” model
but makes behaviour a **compile-time policy** (`default_traits` and custom
specializations):

- **SBO tuning** — buffer size and alignment are traits, not hard-coded.
- **Copy / move / destroy levels** — `support_level::{na,supported,nofail,trivial}`
  so move-only, noexcept-move, or trivial-destruction paths need no copy vtable
  slots (sweater `work_t` uses move-only + trivial dtor).
- **Empty handler** — throw, assert, or no-op on invoke of an empty wrapper.
- **RTTI** — on or off (`default_traits` disables it).
- **Noexcept signature** — trait-driven `noexcept` on `operator()`.
- **Vtable call-site hints** — optional `gnu::pure`, `clang::preserve_most`, … on
  invoke/manager function pointers (`detail::vtable_attrs.hpp`).
- **Less bloat** — front/back split inherited from Functionoid: thin fixed ABI
  “front”, typed invoker/manager “back”; fewer template instantiations and less
  RTTI than classic `boost::function`.

Typical wins: lower per-call overhead, smaller hot-loop binaries, and wrappers
sized to the embedding project instead of the standard library’s compromise.

## Compared to the standard “fix attempts”

C++23 **`std::move_only_function`** and the ongoing **`std::copyable_function`**
proposal address the worst parts of `std::function` (move-only storage, clearer
empty behaviour) but remain **non-configurable**: you pick the standard type, not
policies. You cannot turn off RTTI, shrink the SBO for cache footprint, mark
vtable calls `[[gnu::pure]]`, or select assert-on-empty for debug builds.

**Psi.Functionoid** is the layer below those typedefs: one `callable` template,
many shapes — including move-only noexcept aliases that match or beat
`std::move_only_function` for scheduler/work-queue use, and copyable
`std_traits` for drop-in `std::function` semantics when you need them.

## `function_ref`

Non-owning type-erased reference to a callable with signature `R(Args...)`
(optionally `noexcept`). Same conceptual slot as **`std::function_ref`**
(P2637): pass a callback into an API without allocating or owning it.

Optimizations beyond a naive vtable indirection:

1. **Pointer-sized SBO** — if the decayed callable is trivially copyable and
   fits in the storage word (`sizeof(void*)`), it is **embedded inline** and
   invoked without an extra load.
2. **Borrowed object** — otherwise the ref holds a pointer to the caller’s
   callable (lifetime is the caller’s responsibility, as with `function_ref`).

### Exception tunneling (C / V8 / other `noexcept` APIs)

Many foreign interfaces only accept **`noexcept` function pointers** — C
callbacks, V8 embedder hooks, etc. C++ callables that may throw cannot be passed
directly.

When the source callable is not `noexcept`-compatible, `function_ref` wraps it in
an **`exception_tunneling_callable`**: the exported callback is `noexcept`, catches
all exceptions into a `std::exception_ptr`, returns a default-constructed `R`
(or `void`), and the caller rethrows via **`check_failure()`** after the foreign
call returns. See `make_exception_tunneling_callable` and `make_c_callback` in
`include/psi/functionoid/function_ref.hpp`.

## Quick start (standalone)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build
cd build/test && ctest --output-on-failure
```

## Embedding (sweater / host projects)

- `functionoid.cmake` → `Psi::Functionoid` INTERFACE target
- Include: `#include <psi/functionoid/functionoid.hpp>` or `function_ref.hpp`
- sweater `work_t` = `psi::functionoid::callable<void(), worker_traits>`

## Traits

`default_traits` is copyable + RTTI-off. Hot paths use move-only noexcept variants
(`copyable=na`, `moveable=nofail`, `destructor=trivial`). Optional vtable function
pointer attributes via `PSI_FUNCTIONOID_DETAIL_INVOKE_FN_ATTR` — see
`include/psi/functionoid/detail/vtable_attrs.hpp`.

---

### History

An alternate Boost.Function implementation (minimize template/RTTI bloat, lower
runtime overhead, policy configurability). Lineage: Boost.Function 1.43, rebased
through the Functionoid fork. For original Boost.Function docs see
https://www.boost.org/doc/libs/1_43_0/doc/html/function.html .

Related discussion and delegate implementations are listed in the archived notes
at the bottom of this file’s git history; the design glossary (front side / back
side, empty handler, SBO policy) still applies to `callable`.
