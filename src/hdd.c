/* hdd.c
 *
 * WD1010 hard-disk controller emulation for TIKI-100_emul
 *
 * Ported from the SDL2/CMake fork at HackerCorpLabs/tiki100 (hdd.c / hdd.h),
 * which itself was ported from the RetroCore C# implementation by Ronny
 * Hansen. Style-adjusted to match the surrounding codebase (tiki_bool
 * instead of int for flags, LOG_* instead of printf/fprintf, 2-space
 * K&R, single-file device module analogous to disk.c).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 1985-2026 Ronny Hansen
 *
 * Commands implemented:
 *   0x10  RESTORE      — seek to track 0
 *   0x20  READ SECTOR  — read one 512-byte sector into the host-facing buffer
 *   0x30  WRITE SECTOR — accept 512 bytes from the host and flush to disk
 *
 * SDH register format (port 0x26):
 *   Bits 0-2  Head number (0-7)
 *   Bits 3-4  Drive number (0-3)
 *   Bits 5-6  Sector size  (0=256, 1=512, 2=1024, 3=128)
 *   Bit  7    Extended data field
 *
 * TIKI-100 specific: heads 0-1 address physical drive 0,
 *                    heads 2-3 address physical drive 1 (head - 2).
 *
 * Disk image format: raw binary, 8 MB per drive, CHS-linear layout
 *   offset = cyl * 0x4000 + head * 0x2000 + (sector - 1) * 0x200
 *   (256 cylinders × 2 heads × 16 sectors × 512 bytes per drive)
 */

#include "hdd.h"
#include "TIKI-100_emul.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/* Kommando-tilstand */
enum hddCommand {
  HDC_CMD_NONE,
  HDC_CMD_RESTORE,
  HDC_CMD_READ_SECTOR,
  HDC_CMD_WRITE_SECTOR
};

/* Controller-tilstand ------------------------------------------------------*/

static FILE *hddDisc[HDD_MAX_DISKS];

static byte             hddDataBuf[HDD_SECTOR_SIZE];
static unsigned short   hddDataPtr;
static enum hddCommand  hddActiveCmd;

/* Task-file-registre */
static byte hddTrackLo;
static byte hddTrackHi;
static byte hddSectorReg;
static byte hddSectorCount;
static byte hddSDH;
static byte hddCommandReg;
static byte hddPrecomp;

/* Statusflagg */
static tiki_bool hddBusy;
static tiki_bool hddReady;
static tiki_bool hddDRQ;
static tiki_bool hddSeekComplete;

/* Fysisk stasjon valgt av siste offset-kalkulasjon */
static int hddPhysDrive;

/* Indikatorlys-tilstand per fysisk stasjon, for å unngå å sende
   overflødige hddLight() kall på hver byte i et sektoroppslag. */
static tiki_bool hddLightOn[HDD_MAX_DISKS];

/*****************************************************************************/
/* Oppstart / reset                                                          */
/*****************************************************************************/

void hddInit (void) {
  int i;
  for (i = 0; i < HDD_MAX_DISKS; i++) {
    hddDisc[i]     = NULL;
    hddLightOn[i]  = FALSE;
  }
  hddDataPtr      = 0;
  hddActiveCmd    = HDC_CMD_NONE;
  hddTrackLo      = 0;
  hddTrackHi      = 0;
  hddSectorReg    = 0;
  hddSectorCount  = 0;
  hddSDH          = 0;
  hddCommandReg   = 0;
  hddPrecomp      = 0;
  hddBusy         = FALSE;
  hddReady        = TRUE;
  hddDRQ          = FALSE;
  hddSeekComplete = FALSE;
  hddPhysDrive    = 0;

  for (i = 0; i < HDD_SECTOR_SIZE; i++) {
    hddDataBuf[i] = (byte)i;
  }
}

void hddReset (void) {
  int i;
  hddDataPtr      = 0;
  hddActiveCmd    = HDC_CMD_NONE;
  hddTrackLo      = 0;
  hddTrackHi      = 0;
  hddSectorReg    = 0;
  hddSectorCount  = 0;
  hddSDH          = 0;
  hddCommandReg   = 0;
  hddBusy         = FALSE;
  hddReady        = TRUE;
  hddDRQ          = FALSE;
  hddSeekComplete = FALSE;
  for (i = 0; i < HDD_MAX_DISKS; i++) {
    if (hddLightOn[i]) {
      hddLight (i, FALSE);
      hddLightOn[i] = FALSE;
    }
  }
}

/*****************************************************************************/
/* Montering / avmontering                                                   */
/*****************************************************************************/

tiki_bool hddMount (int diskNo, const char *filename) {
  if (diskNo < 0 || diskNo >= HDD_MAX_DISKS) return FALSE;
  if (!filename) return FALSE;

  /* Lukk eksisterende fil hvis denne sloten allerede er i bruk */
  if (hddDisc[diskNo]) {
    fclose (hddDisc[diskNo]);
    hddDisc[diskNo] = NULL;
  }

  hddDisc[diskNo] = fopen (filename, "r+b");
  if (!hddDisc[diskNo]) {
    /* Faller tilbake til skrivebeskyttet hvis filen ikke er skrivbar */
    hddDisc[diskNo] = fopen (filename, "rb");
    if (hddDisc[diskNo]) {
      LOG_W ("HDD: %s mounted read-only", filename);
    }
  }

  if (!hddDisc[diskNo]) {
    LOG_E ("HDD: cannot open image: %s", filename);
    return FALSE;
  }

  LOG_I ("HDD: mounted drive %d: %s", diskNo, filename);
  return TRUE;
}

void hddUnmount (int diskNo) {
  if (diskNo < 0 || diskNo >= HDD_MAX_DISKS) return;
  if (hddDisc[diskNo]) {
    fclose (hddDisc[diskNo]);
    hddDisc[diskNo] = NULL;
    if (hddLightOn[diskNo]) {
      hddLight (diskNo, FALSE);
      hddLightOn[diskNo] = FALSE;
    }
    LOG_I ("HDD: unmounted drive %d", diskNo);
  }
}

/*****************************************************************************/
/* Stasjonsnærvær / "drive-present" check                                    */
/*****************************************************************************/

/* Is the drive currently addressed by the SDH register actually mounted?
 *
 * Used by the port read/write functions below to make the controller look
 * like "the WD1010 chip isn't installed" when no image is mounted on the
 * selected drive. On real TIKI-100 hardware, if the HDD chip or the drive
 * is absent, reading any of the WD1010 ports returns 0xFF (the data bus
 * floats high because nothing drives it). The TIKI-100 BIOS probe relies
 * on this: it writes the sector number to port 0x23 and reads it back; if
 * the readback isn't what it wrote, the probe bails out cleanly.
 *
 * When we emulate a real WD1010 state machine without an image, the probe
 * DOES see a correct readback (because we store the written value), so it
 * goes further, issues a READ SECTOR, reads 512 bytes of our buffer, and
 * jumps to execute the garbage as code - eventually landing in the BIOS
 * "UDEFINERT INTERRUPT" handler. The cure is to report "no chip" for any
 * drive that isn't mounted, matching real hardware exactly.
 */
static tiki_bool hddSelectedDriveMounted (void) {
  int head  = hddSDH & 0x07;
  int drive = (head >= 2) ? 1 : 0;
  return (drive >= 0 && drive < HDD_MAX_DISKS && hddDisc[drive] != NULL);
}

/*****************************************************************************/
/* Offsetberegning — TIKI-100-spesifikk                                      */
/*****************************************************************************/

/* Internal: side-effect-free CHS-to-linear translation. Used by both the
   WD1010 state-machine path (via hddCalcOffset) and hddReadSectorDirect. */
static long hddCalcOffsetFor (int cyl, int head, int sector,
                              int *outPhysDrive) {
  int sectorZero = sector;
  int physDrive  = 0;
  long offset;

  if (sectorZero > 0) sectorZero--;  /* sektorene er 0-indekserte i filen */

  /* TIKI-100: heads 0-1 => fysisk stasjon 0, heads 2-3 => fysisk stasjon 1 */
  if (head >= 2) {
    physDrive = 1;
    head -= 2;
  }

  offset = (long)cyl    * 0x4000L
         + (long)head   * 0x2000L
         + (long)sectorZero * 0x200L;

  if (outPhysDrive) *outPhysDrive = physDrive;
  return offset;
}

static long hddCalcOffset (void) {
  int head = hddSDH & 0x07;
  int cyl  = (hddTrackHi << 8) | hddTrackLo;
  return hddCalcOffsetFor (cyl, head, hddSectorReg, &hddPhysDrive);
}

/*****************************************************************************/
/* Aktivitetslys-hjelpere                                                    */
/*****************************************************************************/

static void hddLightActivate (int drive) {
  if (drive < 0 || drive >= HDD_MAX_DISKS) return;
  if (!hddLightOn[drive]) {
    hddLight (drive, TRUE);
    hddLightOn[drive] = TRUE;
  }
}

static void hddLightDeactivate (int drive) {
  if (drive < 0 || drive >= HDD_MAX_DISKS) return;
  if (hddLightOn[drive]) {
    hddLight (drive, FALSE);
    hddLightOn[drive] = FALSE;
  }
}

/*****************************************************************************/
/* Disk I/O                                                                  */
/*****************************************************************************/

static tiki_bool hddReadFromDisc (void) {
  long offset;
  FILE *fp;

  if (hddActiveCmd != HDC_CMD_READ_SECTOR) return FALSE;

  hddReady = FALSE;
  offset = hddCalcOffset();

  if (hddPhysDrive >= HDD_MAX_DISKS) return FALSE;
  fp = hddDisc[hddPhysDrive];
  if (!fp) {
    LOG_W ("HDD: read on unmounted drive %d", hddPhysDrive);
    return FALSE;
  }

  hddLightActivate (hddPhysDrive);

  if (fseek (fp, offset, SEEK_SET) != 0) {
    LOG_W ("HDD: seek failed (drive %d, offset %ld)", hddPhysDrive, offset);
    hddLightDeactivate (hddPhysDrive);
    return FALSE;
  }
  if (fread (hddDataBuf, 1, HDD_SECTOR_SIZE, fp) != HDD_SECTOR_SIZE) {
    LOG_W ("HDD: short read at offset %ld on drive %d", offset, hddPhysDrive);
    hddLightDeactivate (hddPhysDrive);
    return FALSE;
  }

  hddReady = TRUE;
  return TRUE;
}

static tiki_bool hddWriteToDisc (void) {
  long offset;
  FILE *fp;

  if (hddActiveCmd != HDC_CMD_WRITE_SECTOR) return FALSE;

  offset = hddCalcOffset();

  if (hddPhysDrive >= HDD_MAX_DISKS) return FALSE;
  fp = hddDisc[hddPhysDrive];
  if (!fp) {
    LOG_W ("HDD: write on unmounted drive %d", hddPhysDrive);
    return FALSE;
  }

  hddLightActivate (hddPhysDrive);

  if (fseek (fp, offset, SEEK_SET) != 0) {
    LOG_W ("HDD: seek failed (drive %d, offset %ld)", hddPhysDrive, offset);
    hddLightDeactivate (hddPhysDrive);
    return FALSE;
  }
  if (fwrite (hddDataBuf, 1, HDD_SECTOR_SIZE, fp) != HDD_SECTOR_SIZE) {
    LOG_W ("HDD: short write at offset %ld on drive %d", offset, hddPhysDrive);
    hddLightDeactivate (hddPhysDrive);
    return FALSE;
  }
  fflush (fp);

  return TRUE;
}

/*****************************************************************************/
/* Kommando-eksekvering                                                      */
/*****************************************************************************/

static void hddExecute (void) {
  hddReady = TRUE;

  switch (hddCommandReg) {
    case 0x10:  /* RESTORE — søk til spor 0 */
      hddActiveCmd    = HDC_CMD_RESTORE;
      hddTrackLo      = 0;
      hddSectorReg    = 0;
      hddDataPtr      = 0;
      hddSeekComplete = FALSE;
      hddDRQ          = FALSE;
      break;

    case 0x20:  /* READ SECTOR */
      hddActiveCmd = HDC_CMD_READ_SECTOR;

      if (hddTrackLo > HDD_MAX_TRACKS) {
        hddActiveCmd = HDC_CMD_NONE;
        return;
      }
      if (hddSectorReg > HDD_MAX_SECTORS) {
        hddActiveCmd = HDC_CMD_NONE;
        return;
      }

      if (hddReadFromDisc()) {
        hddDataPtr      = 0;
        hddSeekComplete = TRUE;
        hddDRQ          = TRUE;
      } else {
        hddActiveCmd = HDC_CMD_NONE;
      }
      break;

    case 0x30:  /* WRITE SECTOR */
      hddActiveCmd    = HDC_CMD_WRITE_SECTOR;
      hddDataPtr      = 0;
      hddSeekComplete = TRUE;
      hddDRQ          = TRUE;
      break;

    default:
      break;
  }
}

/*****************************************************************************/
/* Port 0x20..0x23 — data register block                                     */
/*****************************************************************************/

void hddWriteData (byte value) {
  if (hddDataPtr < HDD_SECTOR_SIZE) {
    hddDataBuf[hddDataPtr] = value;
    hddDataPtr++;
    if (hddDataPtr >= HDD_SECTOR_SIZE) {
      hddDRQ = FALSE;
      if (!hddWriteToDisc()) {
        hddReady = FALSE;
      }
      hddActiveCmd = HDC_CMD_NONE;
      hddLightDeactivate (hddPhysDrive);
    }
  }
}

byte hddReadData (void) {
  byte retval = 0x00;
  if (!hddSelectedDriveMounted ()) return 0xff;
  if (hddDataPtr < HDD_SECTOR_SIZE) {
    retval = hddDataBuf[hddDataPtr];
    hddDataPtr++;
    if (hddDataPtr >= HDD_SECTOR_SIZE) {
      hddDRQ = FALSE;
      hddActiveCmd = HDC_CMD_NONE;
      hddLightDeactivate (hddPhysDrive);
    }
  }
  return retval;
}

void hddWritePrecomp     (byte value) { hddPrecomp     = value; }
void hddWriteSectorCount (byte value) { hddSectorCount = value; }
void hddWriteSector      (byte value) { hddSectorReg   = value; }

byte hddReadError        (void) { return hddSelectedDriveMounted () ? 0x00 : 0xff; }
byte hddReadSectorCount  (void) { return hddSelectedDriveMounted () ? hddSectorCount : 0xff; }
byte hddReadSector       (void) { return hddSelectedDriveMounted () ? hddSectorReg   : 0xff; }

/*****************************************************************************/
/* Port 0x24..0x27 — control register block                                  */
/*****************************************************************************/

void hddWriteTrackLo (byte value) { hddTrackLo = value; }
void hddWriteTrackHi (byte value) { hddTrackHi = value; }
void hddWriteSDH     (byte value) { hddSDH     = value; }

void hddWriteCommand (byte value) {
  hddCommandReg = value;
  /* If the selected drive has no image mounted, behave like the WD1010
   * chip isn't present: ignore the command entirely, leave internal
   * state machine idle, so subsequent status/register reads keep
   * returning 0xFF and the BIOS probe bails out. */
  if (!hddSelectedDriveMounted ()) {
    hddActiveCmd    = HDC_CMD_NONE;
    hddDRQ          = FALSE;
    hddSeekComplete = FALSE;
    return;
  }
  hddExecute();
}

byte hddReadTrackLo (void) {
  return hddSelectedDriveMounted () ? hddTrackLo : 0xff;
}

byte hddReadStatus (void) {
  byte retval = 0;
  /* No chip/drive at the selected slot -> bus floats high. This is the
   * critical path: the BIOS probe reads this register first and will
   * bail out when it sees 0xFF (BUSY bit set -> RET NZ at f942). */
  if (!hddSelectedDriveMounted ()) return 0xff;
  if (hddBusy)         retval |= 0x80;
  if (hddReady)        retval |= 0x40;
  if (hddSeekComplete) retval |= 0x10;
  if (hddDRQ)          retval |= 0x08;
  return retval;
}

/*****************************************************************************/
/* Out-of-band sector accessor (for UI directory browsers)                   */
/*****************************************************************************/

tiki_bool hddReadSectorDirect (int drive, int cyl, int head, int sector,
                               byte *buf) {
  FILE *fp;
  long offset;
  int  physDrive = 0;

  if (drive < 0 || drive >= HDD_MAX_DISKS) return FALSE;
  if (!buf) return FALSE;

  fp = hddDisc[drive];
  if (!fp) return FALSE;

  /* The caller addresses the drive directly (bypassing the TIKI-100 SDH
     head-to-drive remap), so we pass head as-is and drive as the physical
     drive. We still go through the shared CHS-to-linear helper so the
     arithmetic stays in one place. */
  offset = hddCalcOffsetFor (cyl, head, sector, &physDrive);
  (void)physDrive;  /* caller chose the drive explicitly */

  if (fseek (fp, offset, SEEK_SET) != 0) return FALSE;
  if (fread (buf, 1, HDD_SECTOR_SIZE, fp) != HDD_SECTOR_SIZE) return FALSE;
  return TRUE;
}

/*****************************************************************************/
/* Platform-facing mount wrappers (declared in TIKI-100_emul.h)              */
/*****************************************************************************/

tiki_bool insertHdd (int drive, const char *path) {
  return hddMount (drive, path);
}

void removeHdd (int drive) {
  hddUnmount (drive);
}
