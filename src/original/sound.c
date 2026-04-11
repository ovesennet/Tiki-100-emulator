/* sound.c V1.1.0
 *
 * AY-3-8912 emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"

/* variabler */

static byte reg = 0;

/*****************************************************************************/

/* skriv til kontrollregister */
void soundReg (byte value) {
  reg = value;
}
/* skriv til dataregister */
void soundData (byte value) {
  switch (reg) {
    case 14: newOffset (value); break;
  }
}
/* les dataregister */
byte getSoundData (void) {
  return 0xff;
}

