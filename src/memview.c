/* memview.c
 *
 * Memory viewer window - hex dump of 64KB Z80 RAM
 * Click-to-edit hex editor with inline byte editing
 */

#include "TIKI-100_emul.h"
#include "memview.h"

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif

#define IDC_DUMP_BTN      100
#define IDC_EDIT_BTN      101
#define IDC_SEARCH_EDIT   102
#define IDC_FIND_BTN      103
#define IDC_NEXT_BTN      104
#define MEMVIEW_TOTAL    4096
#define HEX_START_CHAR      6   /* "XXXX: " = 6 chars before hex data */
#define SEARCH_MAX         64

extern byte ram[];
static HWND *pHwndMemInfo;
static HWND hwndMemView;        /* the memview window itself */
static int memViewScroll = 0;
static tiki_bool editMode = FALSE;
static int editAddr = -1;       /* address of byte being edited, -1 = none */
static int editNibble = 0;      /* 0 = waiting for high nibble, 1 = got high */
static byte editValue = 0;      /* value being composed */
static int charW = 7;           /* character width in pixels (measured at create) */
static int charH = 14;          /* character/line height */
static HWND hwndEditBtn;
static HWND hwndSearchEdit;     /* search text field */
static HWND hwndFindBtn;
static HWND hwndNextBtn;
static int searchResultAddr = -1;  /* start addr of last match */
static int searchResultLen = 0;    /* length of last match */

/* search for needle in ram[], starting from startAddr+1, wrapping around */
static int searchMemory (const char *needle, int len, int startAddr) {
  int i, j;
  if (len <= 0 || len > 0x10000) return -1;
  for (i = 0; i < 0x10000; i++) {
    int addr = (startAddr + 1 + i) & 0xFFFF;
    tiki_bool found = TRUE;
    for (j = 0; j < len; j++) {
      if ((char)ram[(addr + j) & 0xFFFF] != needle[j]) {
        found = FALSE;
        break;
      }
    }
    if (found) return addr;
  }
  return -1;
}

void memViewSetHwndPtr (HWND *p) {
  pHwndMemInfo = p;
}

/* returns TRUE if hwnd is a child of the memview window (for TranslateMessage) */
tiki_bool memViewIsChild (HWND hwnd) {
  if (!hwndMemView) return FALSE;
  return hwnd == hwndSearchEdit || GetParent (hwnd) == hwndMemView;
}

/* convert pixel click to memory address, returns -1 if outside hex area */
static int hitTest (HWND hwnd, int mx, int my) {
  int row = (my - MEMVIEW_TOOLBAR - 2) / charH;
  if (row < 0 || row >= MEMVIEW_LINES) return -1;
  int col = (mx - 6) / charW;  /* char column */
  col -= HEX_START_CHAR;        /* offset past "XXXX: " */
  if (col < 0 || col >= 48) return -1;  /* 16 bytes * 3 chars each */
  int byteIdx = col / 3;
  if (byteIdx < 0 || byteIdx > 15) return -1;
  int addr = (memViewScroll + row) * 16 + byteIdx;
  return (addr >= 0 && addr <= 0xFFFF) ? addr : -1;
}

/* ensure editAddr is visible, scroll if needed */
static void ensureVisible (HWND hwnd) {
  if (editAddr < 0) return;
  int line = editAddr / 16;
  if (line < memViewScroll) {
    memViewScroll = line;
  } else if (line >= memViewScroll + MEMVIEW_LINES) {
    memViewScroll = line - MEMVIEW_LINES + 1;
  }
  if (memViewScroll < 0) memViewScroll = 0;
  if (memViewScroll > MEMVIEW_TOTAL - MEMVIEW_LINES) memViewScroll = MEMVIEW_TOTAL - MEMVIEW_LINES;
  SCROLLINFO si = {0};
  si.cbSize = sizeof (SCROLLINFO);
  si.fMask = SIF_POS;
  si.nPos = memViewScroll;
  SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
}

LRESULT CALLBACK MemViewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      hwndMemView = hwnd;
      memViewScroll = 0;
      editMode = FALSE;
      editAddr = -1;
      editNibble = 0;
      searchResultAddr = -1;
      searchResultLen = 0;
      SetTimer (hwnd, 1, 200, NULL);
      {
        SCROLLINFO si = {0};
        si.cbSize = sizeof (SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = MEMVIEW_TOTAL - 1;
        si.nPage = MEMVIEW_LINES;
        si.nPos = 0;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
      }
      {
        HINSTANCE hInst = GetModuleHandle (NULL);
        HFONT guiFont = (HFONT)GetStockObject (DEFAULT_GUI_FONT);
        HWND btn;
        btn = CreateWindow ("BUTTON", "Dump",
          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          4, 3, 50, 22,
          hwnd, (HMENU)IDC_DUMP_BTN, hInst, NULL);
        SendMessage (btn, WM_SETFONT, (WPARAM)guiFont, TRUE);
        hwndEditBtn = CreateWindow ("BUTTON", "Edit",
          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          58, 3, 50, 22,
          hwnd, (HMENU)IDC_EDIT_BTN, hInst, NULL);
        SendMessage (hwndEditBtn, WM_SETFONT, (WPARAM)guiFont, TRUE);
        /* search controls - visible only in edit mode */
        hwndSearchEdit = CreateWindowEx (WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | ES_AUTOHSCROLL,
          280, 3, 140, 22,
          hwnd, (HMENU)IDC_SEARCH_EDIT, hInst, NULL);
        SendMessage (hwndSearchEdit, WM_SETFONT, (WPARAM)guiFont, TRUE);
        SendMessage (hwndSearchEdit, EM_SETCUEBANNER, 0, (LPARAM)L"Search string...");
        hwndFindBtn = CreateWindow ("BUTTON", "Find",
          WS_CHILD | BS_PUSHBUTTON,
          424, 3, 42, 22,
          hwnd, (HMENU)IDC_FIND_BTN, hInst, NULL);
        SendMessage (hwndFindBtn, WM_SETFONT, (WPARAM)guiFont, TRUE);
        hwndNextBtn = CreateWindow ("BUTTON", "Next",
          WS_CHILD | BS_PUSHBUTTON,
          470, 3, 42, 22,
          hwnd, (HMENU)IDC_NEXT_BTN, hInst, NULL);
        SendMessage (hwndNextBtn, WM_SETFONT, (WPARAM)guiFont, TRUE);
      }
      /* measure monospace character size */
      {
        HDC hdc = GetDC (hwnd);
        HFONT font = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT old = SelectObject (hdc, font);
        TEXTMETRIC tm;
        GetTextMetrics (hdc, &tm);
        charW = tm.tmAveCharWidth;
        charH = tm.tmHeight;
        SelectObject (hdc, old);
        DeleteObject (font);
        ReleaseDC (hwnd, hdc);
      }
      return 0;
    case WM_COMMAND:
      switch (LOWORD (wParam)) {
        case IDC_DUMP_BTN:
          {
            OPENFILENAME ofn;
            char fn[256] = "binary.bin";
            memset (&ofn, 0, sizeof (OPENFILENAME));
            ofn.lStructSize = sizeof (OPENFILENAME);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = "Binary files (*.bin)\0*.bin\0All files\0*.*\0";
            ofn.lpstrFile = fn;
            ofn.nMaxFile = sizeof (fn);
            ofn.lpstrDefExt = "bin";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileName (&ofn)) {
              HANDLE hFile = CreateFile (fn, GENERIC_WRITE, 0, NULL,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
              if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written;
                WriteFile (hFile, ram, 64 * 1024, &written, NULL);
                CloseHandle (hFile);
              }
            }
          }
          break;
        case IDC_EDIT_BTN:
          editMode = !editMode;
          editAddr = -1;
          editNibble = 0;
          searchResultAddr = -1;
          searchResultLen = 0;
          SetWindowText (hwndEditBtn, editMode ? "Done" : "Edit");
          ShowWindow (hwndSearchEdit, editMode ? SW_SHOW : SW_HIDE);
          ShowWindow (hwndFindBtn, editMode ? SW_SHOW : SW_HIDE);
          ShowWindow (hwndNextBtn, editMode ? SW_SHOW : SW_HIDE);
          SetFocus (hwnd);
          InvalidateRect (hwnd, NULL, FALSE);
          break;
        case IDC_FIND_BTN:
        case IDC_NEXT_BTN:
          {
            char needle[SEARCH_MAX + 1];
            int len = GetWindowText (hwndSearchEdit, needle, sizeof(needle));
            if (len > 0) {
              int startFrom = (LOWORD (wParam) == IDC_NEXT_BTN && searchResultAddr >= 0)
                ? searchResultAddr : -1;
              int found = searchMemory (needle, len, startFrom);
              if (found >= 0) {
                searchResultAddr = found;
                searchResultLen = len;
                editAddr = found;
                editNibble = 0;
                editValue = ram[found];
                ensureVisible (hwnd);
              } else {
                searchResultAddr = -1;
                searchResultLen = 0;
                MessageBeep (MB_ICONEXCLAMATION);
              }
              SetFocus (hwnd);
              InvalidateRect (hwnd, NULL, FALSE);
            }
          }
          break;
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;  /* suppress background erase to prevent flicker */
    case WM_LBUTTONDOWN:
      if (editMode) {
        int mx = LOWORD (lParam);
        int my = HIWORD (lParam);
        int addr = hitTest (hwnd, mx, my);
        if (addr >= 0) {
          editAddr = addr;
          editNibble = 0;
          editValue = ram[addr];
          SetFocus (hwnd);
          InvalidateRect (hwnd, NULL, FALSE);
        }
      }
      return 0;
    case WM_KEYDOWN:
      if (editMode && editAddr >= 0) {
        if (wParam == VK_RETURN) {
          /* commit and advance to next byte */
          if (editNibble == 1) {
            /* only high nibble entered - treat as full byte */
            ram[editAddr] = editValue;
          }
          if (editAddr < 0xFFFF) {
            editAddr++;
            editNibble = 0;
            editValue = ram[editAddr];
            ensureVisible (hwnd);
          }
          InvalidateRect (hwnd, NULL, FALSE);
          return 0;
        } else if (wParam == VK_ESCAPE) {
          /* cancel current edit */
          editAddr = -1;
          editNibble = 0;
          InvalidateRect (hwnd, NULL, FALSE);
          return 0;
        } else if (wParam == VK_TAB) {
          /* advance without writing */
          if (editAddr < 0xFFFF) {
            editAddr++;
            editNibble = 0;
            editValue = ram[editAddr];
            ensureVisible (hwnd);
            InvalidateRect (hwnd, NULL, FALSE);
          }
          return 0;
        }
      }
      /* hex digit input via WM_KEYDOWN (no TranslateMessage in main loop) */
      if (editMode && editAddr >= 0) {
        int nib = -1;
        if (wParam >= '0' && wParam <= '9')
          nib = (int)(wParam - '0');
        else if (wParam >= 'A' && wParam <= 'F')
          nib = (int)(wParam - 'A' + 10);
        if (nib >= 0) {
          if (editNibble == 0) {
            editValue = (byte)(nib << 4);
            editNibble = 1;
          } else {
            editValue = (editValue & 0xF0) | (byte)nib;
            /* auto-commit and advance */
            ram[editAddr] = editValue;
            if (editAddr < 0xFFFF) {
              editAddr++;
              editValue = ram[editAddr];
            }
            editNibble = 0;
            ensureVisible (hwnd);
          }
          InvalidateRect (hwnd, NULL, FALSE);
          return 0;
        }
      }
      break;
    case WM_TIMER:
      if (!editMode) {
        RECT rc;
        GetClientRect (hwnd, &rc);
        rc.top = MEMVIEW_TOOLBAR;
        InvalidateRect (hwnd, &rc, FALSE);
      }
      return 0;
    case WM_VSCROLL:
      {
        SCROLLINFO si = {0};
        si.cbSize = sizeof (SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo (hwnd, SB_VERT, &si);
        int pos = si.nPos;
        switch (LOWORD (wParam)) {
          case SB_LINEUP:       pos -= 1; break;
          case SB_LINEDOWN:     pos += 1; break;
          case SB_PAGEUP:       pos -= MEMVIEW_LINES; break;
          case SB_PAGEDOWN:     pos += MEMVIEW_LINES; break;
          case SB_THUMBTRACK:   pos = si.nTrackPos; break;
        }
        if (pos < 0) pos = 0;
        if (pos > MEMVIEW_TOTAL - MEMVIEW_LINES) pos = MEMVIEW_TOTAL - MEMVIEW_LINES;
        memViewScroll = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_MOUSEWHEEL:
      {
        int delta = GET_WHEEL_DELTA_WPARAM (wParam);
        int lines = -delta / WHEEL_DELTA * 3;
        memViewScroll += lines;
        if (memViewScroll < 0) memViewScroll = 0;
        if (memViewScroll > MEMVIEW_TOTAL - MEMVIEW_LINES) memViewScroll = MEMVIEW_TOTAL - MEMVIEW_LINES;
        SCROLLINFO si = {0};
        si.cbSize = sizeof (SCROLLINFO);
        si.fMask = SIF_POS;
        si.nPos = memViewScroll;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint (hwnd, &ps);
        RECT rcClient;
        GetClientRect (hwnd, &rcClient);
        int cw = rcClient.right - rcClient.left;
        int ch = rcClient.bottom - rcClient.top;
        /* double-buffer: paint to off-screen bitmap */
        HDC hdc = CreateCompatibleDC (hdcScreen);
        HBITMAP hbmBuf = CreateCompatibleBitmap (hdcScreen, cw, ch);
        HBITMAP hbmOld = SelectObject (hdc, hbmBuf);
        /* fill background */
        FillRect (hdc, &rcClient, GetSysColorBrush (COLOR_3DFACE));
        HFONT font = CreateFont (14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT oldFont = SelectObject (hdc, font);
        SetBkMode (hdc, OPAQUE);
        int y = MEMVIEW_TOOLBAR + 2;
        int line;
        for (line = 0; line < MEMVIEW_LINES; line++) {
          int baseAddr = (memViewScroll + line) * 16;
          if (baseAddr >= 65536) break;
          /* draw address */
          char addrBuf[8];
          snprintf (addrBuf, sizeof(addrBuf), "%04X: ", baseAddr);
          SetBkColor (hdc, GetSysColor (COLOR_3DFACE));
          SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
          TextOut (hdc, 6, y, addrBuf, 6);
          /* draw 16 hex bytes */
          int i;
          for (i = 0; i < 16; i++) {
            int addr = baseAddr + i;
            byte b;
            char hexBuf[4];
            int xPos = 6 + (HEX_START_CHAR + i * 3) * charW;
            if (editMode && addr == editAddr) {
              /* selected byte - highlight */
              SetBkColor (hdc, RGB (0, 0, 160));
              SetTextColor (hdc, RGB (255, 255, 255));
              if (editNibble == 0) {
                snprintf (hexBuf, sizeof(hexBuf), "%02X ", ram[addr]);
              } else {
                /* show partial edit: high nibble entered, low nibble as underscore */
                snprintf (hexBuf, sizeof(hexBuf), "%X_ ", editValue >> 4);
              }
            } else if (searchResultAddr >= 0 && searchResultLen > 0
                       && addr >= searchResultAddr
                       && addr < searchResultAddr + searchResultLen) {
              /* search result highlight */
              b = ram[addr];
              SetBkColor (hdc, RGB (255, 200, 0));
              SetTextColor (hdc, RGB (0, 0, 0));
              snprintf (hexBuf, sizeof(hexBuf), "%02X ", b);
            } else {
              b = ram[addr];
              SetBkColor (hdc, GetSysColor (COLOR_3DFACE));
              SetTextColor (hdc, editMode ? RGB (80, 80, 80) : GetSysColor (COLOR_BTNTEXT));
              snprintf (hexBuf, sizeof(hexBuf), "%02X ", b);
            }
            TextOut (hdc, xPos, y, hexBuf, 3);
          }
          /* draw ASCII column */
          SetBkColor (hdc, GetSysColor (COLOR_3DFACE));
          SetTextColor (hdc, RGB (100, 100, 100));
          char ascBuf[20];
          ascBuf[0] = ' ';
          for (i = 0; i < 16; i++) {
            byte b = ram[baseAddr + i];
            ascBuf[1 + i] = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
          }
          ascBuf[17] = '\0';
          int ascX = 6 + (HEX_START_CHAR + 48) * charW;
          TextOut (hdc, ascX, y, ascBuf, 17);
          y += charH;
        }
        /* edit mode status */
        if (editMode) {
          SetBkColor (hdc, GetSysColor (COLOR_3DFACE));
          SetTextColor (hdc, RGB (180, 0, 0));
          HFONT guiFont = (HFONT)GetStockObject (DEFAULT_GUI_FONT);
          SelectObject (hdc, guiFont);
          char status[64];
          if (editAddr >= 0) {
            snprintf (status, sizeof(status), " EDIT %04X = %02X  ", editAddr, editNibble ? editValue : ram[editAddr]);
          } else {
            snprintf (status, sizeof(status), " EDIT - click a byte ");
          }
          TextOut (hdc, 120, 6, status, strlen(status));
        }
        SelectObject (hdc, oldFont);
        DeleteObject (font);
        /* blit off-screen buffer to screen in one operation */
        BitBlt (hdcScreen, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);
        SelectObject (hdc, hbmOld);
        DeleteObject (hbmBuf);
        DeleteDC (hdc);
        EndPaint (hwnd, &ps);
      }
      return 0;
    case WM_DESTROY:
      KillTimer (hwnd, 1);
      editMode = FALSE;
      editAddr = -1;
      searchResultAddr = -1;
      searchResultLen = 0;
      hwndMemView = NULL;
      if (pHwndMemInfo) *pHwndMemInfo = NULL;
      return 0;
  }
  return DefWindowProc (hwnd, msg, wParam, lParam);
}
