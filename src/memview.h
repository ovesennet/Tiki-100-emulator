/* memview.h
 *
 * Memory viewer window
 */

#ifndef MEMVIEW_H
#define MEMVIEW_H

#include <windows.h>

#define MEMVIEW_TOOLBAR    28
#define MEMVIEW_LINES      32

LRESULT CALLBACK MemViewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void memViewSetHwndPtr (HWND *p);
tiki_bool memViewIsChild (HWND hwnd);

#endif
