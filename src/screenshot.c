/* screenshot.c
 *
 * Screenshot capture for TIKI-100_emul
 * Copies the emulator display area to the clipboard
 */

#include <windows.h>

#include "screenshot.h"
#include "log.h"

/* Copy the emulator screen area to the clipboard */
void copyScreenshot (HWND hwndParent, HDC srcDC, int srcX, int srcY, int srcW, int srcH) {
  /* create a compatible DC and bitmap */
  HDC captureDC = CreateCompatibleDC (srcDC);
  HBITMAP captureBmp = CreateCompatibleBitmap (srcDC, srcW, srcH);
  HBITMAP oldBmp = SelectObject (captureDC, captureBmp);

  /* copy the emulator area */
  BitBlt (captureDC, 0, 0, srcW, srcH, srcDC, srcX, srcY, SRCCOPY);

  /* deselect the bitmap before giving it to the clipboard */
  SelectObject (captureDC, oldBmp);
  DeleteDC (captureDC);

  /* place on clipboard */
  if (OpenClipboard (hwndParent)) {
    EmptyClipboard ();
    SetClipboardData (CF_BITMAP, captureBmp);
    CloseClipboard ();
    LOG_I ("Screenshot copied to clipboard (%dx%d)", srcW, srcH);
  } else {
    LOG_E ("Screenshot: failed to open clipboard");
    DeleteObject (captureBmp);
  }
}
