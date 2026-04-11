/* TIKI-100_emul.c V1.1.0
 *
 * Hovedmodul for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"

/* variabler */

Z80 cpu;
static boolean done = FALSE;

/*****************************************************************************/

/* starter emulering, returnerer når emulering avslutter */
boolean runEmul (void) {
#ifdef DEBUG
  cpu.Trap = 0xffff;
#endif
  cpu.IPeriod = 4000;
  if (initMem()) {
    ResetZ80 (&cpu);
    RunZ80 (&cpu);
    return TRUE;
  }
  return FALSE;
}
/* ikke i bruk */
void PatchZ80 (register Z80 *R) {
}
/* kalles regelmessig av z80-emulator */
word LoopZ80 (register Z80 *R) {
  static int guiCount = 20;
  if (done) return INT_QUIT;
  updateCTC (cpu.IPeriod);
  if (--guiCount == 0) {
    loopEmul (20);
    guiCount = 20;
  }
  return INT_NONE;
}
/* reset emulator */
void resetEmul (void) {
  OutZ80 (0x1c, 0x00);
  ResetZ80 (&cpu);
}
/* avslutt emulator */
void quitEmul (void) {
  done = TRUE;
}
#ifdef DEBUG
/* start z80-debugger */
void trace (void) {
  cpu.Trace = 1;
}
#endif
