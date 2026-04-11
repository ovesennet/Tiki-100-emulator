/* sound.c V1.2.0
 *
 * AY-3-8912 emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 * Audio output added 2026 Arctic Retro
 */

#include "TIKI-100_emul.h"
#include "protos.h"

#include <windows.h>
#include <mmsystem.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define CPU_CLOCK      4000000
#define AY_CLOCK       2000000          /* CPU clock / 2 */
#define AY_DIV         16               /* internal prescaler */
#define AY_BASE_RATE   (AY_CLOCK / AY_DIV)  /* 125 000 Hz */

#define SAMPLE_RATE    44100
#define NUM_BUFFERS    4
#define BUFFER_SAMPLES 1024

/*---------------------------------------------------------------------------
 * AY-3-8912 volume table (logarithmic DAC, measured values)
 * Scaled so 3 channels at maximum = ~30 400 (fits in signed 16-bit)
 *---------------------------------------------------------------------------*/

static const int ayVolume[16] = {
  0,   129,  187,  274,  405,  599,  834, 1365,
  1608, 2586, 3615, 4536, 5725, 7190, 8551, 10148
};

/*---------------------------------------------------------------------------
 * AY register file
 *---------------------------------------------------------------------------*/

static byte ayRegs[16];
static byte ayRegSelect = 0;

/*---------------------------------------------------------------------------
 * Tone channel state
 *---------------------------------------------------------------------------*/

static int toneCountA = 1, toneCountB = 1, toneCountC = 1;
static int toneOutA = 0, toneOutB = 0, toneOutC = 0;

/*---------------------------------------------------------------------------
 * Noise generator (17-bit LFSR)
 *---------------------------------------------------------------------------*/

static int noiseCount = 1;
static int noiseOut = 0;
static unsigned int noiseLFSR = 1;

/*---------------------------------------------------------------------------
 * Envelope generator
 *---------------------------------------------------------------------------*/

static int envCount = 1;
static int envStep = 0;            /* 0-15 within current cycle */
static int envVolume = 0;          /* current envelope amplitude 0-15 */
static tiki_bool envHolding = FALSE;
/* decoded shape bits */
static tiki_bool envContinue = FALSE;
static tiki_bool envAttack = FALSE;
static tiki_bool envAlternate = FALSE;
static tiki_bool envHold = FALSE;

/*---------------------------------------------------------------------------
 * Sample generation accumulators (Bresenham-style)
 *---------------------------------------------------------------------------*/

static int sampleAccum = 0;
static int tickAccum = 0;

/*---------------------------------------------------------------------------
 * waveOut audio output
 *---------------------------------------------------------------------------*/

static HWAVEOUT hWaveOut = NULL;
static WAVEHDR waveHdr[NUM_BUFFERS];
static short waveBuf[NUM_BUFFERS][BUFFER_SAMPLES];
static int curBuf = 0;
static int bufPos = 0;
static tiki_bool audioOpen = FALSE;

/*===========================================================================
 * Internal helpers
 *===========================================================================*/

static int getTonePeriod (int fine, int coarse) {
  int p = ((coarse & 0x0F) << 8) | fine;
  return p < 1 ? 1 : p;
}

static int getNoisePeriod (void) {
  int p = ayRegs[6] & 0x1F;
  return p < 1 ? 1 : p;
}

static int getEnvPeriod (void) {
  int p = ayRegs[11] | (ayRegs[12] << 8);
  return p < 1 ? 1 : p;
}

/*---------------------------------------------------------------------------
 * Envelope
 *---------------------------------------------------------------------------*/

static void resetEnvelope (void) {
  byte shape = ayRegs[13] & 0x0F;
  envContinue  = (shape & 0x08) != 0;
  envAttack    = (shape & 0x04) != 0;
  envAlternate = (shape & 0x02) != 0;
  envHold      = (shape & 0x01) != 0;
  envStep = 0;
  envHolding = FALSE;
  envVolume = envAttack ? 0 : 15;
  envCount = getEnvPeriod();
}

static void stepEnvelope (void) {
  if (envHolding) return;

  if (--envCount <= 0) {
    envCount = getEnvPeriod();
    envStep++;

    if (envStep >= 16) {
      /* end of one cycle */
      if (!envContinue) {
        envVolume = 0;
        envHolding = TRUE;
        return;
      }
      if (envHold) {
        /* hold at final value */
        envVolume = envAlternate ? (envAttack ? 0 : 15)
                                : (envAttack ? 15 : 0);
        envHolding = TRUE;
        return;
      }
      if (envAlternate)
        envAttack = !envAttack;
      envStep = 0;
    }
    envVolume = envAttack ? envStep : (15 - envStep);
  }
}

/*---------------------------------------------------------------------------
 * Step all AY counters by one base tick (125 kHz)
 *---------------------------------------------------------------------------*/

static void stepAY (void) {
  /* Tone channels */
  if (--toneCountA <= 0) {
    toneCountA = getTonePeriod(ayRegs[0], ayRegs[1]);
    toneOutA ^= 1;
  }
  if (--toneCountB <= 0) {
    toneCountB = getTonePeriod(ayRegs[2], ayRegs[3]);
    toneOutB ^= 1;
  }
  if (--toneCountC <= 0) {
    toneCountC = getTonePeriod(ayRegs[4], ayRegs[5]);
    toneOutC ^= 1;
  }

  /* Noise — 17-bit LFSR, taps at bits 0 and 3 */
  if (--noiseCount <= 0) {
    noiseCount = getNoisePeriod();
    noiseLFSR = (noiseLFSR >> 1) |
                (((noiseLFSR ^ (noiseLFSR >> 3)) & 1) << 16);
    noiseOut = noiseLFSR & 1;
  }

  /* Envelope */
  stepEnvelope();
}

/*---------------------------------------------------------------------------
 * Mix the three channels into one signed 16-bit sample
 *---------------------------------------------------------------------------*/

static short mixSample (void) {
  byte mixer = ayRegs[7];
  int sum = 0;

  /* Channel A */
  {
    int toneEn  = !(mixer & 0x01);        /* bit 0 low = tone on */
    int noiseEn = !(mixer & 0x08);        /* bit 3 low = noise on */
    int out = (toneEn ? toneOutA : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[8] & 0x10) ? envVolume : (ayRegs[8] & 0x0F);
    if (out) sum += ayVolume[vol];
  }

  /* Channel B */
  {
    int toneEn  = !(mixer & 0x02);
    int noiseEn = !(mixer & 0x10);
    int out = (toneEn ? toneOutB : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[9] & 0x10) ? envVolume : (ayRegs[9] & 0x0F);
    if (out) sum += ayVolume[vol];
  }

  /* Channel C */
  {
    int toneEn  = !(mixer & 0x04);
    int noiseEn = !(mixer & 0x20);
    int out = (toneEn ? toneOutC : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[10] & 0x10) ? envVolume : (ayRegs[10] & 0x0F);
    if (out) sum += ayVolume[vol];
  }

  return (short)sum;
}

/*---------------------------------------------------------------------------
 * waveOut helpers
 *---------------------------------------------------------------------------*/

static void submitBuffer (int idx) {
  if (!audioOpen) return;
  waveOutWrite(hWaveOut, &waveHdr[idx], sizeof(WAVEHDR));
}

static void addSample (short sample) {
  if (!audioOpen) return;
  waveBuf[curBuf][bufPos++] = sample;
  if (bufPos >= BUFFER_SAMPLES) {
    if (waveHdr[curBuf].dwFlags & WHDR_DONE)
      submitBuffer(curBuf);
    /* else: buffer still playing — skip to avoid blocking */
    curBuf = (curBuf + 1) % NUM_BUFFERS;
    bufPos = 0;
  }
}

/*===========================================================================
 * Public interface
 *===========================================================================*/

/* initialiser lyd-maskinvare */
void soundInit (void) {
  WAVEFORMATEX wfx;
  memset(&wfx, 0, sizeof(wfx));
  wfx.wFormatTag       = WAVE_FORMAT_PCM;
  wfx.nChannels        = 1;
  wfx.nSamplesPerSec   = SAMPLE_RATE;
  wfx.wBitsPerSample   = 16;
  wfx.nBlockAlign      = 2;
  wfx.nAvgBytesPerSec  = SAMPLE_RATE * 2;

  if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx,
                  0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
    hWaveOut = NULL;
    audioOpen = FALSE;
    return;
  }

  for (int i = 0; i < NUM_BUFFERS; i++) {
    memset(&waveHdr[i], 0, sizeof(WAVEHDR));
    waveHdr[i].lpData         = (LPSTR)waveBuf[i];
    waveHdr[i].dwBufferLength = BUFFER_SAMPLES * sizeof(short);
    waveOutPrepareHeader(hWaveOut, &waveHdr[i], sizeof(WAVEHDR));
    waveHdr[i].dwFlags |= WHDR_DONE;   /* mark as available */
  }

  curBuf = 0;
  bufPos = 0;
  audioOpen = TRUE;

  /* default register state: all channels off */
  memset(ayRegs, 0, sizeof(ayRegs));
  ayRegs[7] = 0xFF;
}

/* rydd opp lyd-maskinvare */
void soundCleanup (void) {
  if (hWaveOut) {
    waveOutReset(hWaveOut);
    for (int i = 0; i < NUM_BUFFERS; i++)
      waveOutUnprepareHeader(hWaveOut, &waveHdr[i], sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
    hWaveOut = NULL;
  }
  audioOpen = FALSE;
}

/* skriv til kontrollregister (registervelger) */
void soundReg (byte value) {
  ayRegSelect = value & 0x0F;
}

/* skriv til dataregister */
void soundData (byte value) {
  /* mask writes to match real AY register widths */
  static const byte regMask[16] = {
    0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
    0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF
  };
  ayRegs[ayRegSelect] = value & regMask[ayRegSelect];

  switch (ayRegSelect) {
    case 13:
      resetEnvelope();
      break;
    case 14:
      newOffset(value);   /* R14 I/O port A = video scroll on TIKI-100 */
      break;
  }
}

/* les dataregister */
byte getSoundData (void) {
  return ayRegs[ayRegSelect];
}

/* generer lydsampler — kalles fra LoopZ80 med antall CPU-sykler */
void updateSound (int cpuCycles) {
  if (!audioOpen) return;

  /* how many samples to generate this call */
  sampleAccum += cpuCycles * SAMPLE_RATE;
  int samples = sampleAccum / CPU_CLOCK;
  sampleAccum %= CPU_CLOCK;

  for (int i = 0; i < samples; i++) {
    /* how many AY base ticks for this sample (Bresenham) */
    tickAccum += AY_BASE_RATE;
    int ticks = tickAccum / SAMPLE_RATE;
    tickAccum %= SAMPLE_RATE;

    for (int t = 0; t < ticks; t++)
      stepAY();

    addSample(mixSample());
  }
}
