/* z80info.h
 *
 * Z80 CPU information window
 */

#ifndef Z80INFO_H
#define Z80INFO_H

#include <windows.h>

LRESULT CALLBACK Z80InfoWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void z80InfoSetHwndPtr (HWND *p);

#endif
