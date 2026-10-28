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


## DSPP/PQ follow-up

The stock libmi_dspp.so SHA-256 is
`05c362e1ddaa2dba5cf7e0dd34dfa5f5d610832fb665f797ce18c052b10bbcd1`;
its packaged proprietary copy is byte-identical. The DSPP vptr offset 0x88
resolves to setC3dLut at 0x133c0. Its AIDL calls resolve through the stock V4
proxy to setDispPQBufHandle (offset 0x118) and setFeatureSwitch (0x290), not
an inferred same-named interface. The mode mapping writes the resulting intent
even when the internal result is nonzero. In particular, the failed PQ-buffer
status/result path at 0x13a68-0x13a78 reaches cleanup at 0x13c88 and returns
zero at 0x13ce8. Do not treat that return or its later callback as LUT success.

The device-specific PQ implementation is
`vendor/lib64/hw/mt6989/vendor.mediatek.hardware.pq_aidl-impl.so`:

* Stock SHA-256: `c18c3bd7271c083f222beaf954038743712948161dcbf6e21237b9e3c19e64c7`.
* Packaged SHA-256: `8a89b861d3ba4b476de736ccfd64bfe453136193df881786fb13c6828c793826`.
* The device compatibility manifest replaces its libui/libtinyxml2 dependencies
  with isolated stock-compatible providers. Both .text and .rodata have identical
  addresses, sizes and hashes; these differences do not change the analyzed
  getter instructions. This does not prove all runtime linkage is equivalent.

PQ getDispC3DMode at 0x804f0 writes the constant 64-bit value
0xffffffff00000000 to its result parcelable and returns Binder OK. No hardware
query occurs in that function. The MPq wrapper calls the same implementation at
0x4e8e0. Therefore neither getter can report current LUT completion on this
firmware. Do not implement a polling completion check using those APIs.

setDispPQBufHandle calls setDispC3DBufHandle, whose bounded body clones/caches
handles, marks pending state and may request display refresh. Those steps alone
are not physical application confirmation. setColorModeWithFence is a separate
operation; the name does not tie its fence to a preceding LUT request. The
relationship to actual HWC/PQ display submission remains to be established.

REA evidence for getter and MPq path:
`ev_9f57452c955104cae9567aa9199da022b94da63a162a60022175b5c787bd3bbc`,
`ev_1f814d934509f28d276a4459f512dd5f8cccb25237b2053054b4252783af8a77`.
Supplementary bounded LLVM/ELF evidence is in the project audit directory
`out/adaptation-principle-audit-20261028/color-mode-chain-02`. This follow-up
changes documentation only; the compatibility fallback remains unresolved.


## HWC/PQ submission boundary

Bounded stock HWC assembly establishes the distinction between parameter
acceptance and later submission. HWCDisplay::setColorModeWithRenderIntent
(0x13ee10) checks advertised mode/intent capability, stores the selection at
0x13f010/0x13f014 and marks a dirty bit before returning success. That success
branch does not call PQ or wait for presentation.

The later HWCDispatcher::PhyPostHandler::set obtains getPqDevice at 0xdb58c,
calls its vptr slot 0x58 at 0xdb5ac and stores its result in the job at 0xdb5b0.
Relocations of both IPqDevice and PqDeviceDrm resolve this slot to
IPqDevice::setDisplayPqMode. Its locked implementation at 0x14d9e0 invokes the
V4 PQ vptr slot 0x4a0: the proxy relocation resolves to
setColorModeWithFenceMPq. It checks Binder status and the returned Result,
returns a fence/error, and does not wait for that returned fence in this body.
The PQ implementation may obtain a DRM fence and enqueue work for its sync
thread. A request, an accepted mode, a submitted job and a signalled fence are
therefore distinct observations.

The existing SF bridge reports setActiveColorMode's software acceptance; it
must not be described as this later job/fence result. A presentation fence by
itself also does not identify a Rothko mode request or prove successful custom
LUT upload. Conversely, ordinary Settings operation should not be redesigned
around an invented physical-LUT ACK contract that stock does not expose.

Do not have this controller independently call setColorModeWithFence or allocate
PQ/DRM fences to manufacture acknowledgement. HWC owns that frame submission
path; an extra writer would need sequencing evidence and can interfere with
HDR/power transitions. Nor should an SF composition hook wait synchronously for
these fences. First establish why intended callbacks are absent/late in the
existing single-writer route, retaining request-generation, scene and software
HWC acceptance checks. The missing-echo fallback remains an explicit audit gap.

Evidence: `out/adaptation-principle-audit-20261028/color-mode-chain-03`, including
bounded function bodies, vtable/proxy corroboration and direct-versus-indirect
call search limits. This is static analysis, not a phone validation or a new
completion implementation.

## Late callback consumption

Host regression exposed a device-controller omission: after the 400 ms
compatibility return, a matching intent event alone was ignored when a valid
gamut value was already known. The worker only watched for another gamut
version. This explains a possible missed policy update; it does not establish
the cause of every missing callback on a phone.

The controller now tracks that late echo until it publishes the matching
accepted intent once. It retains the existing SDR scene, SF generation and
unresolved-intent guards. Starting another mode application, failure or listener
replacement clears this tracking. No OEM command is added and the completed
caller request is not reopened. Gamut notifications remain independent inputs.

The byte-identical Controller.cpp host test fails before this change and passes
after it. A separate timely-echo test verifies that the caller stays pending
until its matching synthetic SF/HWC software acceptance. Wrong-intent late
events do not publish a policy. All eleven host cases pass. Binder delivery,
actual LUT application and visual behaviour still require phone validation.
The missing-echo compatibility success and its persistence semantics remain an
open audit issue; this repair does not turn the fallback into acknowledgement.
