/* mem.c V1.3.0
 *
 * Tar seg av minne-håndtering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"
#include "sleep.h"
#include <stdio.h>

#define ROM_FILENAME    "tiki.rom"

/* variabler */

byte ram[64 * 1024];    /* hoved minne */
extern byte gfxRam[];   /* grafikk minne */
byte rom[16 * 1024];    /* monitor eprom, vanligvis bare 8k men støtter 16k */

static tiki_bool gfxIn;   /* TRUE hvis grafikkram er synlig for prosessor */
static tiki_bool romIn;   /* TRUE hvis rom er synlig for prosessor */

/*****************************************************************************/

/* må kalles før emulering starter */
int initMem() {
  FILE *fp;

  /* hent ROM */
  if ((fp = fopen (ROM_FILENAME, "rb"))) {
    fread (rom, 1, 16 * 1024, fp);
    fclose (fp);
    OutZ80 (0x1c, 0x00);
    return TRUE;
  }
  return FALSE;
}
/* skriv til minnet */
void WrZ80 (register word addr, register byte value) {
  if (gfxIn && !(addr & 0x8000)) {
    if (gfxRam[addr] != value) {
      gfxRam[addr] = value;
      drawByte (addr);
    }
  }
  else if (romIn && !(addr & 0xc000)) {
    return;
  }
  else {
    ram[addr] = value;
  }
}
/* les fra minnet */
byte RdZ80 (register word addr) {
  if (gfxIn && !(addr & 0x8000)) {
    return gfxRam[addr];
  }
  if (romIn && !(addr & 0xc000)) {
    return rom[addr];
  }
  return ram[addr];
}
/* skriv til i/o-port */
void OutZ80 (register word port, register byte value) {
  static int lock = 0;
  static int gfx = 0;

  switch (port) {
    case 0x00:  /* tastatur */
    case 0x01:
    case 0x02:
    case 0x03:
      resetKeyboard();
      break;
    case 0x04:  /* serie A data */
      newSerAData (value);
      break;
    case 0x05:  /* serie B data */
      newSerBData (value);
      break;
    case 0x06:  /* serie A styreord */
      serAControl (value);
      break;
    case 0x07:  /* serie B styreord */
      serBControl (value);
      break;
    case 0x08:  /* parallell A data */
      newParAData (value);
      break;
    case 0x09:  /* parallell B data */
      newParBData (value);
      break;
    case 0x0a:  /* parallell A styreord */
      parAControl (value);
      break;
    case 0x0b:  /* parallell B styreord */
      parBControl (value);
      break;
    case 0x0c:  /* grafikk-modus */
    case 0x0d:
    case 0x0e:
    case 0x0f:
      newMode (value);
      break;
    case 0x10:  /* disk styreord */
      diskControl (value);
      break;
    case 0x11:  /* disk spornummer */
      newTrack (value);
      break;
    case 0x12:  /* disk sektorregister */
      newSector (value);
      break;
    case 0x13:  /* disk dataregister */
      newDiskData (value);
      break;
    case 0x14:  /* farge-register */
    case 0x15:
      newColor (value);
      break;
    case 0x16:  /* lyd-scroll peker */
      soundFlush ();         /* generate audio up to this exact cycle */
      soundReg (value);
      break;
    case 0x17:  /* lyd-scroll data */
      soundFlush ();         /* generate audio up to this exact cycle */
      soundData (value);
      break;
    case 0x18:  /* ctc kanal 0 */
      writeCtc0 (value);
      break;
    case 0x19:  /* ctc kanal 1 */
      writeCtc1 (value);
      break;
    case 0x1a:  /* ctc kanal 2 */
      writeCtc2 (value);
      break;
    case 0x1b:  /* ctc kanal 3 */
      writeCtc3 (value);
      break;
    case 0x20:  /* hdd data */
      hddWriteData (value);
      break;
    case 0x21:  /* hdd precomp */
      hddWritePrecomp (value);
      break;
    case 0x22:  /* hdd sektortelling */
      hddWriteSectorCount (value);
      break;
    case 0x23:  /* hdd sektornummer */
      hddWriteSector (value);
      break;
    case 0x24:  /* hdd spor lav */
      hddWriteTrackLo (value);
      break;
    case 0x25:  /* hdd spor høy */
      hddWriteTrackHi (value);
      break;
    case 0x26:  /* hdd SDH */
      hddWriteSDH (value);
      break;
    case 0x27:  /* hdd kommando */
      hddWriteCommand (value);
      break;
    case 0x1c:  /* system-register */
    case 0x1d:
    case 0x1e:
    case 0x1f:
      disk0 (value & 1);
      disk1 (value & 2);
      romIn = !(value & 4);
      gfxIn = value & 8;
      diskSetSide (value & 0x10);
      if (gfx != !(value & 32)) {
        grafikkLight (!(value & 32));
        gfx = !(value & 32);
      }
      diskMotor (value & 64);
      if (lock != !(value & 128)) {
        lockLight (!(value & 128));
        lock = !(value & 128);
      }
      break;
  }
}
/* les fra i/o-port */
byte InZ80 (register word port) {
  switch (port) {
    case 0x00:  /* tastatur */
    case 0x01:
    case 0x02:
    case 0x03:
      return readKeyboard();
    case 0x04:  /* serie A data */
      return serAData();
    case 0x05:  /* serie B data */
      return serBData();
    case 0x06:  /* serie A status */
      return serAStatus();
    case 0x07:  /* serie B status */
      return serBStatus();
    case 0x08:  /* parallell A data */
      return parAData();
    case 0x09:  /* parallell B data */
      return parBData();
    case 0x0a:  /* parallell A status */
      return parAStatus();
    case 0x0b:  /* parallell B status */
      return parBStatus();
    case 0x10:  /* disk status */
      return diskStatus();
    case 0x11:  /* disk spornummer */
      return getTrack();
    case 0x12:  /* disk sektorregister */
      return getSector();
    case 0x13:  /* disk dataregister */
      return getDiskData();
    case 0x17:  /* lyd-scroll data */
      return getSoundData();
    case 0x18:  /* ctc kanal 0 */
      return readCtc0();
    case 0x19:  /* ctc kanal 1 */
      return readCtc1();
    case 0x1a:  /* ctc kanal 2 */
      return readCtc2();
    case 0x1b:  /* ctc kanal 3 */
      return readCtc3();
    case 0x20:  /* hdd data */
      return hddReadData();
    case 0x21:  /* hdd feilregister */
      return hddReadError();
    case 0x22:  /* hdd sektortelling */
      return hddReadSectorCount();
    case 0x23:  /* hdd sektornummer */
      return hddReadSector();
    case 0x24:  /* hdd spor lav */
      return hddReadTrackLo();
    case 0x27:  /* hdd status */
      return hddReadStatus();
    default:
      return 0xff;
  }
}

/* sleep save/restore */
tiki_bool memSleepSave (FILE *f) {
  if (fwrite (ram, 1, sizeof(ram), f) != sizeof(ram)) return FALSE;
  if (fwrite (&gfxIn, sizeof(gfxIn), 1, f) != 1) return FALSE;
  if (fwrite (&romIn, sizeof(romIn), 1, f) != 1) return FALSE;
  return TRUE;
}

tiki_bool memSleepRestore (FILE *f) {
  if (fread (ram, 1, sizeof(ram), f) != sizeof(ram)) return FALSE;
  if (fread (&gfxIn, sizeof(gfxIn), 1, f) != 1) return FALSE;
  if (fread (&romIn, sizeof(romIn), 1, f) != 1) return FALSE;
  return TRUE;
}
