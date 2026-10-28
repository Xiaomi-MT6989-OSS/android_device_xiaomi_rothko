# Stock user-mode completion contract: audit findings

Scope: rothko OS3.0.307.0 stock displayfeature HAL, SHA-256
`962627b0f6913235b8261bec70c92ea7e55450edc57250fe6e60dc525bf5d9dc`.
This is static evidence, not a new device test. Addresses identify research
locations in that exact artifact, never production memory offsets.

## Established facts

* Feature IDs 0, 1 and 2 dispatch via the table at 0x21d10, branch base 0x30b80,
  to 0x30b80, 0x313bc and 0x30bc0 respectively. These paths construct Norm
  (0x4e6f726d), Ehan (0x4568616e) and Stan (0x5374616e) AMessage objects.
  They share parameter insertion and asynchronous post(0) at 0x31a28.
* The shared path subsequently assigns -1 at 0x31a3c and returns at 0x31a74.
  Propagating this HAL integer as a synchronous mode-success result would
  incorrectly reject that ordinary enqueue path. The stock Binder wrapper
  currently returns OK without propagating it anyway.
* getCurrentColorMode (0x309f0) reads mCurColorMode. That field is assigned at
  0x30b18, before the status check and message post. It is a requested value,
  not completion feedback. Reading it cannot replace the missing-echo fallback.
* onMessageReceived handles the posted mode later. The Stan branch can select
  HandleStandard through vptr slot 0x58. ELF relocations resolve the platform
  implementation to 0x54e20 and its setDisplayEffect slot 0x120 to 0x53770.
  This is an internal path, not a response to the earlier Binder request.
* The color output adapter calls DisplayColorManager::setColorModeWithRenderIntent
  (0x32d00). Its backend object is constructed as DSPP_Platform by the color
  manager constructor. That DSPP implementation lives in libmi_dspp.so.
  The already-audited intent callback has no request token and is not a reliable
  standalone success result, including a missing-backend default-intent branch.

## Consequences for this adapter

The 400 ms missing-echo path is still compatibility behaviour with incomplete
confirmation semantics. Neither a longer fixed delay, getCurrentColorMode,
nor forwarding the existing HAL integer establishes asynchronous completion.
Do not replace this path with any of those shortcuts or claim hardware ACKs.

Next investigate the DSPP/PQ completion/readback contract and how the stock SF
consumer interprets callbacks. Any proposed software acceptance condition must
cover missing backend, delayed work, duplicate requests, stale events, backend
restart and matching SF/HWC profile acceptance. Keep requested/current/default
semantics distinct. Do not add platform hooks or patch stock return values until
the device-side and existing stock-interface alternatives have been evaluated.

## Evidence and limitations

REA/Hopper Evidence: getter
`ev_d7310b3390c2377c70151266d335203861d7a5209b093815dd4bf8fe71ca3366`,
platform HandleStandard
`ev_2426d97a3ef780eb819481ba0e4c93e5735384d4196b4495990e5ea430f54347`,
color output
`ev_b6900359e4246114788bee2f1af3bb92f059c2d6e5477c9cd3e2123a4ca3fb7a`.

The 0x48c10 Hopper pseudocode includes adjacent code beyond its ELF symbol;
use bounded LLVM assembly, not its inferred body extent. A separate 0x53770
pseudocode request stalled and was interrupted; no successful decompilation is
claimed. Packed Android relocations were decoded with llvm-readelf; raw zero
file slots and GNU readelf's empty expansion do not mean null runtime methods.

The project audit evidence directory is
`out/adaptation-principle-audit-20261028/color-mode-chain-01` (external to this
repository). It includes complete bounded disassembly, symbols, relocations,
switch entries, target digests and limitations. No ROM build or device operation
was performed for these findings.
