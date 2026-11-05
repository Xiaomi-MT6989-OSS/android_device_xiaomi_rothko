# Stock APN table

`apns-conf.xml` is copied without edits from the Redmi K70 Ultra (rothko)
HyperOS OS3.0.307.0.WNNCNXM factory firmware, `product/etc/apns-conf.xml`.
The XML retains its original copyright and Apache 2.0 notice.

- SHA-256: `f7f915c81b8dfd9ed389733fdf07483ee2a3292f409e5bd15a98595108632cce`
- XML version: 8 (matches the current framework's internal APN XML version)
- APN entries: 4749, including 52 with MCC 460

RothkoApnsConf installs at `/product/etc/apns-conf.xml` and overrides the
common Lineage `apns-conf.xml` install module. It does not modify the shared
Lineage APN source or merge in a second dataset. TelephonyProvider prefers
the product table to the system table; a data-side OTA APN file, if present,
has still higher priority. This follows the current upstream loading logic.

The separate `/vendor/etc/vendor-apns-conf.xml` is already byte-identical to
stock (SHA-256 `60aeb604d2977913ba2681a3422faa262149cc581e9f62d1ddefc83044abcfe8`)
and remains unchanged, as does the stock modem initialization resource tree.

This source change needs the next unified build. Existing user/carrier APN
database edits can survive an upgrade. Do not delete TelephonyProvider data or
reset APNs automatically: first verify the installed file, selected provider
source and active APN after upgrading. An APN-table change alone is not proof
that China Telecom IMS registration, calls or SMS have been fixed.
