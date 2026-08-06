# ADR-0007: Indexed frame buffer preserves SSH authentication headroom

**Status:** Accepted

**Date:** 2026-08-06

## Context

The Cardputer Adv has no PSRAM. Pocket Deck originally used a 240×135 RGB565
sprite, consuming 64,800 bytes of DMA-capable heap. Persistent v0.9.2 and v0.9.5
logs proved that 3,072-bit RSA authentication was operating at a nondeterministic
heap threshold: it succeeded with 13,632 bytes free before authentication but
failed at 13,488–13,520 bytes with `ssh_userauth_publickey: Out of memory`.
Firmware changes appeared to break SSH because allocation order moved this tiny
margin, even when static RAM and SSH code were unchanged.

Pocket Deck's UI uses a bounded theme and a 16-color ANSI terminal; it does not
render photos, gradients, cover art, or video. A 16-bit frame buffer therefore
spent scarce runtime memory on colors the product never displays.

## Decision

- Use one 8-bit indexed M5GFX sprite for the full 240×135 back buffer.
- Initialize all 256 palette entries and explicitly install every Graphite Mint
  theme color and ANSI terminal color. Drawing code keeps its RGB565 constants;
  their low byte selects the matching palette entry.
- Keep colliding palette indices only where the intended output color is also
  identical. ANSI bright white deliberately reuses the normal text color.
- Keep the existing 20,480-byte SSH worker stack and stream capacities. Do not
  trade stack safety or terminal throughput for heap.
- Lock the memory decision with a native budget test requiring at least 24 KiB
  recovery versus the former RGB565 sprite.

The indexed sprite consumes 32,400 bytes plus a 768-byte palette, recovering
31,632 bytes of dynamic memory. Flash/static-RAM accounting is essentially
unchanged because the frame buffer is allocated at runtime.

## Consequences

- SSH RSA authentication has deterministic headroom instead of relying on an
  approximately 100-byte allocation-order margin.
- Existing fixed UI and terminal colors remain explicit; future arbitrary-color
  graphics must add palette entries or justify returning to a higher-depth path.
- Any future image, cover-art, gradient, or video feature must budget its own
  rendering strategy and must not silently restore a full-screen RGB565 sprite.
- Real v0.9.6 hardware reached `CONNECTED` and opened a shell with 41,240 bytes
  free and a 23,540-byte largest block. Full palette regression coverage remains
  in the hardware checklist.
