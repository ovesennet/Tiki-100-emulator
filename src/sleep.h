/* sleep.h
 *
 * Save/restore complete emulator state ("sleep" feature)
 */

#ifndef SLEEP_H
#define SLEEP_H

#include "TIKI-100_emul.h"
#include <stdio.h>

/* Each module implements save/restore of its own static state.
 * Returns TRUE on success, FALSE on error. */

/* mem.c */
tiki_bool memSleepSave (FILE *f);
tiki_bool memSleepRestore (FILE *f);

/* video.c */
tiki_bool videoSleepSave (FILE *f);
tiki_bool videoSleepRestore (FILE *f);
void videoRedrawAll (void);  /* force full screen repaint after wake */

/* ctc.c */
tiki_bool ctcSleepSave (FILE *f);
tiki_bool ctcSleepRestore (FILE *f);

/* disk.c */
tiki_bool diskSleepSave (FILE *f);
tiki_bool diskSleepRestore (FILE *f);

/* sound.c */
tiki_bool soundSleepSave (FILE *f);
tiki_bool soundSleepRestore (FILE *f);

/* serial.c */
tiki_bool serialSleepSave (FILE *f);
tiki_bool serialSleepRestore (FILE *f);

/* keyboard.c */
tiki_bool kbdSleepSave (FILE *f);
tiki_bool kbdSleepRestore (FILE *f);

/* hdd.c */
tiki_bool hddSleepSave (FILE *f);
tiki_bool hddSleepRestore (FILE *f);

#endif
