/* screenshot.h
 *
 * Screenshot capture for TIKI-100_emul
 */

#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <windows.h>

/* Copy the emulator screen area from memdc to the clipboard.
 * hwndParent: window to open clipboard on
 * srcDC:     the memory DC containing the rendered emulator display
 * srcX, srcY: top-left corner of the emulator area in srcDC
 * srcW, srcH: width and height of the emulator area
 */
void copyScreenshot (HWND hwndParent, HDC srcDC, int srcX, int srcY, int srcW, int srcH);

#endif
