# TIKI-100 Emulator

A freeware emulator for the **TIKI 100 Rev. C** computer, originally developed by Asbjørn Djupdal (2000-2001). The TIKI 100 was a Norwegian Z80-based microcomputer produced in the 1980s, running a CP/M clone, (KP/M - later TIKO) and TIKI-BASIC.

> **Note:** This emulator is **Windows only**. It uses the native Win32 API for display, input, and serial/parallel port access.

 > **Note:** The original source code for Windows and Amiga can be found in the 'original' folder.

## Screenshots

![Screenshot 1](img/screenshot1.png)
![Screenshot 2](img/screenshot2.png)
![Screenshot 3](img/screenshot3.png)

The emulator includes:
- Z80 CPU emulation (based on Marat Fayzullin's Z80 engine)
- Video emulation (1024×256 high-res, 512×256 medium-res, 256×256 low-res modes)
- Floppy disk controller (WD1793) emulation with support for various disk image formats
- Hard disk controller (WD1010) emulation — up to 2 × 8 MB raw disk images
- Z80-CTC (Counter/Timer) emulation
- Z80-DART (serial port) emulation
- Keyboard emulation with Norwegian layout support
- Sound chip emulation (AY-3-8912 PSG — 3 tone channels, noise, envelopes)

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

### Command-line options

```bash
./bin/tikiemul.exe [-diska <path>] [-diskb <path>] [-hd0 <path>] [-hd1 <path>] [-console]
```

| Option | Description |
|--------|-------------|
| `-diska <path>` | Load a floppy disk image into drive A: at startup |
| `-diskb <path>` | Load a floppy disk image into drive B: at startup |
| `-hd0 <path>` | Mount a hard disk image as HDD 0 at startup |
| `-hd1 <path>` | Mount a hard disk image as HDD 1 at startup |
| `-console` | Enable debug logging to stderr and `tikiemul.log` |

> **Note:** Floppy drive A: is automatically disabled while any hard disk is mounted, matching the original TIKI-100 hardware behavior. If both `-hd0` and `-diska` are specified, the HDD takes priority.

## Changes from original v1.1.1

### v1.2.0 (Arctic Retro)

**New features:**
- **WD1010 hard disk controller emulation**: Supports up to 2 × 8 MB raw disk images (256 cylinders × 2 heads × 16 sectors × 512 bytes). I/O ports 0x20–0x27. Commands: RESTORE, READ SECTOR, WRITE SECTOR. Images opened read-write with fallback to read-only
- **Hard disk menu**: New "Hard disk" menu with Load/Eject for HDD 0 and HDD 1. Mount paths are persisted in the `[HardDisks]` section of `tikiemul.ini` and auto-remounted on next launch
- **HDD activity LEDs**: Status bar shows activity indicators for HDD 0 and HDD 1 with an 80 ms decay timer for visibility
- **HDD command-line flags**: `-hd0 <path>` and `-hd1 <path>` mount hard disk images at startup
- **Floppy A lockout**: Loading floppy drive A: is automatically disabled while any hard disk is mounted, matching original TIKI-100 hardware behavior
- **AY-3-8912 sound chip emulation**: Full PSG audio with 3 tone channels, noise generator, envelope generator, and register write masking — output via waveOut API at 44100 Hz
- **Toolbar**: Button row above the emulator screen area with quick-access tools
- **Fullscreen mode**: Integer-scaled fullscreen with black bars, toggle via F12 or toolbar
- **Screenshot to clipboard**: Copies the emulator screen to the clipboard (F11)
- **FPS overlay**: Real-time frame rate counter drawn on the display (F10)
- **Z80 information window**: Live-updating CPU register, flags, and interrupt state viewer (toolbar button)
- **Memory viewer/editor**: Hex/ASCII memory viewer with search, direct editing, and address navigation (toolbar button)
- **Disk directory viewer**: CP/M directory listing for loaded disk images, with file sizes and disk usage summary (toolbar button). Includes an **Add file** button per drive — select any host file to write it into the CP/M disk image (validates free space, creates directory entries, and saves the `.dsk` file automatically)
- **CPU halt/continue**: Pause and resume Z80 execution from the memory viewer toolbar
- **Command-line disk loading**: Load disk images at startup with `-diska <path>` and `-diskb <path>`
- **Amstrad CPC DSK support**: Added direct support for the 200kb Extended CPC DSK format (EDSK). A sligtly different DSK format used by z88dk and other tools — container is transparently converted to raw sector data on load. With this you don't have to run INSTALL on the OS to set up the machine for 200K floppy drive.
- **Debug logging**: Optional `-console` flag enables logging to stderr and `tikiemul.log`
- **Help menu**: Keyboard shortcuts reference dialog
- **Disk filename status bar**: New row at the bottom of the window showing `A: filename.dsk  B: filename.dsk` (or "not loaded") for each floppy drive, plus `HD0:` / `HD1:` when hard disk images are mounted
- **Minimum window size**: Low-res (40-column) mode enforces a 350px minimum width; the emulator area is centered with dark gray margins on all sides
- **Custom About dialog**: Shows the application icon (128×128), version string, credits, and a clickable GitHub repository link
- **EXE version information**: File properties now show version, description, and copyright via embedded VERSIONINFO resource
- **Centralized version constant**: Single `version.h` header defines `VERSION_STR`, `VERSION_MAJOR/MINOR/PATCH` — used by the window title, About dialog, log messages, and EXE resource

**FDC emulation fixes (200KB disk support):**
- **Side select via port 0x1C bit 4**: The system register side select signal was previously ignored by the FDC emulation; READ_ADDR and WRITE_TRACK now use it
- **READ_ADDR Record Not Found for non-existent sides**: Single-sided disks now correctly return RNF (status 0x10) when the BIOS probes side 1, enabling proper format detection
- **Mixed-density boot track simulation**: READ_ADDR reports 128-byte sectors on tracks 0–1 for single-sided DD disks, matching real TIKI-100 200KB media which used single-density boot tracks
- **DPB patching for 200KB disks**: The correct CP/M Disk Parameter Block (SPT=40, BSH=3, OFF=2) is written into Z80 RAM when a 200KB disk is first accessed, since the TIKO BIOS format detection doesn't always update the DPB
- **Type III command mask fix**: READ_ADDR, READ_TRACK, and WRITE_TRACK command masks changed from `0xF8` to `0xF0` to handle all valid WD FD17xx command variants (including head-load flag)

**Other code fixes:**
- Fixed `boolean` type conflict with Windows headers (renamed to `tiki_bool`)
- Fixed Norwegian keyboard input (øæå) — corrected key table alignment and VK code mapping
- Fixed Norwegian characters in source files (converted to UTF-8 with hex escapes)
- Fixed toolbar scroll bug — emulator content no longer bleeds into the toolbar area
- Fixed keyboard input in fast mode — keys are reliably registered at high emulation speed
- Fixed uninitialized `msg.wParam` return value in `WinMain`
- Compiler warnings suppressed for third-party Z80 code

**UI modernization:**
- **Visual styles manifest**: Embedded application manifest enables Windows Common Controls v6 (modern button/dialog rendering)
- **DPI awareness**: Per-Monitor V2 DPI awareness declared in manifest for crisp rendering on high-DPI displays
- **Dark title bar**: Uses `DwmSetWindowAttribute` for immersive dark mode title bar on Windows 10/11
- **Drag-and-drop disk loading**: Drop `.dsk` files onto the window to load into drive A: (hold Shift for drive B:)
- **Most Recently Used (MRU) disk list**: Last 8 loaded disk images appear in the Disk drive menu for quick access; persisted to `tikiemul.ini`
- **Resizable window with integer scaling**: Window is freely resizable; the emulator display scales to the largest integer multiple that fits, centered with dark gray margins
- **Window position persistence**: Window position is saved to `tikiemul.ini` on exit and restored on next launch (validated to ensure it's on-screen)

**Build/platform changes:**
- Renamed all user-visible strings from "TIKI-100_emul" to "TIKI-100 Emulator"
- Updated to v1.2.0, about dialog shows "v1.2.0 by Arctic Retro"
- Simplified Makefile for Windows/MinGW only (removed Amiga and Unix targets); output to `bin/` directory
- Amiga support removed (amiga.c, amiga.cd, amiga_icons/, amiga_translations/)
- Win32 string literals compiled with `-fexec-charset=CP1252` for correct Norwegian character display

## Supported disk image formats

### Floppy disks (WD1793 FDC)

| Tracks | Sides | Sectors | Sector size | Total size |
|--------|-------|---------|-------------|------------|
| 40     | 1     | 18      | 128 bytes   | 90 KB      |
| 40     | 1     | 10      | 512 bytes   | 200 KB     |
| 40     | 2     | 10      | 512 bytes   | 400 KB     |
| 80     | 2     | 10      | 512 bytes   | 800 KB     |

Floppy images can be raw sector dumps or Amstrad CPC DSK / Extended CPC DSK containers (automatically converted on load).

### Hard disks (WD1010 HDC)

| Cylinders | Heads | Sectors | Sector size | Total size |
|-----------|-------|---------|-------------|------------|
| 256       | 2     | 16      | 512 bytes   | 8 MB       |

Hard disk images are raw sector dumps (`.dsk`, `.img`, or `.hdd`). Up to 2 drives can be mounted simultaneously.

## Example disk images

The `src/plater/` directory contains several ready-to-use disk images:

| File | Size | Description |
|------|------|-------------|
| `tiko_kjerne_v4.01.dsk` | 400 KB | TIKO kernel v4.01 — the CP/M-compatible operating system. Boot this first |
| `t90.dsk` | 90 KB | Empty formatted 90 KB disk (40 tracks, 1 side, 128-byte sectors) |
| `t200.dsk` | 200 KB | Empty formatted 200 KB disk (40 tracks, 1 side, 512-byte sectors) |
| `t400.dsk` | 400 KB | Empty formatted 400 KB disk (40 tracks, 2 sides, 512-byte sectors) |
| `t800.dsk` | 800 KB | Empty formatted 800 KB disk (80 tracks, 2 sides, 512-byte sectors) |
| `hello.dsk` | 420 KB | Example application disk (not bootable — load TIKO kernel first) |

To boot the emulator: load `tiko_kjerne_v4.01.dsk` into drive A:, then reset. Application disks can be loaded into drive B: (or swapped into A: after booting).

More TIKI-100 software can be found at [https://www.djupdal.org/tiki/program/](https://www.djupdal.org/tiki/program/).

## Known limitations

> **Note:** Serial port (Z80-DART) and parallel port emulation have not been tested in this version.

## License

Freeware. Z80 emulation copyright © Marat Fayzullin 1994-1997. Remainder copyright © Asbjørn Djupdal 2000-2001.

## Links

- **GitHub**: [https://github.com/ovesennet/Tiki-100-emulator](https://github.com/ovesennet/Tiki-100-emulator)
