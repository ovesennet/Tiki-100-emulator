/* diskview.h
 *
 * Disk directory viewer window
 */

#ifndef DISKVIEW_H
#define DISKVIEW_H

#include <windows.h>

LRESULT CALLBACK DiskViewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void diskViewSetHwndPtr (HWND *p);

/* provide disk image pointers to the viewer
 * call after loading/removing disks to keep the viewer up to date */
void diskViewSetDisk (int drive, const unsigned char *data, unsigned long size);

#endif
