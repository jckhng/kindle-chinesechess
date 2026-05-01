# Kindle ChineseChess Extension Files

Copy these files into your Kindle extension folder:

- `config.xml` -> `/mnt/us/extensions/kindle-chinesechess/config.xml`
- `menu.json` -> `/mnt/us/extensions/kindle-chinesechess/menu.json`
- `launch_kindlechinesechess.sh` -> `/mnt/us/extensions/kindle-chinesechess/launch_kindlechinesechess.sh`
- `stop_kindlechinesechess.sh` -> `/mnt/us/extensions/kindle-chinesechess/stop_kindlechinesechess.sh`
- `tail_log_kindlechinesechess.sh` -> `/mnt/us/extensions/kindle-chinesechess/tail_log_kindlechinesechess.sh`

Optional document shortcut:

- `shortcut_kindlechinesechess.sh` -> `/mnt/us/documents/shortcut_kindlechinesechess.sh`

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
