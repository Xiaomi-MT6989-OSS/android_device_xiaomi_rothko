# Stock VSYNC work-duration mapping

Source: rothko HyperOS OS3.0.307.0.WNNCNXM, `vendor/build.prop`,
`odm/etc/build.prop`, and `system_ext/lib64/libsurfaceflinger.so`.
Library SHA-256:
`a0eea81996f3ead444e2d19180dabd69812394f0cccb532bd65abab1cc46bb42`.

Static LLVM disassembly of the exported stock WorkDuration constructor at
`0x3f9af0` (1936 bytes) ties the high_fps and 144_fps property strings to member
groups starting at offsets `0x180` and `0x1b8`. `constructOffsets(long)` at
`0x3f8cc0` (1212 bytes) selects groups by VSYNC period:

- period <= 6399999 ns: separate 165_fps group, not used by this panel's modes;
- 6399999 < period <= 7999999 ns: 144_fps;
- 7999999 < period <= 14999999 ns: high_fps;
- longer periods: normal group with the stock MtkDuration lookup path.

Thus the verified 90/120 Hz modes use high_fps, and 144 Hz uses 144_fps.
This is static branch/data-flow evidence, not an observed stock runtime trace.
REA/Hopper startup timed out on this library; no REA pseudocode result was used.

| Mode | early / earlyGl / late SF | early / earlyGl / late app |
|---|---:|---:|
| 90, 120 Hz | 10300000 ns | 11600000 ns |
| 144 Hz | 8900000 ns | 10500000 ns |

The current Lineage `Scheduler/VsyncConfiguration.cpp` WorkDuration reader
accepts `debug.sf.<phase>.<app|sf>.duration.<integer-fps>`, not the stock private
high_fps/144_fps duration names. Only device properties are mapped; the existing
Lineage reader is reused without a new scheduler patch.

The effective stock generic values are `debug.sf.hwc.min.duration=2000000`
and `debug.sf.late.sf.duration=15600000`; the latter is the final stock definition.
The other normal early/earlyGl/app defaults remain unchanged. Unused legacy
group keys and a repeated identical touch-timer property are removed.

This does not recreate the complete proprietary MtkDuration configuration,
change panel modes, implement 30 Hz AOD, or establish equivalent latency/power.
ROM build and 60/90/120/144 Hz frame-timeline regression are still required.
