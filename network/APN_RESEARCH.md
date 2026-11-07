# Stock APN integration research, 2026-11-05

Public device trees show two recurring approaches: merge local carrier rows into
`vendor/apn`, or copy a device table into a TelephonyProvider-supported path with
`PRODUCT_COPY_FILES`. A product prebuilt named separately from the common APN
module creates duplicate install rules in the current Android 16 build system.

Current TelephonyProvider source, `getApnConfFile()`, starts with
`/system/etc/apns-conf.xml`, replaces it with `/odm/telephony/apns-conf.xml`
when present, then replaces that with `/product/etc/apns-conf.xml`, then with
`/data/misc/apns/apns-conf.xml`. `pickSecondIfExists()` performs each
replacement.

The device therefore copies the byte-identical OS3.0.307.0 Product table to
ODM `telephony/apns-conf.xml`. Keeping the common Product package intact avoids
packaging conflicts. The installed XML has 4749 entries and 52 entries with
MCC 460; China Telecom IMS rows for MNC 03 and 11 are present.

This is source preparation only. It was not built or read from the device. A
clean install and a retained-data upgrade must both confirm that ODM is the
selected source, followed by APN database refresh, data connectivity and
China Telecom IMS calls/SMS tests.
