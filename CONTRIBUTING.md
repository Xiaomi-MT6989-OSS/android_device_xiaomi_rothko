# Contributing

This is an unofficial device bring-up project. Match the pinned firmware, source
revisions and platform series before comparing results. Open a focused issue or
pull request; describe the trigger, observed behavior, expected behavior and the
evidence supporting a change.

Keep device parameters grounded in rothko firmware, official source or measured
runtime evidence. Similar-device trees are references, not proof of rothko values.
Prefer the stock component for the same role where compatibility and provenance
support it. Avoid fabricated capability reports, relaxed authentication, disabled
security policy, arbitrary delay/filter workarounds and unrelated feature changes.

Keep one purpose per commit. Preserve upstream authors, copyright/license notices
and Change-Ids; credit the source of cherry-picks. Platform changes must remain
separate, explain why a device extension cannot suffice, show default-device
behavior and list cross-device effects. Do not squash independent hardware fixes
into a single platform commit. Rebase against recorded lineage-23.2 bases first.

Preserve file bytes and declared line endings. The source candidates disable
automatic Git line-ending conversion because extraction inputs and platform
patches carry exact hashes; do not mass-normalize stock integration files.

Use local source/host checks where relevant and record exactly what they cover.
Changes to native ABI, Binder lifecycle, suspend, thermal/brightness, IMS or data
mounts require targeted runtime evidence before being called fixed. Unknown
behavior should stay explicitly unverified. Never fabricate a passing test.

Do not attach stock binaries, firmware archives, partition dumps, modem NV,
calibration, face/fingerprint data, private keys, IMEI/serials, locations, wireless
ADB endpoints or unredacted personal logs. Submit extraction rules and hashes
instead. A blob transformation must be justified, replayable and hash-checked;
do not silently accept unknown inputs or use --kang to conceal a mismatch.

`python3 .github/check-source.py --kind KIND` checks the public Git history
mechanically. KIND is device/vendor/kernel/integration. CI runs it without device
access and with read-only repository permissions. It is not a licensing or
security certification. Proprietary redistribution and official-device readiness
remain separate from accepting source collaboration.

Report a suspected security issue privately through the repository's private
reporting facility if enabled; do not publish secrets while opening an issue.
