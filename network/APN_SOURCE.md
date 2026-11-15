# Stock APN table

`apns-conf.xml` is copied without edits from the Redmi K70 Ultra (rothko)
HyperOS OS3.0.307.0.WNNCNXM factory firmware, `product/etc/apns-conf.xml`.
The XML retains its original copyright and Apache 2.0 notice.

- SHA-256: `f7f915c81b8dfd9ed389733fdf07483ee2a3292f409e5bd15a98595108632cce`
- XML version: 8 (matches the current framework's internal APN XML version)
- APN entries: 4749, including 52 with MCC 460

`RothkoApnsConf` installs the table at `/product/etc/apns-conf.xml` and uses the
standard module `overrides` property to replace the common `apns-conf.xml`
installation. The stock table bytes and XML version are preserved.

The former `/odm/telephony/apns-conf.xml` installation did not feed the current
TelephonyProvider. `getApnConfFile()` checks the system file, the OEM directory
(`/oem` unless `OEM_ROOT` overrides it), Product, and the data-side OTA file in
that order. Each existing later file takes precedence. Product therefore also
wins over an OEM file. The OTA APN file retains the highest priority.

The stock table includes Telecom data, MMS and IMS APNs for 46003/46011, and
Broadnet data and IMS APNs for 46013/46015. The 46015 data APN is `cbnet`; the
stock 46013 profile uses `cmnet`. These are separate SIM identities.

The separate `/vendor/etc/vendor-apns-conf.xml` is already byte-identical to
stock (SHA-256 `60aeb604d2977913ba2681a3422faa262149cc581e9f62d1ddefc83044abcfe8`)
and remains unchanged, as does the stock modem initialization resource tree.

This source change needs the next unified build. Existing user/carrier APN
database edits can survive an upgrade. Do not delete TelephonyProvider data or
reset APNs automatically: first verify the installed file, selected provider
source and active APN after upgrading. An APN-table change alone is not proof
that China Telecom IMS registration, calls or SMS have been fixed.
