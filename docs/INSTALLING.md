# Installing On Kindle

## Prerequisites

This package targets jailbroken Kindle devices that can run KUAL extensions.

You need:

- A jailbroken Kindle.
- KUAL installed.
- MRPI installed if your jailbreak/KUAL setup uses MRPI for package installs.
- USB access to copy the extension package.

References:

- Kindle Modding Wiki, jailbreak overview: https://kindlemodding.org/jailbreaking/
- Kindle Modding Wiki, KUAL and MRPI setup: https://kindlemodding.org/jailbreaking/post-jailbreak/installing-kual-mrpi/
- MobileRead KUAL thread: https://www.mobileread.com/forums/showthread.php?t=203326
- MobileRead MRPI wiki: https://wiki.mobileread.com/wiki/MobileRead_Package_Installer
- MobileRead Kindle 5.x jailbreak notes: https://wiki.mobileread.com/wiki/5_x_Jailbreak

Kindle jailbreak compatibility depends on model and firmware. Follow the
current guide for your exact device.

## Install

Unzip `release/kindle-chinesechess-extension.zip` at the Kindle USB-storage root.

Expected paths:

```text
/mnt/us/extensions/kindle-chinesechess
/mnt/us/documents/shortcut_kindlechinesechess.sh
```

Fix execute bits over SSH if needed:

```sh
chmod 755 /mnt/us/extensions/kindle-chinesechess/*.sh
chmod 755 /mnt/us/extensions/kindle-chinesechess/bin/armhf/kindle-chinesechess
chmod 755 /mnt/us/documents/shortcut_kindlechinesechess.sh
```

Launch from KUAL:

```text
KUAL -> Kindle ChineseChess -> Launch
```

The document shortcut is optional. KUAL is the reliable launch path; a stock
Kindle home screen normally will not execute `.sh` files unless another script
launcher/file association is installed.

Logs:

```sh
tail -n 120 /mnt/us/kindle-chinesechess.log
tail -n 120 /mnt/us/kindle-chinesechess-shortcut.log
```
