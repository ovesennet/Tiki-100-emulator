/* ctc.c V1.3.0
 *
 * Z80 CTC emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"
#include "sleep.h"

/* data for en seriekanal */
struct ctcParams {
  tiki_bool interrupt;   /* om interrupt skal genereres */
  tiki_bool enable;      /* om teller av på eller av */
  int in;              /* inngang - ant. sykler mellom hver inn-puls */
  int out;             /* utgang - ant. sykler mellom hver ut-puls */
  tiki_bool use4MHz;     /* om klokke skal brukes i stedet for inngang */
  int count;           /* sykler igjen til neste ut-puls */
  tiki_bool writeTk;     /* skriv tidskonstant neste gang */
  byte timeConst;      /* tidskonstant */
  int scale;           /* skaleringsfaktor */
};

/* protos */

static void writeCtc (byte value, struct ctcParams *params);
static void recalcCtc (void);

/* variabler */

extern Z80 cpu;
static struct ctcParams params0 = {FALSE, FALSE, 2, FALSE, 0, FALSE, 0, 0};
static struct ctcParams params1 = {FALSE, FALSE, 2, FALSE, 0, FALSE, 0, 0};
static struct ctcParams params2 = {FALSE, FALSE, 1, FALSE, 0, FALSE, 0, 0};
static struct ctcParams params3 = {FALSE, FALSE, 1, FALSE, 0, FALSE, 0, 0};

/*****************************************************************************/

/* skriv til ctc0-register */
void writeCtc0 (byte value) {
  writeCtc (value, &params0);
}
/* skriv til ctc1-register */
void writeCtc1 (byte value) {
  writeCtc (value, &params1);
}
/* skriv til ctc2-register */
void writeCtc2 (byte value) {
  writeCtc (value, &params2);
}
/* skriv til ctc3-register */
void writeCtc3 (byte value) {
  writeCtc (value, &params3);
}
/* setter data til gitt seriekanal */
static void writeCtc (byte value, struct ctcParams *params) {
  if (params->writeTk) {
    params->writeTk = FALSE;
    params->timeConst = value;
    params->enable = TRUE;
  } else {
    params->interrupt = value & 128;
    if (value & 64) {
      params->use4MHz = FALSE;
      params->scale = 1;
    } else {
      params->use4MHz = TRUE;
      params->scale = value & 32 ? 256 : 16;
    }
    params->writeTk = value & 4;
    params->enable = !(value & 2);
  }
  recalcCtc();
}
/* rekalkulerer frekvenser for tellerkanalene */
static void recalcCtc (void) {
  params2.in = params0.out;
  params3.in = params2.out;

  params0.out = params0.timeConst * params0.scale * (params0.use4MHz ? 1 : params0.in);
  params1.out = params1.timeConst * params1.scale * (params1.use4MHz ? 1 : params1.in);
  params2.out = params2.timeConst * params2.scale * (params2.use4MHz ? 1 : params2.in);
  params3.out = params3.timeConst * params3.scale * (params3.use4MHz ? 1 : params3.in);

  params0.count = params0.out;   /* dette er ikke helt riktig, men bra nok */
  params1.count = params1.out;
  params2.count = params2.out;
  params3.count = params3.out;

  recalcBaud();
}
/* les ctc0-register */
byte readCtc0 (void) {
  return params0.in ? params0.count / params0.in : 0;
}
/* les ctc1-register */
byte readCtc1 (void) {
  return params1.in ? params1.count / params1.in : 0;
}
/* les ctc2-register */
byte readCtc2 (void) {
  return params2.in ? params2.count / params2.in : 0;
}
/* les ctc3-register */
byte readCtc3 (void) {
  return params3.in ? params3.count / params3.in : 0;
}
/* må kalles regelmessig for å oppdatere tellere og sjekke om interrupt skal genereres */
void updateCTC (int cycles) {
  if (params0.enable) params0.count -= cycles;
  if (params1.enable) params1.count -= cycles;
  if (params2.enable) params2.count -= cycles;
  if (params3.enable) params3.count -= cycles;

  if (params0.count <= 0) {
    params0.count += params0.out;
    if (params0.interrupt) {
      IntZ80 (&cpu, 0x10);
    }
  }
  if (params1.count <= 0) {
    params1.count += params1.out;
    if (params1.interrupt) {
      IntZ80 (&cpu, 0x12);
    }
  }
  if (params2.count <= 0) {
    params2.count += params2.out;
    if (params2.interrupt) {
      IntZ80 (&cpu, 0x14);
    }
  }
  if (params3.count <= 0) {
    params3.count += params3.out;
    if (params3.interrupt) {
      IntZ80 (&cpu, 0x16);
    }
  }
}
/* returnerer antall sykler mellom hver ut puls på gitt ctc-kanal */
int getCtc (int c) {
  switch (c) {
    case 0:  return params0.out;
    case 1:  return params1.out;
    case 2:  return params2.out;
    case 3:  return params3.out;
  }
  return 0;
}

/* sleep save/restore */
tiki_bool ctcSleepSave (FILE *f) {
  if (fwrite (&params0, sizeof(params0), 1, f) != 1) return FALSE;
  if (fwrite (&params1, sizeof(params1), 1, f) != 1) return FALSE;
  if (fwrite (&params2, sizeof(params2), 1, f) != 1) return FALSE;
  if (fwrite (&params3, sizeof(params3), 1, f) != 1) return FALSE;
  return TRUE;
}

tiki_bool ctcSleepRestore (FILE *f) {
  if (fread (&params0, sizeof(params0), 1, f) != 1) return FALSE;
  if (fread (&params1, sizeof(params1), 1, f) != 1) return FALSE;
  if (fread (&params2, sizeof(params2), 1, f) != 1) return FALSE;
  if (fread (&params3, sizeof(params3), 1, f) != 1) return FALSE;
  return TRUE;
}

