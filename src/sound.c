/* sound.c V1.3.0
 *
 * AY-3-8912 emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 * Audio output added 2026 Arctic Retro
 *
 * Improvements over V1.2.0:
 *  - Cycle-accurate register updates via soundFlush()
 *  - Stereo output (A=left, B=center, C=right)
 *  - Measured AY-3-8912 logarithmic DAC volume table
 *  - Single-pole IIR low-pass filter to reduce aliasing
 *  - Small waveOut buffers with timeBeginPeriod for gapless playback
 */

#include "TIKI-100_emul.h"
#include "protos.h"
#include "sleep.h"

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
#define NUM_BUFFERS    12               /* plenty of small buffers */
#define PREFILL_BUFS   4                /* ~23 ms initial silence cushion */
#define BUFFER_SAMPLES 256              /* ~5.8 ms per buffer */
#define NUM_CHANNELS   2                /* stereo */

/*---------------------------------------------------------------------------
 * AY-3-8912 volume table — measured logarithmic DAC curve
 * Based on Matthew Westcott's measurements of a real AY-3-8912.
 * Scaled so 3 channels at maximum ≈ 28 000 (fits signed 16-bit).
 *---------------------------------------------------------------------------*/

static const int ayVolume[16] = {
     0,   113,   164,   241,
   355,   525,   731,  1079,
  1498,  2210,  3087,  4345,
  5765,  7873,  9181, 10000
};

/*---------------------------------------------------------------------------
 * Stereo panning — each channel contributes to L and R (0..100 scale)
 * A: 75% left, 25% right   B: 50/50   C: 25% left, 75% right
 *---------------------------------------------------------------------------*/

#define PAN_A_L  75
#define PAN_A_R  25
#define PAN_B_L  50
#define PAN_B_R  50
#define PAN_C_L  25
#define PAN_C_R  75

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
 * Single-pole IIR low-pass filter state (per channel: L and R)
 * Cutoff ≈ 12 kHz at 44100 Hz — removes harsh aliasing above Nyquist
 * while preserving most of the audible spectrum.
 * alpha = 2*pi*fc / (2*pi*fc + sampleRate) ≈ 0.63
 * Using fixed-point: alpha_fp = 0.63 * 65536 ≈ 41288
 *---------------------------------------------------------------------------*/

#define LP_ALPHA  41288
#define LP_SHIFT  16
static int lpStateL = 0;
static int lpStateR = 0;

/*---------------------------------------------------------------------------
 * Cycle tracking for accurate register-write timing
 *---------------------------------------------------------------------------*/

static int cycleDebt = 0;  /* cycles since last updateSound call */

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
 * Mix the three channels into stereo (L, R)
 *---------------------------------------------------------------------------*/

static void mixSample (int *outL, int *outR) {
  byte mixer = ayRegs[7];
  int ampA = 0, ampB = 0, ampC = 0;

  /* Channel A */
  {
    int toneEn  = !(mixer & 0x01);
    int noiseEn = !(mixer & 0x08);
    int out = (toneEn ? toneOutA : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[8] & 0x10) ? envVolume : (ayRegs[8] & 0x0F);
    if (out) ampA = ayVolume[vol];
  }

  /* Channel B */
  {
    int toneEn  = !(mixer & 0x02);
    int noiseEn = !(mixer & 0x10);
    int out = (toneEn ? toneOutB : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[9] & 0x10) ? envVolume : (ayRegs[9] & 0x0F);
    if (out) ampB = ayVolume[vol];
  }

  /* Channel C */
  {
    int toneEn  = !(mixer & 0x04);
    int noiseEn = !(mixer & 0x20);
    int out = (toneEn ? toneOutC : 1) & (noiseEn ? noiseOut : 1);
    int vol = (ayRegs[10] & 0x10) ? envVolume : (ayRegs[10] & 0x0F);
    if (out) ampC = ayVolume[vol];
  }

  *outL = (ampA * PAN_A_L + ampB * PAN_B_L + ampC * PAN_C_L) / 100;
  *outR = (ampA * PAN_A_R + ampB * PAN_B_R + ampC * PAN_C_R) / 100;
}

/*---------------------------------------------------------------------------
 * waveOut playback — audio-paced direct buffer fill
 *
 * When pacing is enabled (normal speed), submitBuffer blocks after
 * submitting until the next buffer is free.  This makes the 44100 Hz
 * audio device the master clock for the emulation — no reliance on
 * Sleep() accuracy.  In fast mode, pacing is disabled and buffers
 * are overwritten freely.
 *---------------------------------------------------------------------------*/

static HWAVEOUT hWaveOut = NULL;
static WAVEHDR  waveHdr[NUM_BUFFERS];
static short    waveBuf[NUM_BUFFERS][BUFFER_SAMPLES * NUM_CHANNELS];
static HANDLE   hBufferEvent = NULL;  /* signalled by waveOut on WOM_DONE */
static tiki_bool audioOpen = FALSE;
static tiki_bool paceToAudio = TRUE;  /* block in submitBuffer to pace emu */
static int curBuf = 0;    /* buffer currently being filled */
static int bufPos = 0;    /* sample-frame position within current buffer */

static void submitBuffer (void) {
  /* Submit the filled buffer for playback */
  waveOutWrite (hWaveOut, &waveHdr[curBuf], sizeof (WAVEHDR));
  curBuf = (curBuf + 1) % NUM_BUFFERS;
  bufPos = 0;

  /* In paced mode, wait until the next buffer has finished playing.
   * This is the primary timing mechanism — it locks the emulation
   * speed to the audio sample rate (44100 Hz).
   * We pump the Windows message queue during the wait so the UI
   * stays responsive (everything runs on one thread). */
  if (paceToAudio) {
    while (!(waveHdr[curBuf].dwFlags & WHDR_DONE)) {
      MSG msg;
      while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          /* Re-post so loopEmul() sees it, and bail out */
          PostQuitMessage ((int)msg.wParam);
          audioOpen = FALSE;
          return;
        }
        TranslateMessage (&msg);
        DispatchMessage (&msg);
      }
      WaitForSingleObject (hBufferEvent, 2);
      if (!audioOpen) return;
    }
  }
}

/*---------------------------------------------------------------------------
 * addSample — filter and write one stereo sample into the current buffer
 *---------------------------------------------------------------------------*/

static void addSample (int rawL, int rawR) {
  int filtL, filtR;
  if (!audioOpen) return;

  /* single-pole IIR low-pass filter */
  lpStateL += ((rawL - lpStateL) * LP_ALPHA) >> LP_SHIFT;
  lpStateR += ((rawR - lpStateR) * LP_ALPHA) >> LP_SHIFT;
  filtL = lpStateL;
  filtR = lpStateR;

  /* clamp to 16-bit */
  if (filtL >  32767) filtL =  32767;
  if (filtL < -32768) filtL = -32768;
  if (filtR >  32767) filtR =  32767;
  if (filtR < -32768) filtR = -32768;

  waveBuf[curBuf][bufPos * 2]     = (short)filtL;
  waveBuf[curBuf][bufPos * 2 + 1] = (short)filtR;
  if (++bufPos >= BUFFER_SAMPLES)
    submitBuffer();
}

/*===========================================================================
 * Public interface
 *===========================================================================*/

/* initialiser lyd-maskinvare */
void soundInit (void) {
  WAVEFORMATEX wfx;
  int i;

  /* Request 1 ms timer resolution for accurate Sleep in loopEmul
   * fallback (non-audio paced mode). */
  timeBeginPeriod (1);

  /* Create auto-reset event for buffer-done notifications */
  hBufferEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
  if (!hBufferEvent) {
    audioOpen = FALSE;
    return;
  }

  memset(&wfx, 0, sizeof(wfx));
  wfx.wFormatTag       = WAVE_FORMAT_PCM;
  wfx.nChannels        = NUM_CHANNELS;
  wfx.nSamplesPerSec   = SAMPLE_RATE;
  wfx.wBitsPerSample   = 16;
  wfx.nBlockAlign      = NUM_CHANNELS * 2;
  wfx.nAvgBytesPerSec  = SAMPLE_RATE * NUM_CHANNELS * 2;

  if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx,
                  (DWORD_PTR)hBufferEvent, 0,
                  CALLBACK_EVENT) != MMSYSERR_NOERROR) {
    hWaveOut = NULL;
    CloseHandle (hBufferEvent);
    hBufferEvent = NULL;
    audioOpen = FALSE;
    return;
  }

  /* Prepare all buffer headers */
  for (i = 0; i < NUM_BUFFERS; i++) {
    memset(&waveHdr[i], 0, sizeof(WAVEHDR));
    waveHdr[i].lpData         = (LPSTR)waveBuf[i];
    waveHdr[i].dwBufferLength = BUFFER_SAMPLES * NUM_CHANNELS * sizeof(short);
    waveOutPrepareHeader(hWaveOut, &waveHdr[i], sizeof(WAVEHDR));
  }

  /* Pre-fill the first PREFILL_BUFS buffers with silence and submit
   * them to build an initial playback cushion (~23 ms). */
  for (i = 0; i < PREFILL_BUFS; i++) {
    memset(waveBuf[i], 0, BUFFER_SAMPLES * NUM_CHANNELS * sizeof(short));
    waveOutWrite(hWaveOut, &waveHdr[i], sizeof(WAVEHDR));
  }

  /* Start filling from the first non-submitted buffer.
   * Mark non-prefilled buffers WHDR_DONE so submitBuffer sees them
   * as immediately available for writing. */
  curBuf = PREFILL_BUFS;
  bufPos = 0;
  for (i = PREFILL_BUFS; i < NUM_BUFFERS; i++)
    waveHdr[i].dwFlags |= WHDR_DONE;
  sampleAccum = 0;
  tickAccum = 0;
  cycleDebt = 0;
  lpStateL = 0;
  lpStateR = 0;

  /* default register state: all channels off */
  memset(ayRegs, 0, sizeof(ayRegs));
  ayRegs[7] = 0xFF;

  audioOpen = TRUE;
}

/* rydd opp lyd-maskinvare */
void soundCleanup (void) {
  int i;
  if (hWaveOut) {
    audioOpen = FALSE;
    waveOutReset(hWaveOut);
    for (i = 0; i < NUM_BUFFERS; i++)
      waveOutUnprepareHeader(hWaveOut, &waveHdr[i], sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
    hWaveOut = NULL;
  }
  audioOpen = FALSE;
  if (hBufferEvent) {
    CloseHandle (hBufferEvent);
    hBufferEvent = NULL;
  }
  timeEndPeriod (1);
}

/* returns TRUE when audio output is open and driving the emulation clock */
tiki_bool soundIsActive (void) {
  return audioOpen;
}

/* enable/disable audio-paced timing (disable for fast mode) */
void soundSetPacing (tiki_bool enable) {
  paceToAudio = enable;
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

/*---------------------------------------------------------------------------
 * soundFlush — call BEFORE writing AY registers to generate all audio
 * samples up to the exact CPU cycle of the write. This ensures register
 * changes take effect at the correct sample boundary.
 *
 * Uses cpu.IPeriod and cpu.ICount to compute elapsed cycles since
 * the last LoopZ80 call.
 *---------------------------------------------------------------------------*/

extern Z80 cpu;

void soundFlush (void) {
  int elapsed = cpu.IPeriod - cpu.ICount - cycleDebt;
  if (elapsed > 0) {
    updateSound(elapsed);
    cycleDebt += elapsed;
  }
}

/* generer lydsampler — kalles fra LoopZ80 med antall CPU-sykler */
void updateSound (int cpuCycles) {
  int samples, i;
  if (!audioOpen) return;

  /* how many samples to generate this call */
  sampleAccum += cpuCycles * SAMPLE_RATE;
  samples = sampleAccum / CPU_CLOCK;
  sampleAccum %= CPU_CLOCK;

  for (i = 0; i < samples; i++) {
    int ticks, t, rawL, rawR;
    /* how many AY base ticks for this sample (Bresenham) */
    tickAccum += AY_BASE_RATE;
    ticks = tickAccum / SAMPLE_RATE;
    tickAccum %= SAMPLE_RATE;

    for (t = 0; t < ticks; t++)
      stepAY();

    mixSample(&rawL, &rawR);
    addSample(rawL, rawR);
  }
}

/* reset cycle debt — called from LoopZ80 after updateSound */
void soundResetDebt (void) {
  cycleDebt = 0;
}

/* sleep save/restore */
tiki_bool soundSleepSave (FILE *f) {
  if (fwrite (ayRegs, 1, sizeof(ayRegs), f) != sizeof(ayRegs)) return FALSE;
  if (fwrite (&ayRegSelect, sizeof(ayRegSelect), 1, f) != 1) return FALSE;
  if (fwrite (&toneCountA, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&toneCountB, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&toneCountC, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&toneOutA, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&toneOutB, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&toneOutC, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&noiseCount, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&noiseOut, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&noiseLFSR, sizeof(unsigned int), 1, f) != 1) return FALSE;
  if (fwrite (&envCount, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&envStep, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&envVolume, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&envHolding, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&envContinue, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&envAttack, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&envAlternate, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&envHold, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fwrite (&sampleAccum, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&tickAccum, sizeof(int), 1, f) != 1) return FALSE;
  if (fwrite (&cycleDebt, sizeof(int), 1, f) != 1) return FALSE;
  return TRUE;
}

tiki_bool soundSleepRestore (FILE *f) {
  if (fread (ayRegs, 1, sizeof(ayRegs), f) != sizeof(ayRegs)) return FALSE;
  if (fread (&ayRegSelect, sizeof(ayRegSelect), 1, f) != 1) return FALSE;
  if (fread (&toneCountA, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&toneCountB, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&toneCountC, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&toneOutA, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&toneOutB, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&toneOutC, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&noiseCount, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&noiseOut, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&noiseLFSR, sizeof(unsigned int), 1, f) != 1) return FALSE;
  if (fread (&envCount, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&envStep, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&envVolume, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&envHolding, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&envContinue, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&envAttack, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&envAlternate, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&envHold, sizeof(tiki_bool), 1, f) != 1) return FALSE;
  if (fread (&sampleAccum, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&tickAccum, sizeof(int), 1, f) != 1) return FALSE;
  if (fread (&cycleDebt, sizeof(int), 1, f) != 1) return FALSE;
  return TRUE;
}
