# Stock charging-limit submission candidate

This directory is deliberately not included in an Android module, service,
manifest or product package. It does not change charging on a built device.

`StockLimitController.h` encodes the observed stock `smart_chg` record-1
protocol and serializes submissions through an injected I/O boundary. It is
not an AIDL HAL or a file-node implementation. Evidence is recorded in the
project's `bringup/phase-b/CHARGING_CONTROL_AUDIT_20261024.zh-CN.md` and
`research/2026-10-24/charging-control/`.

## Contract

- For max 5..99, min must equal max-5. The packet is `(max << 16) | 3`.
- The canonical reset is min=0, max=100, encoded as `(100 << 16) | 2`.
  It withdraws record 1; it must not issue a zero-selector/global reset.
- A repeated request is skipped only when the same controller previously
  submitted it successfully, the bound driver/node generation is unchanged,
  and the current record-1 enable bit agrees.
  Rewriting the stock record clears its triggered status, so blindly writing
  on every battery broadcast is inappropriate.
- A new controller starts without knowledge of the previous target. It does
  not infer a target from an enabled bit, a journal, or a hard-coded default.
- Pre-read, write and post-read failures are reported. An unexpected change
  in another record's enable bit reports a conflict without rollback/retry.
  A conflict does not prove that this controller caused the other change.
  A changed driver/node generation during a write also leaves it unverified.
- The last submitted request is private command-deduplication state. There
  is no hardware target-value getter. Observing an enable bit is not proof
  of the independently read target percentage or physical charging power.

## Required before integration

Exclusive ownership of record 1 and reliable I/O generation tracking are
prerequisites for the cache. A same-enable target overwrite by another writer cannot be
detected through the stock flags getter. Audit actual writers and permissions,
and wire `invalidate()` to relevant in-place driver state resets before enabling
this code. A new process already has an empty cache. The future node I/O must
not use a constant generation: replacing its binding or observing driver/node
replacement must change it. A stable open node and identity checks are a
possible implementation, still requiring real sysfs lifecycle validation.

The I/O implementation must validate the actual node identity and parse full
integer responses; short writes and errors must be failures. It must not use
`input_suspend`, modify thermal/FCC/FV settings, or poll charging in a new loop.
The future HAL must expose only proven capabilities and report an unknown
target honestly. Lineage provider mode selection, service restart, plugging,
and native charging behavior still require integration and device acceptance.

## Host verification

Compile `tests/StockLimitControllerTest.cpp` with a C++17 host compiler,
`-Wall -Wextra -Werror -pthread`, then run the result. The fake I/O performs
no file or device access. The 26 checks validate the submission logic and
its error boundaries; they do not execute the stock kernel or validate a
real Binder service. No ROM build is needed for this check.
