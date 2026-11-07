# Rothko Dolby control

Device-local controller for the stock rothko DAP implementation. The original vendor libraries and DAX models remain unchanged. No MIUI framework is required.

The Settings dashboard integration follows LineageOS hardware/samsung packages/Dap (lineage-23.2):
https://github.com/LineageOS/android_hardware_samsung/tree/lineage-23.2/packages/Dap
The Samsung parameter-0/19 protocol is deliberately not used: the original rothko MiSound DolbyAudioEffect wrapper uses implementation UUID 9d4921da-8225-4f29-aefa-39537a04bcaa, parameter 5, and little-endian word arrays. Write return codes are checked before preferences are saved. Settings entry/retry reads the engine state; an immediate query is not treated as a completion fence because the stock DMS propagates updates asynchronously.

Only the four default-group profiles in the stock dax-default.xml are exposed: Dynamic, Movie, Music and Custom. Custom exposes the 20 frequencies from the stock DAX model with half-dB steps over the original ±6 dB UI range, plus a flat reset. Only Custom is edited; gains use the stock mean-removal rule before parameter-110 submission and parameter-106 activation. Saved curves are restored at startup. Successful effect commands acknowledge writes; the UI does not claim an unsupported native EQ readback.

The persistent application retains the global session-0 AudioEffect. Saved settings use device-protected storage and are restored on application start and locked boot completion. No preference is imposed on a fresh installation. AudioEffect ownership failures appear as unavailable and can be retried explicitly.

Validated on rothko: build and policy checks, DMS registration, DAP control and custom gain writes, curve editing and playback, and saved settings after booting the final image. Bluetooth software A2DP, speaker, Spatializer and calls still need a dedicated regression pass with the final Dolby stack.
