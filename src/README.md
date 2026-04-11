# TIKI-100 Emulator

A freeware emulator for the **TIKI 100 Rev. C** computer, originally developed by Asbjørn Djupdal (2000-2001). The TIKI 100 was a Norwegian Z80-based microcomputer produced in the 1980s, running CP/M and TIKI-BASIC.

The emulator includes:
- Z80 CPU emulation (based on Marat Fayzullin's Z80 engine)
- Video emulation (1024×256 high-res, 512×256 medium-res, 256×256 low-res modes)
- Floppy disk controller (WD1793) emulation with support for various disk image formats
- Z80-CTC (Counter/Timer) emulation
- Z80-DART (serial port) emulation
- Keyboard emulation with Norwegian layout support
- Sound/speaker emulation

## Building

Requires **MSYS2 MinGW64** on Windows:

```bash
pacman -Syu
pacman -S make mingw-w64-x86_64-gcc
```

Then build from the MSYS2 MINGW64 terminal:

```bash
cd /c/code/TIKI-100_emul-src
make
```

The resulting `tikiemul.exe` should be run from the directory containing the ROM file.

## Changes from original v1.1.1

### v1.2.0 (Arctic Retro)

- **Toolbar**: Added a button row above the emulator screen area
  - "Test" button that opens a test dialog
  - "Senk hastighet" (reduce speed) toggle checkbox, synced with the settings dialog
- **Version**: Updated to v1.2.0
- **About dialog**: Updated text to "v1.2.0 by Arctic Retro"
- **Build system**: Simplified Makefile for Windows/MinGW only (removed Amiga and Unix targets)
- **Code fixes**:
  - Fixed `boolean` type conflict with Windows headers (renamed to `tiki_bool`)
  - Fixed Norwegian keyboard input (øæå) — corrected key table alignment and VK code mapping for modern Norwegian keyboard layouts
  - Fixed Norwegian characters in source files (converted from corrupted CP437/CP1252 to UTF-8 with hex escapes where needed)
  - Fixed uninitialized `msg.wParam` return value in `WinMain`
  - Compiler warnings suppressed for third-party Z80 code (`-Wno-multichar`, `-Wno-overflow`, `-Wno-pointer-to-int-cast`)
  - Win32 string literals compiled with `-fexec-charset=CP1252` for correct Norwegian character display

## Supported disk image formats

| Tracks | Sides | Sectors | Sector size | Total size |
|--------|-------|---------|-------------|------------|
| 40     | 1     | 18      | 128 bytes   | 90 KB      |
| 40     | 1     | 10      | 512 bytes   | 200 KB     |
| 40     | 2     | 10      | 512 bytes   | 400 KB     |
| 80     | 2     | 10      | 512 bytes   | 800 KB     |

## License

Freeware. Z80 emulation copyright © Marat Fayzullin 1994-1997. Remainder copyright © Asbjørn Djupdal 2000-2001.
