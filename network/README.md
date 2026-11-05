# Domestic network services

The user selected domestic connectivity probes and NTP independently of SIM MCC.
This device overlay uses NetworkStackConfig's supported `config_*` hooks. It
retains HTTP captive-portal detection and HTTPS validation. The common Lineage
overlay remains installed; this overlay changes only validation URLs.

HTTP/HTTPS use the stock OS3.0.307.0 product NetworkStackOverlay endpoint,
`connect.rom.miui.com/generate_204`. A Huawei HTTP 204 endpoint supplies fallback.
Unlike stock's additional web-page fallbacks, every selected URL has explicit
204 semantics. No validation mode, TLS verification or success rule is disabled.

The framework `network_time.xml` overlay uses Lineage's existing MCC 460 NTP list
as the device default, including when no SIM is inserted. It does not alter time
source priority, time zone, polling intervals or the automatic-time user setting.

On 2026-11-05 the existing phone was configured through persistent Global settings
without building or flashing. After Wi-Fi reconnected, both Xiaomi probes returned
204 and the network was VALIDATED without PARTIAL_CONNECTIVITY. A forced normal
NTP refresh succeeded with the domestic list; no test server override was used.
Settings-before and sanitized results are kept in the project's private
`out/bringup/domestic-network-services-20261105` directory.

The live settings and the source defaults are separate: these XML files still
require the next unified build and an installed-resource check. The settings have
priority over framework NTP resources; clear `ntp_server` if later testing only the
built-in NTP array. Previously all changed Global settings were absent (`null`),
so `settings delete global <key>` restores their original state.
