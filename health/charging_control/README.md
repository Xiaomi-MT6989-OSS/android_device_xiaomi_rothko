# Stock charging-limit submission candidate

This directory is deliberately not included in an Android module, service,
manifest or product package. It does not change charging on a built device.

`StockLimitController.h` encodes the observed stock `smart_chg` record-1
protocol and serializes submissions through an injected I/O boundary. It is
not an AIDL HAL. `StockLimitFileIo.h` is the uninstalled node I/O candidate.
Evidence is recorded in the
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
this code. A new process already has an empty cache. The node I/O candidate
pins an open descriptor, compares its device/inode identity against the path,
and changes generation after every rebind. `invalidateBinding()` discards a
binding after an explicit reset without writing. These methods still need
actual driver reset/reprobe notification wiring and real sysfs validation.

The I/O candidate's production constructor uses the stock gauge's verified
`smart_chg` path, admits only root-owned regular sysfs files without world-write,
and rejects final-component symlinks. It parses complete nonnegative signed-32
decimal responses, checks EOF to reject a short valid prefix, and refuses
oversized input. A write requires a prior binding
and a record-1 packet; it sends one whole command at offset zero. Errors, short
writes or EINTR discard the binding without retrying the command. It does not use
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

`tests/StockLimitFileIoTest.cpp` checks the real POSIX read/write/identity code
against files it creates under `/tmp`. Only node admission is relaxed in the
test subclass, plus short-read/short-write/EINTR injection. A strict-policy subclass also
verifies that production admission rejects those regular filesystem fixtures.
The 55 assertions (including fixture creation and cleanup) check parsing,
full command writes, replacements before/during writes,
explicit invalidation, symlink/FIFO rejection and cleanup. It never constructs
the production default-path object or opens phone/sysfs nodes. A regular file
is not a kernel protocol simulator: target/enable behavior, `pwrite` support and
real sysfs reprobe semantics still require device verification before integration.
