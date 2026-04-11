/* TIKI-100_emul.h V1.1.0
 *
 * Definisjoner og konstanter nyttige for alle TIKI-100_emul moduler
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#ifndef TIKI_EMUL_H
#define TIKI_EMUL_H

#include "Z80.h"

/* byte og word definert i Z80.h */
typedef short boolean;

#define TRUE  ~0
#define FALSE  0

/* video */

#define HIGHRES   16    /* 1024 * 256 * 2  */
#define MEDRES    32    /*  512 * 256 * 4  */
#define LOWRES    48    /*  256 * 256 * 16 */

/* serie */

enum parity {
  PAR_NONE, PAR_EVEN, PAR_ODD
};
enum stopBits {
  ONE_SB, ONE_PT_FIVE_SB, TWO_SB
};
struct serParams {
  int receiveBits;  /* antall bits i innkommende tegn */
  int sendBits;     /* antall bits i tegn som sendes */
  int parity;       /* paritet, en av de angitt over */
  int stopBits;     /* antall stopbits, en av de angitt over */
  int baud;         /* baud */
  /* følgende er kun for bruk i serial.c: */
  int regPtr;
  int clkDiv;
  boolean exi;
  boolean txi;
  boolean sav;
  int rxi;
  boolean rxe;
  boolean ae;
  boolean txe;
  boolean rxa;
  boolean newChar;
};

/* tastatur - ikke-alfanumeriske taster */

#define KEY_NONE        0x80
#define KEY_CTRL        0x81
#define KEY_SHIFT       0x82
#define KEY_BRYT        0x03
#define KEY_CR          0x0d
#define KEY_SPACE       0x20
#define KEY_SLETT       0x7f
#define KEY_GRAFIKK     0x84
#define KEY_ANGRE       0x1a
#define KEY_LOCK        0x83
#define KEY_HJELP       0x0a
#define KEY_LEFT        0x08
#define KEY_UTVID       0x05
#define KEY_F1          0x01
#define KEY_F4          0x07
#define KEY_RIGHT       0x0c
#define KEY_F2          0x02
#define KEY_F3          0x06
#define KEY_F5          0x0e
#define KEY_F6          0x0f
#define KEY_DOWN        0x1c
#define KEY_PGUP        0x17
#define KEY_PGDOWN      0x1f
#define KEY_UP          0x0b
#define KEY_HOME        0x09
#define KEY_TABLEFT     0x1d
#define KEY_TABRIGHT    0x18
#define KEY_NUMDIV      0x80 | 0x2f
#define KEY_NUMPLUS     0x80 | 0x2b
#define KEY_NUMMINUS    0x80 | 0x2d
#define KEY_NUMMULT     0x80 | 0x2a
#define KEY_NUMPERCENT  0x80 | 0x25
#define KEY_NUMEQU      0x80 | 0x3d
#define KEY_ENTER       0x80 | 0x0d
#define KEY_NUM0        0x80 | 0x30
#define KEY_NUM1        0x80 | 0x31
#define KEY_NUM2        0x80 | 0x32
#define KEY_NUM3        0x80 | 0x33
#define KEY_NUM4        0x80 | 0x34
#define KEY_NUM5        0x80 | 0x35
#define KEY_NUM6        0x80 | 0x36
#define KEY_NUM7        0x80 | 0x37
#define KEY_NUM8        0x80 | 0x38
#define KEY_NUM9        0x80 | 0x39
#define KEY_NUMDOT      0x80 | 0x2e

/* Må implementeres av system-koden
 **********************************/

/* Forandre oppløsning.
 * Må samtidig fylle alle pixler med farge 0
 */
void changeRes (int newRes);

/* Plott en pixel med farge tatt fra pallett */
void plotPixel (int x, int y, int color);

/* Scroll skjerm 'distance' linjer oppover
 * 'distance' kan være både positiv og negativ
 */
void scrollScreen (int distance);

/* Ny farge, gitt pallettnummer og intensitet 0-255.
 * Må oppdatere alle pixler med dette pallettnummeret til ny farge
 */
void changePalette (int colornumber, byte red, byte green, byte blue);

/* Kalles periodisk. Lar system kode måle / senke emuleringshastighet
 * Kan også brukes til sjekk av brukeraktivitet / serieporter
 * ms er antall "emulerte" millisekunder siden forrige gang loopEmul ble kalt
 */
void loopEmul (int ms);

/* Tenn/slukk lock lys */
void lockLight (boolean status);

/* Tenn/slukk grafikk lys */
void grafikkLight (boolean status);

/* Tenn/slukk disk lys for gitt stasjon */
void diskLight (int drive, boolean status);

/* Sjekk status til hver av de gitte tastene
 * Sett bit n i returkode hvis tast n IKKE er nedtrykt
 * Taster er enten ascii-kode eller en av konstantene over
 */
byte testKey (byte keys[8]);

/* Setter seriekanalparametre */
void setParams (struct serParams *p1Params, struct serParams *p2Params);

/* Send tegn til seriekanal
 * port = 0: port 1
 * port = 1: port 2
 */
void sendChar (int port, byte value);

/* Hent tegn fra seriekanal
 * port = 0: port 1
 * port = 1: port 2
 */
byte getChar (int port);

/* Send tegn til skriver (port 3) */
void printChar (byte value);

/* Kan kalles av system-koden
 ****************************/

/* Starter emulering, returnerer når emulering avsluttes
 * Returverdi ved feil: FALSE
 * Ellers returneres TRUE (etter at quitEmul() er kalt)
 */
boolean runEmul (void);

/* Ny diskett i stasjon
 * disk:      Hvilken stasjon (0 eller 1)
 * diskImage: Peker til diskettbilde-data
 * Resten er diskparametre
 */
void insertDisk (int drive, byte *diskImage, int tracks,
                 int sides, int sectors, int sectSize);

/* Fjern diskett fra stasjon (0 eller 1) */
void removeDisk (int drive);

/* Resetter TIKI-100 */
void resetEmul (void);

/* Avslutter emulator */
void quitEmul (void);

#ifdef DEBUG
/* Åpner avlusnings monitor */
void trace (void);
#endif

/* Setter bøyle ST 28 b */
void setST28b (boolean status);

/* Nytt serietegn mottatt
 * port = 0: port 1
 * port = 1: port 2
 */
void charAvailable (int port);

#endif
