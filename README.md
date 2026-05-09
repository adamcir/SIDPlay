# SIDPlay for Commodore 64

SIDPlay is a simple PSID file player for the Commodore 64 written in C with cc65 and a small 6502 assembly helper.

## Features

- Scans disk for `.sid` files
- Selects songs using keyboard menu
- Supports PSID files with normal init/play routines
- Manual timing modes:
  - VBI custom Hz
  - CIA custom Hz
  - Auto from SID header
  - Reject CIA songs
- Can play SID files loaded into higher RAM areas such as `$8000` or `$A000`

## Limitations

This is not a full SID emulator/player.

Currently unsupported or unstable:

- RSID files
- SID files with `Play: $0000`
- Tunes requiring their own IRQ handlers
- Tunes that overwrite the player in memory
- Multi-SID tunes
- Digi/sample-heavy tunes
- Perfect CIA timing
