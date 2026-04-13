/* hdd.h
 *
 * WD1010 hard-disk controller emulation for TIKI-100_emul
 *
 * Ported from the SDL2/CMake fork at HackerCorpLabs/tiki100 (hdd.c / hdd.h),
 * which itself was ported from the RetroCore C# implementation by Ronny
 * Hansen.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 1985-2026 Ronny Hansen
 *
 * The TIKI-100 WD1010 interface uses I/O ports 0x20-0x27:
 *   0x20..0x23  Data block    (data / error+precomp / sector count / sector)
 *   0x24..0x27  Control block (track lo / track hi / SDH / command+status)
 *
 * This module owns a FILE* per mounted image and reads/writes sectors
 * directly to the file — no in-memory image buffer. It has no dependency on
 * the Win32 frontend; the only external symbols it uses from the rest of
 * the emulator are byte / word / tiki_bool (from Z80.h and TIKI-100_emul.h)
 * and the LOG_* macros from log.h, plus the platform-implemented hddLight()
 * callback declared in TIKI-100_emul.h.
 */

#ifndef HDD_H
#define HDD_H

#include "TIKI-100_emul.h"  /* byte, word via Z80.h + tiki_bool */

/* Geometry constants — matches the ronny-tiki100 reference implementation. */
#define HDD_SECTOR_SIZE     512
#define HDD_MAX_TRACKS      256
#define HDD_MAX_SECTORS     16
#define HDD_MAX_HEADS       4
#define HDD_MAX_DISKS       4

/* Lifecycle ----------------------------------------------------------------*/

/* Clear all controller state and detach any mounted images. Must be called
   once during emulator startup. */
void hddInit (void);

/* Soft-reset the controller (clears task-file registers and in-flight
   command state) without unmounting images. Called by resetEmul(). */
void hddReset (void);

/* Mount a raw 8 MB disk image as hard-disk unit diskNo (0..1 used by the
   TIKI-100 hardware; 2..3 reserved but accepted). Attempts to open the file
   read-write ("r+b") first and falls back to read-only ("rb") if the file
   is not writable. Returns TRUE on success, FALSE on failure. */
tiki_bool hddMount (int diskNo, const char *filename);

/* Close and detach any image previously mounted at diskNo. Safe to call on
   an already-empty slot. */
void hddUnmount (int diskNo);

/* Port 0x20..0x23 — data register block ------------------------------------*/

void hddWriteData        (byte value);  /* 0x20 write */
byte hddReadData         (void);        /* 0x20 read  */
void hddWritePrecomp     (byte value);  /* 0x21 write */
byte hddReadError        (void);        /* 0x21 read  */
void hddWriteSectorCount (byte value);  /* 0x22 write */
byte hddReadSectorCount  (void);        /* 0x22 read  */
void hddWriteSector      (byte value);  /* 0x23 write */
byte hddReadSector       (void);        /* 0x23 read  */

/* Port 0x24..0x27 — control register block ---------------------------------*/

void hddWriteTrackLo (byte value);      /* 0x24 write */
byte hddReadTrackLo  (void);            /* 0x24 read  */
void hddWriteTrackHi (byte value);      /* 0x25 write (write-only) */
void hddWriteSDH     (byte value);      /* 0x26 write (write-only) */
void hddWriteCommand (byte value);      /* 0x27 write — triggers execution */
byte hddReadStatus   (void);            /* 0x27 read  */

/* Out-of-band sector accessor ----------------------------------------------*/

/* Read one 512-byte sector from a mounted drive directly, without touching
   the WD1010 command state machine. Intended for UI code (e.g. the CP/M
   directory viewer) that wants to peek at image contents while the guest
   is free to keep running. Returns TRUE on success, FALSE if the drive is
   not mounted or the read fails.
     drive   0..HDD_MAX_DISKS-1
     cyl     cylinder (0..HDD_MAX_TRACKS-1)
     head    head     (0..HDD_MAX_HEADS-1)
     sector  sector   (1-based, 1..HDD_MAX_SECTORS)
     buf     caller-supplied buffer, at least HDD_SECTOR_SIZE bytes */
tiki_bool hddReadSectorDirect (int drive, int cyl, int head, int sector,
                               byte *buf);

#endif
