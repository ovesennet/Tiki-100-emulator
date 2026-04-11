# TIKI-100 Emulator

A freeware emulator for the **TIKI 100 Rev. C** computer, originally developed by Asbjørn Djupdal (2000-2001). The TIKI 100 was a Norwegian Z80-based microcomputer produced in the 1980s, running a CP/M clone, (KP/M - later TIKO) and TIKI-BASIC.

> **Note:** This emulator is **Windows only**. It uses the native Win32 API for display, input, and serial/parallel port access.

The emulator includes:
- Z80 CPU emulation (based on Marat Fayzullin's Z80 engine)
- Video emulation (1024×256 high-res, 512×256 medium-res, 256×256 low-res modes)
- Floppy disk controller (WD1793) emulation with support for various disk image formats
- Z80-CTC (Counter/Timer) emulation
- Z80-DART (serial port) emulation
- Keyboard emulation with Norwegian layout support
- Sound/speaker emulation

## Prerequisites to build from the C source

You need **MSYS2** with the **MinGW64** toolchain installed on Windows.

### 1. Install MSYS2

Download and install MSYS2 from [https://www.msys2.org](https://www.msys2.org). Follow the installer defaults.

### 2. Install build tools

Open the **MSYS2 MINGW64** terminal (not the MSYS2 MSYS terminal) and run:

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc make
```

This installs:
- `gcc` — the C compiler (MinGW 64-bit)
- `make` — GNU Make
- `windres` — Windows resource compiler (included with the gcc package)

### 3. Verify installation

```bash
gcc --version
make --version
windres --version
```

## Building

Open the **MSYS2 MINGW64** terminal and navigate to the `src` directory:

```bash
cd /c/code/Tiki-100-emulator/src
make
```

The build output (object files and `tikiemul.exe`) is placed in the `bin/` directory at the project root.

To clean build artifacts:

```bash
make clean
```

## Running

Copy `tiki.rom` from the `src/` directory into the `bin/` directory, then run:

```bash
./bin/tikiemul.exe
```

The emulator looks for `tiki.rom` in the current working directory. Disk images (`.dsk` files) can be loaded from the **Disk drive** menu.

To enable a debug console with logging output, run with the `-console` flag:

```bash
./bin/tikiemul.exe -console
```

## Changes from original v1.1.1

### v1.2.0 (Arctic Retro)

- **Toolbar**: Added a button row above the emulator screen area
  - Z80 info button — opens a live CPU register viewer window
  - Limit speed toggle checkbox, synced with the settings dialog
  - Fullscreen button (F12) — integer-scaled fullscreen with black bars
  - Screenshot button (F11) — copies the emulator screen to the clipboard
  - FPS overlay button (F10) — toggles a frames-per-second counter on the display
- **Fullscreen mode**: True fullscreen with integer scaling, toggle via F12 or toolbar button
- **Screenshot to clipboard**: Copies the emulator screen area to the clipboard (F11)
- **FPS overlay**: Real-time frame rate counter drawn on the emulator display (F10)
- **Z80 information window**: Live-updating window showing all Z80 CPU registers, flags, and interrupt state
- **Help menu**: Added keyboard shortcuts reference dialog
- **Version**: Updated to v1.2.0
- **About dialog**: Updated text to "v1.2.0 by Arctic Retro"
- **Build system**: Simplified Makefile for Windows/MinGW only (removed Amiga and Unix targets); output to `bin/` directory
- **Amiga support removed**: Removed Amiga platform backend and all related files (amiga.c, amiga.cd, amiga_icons/, amiga_translations/)
- **Code fixes**:
  - Fixed `boolean` type conflict with Windows headers (renamed to `tiki_bool`)
  - Fixed Norwegian keyboard input (øæå) — corrected key table alignment and VK code mapping for modern Norwegian keyboard layouts
  - Fixed Norwegian characters in source files (converted from corrupted CP437/CP1252 to UTF-8 with hex escapes where needed)
  - Fixed toolbar scroll bug — emulator content no longer bleeds into the toolbar area
  - Fixed keyboard input in fast mode — keys are reliably registered even at high emulation speed
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
