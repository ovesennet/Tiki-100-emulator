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
 * call after loading/removing disks to keep the viewer up to date.
 * data is mutable so the "Add file" feature can write to the image.
 * filePath is the full path to the .dsk file on the host (needed to
 * persist changes after adding a file). Pass NULL/"" when ejecting. */
void diskViewSetDisk (int drive, unsigned char *data, unsigned long size,
                      const char *filePath);

#endif
