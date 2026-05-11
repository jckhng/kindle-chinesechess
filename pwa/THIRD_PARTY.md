# Third-Party Notices

## Native Exact Chinese Chess Rules Lineage

- Project lineage: `../xiangqi_engine.c`
- License: GPL-3.0-or-later
- Usage: This PWA ports the native Xiangqi rules and built-in AI to TypeScript.

## Native Exact Chinese Chess Xiangqi Assets

- Asset path: `../assets/xiangqi/`
- Usage: Board and piece PNGs reused by the PWA so the browser build matches
  the native app more closely.
- Provenance: Credited by the parent Exact Chinese Chess project.

## XMuli ChineseChess

- Project: <https://github.com/XMuli/ChineseChess>
- License: GPL-3.0-or-later
- Usage: Rules and AI lineage credited by the native Exact Chinese Chess
  project.

## GnomeGames4Kindle Lineage

- Project lineage: GnomeGames4Kindle, originally by crazy-electron, and later
  contributors.
- Usage: E-ink/KUAL porting groundwork and UX constraints that informed the
  Exact app family.

## Browser Dependencies

- React: MIT.
- Vite and `@vitejs/plugin-react`: MIT.
- TypeScript: Apache-2.0.

## Fairy-Stockfish WASM

- Package: `fairy-stockfish-nnue.wasm`
- Upstream: <https://github.com/fairy-stockfish/fairy-stockfish.wasm>
- Engine lineage: Fairy-Stockfish, <https://github.com/fairy-stockfish/Fairy-Stockfish>
- License: GPL-3.0
- Usage: Browser Xiangqi opponent via UCI with `UCI_Variant=xiangqi`.
- Runtime note: Requires threaded WebAssembly and `SharedArrayBuffer`; the app
  falls back to its built-in TypeScript AI if the browser or host does not
  provide the required isolation headers.
