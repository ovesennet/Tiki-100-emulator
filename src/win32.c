/* win32.c V1.2.0
 *
 * Win32 systemspesifikk kode for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "win32_res.h"

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <commctrl.h>

#include "log.h"
#include "screenshot.h"
#include "z80info.h"
#include "memview.h"

#define ERROR_CAPTION     "TIKI-100_emul error"
#define STATUSBAR_HEIGHT  19
#define TOOLBAR_HEIGHT    30

/* protos */
int WINAPI WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode);
static LRESULT CALLBACK WindowFunc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK DialogFunc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static void update (int x, int y, int w, int h);
static void draw3dBox (int x, int y, int w, int h);
static void getDiskImage (int drive);
static void saveDiskImage (int drive);
static void setParam (HANDLE portHandle, struct serParams *params);
static void toggleFullscreen (void);

/* variabler */

static byte keyTable[256] = {
  /* 0x00 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x08 */ KEY_SLETT, KEY_BRYT, KEY_NONE, KEY_NONE, KEY_NONE, KEY_CR, KEY_NONE, KEY_NONE, 
  /* 0x10 */ KEY_SHIFT, KEY_CTRL, KEY_NONE, KEY_NONE, KEY_LOCK, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x18 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_ANGRE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x20 */ KEY_SPACE, KEY_PGUP, KEY_PGDOWN, KEY_TABRIGHT, KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, 
  /* 0x28 */ KEY_DOWN, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_UTVID, KEY_TABLEFT, KEY_NONE, 
  /* 0x30 */ '0', '1', '2', '3', '4', '5', '6', '7',
  /* 0x38 */ '8', '9', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  /* 0x40 */ KEY_NONE, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
  /* 0x48 */ 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
  /* 0x50 */ 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
  /* 0x58 */ 'x', 'y', 'z', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  /* 0x60 */ KEY_NUM0, KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_NUM5, KEY_NUM6, KEY_NUM7, 
  /* 0x68 */ KEY_NUM8, KEY_NUM9, KEY_NUMMULT, KEY_NUMPLUS, KEY_NONE, KEY_NUMMINUS, KEY_NUMDOT, KEY_NUMDIV, 
  /* 0x70 */ KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_NONE, KEY_HJELP, 
  /* 0x78 */ KEY_ENTER, KEY_NONE, KEY_NUMPERCENT, KEY_NUMEQU, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x80 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x88 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x90 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0x98 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xA0 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xA8 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xB0 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xB8 */ KEY_NONE, KEY_NONE, '^', '+', ',', '-', '.', '\'', 
  /* 0xC0 */ 0xF8, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xC8 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xD0 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xD8 */ KEY_NONE, KEY_NONE, KEY_NONE, '@', KEY_GRAFIKK, 0xE5, 0xE6, KEY_NONE, 
  /* 0xE0 */ KEY_NONE, KEY_NONE, '<', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xE8 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xF0 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  /* 0xF8 */ KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE
};

static HWND hwnd;
static HINSTANCE appInst;
static HDC memdc;
static HBITMAP hbit;
static byte screen[1024*256];
static COLORREF colors[16];
static char defaultDir[256];
static byte pressedKeys[256];
static byte *dsk[2];
static DWORD fileSize[2];
static int resolution;
static tiki_bool slowDown = TRUE;
static tiki_bool scanlines = FALSE;
static tiki_bool st28b = FALSE;
static int width, height;
static int size40x = 1, size80x = 1;
static unsigned int xmin = (unsigned)~0;
static unsigned int ymin = (unsigned)~0;
static unsigned int xmax = 0;
static unsigned int ymax = 0;
static tiki_bool updateWindow = FALSE;
static tiki_bool lock = FALSE;
static tiki_bool grafikk = FALSE;
static tiki_bool disk[2] = {FALSE, FALSE};
static HANDLE port1;
static HANDLE port2;
static HANDLE port3;
static char port1Name[256] = "COM1";
static char port2Name[256] = "COM2";
static char port3Name[256] = "LPT1";
static HWND hwndZ80;
static HWND hwndZ80Info;
static HWND hwndMem;
static HWND hwndMemInfo;
static HWND hwndSpeedToggle;
static HWND hwndFullscreen;
static HWND hwndScreenshot;
static HWND hwndFps;
static HWND hwndTooltip;
static tiki_bool isFullscreen = FALSE;
static tiki_bool showFps = FALSE;
static int currentFps = 0;
static int frameCount = 0;
static LARGE_INTEGER fpsLastTime;
static LARGE_INTEGER fpsFreq;
static WINDOWPLACEMENT wpPrev;
static LONG savedStyle;
static int keyHoldFrames[256]; /* frames remaining for key press in fast mode */

/*****************************************************************************/

int WINAPI WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode) {
  char szWinName[] = "TIKIWin";
  WNDCLASSEX wcl;
  int consoleEnabled = 0;

  /* optional console for debugging */
  if (strstr(lpszArgs, "-console")) {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    consoleEnabled = 1;
  }

  logInit(consoleEnabled);
  LOG_I("TIKI-100_emul v1.2.0 starting");
  LOG_T("Command line: %s", lpszArgs);

  appInst = hThisInst;

  InitCommonControls();

  /* lag og registrer vindusklasse */
  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.hInstance = hThisInst;
  wcl.lpszClassName = szWinName;
  wcl.lpfnWndProc = WindowFunc;
  wcl.style = 0;
  wcl.hIcon = LoadIcon (appInst, "icon");
  wcl.hIconSm = NULL;
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW);
  wcl.lpszMenuName = "menu";
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
  if (!RegisterClassEx (&wcl)) {
    LOG_E("Failed to register window class");
    return 0;
  }

  LOG_T("Window class registered");

  /* create window */
  hwnd = CreateWindow (szWinName, "TIKI-100 Emulator", WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, 
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_DESKTOP, NULL, hThisInst, NULL);
  changeRes (MEDRES);
  ShowWindow (hwnd, nWinMode);

  /* initialiser arrays */
  memset (pressedKeys, 1, 256);
  memset (keyHoldFrames, 0, sizeof(keyHoldFrames));
  memset (screen, 0, 1024*256);

  /* finn katalog */
  GetCurrentDirectory (256, defaultDir);
  LOG_T("Working directory: %s", defaultDir);
  
  /* run emulator */
  if (!runEmul()) {
    LOG_E("ROM file not found");
    MessageBox (hwnd, "ROM file not found!", ERROR_CAPTION, MB_OK);
  }

  /* avslutt */
  free (dsk[0]);
  free (dsk[1]);
  if (port1) CloseHandle (port1);
  if (port2) CloseHandle (port2);
  if (port3) CloseHandle (port3);

  LOG_I("TIKI-100_emul shutting down");

  return 0;
}

static LRESULT CALLBACK WindowFunc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      { /* set up memdc */
        HDC hdc = GetDC (hwnd);
        memdc = CreateCompatibleDC (hdc);
        hbit = CreateCompatibleBitmap (hdc, 1024, 1024 + TOOLBAR_HEIGHT + STATUSBAR_HEIGHT);
        SelectObject (memdc, hbit);
        SelectObject (memdc, GetStockObject (BLACK_BRUSH));
        PatBlt (memdc, 0, 0, 1024, 1024 + TOOLBAR_HEIGHT + STATUSBAR_HEIGHT, PATCOPY);
        /* toolbar background */
        SelectObject (memdc, GetSysColorBrush (COLOR_3DFACE));
        PatBlt (memdc, 0, 0, 1024, TOOLBAR_HEIGHT, PATCOPY);
        ReleaseDC (hwnd, hdc);
        /* create Z80 info button (owner-drawn icon) */
        hwndZ80 = CreateWindow ("BUTTON", NULL,
          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
          4, 3, 24, 24,
          hwnd, (HMENU)IDM_Z80INFO, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        /* create memory viewer button (owner-drawn icon) */
        hwndMem = CreateWindow ("BUTTON", NULL,
          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
          32, 3, 24, 24,
          hwnd, (HMENU)IDM_MEMVIEW, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        /* create fullscreen button (owner-drawn icon) */
        hwndFullscreen = CreateWindow ("BUTTON", NULL,
          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
          60, 3, 24, 24,
          hwnd, (HMENU)IDM_FULLSCREEN, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        /* create tooltip for fullscreen button */
        hwndTooltip = CreateWindowEx (0, TOOLTIPS_CLASS, NULL,
          WS_POPUP | TTS_ALWAYSTIP,
          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
          hwnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        {
          TOOLINFO ti = {0};
          ti.cbSize = sizeof (TOOLINFO);
          ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
          ti.hwnd = hwnd;
          ti.uId = (UINT_PTR)hwndFullscreen;
          ti.lpszText = "Full screen (F12) - ESC to exit";
          SendMessage (hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
        }
        /* create screenshot button (owner-drawn icon) */
        hwndScreenshot = CreateWindow ("BUTTON", NULL,
          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
          88, 3, 24, 24,
          hwnd, (HMENU)IDM_SCREENSHOT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        /* tooltip for screenshot button */
        {
          TOOLINFO ti = {0};
          ti.cbSize = sizeof (TOOLINFO);
          ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
          ti.hwnd = hwnd;
          ti.uId = (UINT_PTR)hwndScreenshot;
          ti.lpszText = "Copy screenshot to clipboard (F11)";
          SendMessage (hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
        }
        /* create FPS toggle button (owner-drawn icon) */
        hwndFps = CreateWindow ("BUTTON", NULL,
          WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
          116, 3, 24, 24,
          hwnd, (HMENU)IDM_FPS, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        /* create speed toggle button */
        hwndSpeedToggle = CreateWindow ("BUTTON", "Limit speed",
          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
          146, 6, 100, 18,
          hwnd, (HMENU)IDM_SPEED_TOGGLE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage (hwndSpeedToggle, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        SendMessage (hwndSpeedToggle, BM_SETCHECK, BST_CHECKED, 0);
        /* tooltip for FPS button */
        {
          TOOLINFO ti = {0};
          ti.cbSize = sizeof (TOOLINFO);
          ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
          ti.hwnd = hwnd;
          ti.uId = (UINT_PTR)hwndFps;
          ti.lpszText = "Toggle FPS display (F10)";
          SendMessage (hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
        }
        /* tooltip for Z80 info button */
        {
          TOOLINFO ti = {0};
          ti.cbSize = sizeof (TOOLINFO);
          ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
          ti.hwnd = hwnd;
          ti.uId = (UINT_PTR)hwndZ80;
          ti.lpszText = "Z80 CPU information";
          SendMessage (hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
        }
        /* tooltip for memory viewer button */
        {
          TOOLINFO ti = {0};
          ti.cbSize = sizeof (TOOLINFO);
          ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
          ti.hwnd = hwnd;
          ti.uId = (UINT_PTR)hwndMem;
          ti.lpszText = "Memory viewer";
          SendMessage (hwndTooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
        }
        /* init FPS counter */
        QueryPerformanceFrequency (&fpsFreq);
        QueryPerformanceCounter (&fpsLastTime);
        wpPrev.length = sizeof (WINDOWPLACEMENT);
        LOG_T("Window created, memdc initialized");
      }
      break;
    case WM_DRAWITEM:
      {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == IDM_Z80INFO) {
          RECT rc = dis->rcItem;
          UINT edge = (dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0;
          DrawFrameControl (dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | edge);
          SetBkMode (dis->hDC, TRANSPARENT);
          SetTextColor (dis->hDC, GetSysColor (COLOR_BTNTEXT));
          HFONT font = CreateFont (12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
          HFONT oldFont = SelectObject (dis->hDC, font);
          DrawText (dis->hDC, "Z80", 3, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
          SelectObject (dis->hDC, oldFont);
          DeleteObject (font);
        }
        if (dis->CtlID == IDM_MEMVIEW) {
          RECT rc = dis->rcItem;
          UINT edge = (dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0;
          DrawFrameControl (dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | edge);
          SetBkMode (dis->hDC, TRANSPARENT);
          SetTextColor (dis->hDC, GetSysColor (COLOR_BTNTEXT));
          HFONT font = CreateFont (11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
          HFONT oldFont = SelectObject (dis->hDC, font);
          DrawText (dis->hDC, "MEM", 3, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
          SelectObject (dis->hDC, oldFont);
          DeleteObject (font);
        }
        if (dis->CtlID == IDM_FULLSCREEN) {
          RECT rc = dis->rcItem;
          UINT edge = (dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0;
          DrawFrameControl (dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | edge);
          /* draw fullscreen icon: a rectangle with arrows pointing to corners */
          int cx = (rc.left + rc.right) / 2;
          int cy = (rc.top + rc.bottom) / 2;
          int s = 6; /* half-size of the icon */
          HPEN pen = CreatePen (PS_SOLID, 2, GetSysColor (COLOR_BTNTEXT));
          HPEN oldPen = SelectObject (dis->hDC, pen);
          /* outer rectangle */
          MoveToEx (dis->hDC, cx - s, cy - s, NULL); LineTo (dis->hDC, cx + s, cy - s);
          LineTo (dis->hDC, cx + s, cy + s); LineTo (dis->hDC, cx - s, cy + s);
          LineTo (dis->hDC, cx - s, cy - s);
          /* top-left corner arrow */
          MoveToEx (dis->hDC, cx - s, cy - s + 4, NULL); LineTo (dis->hDC, cx - s, cy - s);
          LineTo (dis->hDC, cx - s + 4, cy - s);
          /* top-right corner arrow */
          MoveToEx (dis->hDC, cx + s - 4, cy - s, NULL); LineTo (dis->hDC, cx + s, cy - s);
          LineTo (dis->hDC, cx + s, cy - s + 4);
          /* bottom-right corner arrow */
          MoveToEx (dis->hDC, cx + s, cy + s - 4, NULL); LineTo (dis->hDC, cx + s, cy + s);
          LineTo (dis->hDC, cx + s - 4, cy + s);
          /* bottom-left corner arrow */
          MoveToEx (dis->hDC, cx - s + 4, cy + s, NULL); LineTo (dis->hDC, cx - s, cy + s);
          LineTo (dis->hDC, cx - s, cy + s - 4);
          SelectObject (dis->hDC, oldPen);
          DeleteObject (pen);
        }
        if (dis->CtlID == IDM_SCREENSHOT) {
          RECT rc = dis->rcItem;
          UINT edge = (dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0;
          DrawFrameControl (dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | edge);
          /* draw camera icon */
          int cx = (rc.left + rc.right) / 2;
          int cy = (rc.top + rc.bottom) / 2;
          HPEN pen = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_BTNTEXT));
          HBRUSH brush = CreateSolidBrush (GetSysColor (COLOR_BTNTEXT));
          HPEN oldPen = SelectObject (dis->hDC, pen);
          HBRUSH oldBrush = SelectObject (dis->hDC, GetStockObject (NULL_BRUSH));
          /* camera body */
          Rectangle (dis->hDC, cx - 7, cy - 3, cx + 7, cy + 6);
          /* lens (circle) */
          Ellipse (dis->hDC, cx - 3, cy - 1, cx + 3, cy + 5);
          /* viewfinder bump */
          SelectObject (dis->hDC, brush);
          Rectangle (dis->hDC, cx - 2, cy - 5, cx + 2, cy - 3);
          SelectObject (dis->hDC, oldBrush);
          SelectObject (dis->hDC, oldPen);
          DeleteObject (pen);
          DeleteObject (brush);
        }
        if (dis->CtlID == IDM_FPS) {
          RECT rc = dis->rcItem;
          UINT edge = (dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0;
          DrawFrameControl (dis->hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | edge);
          /* draw "FPS" text as icon */
          SetBkMode (dis->hDC, TRANSPARENT);
          SetTextColor (dis->hDC, showFps ? RGB(0, 180, 0) : GetSysColor (COLOR_BTNTEXT));
          HFONT font = CreateFont (11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
          HFONT oldFont = SelectObject (dis->hDC, font);
          DrawText (dis->hDC, "FPS", 3, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
          SelectObject (dis->hDC, oldFont);
          DeleteObject (font);
        }
      }
      break;
    case WM_PAINT:
      { /* kopier fra memdc til hdc */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hwnd, &ps);
        if (isFullscreen) {
          /* scale emulator area to fill screen, centered with black bars */
          RECT cr;
          GetClientRect (hwnd, &cr);
          int screenW = cr.right - cr.left;
          int screenH = cr.bottom - cr.top;
          int scaleW = screenW / width;
          int scaleH = screenH / height;
          int scale = (scaleW < scaleH) ? scaleW : scaleH;
          if (scale < 1) scale = 1;
          int dstW = width * scale;
          int dstH = height * scale;
          int dstX = (screenW - dstW) / 2;
          int dstY = (screenH - dstH) / 2;
          /* black bars */
          if (dstY > 0) {
            RECT top = {0, 0, screenW, dstY};
            FillRect (hdc, &top, (HBRUSH)GetStockObject (BLACK_BRUSH));
            RECT bot = {0, dstY + dstH, screenW, screenH};
            FillRect (hdc, &bot, (HBRUSH)GetStockObject (BLACK_BRUSH));
          }
          if (dstX > 0) {
            RECT lft = {0, dstY, dstX, dstY + dstH};
            FillRect (hdc, &lft, (HBRUSH)GetStockObject (BLACK_BRUSH));
            RECT rgt = {dstX + dstW, dstY, screenW, dstY + dstH};
            FillRect (hdc, &rgt, (HBRUSH)GetStockObject (BLACK_BRUSH));
          }
          SetStretchBltMode (hdc, COLORONCOLOR);
          StretchBlt (hdc, dstX, dstY, dstW, dstH,
                      memdc, 0, TOOLBAR_HEIGHT, width, height, SRCCOPY);
        } else {
          BitBlt (hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, 
                  ps.rcPaint.bottom - ps.rcPaint.top, memdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        }
        /* FPS overlay */
        if (showFps) {
          char fpsText[16];
          snprintf (fpsText, sizeof (fpsText), "FPS: %d", currentFps);
          SetBkMode (hdc, TRANSPARENT);
          SetTextColor (hdc, RGB (0, 255, 0));
          HFONT font = CreateFont (16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
          HFONT oldFont = SelectObject (hdc, font);
          if (isFullscreen) {
            RECT cr;
            GetClientRect (hwnd, &cr);
            int screenW = cr.right - cr.left;
            int scaleW = screenW / width;
            int scaleH = (cr.bottom - cr.top) / height;
            int scale = (scaleW < scaleH) ? scaleW : scaleH;
            if (scale < 1) scale = 1;
            int dstW = width * scale;
            int dstX = (screenW - dstW) / 2;
            TextOut (hdc, dstX + dstW - 80, 5, fpsText, strlen (fpsText));
          } else {
            TextOut (hdc, width - 80, TOOLBAR_HEIGHT + 5, fpsText, strlen (fpsText));
          }
          SelectObject (hdc, oldFont);
          DeleteObject (font);
        }
        EndPaint (hwnd, &ps);
      }
      break;
    case WM_SIZE:
      /* statusbar */
      SelectObject (memdc, GetSysColorBrush (COLOR_3DFACE));
      PatBlt (memdc, 0, TOOLBAR_HEIGHT + height, width, STATUSBAR_HEIGHT, PATCOPY);
      
      /* tegn småbokser */
      draw3dBox (20, TOOLBAR_HEIGHT + height + 5, 9, 9);
      draw3dBox (40, TOOLBAR_HEIGHT + height + 5, 9, 9);
      draw3dBox (60, TOOLBAR_HEIGHT + height + 5, 9, 9);
      draw3dBox (80, TOOLBAR_HEIGHT + height + 5, 9, 9);
      
      /* oppdater diodelys */
      lockLight (lock);
      grafikkLight (grafikk);
      diskLight (0, disk[0]);
      diskLight (1, disk[1]);
      break;
    case WM_DESTROY:
      PostQuitMessage (0);
      break;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE && isFullscreen) {
        toggleFullscreen();
      } else if (wParam == VK_F12) {
        toggleFullscreen();
      } else if (wParam == VK_F11) {
        copyScreenshot (hwnd, memdc, 0, TOOLBAR_HEIGHT, width, height);
      } else {
        pressedKeys[keyTable[(byte)wParam]] = 0;
      }
      break;
    case WM_SYSKEYDOWN:
      if (wParam == VK_F10) {
        showFps = !showFps;
        if (showFps) QueryPerformanceCounter (&fpsLastTime);
        frameCount = 0;
        currentFps = 0;
        InvalidateRect (hwndFps, NULL, FALSE);
        InvalidateRect (hwnd, NULL, FALSE);
        LOG_I("FPS display %s", showFps ? "enabled" : "disabled");
        return 0; /* prevent menu activation */
      }
      return DefWindowProc (hwnd, msg, wParam, lParam);
    case WM_KEYUP:
      pressedKeys[keyTable[(byte)wParam]] = 1;
      break;
    case WM_COMMAND:
      /* return focus to main window after toolbar button clicks */
      if (HIWORD (wParam) == BN_CLICKED && (HWND)lParam != NULL) {
        SetFocus (hwnd);
      }
      switch (LOWORD (wParam)) {
        case IDM_RESET:
          LOG_I("Emulator reset");
          resetEmul();
          break;
        case IDM_INNSTILLINGER:
          LOG_T("Opening settings dialog");
          DialogBox (appInst, "dialog", hwnd, (DLGPROC)DialogFunc);
          break;
        case IDM_OM:
          MessageBox (hwnd, 
                      "TIKI-100_emul v1.2.0 by Arctic Retro\n"
                      "A freeware TIKI 100 Rev. C emulator.\n"
                      "Z80 emulation copyright (C) Marat Fayzullin 1994,1995,1996,1997.\n"
                      "Original code copyright (C) Asbjorn Djupdal 2000-2001.", 
                      "About TIKI-100_emul",
                      MB_OK);
          break;
        case IDM_HELP:
          MessageBox (hwnd,
                      "Keyboard shortcuts:\n\n"
                      "F10\tToggle FPS display\n"
                      "F11\tCopy screenshot to clipboard\n"
                      "F12\tToggle full screen\n"
                      "ESC\tExit full screen\n\n"
                      "Status bar lights (bottom left):\n\n"
                      "1\tLOCK key active\n"
                      "2\tGRAFIKK (graphics) mode active\n"
                      "3\tDrive A activity\n"
                      "4\tDrive B activity\n",
                      "TIKI-100 Emulator - Keyboard shortcuts",
                      MB_OK | MB_ICONINFORMATION);
          break;
        case IDM_AVSLUTT:
          PostQuitMessage (0);
          break;
        case IDM_HENT_A:
          getDiskImage (0);
          break;
        case IDM_HENT_B:
          getDiskImage (1);
          break;
        case IDM_LAGRE_A:
          saveDiskImage (0);
          break;
        case IDM_LAGRE_B:
          saveDiskImage (1);
          break;
        case IDM_FJERN_A:
          EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), IDM_LAGRE_A, MF_BYCOMMAND | MF_GRAYED);
          removeDisk (0);
          break;
        case IDM_FJERN_B:
          EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), IDM_LAGRE_B, MF_BYCOMMAND | MF_GRAYED);
          removeDisk (1);
          break;
        case IDM_Z80INFO:
          if (hwndZ80Info && IsWindow (hwndZ80Info)) {
            SetForegroundWindow (hwndZ80Info);
          } else {
            WNDCLASS wc = {0};
            wc.lpfnWndProc = Z80InfoWndProc;
            wc.hInstance = appInst;
            wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
            wc.lpszClassName = "Z80InfoClass";
            wc.hCursor = LoadCursor (NULL, IDC_ARROW);
            RegisterClass (&wc);
            z80InfoSetHwndPtr (&hwndZ80Info);
            hwndZ80Info = CreateWindowEx (WS_EX_TOOLWINDOW, "Z80InfoClass", "Z80 Information",
              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
              CW_USEDEFAULT, CW_USEDEFAULT, 280, 320,
              hwnd, NULL, appInst, NULL);
            ShowWindow (hwndZ80Info, SW_SHOW);
          }
          break;
        case IDM_MEMVIEW:
          if (hwndMemInfo && IsWindow (hwndMemInfo)) {
            SetForegroundWindow (hwndMemInfo);
          } else {
            WNDCLASS wc = {0};
            wc.lpfnWndProc = MemViewWndProc;
            wc.hInstance = appInst;
            wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
            wc.lpszClassName = "MemViewClass";
            wc.hCursor = LoadCursor (NULL, IDC_ARROW);
            RegisterClass (&wc);
            memViewSetHwndPtr (&hwndMemInfo);
            {
              /* compute window size to fit 32 lines + toolbar exactly */
              RECT rc = {0, 0, 620, MEMVIEW_TOOLBAR + 2 + MEMVIEW_LINES * 14};
              AdjustWindowRectEx (&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL,
                FALSE, WS_EX_TOOLWINDOW);
              hwndMemInfo = CreateWindowEx (WS_EX_TOOLWINDOW, "MemViewClass", "Memory Viewer",
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL,
                CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                hwnd, NULL, appInst, NULL);
            }
            ShowWindow (hwndMemInfo, SW_SHOW);
          }
          break;
        case IDM_SPEED_TOGGLE:
          slowDown = SendMessage (hwndSpeedToggle, BM_GETCHECK, 0, 0);
          LOG_I("Speed limit %s", slowDown ? "enabled" : "disabled");
          break;
        case IDM_FULLSCREEN:
          toggleFullscreen();
          break;
        case IDM_SCREENSHOT:
          copyScreenshot (hwnd, memdc, 0, TOOLBAR_HEIGHT, width, height);
          break;
        case IDM_FPS:
          showFps = !showFps;
          if (showFps) QueryPerformanceCounter (&fpsLastTime);
          frameCount = 0;
          currentFps = 0;
          InvalidateRect (hwndFps, NULL, FALSE);
          InvalidateRect (hwnd, NULL, FALSE);
          LOG_I("FPS display %s", showFps ? "enabled" : "disabled");
          break;
#ifdef DEBUG
        case IDM_MONITOR:
          trace();
          break;
#endif
      }
    default:
      return DefWindowProc (hwnd, msg, wParam, lParam);
  }
  return 0;
}
/* tegner en 3d-boks */
static void draw3dBox (int x, int y, int w, int h) {
  HGDIOBJ obj;
  HPEN pen1 = CreatePen (PS_SOLID, 0, GetSysColor (COLOR_3DDKSHADOW));
  HPEN pen2 = CreatePen (PS_SOLID, 0, GetSysColor (COLOR_3DHILIGHT));

  MoveToEx (memdc, x, y + h - 1, NULL);
  obj = SelectObject (memdc, pen1);
  LineTo (memdc, x, y);
  LineTo (memdc, x + w - 1, y);
  SelectObject (memdc, pen2);
  LineTo (memdc, x + w - 1, y + h - 1);
  LineTo (memdc, x, y + h - 1);

  SelectObject (memdc, obj);
  DeleteObject (pen2);
  DeleteObject (pen1);
}
static BOOL CALLBACK DialogFunc (HWND hdwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  static HWND ud40Wnd, ud80Wnd;

  switch (message) {
    case WM_INITDIALOG:
      /* oppdater alle felter */
      ud40Wnd = CreateUpDownControl (WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
                                     48, 52, 11, 14, hdwnd, IDD_40SPIN, appInst, GetDlgItem (hdwnd, IDD_40EDIT),
                                     4, 1, size40x);
      ud80Wnd = CreateUpDownControl (WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
                                     48, 71, 11, 14, hdwnd, IDD_80SPIN, appInst, GetDlgItem (hdwnd, IDD_80EDIT),
                                     2, 1, size80x);
      if (slowDown) SendDlgItemMessage (hdwnd, IDD_HASTIGHET, BM_SETCHECK, 1, 0);
      if (scanlines) SendDlgItemMessage (hdwnd, IDD_BEVARFORHOLD, BM_SETCHECK, 1, 0);
      if (st28b) SendDlgItemMessage (hdwnd, IDD_ST28B, BM_SETCHECK, 1, 0);
      if (port1) SendDlgItemMessage (hdwnd, IDD_P1, BM_SETCHECK, 1, 0);
      if (port2) SendDlgItemMessage (hdwnd, IDD_P2, BM_SETCHECK, 1, 0);
      if (port3) SendDlgItemMessage (hdwnd, IDD_P3, BM_SETCHECK, 1, 0);
      SendDlgItemMessage (hdwnd, IDD_P1EDIT, WM_SETTEXT, 0, (LPARAM)port1Name);
      SendDlgItemMessage (hdwnd, IDD_P2EDIT, WM_SETTEXT, 0, (LPARAM)port2Name);
      SendDlgItemMessage (hdwnd, IDD_P3EDIT, WM_SETTEXT, 0, (LPARAM)port3Name);
      break;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDD_OK: {
          int s40x, s80x;
          tiki_bool sl;
          tiki_bool st;
          char p1[256];
          char p2[256];
          char p3[256];
          
          /* begrense hastighet */
          slowDown = SendDlgItemMessage (hdwnd, IDD_HASTIGHET, BM_GETCHECK, 0, 0);
          SendMessage (hwndSpeedToggle, BM_SETCHECK, slowDown ? BST_CHECKED : BST_UNCHECKED, 0);

          /* forandre vindus-størrelse */
          s40x = SendMessage (ud40Wnd, UDM_GETPOS, 0, 0);
          s80x = SendMessage (ud80Wnd, UDM_GETPOS, 0, 0);
          sl = SendDlgItemMessage (hdwnd, IDD_BEVARFORHOLD, BM_GETCHECK, 0, 0);
          if (s40x != size40x || s80x != size80x || sl != scanlines) {
            size40x = s40x; size80x = s80x; scanlines = sl;
            changeRes (resolution);
            resetEmul();
          }

          /* sett ST 28 b */
          st = SendDlgItemMessage (hdwnd, IDD_ST28B, BM_GETCHECK, 0, 0);
          if (st != st28b) {
            st28b = st;
            setST28b (st28b);
          }

          /* skaff nye portnavn */
          SendDlgItemMessage (hdwnd, IDD_P1EDIT, WM_GETTEXT, 256, (LPARAM)p1);
          SendDlgItemMessage (hdwnd, IDD_P2EDIT, WM_GETTEXT, 256, (LPARAM)p2);
          SendDlgItemMessage (hdwnd, IDD_P3EDIT, WM_GETTEXT, 256, (LPARAM)p3);

          /* åpne/lukke porter */
          if (SendDlgItemMessage (hdwnd, IDD_P1, BM_GETCHECK, 0, 0)) {
            /* port 1 */
            if (!port1 || strcmp (p1, port1Name)) {
              if (port1) CloseHandle (port1);
              SetCurrentDirectory (defaultDir);
              if ((port1 = CreateFile(p1, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL)) == 
                  INVALID_HANDLE_VALUE) {
                MessageBox (hwnd, "Specified name for P1 not available!", ERROR_CAPTION, MB_OK);
                LOG_E("Failed to open port P1: %s", p1);
                port1 = 0;
              }
            }
          } else if (port1) {
            CloseHandle (port1);
            port1 = 0;
          }
          if (SendDlgItemMessage (hdwnd, IDD_P2, BM_GETCHECK, 0, 0)) {
            /* port 2 */
            if (!port2 || strcmp (p2, port2Name)) {
              if (port2) CloseHandle (port2);
              SetCurrentDirectory (defaultDir);
              if ((port2 = CreateFile(p2, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL)) == 
                  INVALID_HANDLE_VALUE) {
                MessageBox (hwnd, "Specified name for P2 not available!", ERROR_CAPTION, MB_OK);
                LOG_E("Failed to open port P2: %s", p2);
                port2 = 0;
              }
            }
          } else if (port2) {
            CloseHandle (port2);
            port2 = 0;
          }
          if (SendDlgItemMessage (hdwnd, IDD_P3, BM_GETCHECK, 0, 0)) {
            /* port 3 */
            if (!port3 || strcmp (p3, port3Name)) {
              if (port3) CloseHandle (port3);
              SetCurrentDirectory (defaultDir);
              if ((port3 = CreateFile(p3, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL)) == 
                  INVALID_HANDLE_VALUE) {
                MessageBox (hwnd, "Specified name for P3 not available!", ERROR_CAPTION, MB_OK);
                LOG_E("Failed to open port P3: %s", p3);
                port3 = 0;
              }
            }
          } else if (port3) {
            CloseHandle (port3);
            port3 = 0;
          }

          /* ta vare på portnavn */
          strcpy (port1Name, p1);
          strcpy (port2Name, p2);
          strcpy (port3Name, p3);
        }
        case 2: /* lukkeknapp for dialogboks, vet ikke makronavnet for den */
        case IDD_AVBRYT:
          EndDialog (hdwnd, 0);
          break;
      }
      break;
  }
  return 0;
}
/* hent inn diskbilde fra fil */
static void getDiskImage (int drive) {
  OPENFILENAME fname;
  char fn[256] = "\0";
  HANDLE hFile;

  memset (&fname, 0, sizeof (OPENFILENAME));
  fname.lStructSize = sizeof (OPENFILENAME);
  fname.hwndOwner = hwnd;
  fname.lpstrFile = fn;
  fname.nMaxFile = sizeof (fn);
  fname.Flags = OFN_FILEMUSTEXIST;
  
  if (GetOpenFileName (&fname)) {
    if ((hFile = CreateFile (fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
      fileSize[drive] = GetFileSize (hFile, NULL);
      free (dsk[drive]);
      if ((dsk[drive] = (byte *)malloc (fileSize[drive]))) {
        DWORD dwRead;
        if (ReadFile (hFile, dsk[drive], fileSize[drive], &dwRead, NULL)) {
          switch (fileSize[drive]) {
            case 40*1*18*128:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 40, 1, 18, 128);
              LOG_I("Loaded disk %c: 40x1x18x128", 'A' + drive);
              break;
            case 40*1*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 40, 1, 10, 512);
              LOG_I("Loaded disk %c: 40x1x10x512", 'A' + drive);
              break;
            case 40*2*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 40, 2, 10, 512);
              LOG_I("Loaded disk %c: 40x2x10x512", 'A' + drive);
              break;
            case 80*2*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 80, 2, 10, 512);
              LOG_I("Loaded disk %c: 80x2x10x512", 'A' + drive);
              break;
            default:
              LOG_W("Unsupported disk image size: %lu bytes", fileSize[drive]);
              removeDisk (drive);
              fileSize[drive] = 0;
          }
        }
      }
      CloseHandle (hFile);
    }  
  }
}
/* lagre diskbilde til fil */
static void saveDiskImage (int drive) {
  OPENFILENAME fname;
  char fn[256] = "\0";
  HANDLE hFile;

  memset (&fname, 0, sizeof (OPENFILENAME));
  fname.lStructSize = sizeof (OPENFILENAME);
  fname.hwndOwner = hwnd;
  fname.lpstrFile = fn;
  fname.nMaxFile = sizeof (fn);
  fname.Flags = OFN_HIDEREADONLY;

  if (GetSaveFileName (&fname)) {
    if ((hFile = CreateFile (fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
        != INVALID_HANDLE_VALUE) {
      DWORD dwWritten;
      WriteFile (hFile, dsk[drive], fileSize[drive], &dwWritten, NULL);
      CloseHandle (hFile);
    }  
  }
}
/* Toggle fullscreen mode */
static void toggleFullscreen (void) {
  if (!isFullscreen) {
    MONITORINFO mi = { sizeof (MONITORINFO) };
    if (GetWindowPlacement (hwnd, &wpPrev) &&
        GetMonitorInfo (MonitorFromWindow (hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
      savedStyle = GetWindowLong (hwnd, GWL_STYLE);
      SetWindowLong (hwnd, GWL_STYLE, savedStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_OVERLAPPEDWINDOW));
      SetMenu (hwnd, NULL);
      ShowWindow (hwndZ80, SW_HIDE);
      ShowWindow (hwndMem, SW_HIDE);
      ShowWindow (hwndSpeedToggle, SW_HIDE);
      ShowWindow (hwndFullscreen, SW_HIDE);
      ShowWindow (hwndScreenshot, SW_HIDE);
      ShowWindow (hwndFps, SW_HIDE);
      SetWindowPos (hwnd, HWND_TOP,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      isFullscreen = TRUE;
      LOG_I("Entered fullscreen");
    }
  } else {
    SetWindowLong (hwnd, GWL_STYLE, savedStyle);
    SetMenu (hwnd, LoadMenu (appInst, "menu"));
    ShowWindow (hwndZ80, SW_SHOW);
    ShowWindow (hwndMem, SW_SHOW);
    ShowWindow (hwndSpeedToggle, SW_SHOW);
    ShowWindow (hwndFullscreen, SW_SHOW);
    ShowWindow (hwndScreenshot, SW_SHOW);
    ShowWindow (hwndFps, SW_SHOW);
    SetWindowPlacement (hwnd, &wpPrev);
    SetWindowPos (hwnd, NULL, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    isFullscreen = FALSE;
    LOG_I("Exited fullscreen");
  }
  if (!isFullscreen) {
    /* redraw toolbar area */
    SelectObject (memdc, GetSysColorBrush (COLOR_3DFACE));
    PatBlt (memdc, 0, 0, 1024, TOOLBAR_HEIGHT, PATCOPY);
  }
  /* force full repaint */
  InvalidateRect (hwnd, NULL, FALSE);
}
/* Forandre oppløsning */
void changeRes (int newRes) {
  LOG_I("Changing resolution to %s", newRes == HIGHRES ? "high" : newRes == MEDRES ? "medium" : "low");
  RECT windowRect;

  switch (newRes) {
    case HIGHRES:
      width = 1024;
      height = 256 * (scanlines ? 4 : 1);
      break;
    case MEDRES:
      width = 512 * size80x;
      height = 256 * size80x * (scanlines ? 2 : 1);
      break;
    case LOWRES: 
    default:
      width = 256 * size40x;
      height = 256 * size40x;
      break;
  }
  if (!isFullscreen) {
    GetWindowRect (hwnd, &windowRect);
    MoveWindow (hwnd, windowRect.left, windowRect.top, width + 2 * GetSystemMetrics (SM_CXFIXEDFRAME), 
                height + TOOLBAR_HEIGHT + 2 * GetSystemMetrics (SM_CYFIXEDFRAME) + GetSystemMetrics (SM_CYCAPTION) +
                GetSystemMetrics (SM_CYMENU) + STATUSBAR_HEIGHT, 1);
  }
  /* slett bakgrunn */
  SelectObject (memdc, GetStockObject (BLACK_BRUSH));
  PatBlt (memdc, 0, TOOLBAR_HEIGHT, width, height, PATCOPY);
  if (newRes != resolution) {
    memset (screen, 0, 1024*256);
    resolution = newRes;
  }
  /* oppdater vindu */
  InvalidateRect (hwnd, NULL, FALSE);
}
/* Plotter en pixel med farge tatt fra pallett */
void plotPixel (int x, int y, int color) {
  screen[x+y*1024] = color;
  switch (resolution) {
    case HIGHRES: 
      y *= scanlines ? 4 : 1;
      break;
    case MEDRES:
      x *= size80x;
      y *= size80x * (scanlines ? 2 : 1);
      break;
    case LOWRES:
      x *= size40x;
      y *= size40x;
      break;
  }
  SetPixelV (memdc, x, y + TOOLBAR_HEIGHT, colors[color]);
  update (x, y + TOOLBAR_HEIGHT, 1, 1);
}
/* Scroller skjerm 'distance' linjer oppover */
void scrollScreen (int distance) {
  /* scroll screen */
  if (distance > 0) {
    memmove (screen, screen + distance * 1024, (256 - distance) * 1024);
    memset (screen + distance * 1024, 0, (256 - distance) * 1024);
  } else {
    memmove (screen + -(distance * 1024), screen, (byte)distance * 1024);
    memset (screen, 0, -distance * 1024);
  }
  /* finn avstand */
  switch (resolution) {
    case HIGHRES: 
      distance *= scanlines ? 4 : 1;
      break;
    case MEDRES:
      distance *= size80x * (scanlines ? 2 : 1);
      break;  
    case LOWRES:
    default:
      distance *= size40x;
      break;
  }
  /* scroll memdc */
  if (distance > 0) {
    BitBlt (memdc, 0, TOOLBAR_HEIGHT, width, height - distance, memdc, 0, TOOLBAR_HEIGHT + distance, SRCCOPY);
  } else {
    BitBlt (memdc, 0, TOOLBAR_HEIGHT - distance, width, height + distance, memdc, 0, TOOLBAR_HEIGHT, SRCCOPY);
  }
  SelectObject (memdc, GetStockObject (BLACK_BRUSH));
  /* slett resten */
  if (distance > 0) {
    PatBlt (memdc, 0, TOOLBAR_HEIGHT + height - distance, width, distance, PATCOPY);
  } else {
    PatBlt (memdc, 0, TOOLBAR_HEIGHT, width, -distance, PATCOPY);
  }
  /* oppdater vindu */
  InvalidateRect (hwnd, NULL, FALSE);
}
/* Ny farge, gitt pallett nummer og intensitet 0-255 */
void changePalette (int colornumber, byte red, byte green, byte blue) {
  if (colors[colornumber] != RGB (red, green, blue)) {
    int x, y;

    colors[colornumber] = RGB (red, green, blue);

    /* oppdater pixler med denne fargen */
    for (y = 0; y < 256; y++) {
      for (x = 0; x < 1024; x++) {
        if (screen[x+y*1024] == colornumber) {
          plotPixel (x, y, colornumber);
        }
      }
    }
  }
}
/* Kalles periodisk. Lar system kode måle / senke emuleringshastighet
 * Kan også brukes til sjekk av brukeraktivitet
 * ms er antall "emulerte" millisekunder siden forrige gang loopEmul ble kalt
 */
void loopEmul (int ms) {
  /* senk hastigheten */
  if (slowDown) {
    static tiki_bool firstTime = TRUE;
    static LARGE_INTEGER lastTime;
    LARGE_INTEGER currentTime;
    static LARGE_INTEGER freq;
    
    if (QueryPerformanceCounter (&currentTime)) {
      if (firstTime) {
        QueryPerformanceFrequency (&freq);
        lastTime = currentTime;
        firstTime = FALSE;
      } else {
        long sleepPeriod = ms - ((currentTime.QuadPart - lastTime.QuadPart) * 1000 / freq.QuadPart);
      
        lastTime = currentTime;
        
        if (sleepPeriod > 0) {
          Sleep (sleepPeriod);
          QueryPerformanceCounter (&lastTime);
        }
      }
    }
  }
  /* FPS counting */
  if (showFps) {
    frameCount++;
    LARGE_INTEGER now;
    QueryPerformanceCounter (&now);
    LONGLONG elapsed = now.QuadPart - fpsLastTime.QuadPart;
    if (elapsed >= fpsFreq.QuadPart) { /* one second passed */
      currentFps = (int)(frameCount * fpsFreq.QuadPart / elapsed);
      frameCount = 0;
      fpsLastTime = now;
    }
  }
  /* oppdater skjerm */
  if (updateWindow || showFps) {
    if (isFullscreen) {
      InvalidateRect (hwnd, NULL, FALSE);
    } else if (showFps) {
      /* invalidate only emulator + statusbar area, not toolbar */
      RECT rect = {0, TOOLBAR_HEIGHT, width, TOOLBAR_HEIGHT + height + STATUSBAR_HEIGHT};
      InvalidateRect (hwnd, &rect, FALSE);
    } else {
      RECT rect = {xmin, ymin, xmax, ymax};
      InvalidateRect (hwnd, &rect, FALSE);
    }
    xmin = (unsigned)~0, ymin = (unsigned)~0, xmax = 0, ymax = 0;
    updateWindow = FALSE;
  }
  /* sjekk seriekanaler */
  {
    DWORD error;
    COMSTAT comstat;

    if (port1) {
      if (ClearCommError (port1, &error, &comstat)) {
        if (comstat.cbInQue != 0) {
          charAvailable (0);
        }
      }
    }
    if (port2) {
      if (ClearCommError (port2, &error, &comstat)) {
        if (comstat.cbInQue != 0) {
          charAvailable (1);
        }
      }
    }
  }
  /* sjekk om der er events som venter */
  {
    MSG msg;
    
    while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        DeleteDC (memdc);
        DeleteObject (hbit);
        quitEmul();
      } else {
        /* TranslateMessage for memview search box (EDIT control needs WM_CHAR) */
        if (memViewIsChild (msg.hwnd))
          TranslateMessage (&msg);
        DispatchMessage (&msg);
      }
    }
  }
  /* In fast mode, poll physical key state to prevent key repeat.
   * Each physical press→release cycle gives exactly one keypress
   * held for 2 frames. Key must be physically released before it
   * can trigger again. */
  if (!slowDown) {
    int i;
    for (i = 0; i < 256; i++) {
      byte key = keyTable[i];
      if (key == KEY_NONE || key == KEY_SHIFT || key == KEY_CTRL ||
          key == KEY_LOCK || key == KEY_GRAFIKK) continue;
      if (GetAsyncKeyState (i) & 0x8000) {
        /* key physically down */
        if (keyHoldFrames[key] == 0) {
          /* fresh press - hold for 2 frames */
          pressedKeys[key] = 0;
          keyHoldFrames[key] = 2;
        } else if (keyHoldFrames[key] > 1) {
          keyHoldFrames[key]--;
        } else if (keyHoldFrames[key] == 1) {
          /* 2 frames done - release, but mark as "waiting for physical release" */
          pressedKeys[key] = 1;
          keyHoldFrames[key] = -1;
        }
        /* keyHoldFrames[key] == -1: waiting for physical release, do nothing */
      } else {
        /* key physically up - reset for next press */
        pressedKeys[key] = 1;
        keyHoldFrames[key] = 0;
      }
    }
  }
}
/* Tenn/slukk lock lys */
void lockLight (tiki_bool status) {
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, 21, TOOLBAR_HEIGHT + height + 6, 7, 7, PATCOPY);
  update (21, TOOLBAR_HEIGHT + height + 6, 7, 7);
  lock = status;
}
/* Tenn/slukk grafikk lys */
void grafikkLight (tiki_bool status) {
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, 41, TOOLBAR_HEIGHT + height + 6, 7, 7, PATCOPY);
  update (41, TOOLBAR_HEIGHT + height + 6, 7, 7);
  grafikk = status;
}
/* Tenn/slukk disk lys for gitt stasjon */
void diskLight (int drive, tiki_bool status) {
  int x = (drive ? 81 : 61);
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, x, TOOLBAR_HEIGHT + height + 6, 7, 7, PATCOPY);
  update (61, TOOLBAR_HEIGHT + height + 6, 7, 7);
  disk[drive] = status;
}
static void update (int x, int y, int w, int h) {
  if (xmin > x) xmin = x;
  if (ymin > y) ymin = y;
  if (xmax < (x + w)) xmax = x + w;
  if (ymax < (y + h)) ymax = y + h;
  updateWindow = TRUE;
}
/* Sjekk status til hver av de gitte tastene
 * Sett bit n i returkode hvis tast n IKKE er nedtrykt
 * Taster er enten ascii-kode eller en av konstantene over
 */
byte testKey (byte keys[8]) {
  return pressedKeys[keys[0]] | (pressedKeys[keys[1]] << 1) | (pressedKeys[keys[2]] << 2) |
    (pressedKeys[keys[3]] << 3) | (pressedKeys[keys[4]] << 4) | (pressedKeys[keys[5]] << 5) |
    (pressedKeys[keys[6]] << 6) | (pressedKeys[keys[7]] << 7);
}
/* setter seriekanalparametre */
void setParams (struct serParams *p1Params, struct serParams *p2Params) {
  if (port1) setParam (port1, p1Params);
  if (port2) setParam (port2, p2Params);
}
static void setParam (HANDLE portHandle, struct serParams *params) {
  DCB dcb = {sizeof (DCB),
             0,                     /* BaudRate */
             TRUE,                  /* fBinary */
             0,                     /* fParity */
             FALSE,                 /* fOutxCtsFlow */
             FALSE,                 /* fOutxDsrFlow */
             DTR_CONTROL_ENABLE,    /* fDtrControl */
             FALSE,                 /* fDsrSensitivity */
             TRUE,                  /* fTXContinueOnXoff */
             FALSE,                 /* fOutX */
             FALSE,                 /* fInX */
             FALSE,                 /* fErrorChar */
             FALSE,                 /* fNull */
             RTS_CONTROL_ENABLE,    /* fRtsControl */
             FALSE,                 /* fAbortOnError */
             17,                    /* fDummy2 */
             0,                     /* wReserved */
             0,                     /* XonLim */
             0,                     /* XoffLim */
             0,                     /* ByteSize */
             0,                     /* Parity */
             0,                     /* StopBits */
             0,                     /* XonChar */
             0,                     /* XoffChar */
             0,                     /* ErrorChar */
             -1,                    /* EofChar */
             0,                     /* EvtChar */
             0};                    /* wReservedl */

  dcb.BaudRate = params->baud; 
  dcb.BaudRate = dcb.BaudRate - dcb.BaudRate % 100;

  dcb.ByteSize = params->sendBits; /* obs: receiveBits blir ikke tatt hensyn til */
  switch (params->parity) {
    case PAR_NONE:
      dcb.Parity = NOPARITY;
      dcb.fParity = FALSE;
      break;
    case PAR_EVEN:
      dcb.Parity = EVENPARITY;
      dcb.fParity = TRUE;
      break;
    case PAR_ODD:
      dcb.Parity = ODDPARITY;
      dcb.fParity = TRUE;
      break;
  }
  switch (params->stopBits) {
    case ONE_SB: dcb.StopBits = ONESTOPBIT; break;
    case ONE_PT_FIVE_SB: dcb.StopBits = ONE5STOPBITS; break;
    case TWO_SB: dcb.StopBits = TWOSTOPBITS; break;
  }

  if (!SetCommState(portHandle, &dcb)) {
    LOG_E("SetCommState failed (baud=%lu, bits=%d)", dcb.BaudRate, dcb.ByteSize);
  }
  return;
}
/* send tegn til seriekanal */
void sendChar (int port, byte value) {
  DWORD bytesWritten;
  HANDLE portHandle = port ? port2 : port1;

  if (portHandle) {
    if (!WriteFile (portHandle, &value, 1, &bytesWritten, NULL)) {
      LOG_W("Failed to write to serial port %d", port);
    }
  }
}
/* hent tegn fra seriekanal */
byte getChar (int port) {
  HANDLE portHandle = port ? port2 : port1;
  byte value = 0;
  DWORD bytesRead;

  if (portHandle) {
    if (!ReadFile (portHandle, &value, 1, &bytesRead, NULL)) {
      LOG_W("Failed to read from serial port %d", port);
    }

    /* flere tegn i buffer? */
    {
      DWORD error;
      COMSTAT comstat;
      
      if (ClearCommError (portHandle, &error, &comstat)) {
        if (comstat.cbInQue != 0) {
          charAvailable (port);
        }
      }
    }
  }
  return value;
}
/* send tegn til skriver */
void printChar (byte value) {
  DWORD bytesWritten;
  
  if (port3) {
    if (!WriteFile (port3, &value, 1, &bytesWritten, NULL)) {
      LOG_W("Failed to write to parallel port");
    }
  }
}
