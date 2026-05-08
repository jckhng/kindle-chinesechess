# Provenance And Licensing

Exact Chinese Chess is an unofficial Kindle-focused Xiangqi / Chinese Chess
port.

It combines three distinct upstream references:

- Rules and built-in AI reference: <https://github.com/XMuli/ChineseChess>
- Board and piece artwork: <https://github.com/Augus1217/Chinese-Chess>
- Optional external engine: <https://github.com/official-pikafish/Pikafish>

## What Comes From XMuli ChineseChess

- Xiangqi starting position and piece model.
- Legal move behavior for general, advisor, elephant, horse, rook, cannon, and
  soldier pieces.
- Built-in minimax/alpha-beta AI approach and material/position scoring ideas.
- GPL-3.0-or-later licensing basis.

## What Comes From Augus1217 Chinese-Chess

- Board and piece PNG artwork copied from `public/images`.
- The source project's `package.json` declares MIT licensing for that project.
- The package credits Augus1217 Chinese-Chess and includes `licenses/MIT`.

## What Comes From Pikafish

- Optional UCI Xiangqi engine binary for stronger AI.
- Pikafish is GPLv3 and requires corresponding source for any distributed
  binary.
- The source is pinned as the top-level `Pikafish` Git submodule at the tested
  commit used for release builds.
- ARM32 build patches are embedded verbatim in `build_pikafish.sh`.
- Reproducible build notes are in `docs/PIKAFISH_ARM32_BUILD.md`.

## What Is Kindle-Specific

- Compact GTK2/Cairo Kindle interface in `main.c`.
- Standalone non-Qt rules/AI engine in `xiangqi_engine.c`.
- Optional external Pikafish UCI process adapter in `pikafish_uci.c`.
- KUAL extension files in `extension/`.
- Docker ARM hard-float build and release packaging scripts.
- Runtime-library bundling for easier Kindle installation.

## Pikafish Notes

Do not commit `bin/armhf/pikafish` or `bin/armhf/pikafish.nnue` to Git. They
are build/download artifacts. The release zip is the distribution unit that
ships the binary and NNUE.

For binary releases with Pikafish included, keep these files in the generated
package:

```text
extensions/exact-chinesechess/LICENSES/PIKAFISH-COPYING.txt
extensions/exact-chinesechess/LICENSES/PIKAFISH-AUTHORS.txt
extensions/exact-chinesechess/LICENSES/PIKAFISH-SOURCE.txt
```

## License Notes

The project keeps GPL-family and MIT license texts in:

```text
licenses/
```

The release zip also bundles shared runtime libraries from Debian Bullseye ARM.
Those libraries keep their own upstream licenses. The generated extension
package includes:

```text
extensions/exact-chinesechess/LICENSES/RUNTIME-LIBS.txt
extensions/exact-chinesechess/LICENSES/THIRD-PARTY-NOTICE.txt
```

If publishing binary releases, keep the license files and runtime notices with
the package.
