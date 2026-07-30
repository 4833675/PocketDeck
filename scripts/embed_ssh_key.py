Import("env")

import os
from pathlib import Path


def render_header(key_bytes: bytes) -> str:
    payload = key_bytes + b"\0"
    rows = []
    for start in range(0, len(payload), 16):
        chunk = payload[start : start + 16]
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    available = "true" if key_bytes else "false"
    return "\n".join(
        [
            "#pragma once",
            "",
            "#include <cstddef>",
            "",
            "namespace pd::generated {",
            "inline constexpr unsigned char kSshPrivateKey[] = {",
            *rows,
            "};",
            f"inline constexpr std::size_t kSshPrivateKeySize = {len(key_bytes)};",
            f"inline constexpr bool kSshPrivateKeyAvailable = {available};",
            "}  // namespace pd::generated",
            "",
        ]
    )


key_path = Path(os.environ.get("POCKETDECK_SSH_KEY", "~/.ssh/id_rsa")).expanduser()
key_bytes = b""
if key_path.is_file():
    candidate = key_path.read_bytes()
    if b"\0" in candidate:
        raise RuntimeError("Pocket Deck SSH key must be a text private key")
    if len(candidate) > 16384:
        raise RuntimeError("Pocket Deck SSH key is unexpectedly large")
    key_bytes = candidate

generated_dir = Path(env.subst("$BUILD_DIR")) / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)
header_path = generated_dir / "ssh_private_key.h"
content = render_header(key_bytes)
if not header_path.exists() or header_path.read_text(encoding="utf-8") != content:
    header_path.write_text(content, encoding="utf-8")

env.AppendUnique(CPPPATH=[str(generated_dir)])
print("[ssh-key] private key available" if key_bytes else
      "[ssh-key] no private key; SSH connections will be disabled")
