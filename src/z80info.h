/* z80info.h
 *
 * Z80 CPU information window
 */

#ifndef Z80INFO_H
#define Z80INFO_H

#include <windows.h>

#define CPU_LOAD_HISTORY_SIZE 60

LRESULT CALLBACK Z80InfoWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void z80InfoSetHwndPtr (HWND *p);

/* CPU load history (defined in win32.c) */
extern int cpuLoadHistory[CPU_LOAD_HISTORY_SIZE];
extern int cpuLoadHistoryPos;
extern int cpuLoadHistoryCount;
extern int cpuLoadCurrent;

/* Z80 PC-diversity counters (defined in TIKI-100_emul.c) */
extern unsigned char z80PcBitmap[8192];
extern unsigned int z80UniquePC;

#endif
