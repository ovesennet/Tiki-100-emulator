/* disk.c V1.3.0
 *
 * FD17xx emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"
#include "sleep.h"

#include <string.h>

extern byte ram[];

enum diskCommands {
  NO_COMMAND, READ_SECT, READ_ADDR, WRITE_SECT, READ_TRACK, WRITE_TRACK
};

#define STEPIN     1
#define STEPOUT   -1

/* data for en diskettstasjon */
struct diskParams {
  byte *diskImage;  /* diskettbilde */
  long diskImageSize; /* størrelse på diskettbilde i bytes */
  tiki_bool active;   /* om det er diskett i stasjonen */
  int tracks;       /* antall spor */
  int sides;        /* antall sider - 1 */
  int sectors;      /* sektorer pr spor */
  int sectSize;     /* bytes pr sektor */
  int realTrack;    /* hvilken spor r/w-hode står over */
};

/* protos */

static void setLights (void);
static void setLight (int drive, tiki_bool status);

/* variabler */

static struct diskParams params0;   /* data for stasjon 0 */
static struct diskParams params1;   /* data for stasjon 1 */
static struct diskParams *params;   /* data for aktiv plate */

static int side = 0;                /* gjeldende side (0 eller 1) */
static int sideSelect = 0;          /* sidevelger fra systemregister (port 0x1C bit 4) */
static tiki_bool motOn = FALSE;       /* om motor er på */

static byte track = 0;              /* sporregister */
static byte sector = 1;             /* sektorregister */
static byte data = 0;               /* dataregister */
static byte status = 0x20;          /* statusregister */

static byte stepDir = STEPIN;       /* verdi som legges til realTrack ved STEP */
static byte command = NO_COMMAND;   /* kommando som pågår */
static long imageOffset = 0;        /* offset inn i diskImage, ved r/w */
static long endOffset = 0;          /* hvor en r/w kommando skal avslutte */
static int byteCount = 0;           /* antall behandlede bytes ved READ_ADDR, WRITE_TRACK */


/*****************************************************************************/

/* skriv til kontrollregister */
void diskControl (byte value) {
  /* restore */
  if ((value & 0xf0) == 0x00) {
    track = 0;
    params->realTrack = 0;
    status = 0x24;
  }
  /* søk */
  else if ((value & 0xf0) == 0x10) {
    if (data >= params->tracks)
      status = 0x30 | (params->realTrack == 0 ? 0x04 : 0x00);
    else {
      params->realTrack = data;
      track = params->realTrack;
      status = 0x20 | (params->realTrack == 0 ? 0x04 : 0x00);
    }
  }
  /* step */
  else if ((value & 0xe0) == 0x20) {
    params->realTrack += stepDir;
    if (params->realTrack >= params->tracks) params->realTrack = params->tracks - 1;
    if (params->realTrack < 0) params->realTrack = 0;
    status = 0x20 | (params->realTrack == 0 ? 0x04 : 0x00);
    if (value & 0x10)
      track = params->realTrack;
  }
  /* step inn */
  else if ((value & 0xe0) == 0x40) {
    params->realTrack += STEPIN;
    if (params->realTrack >= params->tracks) params->realTrack = params->tracks - 1;
    stepDir = STEPIN;
    status = 0x20;
    if (value & 0x10)
      track = params->realTrack;
  }
  /* step ut */
  else if ((value & 0xe0) == 0x60) {
    params->realTrack += STEPOUT;
    if (params->realTrack < 0) params->realTrack = 0;
    stepDir = STEPOUT;
    status = 0x20 | (params->realTrack == 0 ? 0x04 : 0x00);
    if (value & 0x10)
      track = params->realTrack;
  }
  /* les sektor */
  else if ((value & 0xe0) == 0x80) {
    if (params->active) {
      side = (value & 0x02) ? 1 : 0;
      if ((sector > params->sectors) || (side > params->sides)) {
        status = 0x10;
      }
      else {
        if (value & 0x10) /* les resten av sektor i spor */
          endOffset = (((params->realTrack+1) << params->sides) + side) *      /* BUG ?? */
            params->sectors * params->sectSize;
        else  /* les kun 1 sektor */
          endOffset = ((params->realTrack << params->sides) + side) * params->sectors *
            params->sectSize + sector * params->sectSize;
        imageOffset = ((params->realTrack << params->sides) + side) * params->sectors *
          params->sectSize + (sector-1) * params->sectSize;
        /* Patch the CP/M DPB for 200KB single-sided disks.
         * The TIKO BIOS format detection doesn't always update
         * the DPB when a 200KB disk is loaded in a drive that
         * was previously using a different format.  Write the
         * correct 200KB DPB values directly into Z80 RAM so
         * that CP/M uses the right disk geometry. */
        { static int patched[2] = {0, 0};
          int drv = (params == &params1) ? 1 : 0;
          if (!patched[drv] && params->sides == 0 && params->sectSize == 512) {
            patched[drv] = 1;
            int attrAddr = 0xDA38 + drv;
            int attr = ram[attrAddr];
            int dphIdx = (attr & 0x30) >> 4;
            int dphAddr = 0xDA48 + dphIdx * 16;
            int dpbPtr = ram[dphAddr+10] | (ram[dphAddr+11] << 8);
            if (dpbPtr >= 0xD000 && dpbPtr < 0xFF00) {
              /* 200KB DPB: SPT=40 BSH=3 BLM=7 EXM=0 DSM=189 DRM=63
               * AL0=0xC0 AL1=0x00 CKS=16 OFF=2 */
              static const byte dpb200[15] = {
                0x28,0x00, 0x03, 0x07, 0x00, 0xBD,0x00,
                0x3F,0x00, 0xC0,0x00, 0x10,0x00, 0x02,0x00
              };
              memcpy(&ram[dpbPtr], dpb200, 15);
            }
          }
        }
        command = READ_SECT;
        status = 0x02;
      }
    } else {
      status = 0x80;
    }
  }
  /* skriv sektor */
  else if ((value & 0xe0) == 0xa0) {
    if (params->active) {
      side = (value & 0x02) ? 1 : 0;
      if ((sector > params->sectors) || (side > params->sides)) {
        status = 0x10;
      }
      else {
        if (value & 0x10) /* skriv resten av sektor i spor */
          endOffset = (((params->realTrack+1) << params->sides) + side) *      /* BUG ?? */
            params->sectors * params->sectSize;
        else  /* skriv kun 1 sektor */
          endOffset = ((params->realTrack << params->sides) + side) * params->sectors *
            params->sectSize + sector * params->sectSize;
        imageOffset = ((params->realTrack << params->sides) + side) * params->sectors *
          params->sectSize + (sector-1) * params->sectSize;
        command = WRITE_SECT;
        status = 0x02;
      }
    } else {
      status = 0x80;
    }
  }
  /* les adresse */
  else if ((value & 0xf0) == 0xc0) {
    if (params->active) {
      side = sideSelect;
      if (side > params->sides) {
        /* Single-sided drive: no ID fields exist on side 1.
         * Real hardware times out with Record Not Found.
         * This is critical for TIKO format detection to
         * correctly identify single-sided (200KB) disks. */
        status = 0x10;  /* Record Not Found */
      } else {
        command = READ_ADDR;
        byteCount = 0;
        status = 0x02;
      }
    } else {
      status = 0x80;
    }
  }
  /* les spor -- ikke implementert */
  else if ((value & 0xf0) == 0xe0) {
    status = 0x80;
  }
  /* skriv spor -- kun delvis implementert */
  else if ((value & 0xf0) == 0xf0) {
    if (params->active) {
      side = sideSelect;
      if (side > params->sides)
        side = 0;  /* single-sided: fall back to side 0 */
      command = WRITE_TRACK;
      byteCount = 0;
      status = 0x02;
    } else {
      status = 0x80;
    }
  }
  /* avbryt */
  else if ((value & 0xf0) == 0xd0) {
    command = NO_COMMAND;
    if (status & 0x01) {
      status = status & 0xfe;
    } else {
      status = 0x20 | (params->realTrack == 0 ? 0x04 : 0x00);
    }
  }
}
/* skriv til sporregister */
void newTrack (byte value) {
  track = value;
}
/* skriv til sektorregister */
void newSector (byte value) {
  sector = value;
}
/* skriv til dataregister */
void newDiskData (byte value) {
  data = value;
  switch (command) {
    case WRITE_SECT:
      if (imageOffset >= 0 && imageOffset < params->diskImageSize)
        params->diskImage[imageOffset] = data;
      imageOffset++;
      if (imageOffset >= endOffset) {
        command = NO_COMMAND;
        status = 0x00;
      }
      break;
    case WRITE_TRACK:
      /* kun delvis implementert */
      if (++byteCount > 6200) {
        memset (&params->diskImage[((params->realTrack << params->sides) + side) * params->sectors *
                                  params->sectSize],
                0xe5, params->sectors * params->sectSize);
        command = NO_COMMAND;
        status = 0x00;
      }
      break;
  }
}
/* les statusregister */
byte diskStatus (void) {
  return status;
}
/* les sporregister */
byte getTrack (void) {
  return track;
}
/* les sektorregister */
byte getSector (void) {
  return sector;
}
/* les dataregister */
byte getDiskData (void) {
  switch (command) {
    case READ_SECT:
      data = (imageOffset >= 0 && imageOffset < params->diskImageSize)
           ? params->diskImage[imageOffset] : 0;
      imageOffset++;
      if (imageOffset >= endOffset) {
        command = NO_COMMAND;
        status = 0x00;
      }
      break;
    case READ_ADDR:
      switch (byteCount++) {
        case 0: data = params->realTrack; break;
        case 1: data = side; break;
        case 2: data = 1; break;
        case 3:
          /* Real TIKI-100 200KB disks used mixed density: tracks 0-1
           * were single-density (128-byte sectors) while tracks 2+
           * were double-density (512-byte sectors).  The BIOS format
           * detection compares sector sizes at tracks 0 and 2 to
           * identify 200KB disks.  Simulate this for single-sided
           * DD disk images which are stored as uniform 512-byte
           * sectors. */
          if (params->sides == 0 && params->sectSize == 512
              && params->realTrack <= 1) {
            data = 0;  /* 128-byte (single density boot track) */
          } else {
            switch (params->sectSize) {
              case 128:  data = 0; break;
              case 256:  data = 1; break;
              case 512:  data = 2; break;
              case 1024: data = 3; break;
            }
          }
        break;
        case 4: data = 0; break;
        case 5: data = 0;
          command = NO_COMMAND;
          status = 0x00;
          break;
      }
      break;
    case READ_TRACK:
      /* ikke implementert */
      break;
  }
  return data;
}
/* sett disk 0 aktiv / inaktiv */
void disk0 (tiki_bool status) {
  if (status) {
    params = &params0;
  } else {
    params = &params1;
  }
  setLights();
}
/* sett disk 1 aktiv / inaktiv */
void disk1 (tiki_bool status) {
  if (status) {
    params = &params1;
  } else {
    params = &params0;
  }
  setLights();
}
/* skru motor på / av */
void diskMotor (tiki_bool status) {
  motOn = status;
  setLights();
}
/* sett side fra systemregister (port 0x1C bit 4) */
void diskSetSide (tiki_bool s) {
  sideSelect = s ? 1 : 0;
}
/* sett alle disklys */
static void setLights (void) {
  int disk = params == &params0 ? 0 : 1;
  int otherDisk = disk == 1 ? 0 : 1;

  if (motOn) {
    setLight (disk, 1);
    setLight (otherDisk, 0);
  } else {
    setLight (0, 0);
    setLight (1, 0);
  }
}
/* sett disk lys til gitt stasjon og status */
static void setLight (int drive, tiki_bool status) {
  static int l[2] = {0, 0};

  if (l[drive] != status) {
    l[drive] = status;
    diskLight (drive, status);
  }
}
/* ny diskett i stasjon
 * disk:      Hvilken stasjon
 * diskImage: Peker til diskettbilde-data
 * resten er diskparametre
 */
void insertDisk (int drive, byte *dskImg, int trcks, int sds,
                 int sctrs, int sctSize) {
  struct diskParams *dp = drive ? &params1 : &params0;

  dp->active = TRUE;
  dp->diskImage = dskImg;
  dp->diskImageSize = (long)trcks * sds * sctrs * sctSize;
  dp->tracks = trcks;
  dp->sides = sds - 1;
  dp->sectors = sctrs;
  dp->sectSize = sctSize;
}
/* fjern diskett fra stasjon */
void removeDisk (int drive) {
  struct diskParams *dp = drive ? &params1 :&params0;

  dp->active = FALSE;
}

/* sleep save/restore — saves FDC controller state, not disk image data */
tiki_bool diskSleepSave (FILE *f) {
  /* save which drive is active (0 or 1) */
  int activeDrive = (params == &params1) ? 1 : 0;
  if (fwrite (&activeDrive, sizeof(activeDrive), 1, f) != 1) return FALSE;
  /* save diskParams metadata (not the image pointer) */
  if (fwrite (&params0.diskImageSize, sizeof(long), 1, f) != 1) return FALSE;
  if (fwrite (&params0.active, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&params0.tracks, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params0.sides, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params0.sectors, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params0.sectSize, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params0.realTrack, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params1.diskImageSize, sizeof(long), 1, f) != 1) return FALSE;
  if (fwrite (&params1.active, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&params1.tracks, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params1.sides, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params1.sectors, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params1.sectSize, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&params1.realTrack, sizeof(int), 1, f) != 1) return FALSE;
  /* save controller registers */
  if (fwrite (&side, sizeof(side), 1, f) != 1) return FALSE;
  if (fwrite (&sideSelect, sizeof(sideSelect), 1, f) != 1) return FALSE;
  if (fwrite (&motOn, sizeof(motOn), 1, f) != 1) return FALSE;
  if (fwrite (&track, sizeof(track), 1, f) != 1) return FALSE;
  if (fwrite (&sector, sizeof(sector), 1, f) != 1) return FALSE;
  if (fwrite (&data, sizeof(data), 1, f) != 1) return FALSE;
  if (fwrite (&status, sizeof(status), 1, f) != 1) return FALSE;
  if (fwrite (&stepDir, sizeof(stepDir), 1, f) != 1) return FALSE;
  if (fwrite (&command, sizeof(command), 1, f) != 1) return FALSE;
  if (fwrite (&imageOffset, sizeof(imageOffset), 1, f) != 1) return FALSE;
  if (fwrite (&endOffset, sizeof(endOffset), 1, f) != 1) return FALSE;
  if (fwrite (&byteCount, sizeof(byteCount), 1, f) != 1) return FALSE;
  return TRUE;
}

tiki_bool diskSleepRestore (FILE *f) {
  int activeDrive;
  if (fread (&activeDrive, sizeof(activeDrive), 1, f) != 1) return FALSE;
  if (fread (&params0.diskImageSize, sizeof(long), 1, f) != 1) return FALSE;
  if (fread (&params0.active, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&params0.tracks, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params0.sides, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params0.sectors, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params0.sectSize, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params0.realTrack, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params1.diskImageSize, sizeof(long), 1, f) != 1) return FALSE;
  if (fread (&params1.active, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&params1.tracks, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params1.sides, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params1.sectors, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params1.sectSize, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&params1.realTrack, sizeof(int), 1, f) != 1) return FALSE;
  params = activeDrive ? &params1 : &params0;
  if (fread (&side, sizeof(side), 1, f) != 1) return FALSE;
  if (fread (&sideSelect, sizeof(sideSelect), 1, f) != 1) return FALSE;
  if (fread (&motOn, sizeof(motOn), 1, f) != 1) return FALSE;
  if (fread (&track, sizeof(track), 1, f) != 1) return FALSE;
  if (fread (&sector, sizeof(sector), 1, f) != 1) return FALSE;
  if (fread (&data, sizeof(data), 1, f) != 1) return FALSE;
  if (fread (&status, sizeof(status), 1, f) != 1) return FALSE;
  if (fread (&stepDir, sizeof(stepDir), 1, f) != 1) return FALSE;
  if (fread (&command, sizeof(command), 1, f) != 1) return FALSE;
  if (fread (&imageOffset, sizeof(imageOffset), 1, f) != 1) return FALSE;
  if (fread (&endOffset, sizeof(endOffset), 1, f) != 1) return FALSE;
  if (fread (&byteCount, sizeof(byteCount), 1, f) != 1) return FALSE;
  return TRUE;
}
