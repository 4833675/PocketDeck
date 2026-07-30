# Foreground resource isolation smoke test

Use a TF card with `/Music` content and retain `/PocketDeck/system.log`. Do not
clear bonds, NVS, or logs for this test.

## App transitions

- [ ] Cold boot reaches Launcher with BT and Wi-Fi shown as policy-suspended.
- [ ] Enter Keyboard: BLE advertises/reconnects and typing works.
- [ ] Return Home: the Mac disconnects and BLE stops advertising.
- [ ] Enter SSH with Wi-Fi enabled: Wi-Fi connects and the host list remains responsive.
- [ ] Remain in SSH or Weather for several minutes: record whether Wi-Fi unexpectedly cycles through disconnect, scan, and reconnect (known unresolved issue).
- [ ] Return Home: Wi-Fi disconnects and no further scans/retries appear in the log.
- [ ] Enter GPS: RX/checksum counters advance; leave GPS and confirm they stop advancing.
- [ ] Enter LoRa with an antenna attached: radio listens; leave and confirm it sleeps.
- [ ] Enter Weather: GPS and Wi-Fi run together and cached data remains visible.
- [ ] Enter Settings: Wi-Fi scan and BLE host controls still work.

## MEDIA realtime isolation

- [ ] Enter MEDIA while Wi-Fi and BLE master settings are enabled.
- [ ] Confirm BT/Wi-Fi are policy-suspended and GPS counters stop advancing.
- [ ] Play several 128 kbps / 44.1 kHz tracks and listen for underruns.
- [ ] Confirm there are no periodic Wi-Fi scan/reconnect events during playback.
- [ ] Exit MEDIA; verify queued system events flush and `/PocketDeck/system.log` remains readable.
- [ ] Re-enter Keyboard and SSH; verify BLE bond and saved Wi-Fi profiles survived.

Compilation alone does not pass any row above.
