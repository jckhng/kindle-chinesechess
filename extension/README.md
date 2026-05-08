# Exact Chinese Chess Extension Files

Copy these files into your Kindle extension folder:

- `config.xml` -> `/mnt/us/extensions/exact-chinesechess/config.xml`
- `menu.json` -> `/mnt/us/extensions/exact-chinesechess/menu.json`
- `launch_exactchinesechess.sh` -> `/mnt/us/extensions/exact-chinesechess/launch_exactchinesechess.sh`
- `stop_exactchinesechess.sh` -> `/mnt/us/extensions/exact-chinesechess/stop_exactchinesechess.sh`
- `tail_log_exactchinesechess.sh` -> `/mnt/us/extensions/exact-chinesechess/tail_log_exactchinesechess.sh`

Optional document shortcut:

- `shortcut_exactchinesechess.sh` -> `/mnt/us/documents/shortcut_exactchinesechess.sh`

KUAL is the reliable tap-launch path. The stock Kindle home screen normally
does not execute `.sh` files directly.

Credits and licensing:

- Rules and built-in AI reference: XMuli ChineseChess, GPL-3.0-or-later,
  https://github.com/XMuli/ChineseChess
- Board and piece artwork: Augus1217 Chinese-Chess, MIT per package metadata,
  https://github.com/Augus1217/Chinese-Chess
- Optional bundled engine: Pikafish, GPLv3,
  https://github.com/official-pikafish/Pikafish

When Pikafish is bundled, see LICENSES/PIKAFISH-SOURCE.txt for the exact
upstream commit and corresponding source/build notes.
