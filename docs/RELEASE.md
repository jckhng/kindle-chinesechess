# Release Notes

## v0.1.0

Artifact:

```text
release/kindle-chinesechess-extension.zip
```

Verify:

```bash
cd release
sha256sum -c SHA256SUMS
```

Current checksum:

```text
18ac1fec09f8c3e30970468666d566a4f71c7d87c78e199530f19aa61afa26d6  kindle-chinesechess-extension.zip
```

Contents:

- ARM hard-float `kindle-chinesechess` executable.
- KUAL extension metadata and launch scripts.
- PNG Xiangqi board and piece artwork.
- Bundled Pikafish UCI engine support.
- Built-in non-Pikafish AI fallback.
- Bundled GTK2/Cairo runtime library set copied from the ARM Docker builder.
- License, artwork, Pikafish source, and third-party runtime notices.

Release highlights:

- Kindle-friendly Xiangqi board with touch input, move history, undo, save, and load.
- Play Red, Play Black, two-player, and AI demo modes.
- Stronger AI through bundled Pikafish, with automatic fallback to the embedded AI.
- PNG artwork adapted for e-ink contrast, including a white board background and
  mid-grey black-side piece backing.

Known constraints:

- This is an unofficial derivative/adaptation release, not an official XMuli
  ChineseChess, Augus1217 Chinese-Chess, Pikafish, or GnomeGames4Kindle
  release.
- Requires a jailbroken Kindle with KUAL.
- Kindle home-screen `.sh` tapping is not reliable unless another launcher/file
  association is installed. Use KUAL.
