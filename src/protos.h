/* protos.h V1.1.0
 *
 * Prototyper for funksjoner som kun skal brukes av den systemuavhengige koden. 
 * Resten av prototypene er i TIKI-100_emul.h
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#ifndef PROTOS_H
#define PROTOS_H

/*****************************************************************************/
/* ctc.c                                                                     */
/*****************************************************************************/

/* skriv til ctc-kontrollregister */
void writeCtc0 (byte value);
void writeCtc1 (byte value);
void writeCtc2 (byte value);
void writeCtc3 (byte value);
/* les ctc-register */
byte readCtc0 (void);
byte readCtc1 (void);
byte readCtc2 (void);
byte readCtc3 (void);
/* må kalles regelmessig for å oppdatere tellere og sjekke om interrupt skal genereres */
void updateCTC (int cycles);
/* returnerer antall sykler mellom hver ut puls på gitt ctc-kanal */
int getCtc (int c);

/*****************************************************************************/
/* disk.c                                                                    */
/*****************************************************************************/

/* skriv til disk-register */
void diskControl (byte value);
void newTrack (byte value);
void newSector (byte value);
void newDiskData (byte value);
/* les disk-register */
byte diskStatus (void);
byte getTrack (void);
byte getSector (void);
byte getDiskData (void);
/* sett stasjon aktiv / inaktiv */
void disk0 (boolean status);
void disk1 (boolean status);
/* skru motor på / av */
void diskMotor (boolean status);

/*****************************************************************************/
/* keyboard.c                                                                */
/*****************************************************************************/

/* skriv til tastatur-register */
void resetKeyboard (void);
/* les tastatur-register */
byte readKeyboard (void);

/*****************************************************************************/
/* mem.c                                                                     */
/*****************************************************************************/

/* må kalles før emulering starter */
int initMem();

/*****************************************************************************/
/* sound.c                                                                   */
/*****************************************************************************/

/* skriv til lyd-register */
void soundReg (byte value);
void soundData (byte value);
/* les lyd-register */
byte getSoundData (void);

/*****************************************************************************/
/* video.c                                                                   */
/*****************************************************************************/

/* tegn gitt byte i grafikkminne til skjerm */
void drawByte (word addr);
/* skriv til grafikk-sregister */
void newMode (byte mode);
void newColor (byte color);
/* ny offset (dvs scroll) */
void newOffset (byte newOffset);

/*****************************************************************************/
/* serial.c                                                                  */
/*****************************************************************************/

/* skriv til data-register */
void newSerAData (byte value);
void newSerBData (byte value);
/* skriv til kontroll-register */
void serAControl (byte value);
void serBControl (byte value);
/* les data-register */
byte serAData (void);
byte serBData (void);
/* les status-register */
byte serAStatus (void);
byte serBStatus (void);
/* rekalkuler buad-rate (nye ctc-verdier) */
void recalcBaud (void);

/*****************************************************************************/
/* parallel.c                                                                */
/*****************************************************************************/

/* skriv til data-register */
void newParAData (byte value);
void newParBData (byte value);
/* skriv til kontroll-register */
void parAControl (byte value);
void parBControl (byte value);
/* les data-register */
byte parAData (void);
byte parBData (void);
/* les status-register */
byte parAStatus (void);
byte parBStatus (void);

#endif
