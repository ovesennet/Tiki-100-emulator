/* video.c V1.1.0
 *
 * Grafikk emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"
#include <string.h>

/* variabler */

/* konverteringstabeller */
static const byte redGreenTable[8] = {0, 36, 73, 109, 146, 182, 219, 255};
static const byte blueTable[4] = {0, 85, 170, 255};

byte gfxRam[32 * 1024];

static byte red = 0, green = 0, blue = 0;     /* farge-register */
static int res = MEDRES;    /* gjeldende oppløsning */
static tiki_bool changeColor = FALSE;  /* forandre farge hver gang fargeregister forandres */
static int colornumber = 0;

/*****************************************************************************/

/* Tegn alle pixler representert i gitt byte */
void drawByte (word addr) {
  int x;
  byte y = addr >> 7;

  switch (res) {
    case HIGHRES:  x = (addr & 0x7f) << 3;
      plotPixel (x, y, gfxRam[addr] & 0x01);
      plotPixel (++x, y, (gfxRam[addr] & 0x02) >> 1);
      plotPixel (++x, y, (gfxRam[addr] & 0x04) >> 2);
      plotPixel (++x, y, (gfxRam[addr] & 0x08) >> 3);
      plotPixel (++x, y, (gfxRam[addr] & 0x10) >> 4);
      plotPixel (++x, y, (gfxRam[addr] & 0x20) >> 5);
      plotPixel (++x, y, (gfxRam[addr] & 0x40) >> 6);
      plotPixel (++x, y, (gfxRam[addr] & 0x80) >> 7);
      break;
    case MEDRES:   x = (addr & 0x7f) << 2;
      plotPixel (x, y, gfxRam[addr] & 0x03);
      plotPixel (++x, y, (gfxRam[addr] & 0x0c) >> 2);
      plotPixel (++x, y, (gfxRam[addr] & 0x30) >> 4);
      plotPixel (++x, y, (gfxRam[addr] & 0xc0) >> 6);
      break;
    case LOWRES:   x = (addr & 0x7f) << 1;
      plotPixel (x, y, gfxRam[addr] & 0x0f);
      plotPixel (++x, y, (gfxRam[addr] & 0xf0) >> 4);
      break;
  }
}
/* Ny verdi til modusregister */
void newMode (byte mode) {
  colornumber = mode & 0x0f;
  if (mode & 128) {    /* sett farge */
    changeColor = TRUE;
    changePalette (colornumber, red, green, blue);
  } else {
    changeColor = FALSE;
  }
  if (res != (mode & 48)) {
    res = (mode & 48);  /* ny oppløsning */
    changeRes (res);
    {  /* oppdater skjerm */
      int i;

      for (i = 0; i < 32768; i++) {
        if (gfxRam[i]) drawByte (i);
      }
    }
  }
}
/* Ny verdi i fargeregister */
void newColor (byte color) {
  red = redGreenTable[(~color & 224) >> 5];
  green = redGreenTable[(~color & 28) >> 2];
  blue = blueTable[~color & 3];
  if (changeColor) {
    changePalette (colornumber, red, green, blue);
  }
}
/* Ny verdi i scrollregister fra AY-3-8912 */
void newOffset (byte newOffset) {
  static byte vOffset = 0;   /* scrollregister */

  /* ignorer hvis ikke noen forskjell */
  if (newOffset != vOffset) {
    int distance = (signed char)(newOffset - vOffset);
    byte buffer[128 * 128]; /* skroller aldri mer enn halve skjermen */
    word addr;

    /* flytt på skjerm */
    scrollScreen (distance);

    if (distance > 0) {
      /* flytt i ram */
      memmove (buffer, gfxRam, distance * 128); /* øverst -> buffer */
      memmove (gfxRam, gfxRam + distance * 128, (256 - distance) * 128); /* nederst -> øverst */
      memmove (gfxRam + (256 - distance) * 128, buffer, distance * 128); /* buffer -> nederst */

      /* oppdater nederste del av skjerm */
      for (addr = 32768 - (distance * 128); addr < 32768; addr++) {
        if (gfxRam[addr]) drawByte (addr);
      }
    } else {
      /* flytt i ram */
      memmove (buffer, gfxRam + (byte)distance * 128, -distance * 128); /* nederst -> buffer */
      memmove (gfxRam + (-distance * 128), gfxRam, (byte)distance * 128); /* øverst -> nederst */
      memmove (gfxRam, buffer, -distance * 128); /* buffer -> øverst */

      /* oppdater øverste del av skjerm */
      for (addr = 0; addr < -distance * 128; addr++) {
        if (gfxRam[addr]) drawByte (addr);
      }
    }
    
    /* oppdater scrollregister */
    vOffset = newOffset;
  }
}

