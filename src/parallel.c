/* parallel.c V1.1.0
 *
 * Z80 PIO emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"

/*****************************************************************************/

/* skriv til dataregister A */
void newParAData (byte value) {
  printChar (value);
}
/* skriv til dataregister B */
void newParBData (byte value) {
}
/* skriv til kontrollregister A */
void parAControl (byte value) {
}
/* skriv til kontrollregister B */
void parBControl (byte value) {
}
/* les dataregister A */
byte parAData (void) {
  return 0;
}
/* les dataregister B */
byte parBData (void) {
  return 0x10; /* Ack på, busy og no-paper av */
}
/* les statusregister A */
byte parAStatus (void) {
  return 0;
}
/* les statusregister B */
byte parBStatus (void) {
  return 0;
}
