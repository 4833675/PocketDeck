# ADR-0003: Pocket SSH terminal and build-time private key

**Status:** Accepted

**Date:** 2026-07-27

**Decider:** KEXIN

## Context

Pocket Deck needs a small interactive remote shell that remains responsive while
DNS, TCP, SSH negotiation, or a remote command stalls. The Cardputer Adv has a
240×135 display, no PSRAM, and only about 320 KB SRAM. The chosen private key is
local to the build machine and must never enter the public repository.

## Decision

- Use `ewpa/LibSSH-ESP32` 5.9.0 and one interactive PTY-backed shell session.
- Run all blocking libssh operations in one lazily created FreeRTOS task with a
  20,480-byte stack. UI/task traffic passes through a fixed control queue, a
  512-byte transmit stream, and a 1,024-byte receive stream; apps never call
  libssh directly.
- Read `POCKETDECK_SSH_KEY`, falling back to `~/.ssh/id_rsa`, in a PlatformIO
  pre-build script. Generate the numeric key array only under the ignored
  `.pio/build/.../generated` directory. Missing keys produce a valid firmware
  with SSH disabled rather than breaking public builds.
- Embed the complete text key in flash and import it from memory with
  `ssh_pki_import_privkey_base64`. Parse it once when the worker starts and retain
  the resulting `ssh_key` for reconnects; importing a 3,072-bit RSA key after key
  exchange was unreliable at the session's heap low-water point. Never write key
  bytes, terminal input, terminal output, or submitted commands to serial or TF
  diagnostics.
- Store at most six label/host/user/port records in a versioned, checksummed
  `pocketssh` NVS blob. Store no password or private key in NVS.
- Render one 40×13 terminal with 64 lines of fixed-capacity scrollback. Support
  printable ASCII, CR/LF/BS/Tab, common cursor and erase CSI commands, basic ANSI
  colors, OSC suppression, and shell charset-control suppression.
- Reserve `Fn+Tab` for fixed quick commands and `Opt+Fn+Up/Down` for local
  scrollback. All other terminal keys are converted into byte sequences before
  entering the SSH transmit queue.
- Disconnect when the SSH app exits. Automatically retry only a connection made
  before Wi-Fi became ready. Transport, remote-close, and write failures stop for
  explicit Enter so a broken path cannot trigger OpenSSH per-source penalties.
- Fix both transport directions to `aes128-ctr`; LibSSH-ESP32 otherwise prefers
  AES-256-GCM, which exhausted contiguous heap during observed ESP32 key exchange.
- Compile LibSSH-ESP32 with `MAX_BUF_SIZE=1024`. Its 4,096-byte socket allocation
  can fail on fragmented no-PSRAM heap and is incorrectly reported upstream as EOF.
- Track connection milestones in RAM and log the final stage only on failure;
  synchronous TF writes during KEX/RSA authentication consume critical heap.
  Never log endpoints, key material, terminal bytes, or submitted commands.
- Temporarily skip SSH server host-key verification, as explicitly accepted for
  this single-owner prototype. Add TOFU fingerprint persistence before treating
  the terminal as safe on untrusted networks.

## Consequences

- Tracked source and Git history contain no private key, but every generated
  firmware binary contains the selected key and must be treated as a credential.
- Factory reset removes SSH host records but cannot remove a key compiled into
  the application image; replacing it requires rebuilding and reflashing.
- The worker and queues are lazy. On the validated Cardputer Adv, a connected
  shell previously operated at the edge of available heap. ADR-0007 moves the
  frame buffer to an exact-color indexed palette, recovering about 31 KB of
  runtime heap without shrinking the 6.7 KB task-stack high-water margin. There
  is no simultaneous second session.
- The first version intentionally omits SFTP, SCP UI, tunnels, agent forwarding,
  password authentication, custom quick commands, and full xterm/Unicode
  emulation.
- BLE and Wi-Fi continue sharing one radio. SSH must be hardware-tested alongside
  BLE reconnect and weather before this version is committed or pushed.
