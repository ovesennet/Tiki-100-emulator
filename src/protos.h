/* protos.h V1.1.0
 *
 * Prototyper for funksjoner som kun skal brukes av den systemuavhengige koden. 
 * Resten av prototypene er i TIKI-100_emul.h
 * Copyright (C) Asbj�rn Djupdal 2000-2001
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
/* m� kalles regelmessig for � oppdatere tellere og sjekke om interrupt skal genereres */
void updateCTC (int cycles);
/* returnerer antall sykler mellom hver ut puls p� gitt ctc-kanal */
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
void disk0 (tiki_bool status);
void disk1 (tiki_bool status);
/* skru motor p� / av */
void diskMotor (tiki_bool status);
/* sett side fra systemregister (port 0x1C bit 4) */
void diskSetSide (tiki_bool s);

/****************************************************************************/
/* keyboard.c                                                                */
/*****************************************************************************/

/* skriv til tastatur-register */
void resetKeyboard (void);
/* les tastatur-register */
byte readKeyboard (void);

/*****************************************************************************/
/* mem.c                                                                     */
/*****************************************************************************/

/* m� kalles f�r emulering starter */
int initMem();

/*****************************************************************************/
/* sound.c                                                                   */
/*****************************************************************************/

/* initialiser / rydd opp lyd-maskinvare */
void soundInit (void);
void soundCleanup (void);
/* skriv til lyd-register */
void soundReg (byte value);
void soundData (byte value);
/* les lyd-register */
byte getSoundData (void);
/* generer lydsampler */
void updateSound (int cpuCycles);

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

/*****************************************************************************/
/* hdd.c                                                                     */
/*****************************************************************************/

/* må kalles før emulering starter */
void hddInit (void);
/* reset av kontroller (kalles fra resetEmul) */
void hddReset (void);

/* port 0x20..0x23 — dataregister-blokk */
void hddWriteData        (byte value);
byte hddReadData         (void);
void hddWritePrecomp     (byte value);
byte hddReadError        (void);
void hddWriteSectorCount (byte value);
byte hddReadSectorCount  (void);
void hddWriteSector      (byte value);
byte hddReadSector       (void);

/* port 0x24..0x27 — kontrollregister-blokk */
void hddWriteTrackLo (byte value);
byte hddReadTrackLo  (void);
void hddWriteTrackHi (byte value);
void hddWriteSDH     (byte value);
void hddWriteCommand (byte value);
byte hddReadStatus   (void);

#endif
