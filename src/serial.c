/* serial.c V1.1.0
 *
 * Z80 DART emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"

enum rxiType {
  RXI_NONE, RXI_FIRST, RXI_ALL_PAR_CHANGE_VEC, RXI_ALL_PAR_DONT_CHANGE_VEC
};

/* protos */

static void serControl (byte value, struct serParams *sParams);
static byte serStatus (struct serParams *sParams);

/* variabler */

extern Z80 cpu;   /* må ha tilgang til cpu for å gjøre interrupt */

static boolean st28b = FALSE;    /* jumper i TIKI-100 */

/* parametre for de to seriekanalene */
static struct serParams serAParams;
static struct serParams serBParams;

static byte intVector = 0;     /* interrupt vektor, delt mellom kanalene */

/*****************************************************************************/

/* skriv til dataregister A */
void newSerAData (byte value) {
  if (serAParams.txe) {
    sendChar (1, value);
  }
}
/* skriv til dataregister B */
void newSerBData (byte value) {
  if (serBParams.txe) {
    sendChar (0, value);
  }
}
/* skriv til kontrollregister A */
void serAControl (byte value) {
  serControl (value, &serAParams);
}
/* skriv til kontrollregister B */
void serBControl (byte value) {
  serControl (value, &serBParams);
}
static void serControl (byte value, struct serParams *sParams) {
  switch (sParams->regPtr) {
    case 0:  /* Command Register */
      sParams->regPtr = value & 7;
      /* nullstill status interrupt */
      /* nullstill hele kanalen */
      if ((value & 0x38) == 0x18) {
        sParams->newChar = FALSE;
      }
      /* Sett interrupt når neste tegn mottas */
      if ((value & 0x38) == 0x20) {
        sParams->newChar = FALSE;
      }
      /* nullstill senderinterrupt */
      /* nullstill feilmelding */
      /* retur fra interrupt */
      break;
    case 1:  /* Transmit/Receive Interrupt */
      sParams->regPtr = 0;
      sParams->exi = value & 0x01;
      sParams->txi = value & 0x02;
      sParams->sav = value & 0x04;
      switch (value & 0x18) {
        case 0x00: sParams->rxi = RXI_NONE; break;
        case 0x08: sParams->rxi = RXI_FIRST; break;
        case 0x10: sParams->rxi = RXI_ALL_PAR_CHANGE_VEC; break;
        case 0x18: sParams->rxi = RXI_ALL_PAR_DONT_CHANGE_VEC; break;
      }
      break;
    case 2:  /* Interrupt Vector */
      sParams->regPtr = 0;
      intVector = value;
      break;
    case 3:  /* Receive Parameters */
      sParams->regPtr = 0;
      sParams->rxe = value & 0x01;
      sParams->ae = value & 0x20;
      switch (value & 0xc0) {
        case 0x00: sParams->receiveBits = 5; break;
        case 0x40: sParams->receiveBits = 7; break;
        case 0x80: sParams->receiveBits = 6; break;
        case 0xc0: sParams->receiveBits = 8; break;
      }
      break;
    case 4:  /* Transmit/Receive Misc Parameters */
      sParams->regPtr = 0;
      if (!(value & 0x01)) {
        sParams->parity = PAR_NONE;
      } else {
        sParams->parity = value & 0x02 ? PAR_EVEN : PAR_ODD;
      }
      switch (value & 0x0c) {
        case 0x00: /* ugyldig */
        case 0x04: sParams->stopBits = ONE_SB; break;
        case 0x08: sParams->stopBits = ONE_PT_FIVE_SB; break;
        case 0x0c: sParams->stopBits = TWO_SB; break;
      }
      switch (value & 0xc0) {
        case 0x00: sParams->clkDiv = 1; break;
        case 0x40: sParams->clkDiv = 16; break;
        case 0x80: sParams->clkDiv = 32; break;
        case 0xc0: sParams->clkDiv = 64; break;
      }
      break;
    case 5:  /* Transmit Parameters */
      sParams->regPtr = 0;
      /* rts */
      sParams->txe = value & 0x08;
      /* break */
      switch (value & 0xc0) {
        case 0x00: sParams->sendBits = 5; break;
        case 0x40: sParams->sendBits = 7; break;
        case 0x80: sParams->sendBits = 6; break;
        case 0xc0: sParams->sendBits = 8; break;
      }
      break;
  }
  recalcBaud();
}
/* rekalkuler baud-rate (nye ctc-verdier) */
void recalcBaud (void) {
  int ctc0 = getCtc (0);
  int ctc1 = getCtc (1);
  int ctc2 = getCtc (2);

  if ((ctc0 != 0) && ((st28b ? ctc2 : ctc1) != 0) && (serAParams.clkDiv != 0) && (serBParams.clkDiv != 0)) {
    serAParams.baud = 4000000/ctc0/serAParams.clkDiv;
    serBParams.baud = 4000000/(st28b ? ctc2 : ctc1)/serBParams.clkDiv;
    setParams (&serBParams, &serAParams);
  }
}
/* les dataregister A */
byte serAData (void) {
  if (serAParams.rxa) {
    serAParams.rxa = FALSE;
    return getChar (1);
  }
  return 0;
}
/* les dataregister B */
byte serBData (void) {
  if (serBParams.rxa) {
    serBParams.rxa = FALSE;
    return getChar (0);
  }
  return 0;
}
/* les statusregister A */
byte serAStatus (void) {
  return serStatus (&serAParams);
}
/* les statusregister B */
byte serBStatus (void) {
  return serStatus (&serBParams);
}
static byte serStatus (struct serParams *sParams) {
  byte returnValue = 0;

  switch (sParams->regPtr) {
    case 0:  returnValue = 0x34 | (sParams->rxa ? 1 : 0);
      break;
    case 1:  returnValue = 0x01;
      break;
    case 2:  returnValue = intVector;
      break;
  }
  sParams->regPtr = 0;
  return returnValue;
}
/* sett jumper ST28b */
void setST28b (boolean status) {
  st28b = status;
  recalcBaud();
}
/* nytt tegn tilgjengelig */
void charAvailable (int port) {
  struct serParams *sParams = port ? &serAParams : &serBParams;
  int channel = port ? 0 : 1;
  
  sParams->rxa = TRUE;
  sParams->newChar = TRUE;
  
  if (sParams->sav) {
    intVector = (intVector & 0xf1) | (channel ? 0 : 8) | 4;
  }
  switch (sParams->rxi) {
    case RXI_FIRST: if (!(sParams->newChar)) IntZ80 (&cpu, intVector);
      break;
    case RXI_ALL_PAR_CHANGE_VEC:
    case RXI_ALL_PAR_DONT_CHANGE_VEC:
      IntZ80 (&cpu, intVector);
      break;
  }
}
