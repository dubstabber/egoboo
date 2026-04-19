# Error-Handling Policy

This document defines the active error-handling policy for Egoboo runtime and tool code.

The current codebase still contains three historical styles:

- C++ exceptions
- `egolib_rv` / `gfx_rv` return codes
- silent failure via ignored null / false returns

New code should follow the rules below. Existing code should be migrated toward them incrementally, one bounded subsystem at a time.

## Principles

- Use one error-reporting style per API.
- Prefer explicit contracts over defensive silent failure.
- Treat programmer mistakes and invariant violations as exceptional.
- Treat expected runtime outcomes at subsystem boundaries as ordinary control flow.

## Use Exceptions For Exceptional Failures

Use exceptions when the caller cannot reasonably continue without fixing the program state or the call site.

Typical cases:

- construction or initialization failure
- violated invariants
- invalid internal state
- programmer error, such as passing a required null dependency
- impossible or unhandled internal cases

Preferred exception categories follow existing project practice:

- `idlib::argument_null_error` for required null arguments
- `std::invalid_argument` or `idlib::argument_out_of_bounds_error` for invalid inputs
- `std::logic_error` or project-specific logic exceptions for violated assumptions
- `idlib::runtime_error` or domain-specific runtime exceptions for operational failures that are not expected branch outcomes

## Use Return Values For Expected Boundary Outcomes

Use normal return values when failure is part of the ordinary contract and the caller is expected to branch on it.

Preferred forms:

- `bool` for success or failure without payload
- `std::optional<T>` for present-or-absent results
- value-returning APIs with documented sentinel values only when that convention is already established and unambiguous

Typical cases:

- lookup miss
- optional resource not present
- unsupported content or script condition that is already part of a caller-visible branch
- gameplay predicates

## Do Not Introduce New Silent Failure

Do not add APIs that discard errors without one of:

- throwing
- returning an explicit status or optional result
- logging a deliberate best-effort fallback at a well-defined boundary

If an operation is best-effort, document that contract and keep the fallback localized.

## `egolib_rv` And `gfx_rv`

`egolib_rv` and the `gfx_rv` alias are legacy return-code conventions.

Policy:

- Do not introduce new C++ APIs that return `egolib_rv` or `gfx_rv`.
- Retire existing C++ uses incrementally when the surrounding semantics are clear.
- Keep legacy C-era or tri-state-heavy uses in place until a bounded pass can replace them without changing behavior.

When migrating an existing `egolib_rv` C++ API:

- replace programmer-error branches with exceptions
- replace expected success or failure branches with `bool` or `std::optional`
- avoid collapsing meaningful tri-state semantics into `bool` until the contract is explicitly redesigned

## Practical Defaults

- Required callback or dependency missing: throw.
- Query or lookup can legitimately miss: return `nullptr`, `false`, or `std::optional`.
- Content or script condition intended for branching: return an ordinary value, not an exception.
- Internal impossible branch: throw.

## Scope Of Incremental Migration

This policy is forward-looking. It does not require sweeping rewrites.

Migration should proceed in small passes:

1. document the intended contract
2. convert one bounded API or subsystem
3. preserve behavior except for the clarified error contract
4. add focused verification where a stable seam exists
