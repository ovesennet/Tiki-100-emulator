/* win32.c V1.1.1
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

#define ERROR_CAPTION     "TIKI-100_emul feilmelding"
#define STATUSBAR_HEIGHT  19

/* protos */
int WINAPI WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode);
static LRESULT CALLBACK WindowFunc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK DialogFunc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

static void update (int x, int y, int w, int h);
static void draw3dBox (int x, int y, int w, int h);
static void getDiskImage (int drive);
static void saveDiskImage (int drive);
static void setParam (HANDLE portHandle, struct serParams *params);

/* variabler */

static byte keyTable[256] = {
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_SLETT, KEY_BRYT, KEY_NONE, KEY_NONE, KEY_NONE, KEY_CR, KEY_NONE, KEY_NONE, 
  KEY_SHIFT, KEY_CTRL, KEY_NONE, KEY_NONE, KEY_LOCK, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_ANGRE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_SPACE, KEY_PGUP, KEY_PGDOWN, KEY_TABRIGHT, KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, 
  KEY_DOWN, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_UTVID, KEY_TABLEFT, KEY_NONE, 
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  KEY_NONE, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 
  'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 
  'x', 'y', 'z', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  KEY_NUM0, KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_NUM5, KEY_NUM6, KEY_NUM7, 
  KEY_NUM8, KEY_NUM9, KEY_NUMMULT, KEY_NUMPLUS, KEY_NONE, KEY_NUMMINUS, KEY_NUMDOT, KEY_NUMDIV, 
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_NONE, KEY_HJELP, 
  KEY_ENTER, KEY_NONE, KEY_NUMPERCENT, KEY_NUMEQU, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, '^', '+', ',', '-', '.', '\'', 
  'ø', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, '@', KEY_GRAFIKK, 'å', 'æ', KEY_NONE, 
  KEY_NONE, KEY_NONE, '<', KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, 
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE
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
static boolean slowDown = TRUE;
static boolean scanlines = FALSE;
static boolean st28b = FALSE;
static int width, height;
static int size40x = 1, size80x = 1;
static unsigned int xmin = (unsigned)~0;
static unsigned int ymin = (unsigned)~0;
static unsigned int xmax = 0;
static unsigned int ymax = 0;
static boolean updateWindow = FALSE;
static boolean lock = FALSE;
static boolean grafikk = FALSE;
static boolean disk[2] = {FALSE, FALSE};
static HANDLE port1;
static HANDLE port2;
static HANDLE port3;
static char port1Name[256] = "COM1";
static char port2Name[256] = "COM2";
static char port3Name[256] = "LPT1";

/*****************************************************************************/

int WINAPI WinMain (HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode) {
  char szWinName[] = "TIKIWin";
  MSG msg;
  WNDCLASSEX wcl;

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
    return 0;
  }

  /* lag vindu */
  hwnd = CreateWindow (szWinName, "TIKI-100_emul", WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, 
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_DESKTOP, NULL, hThisInst, NULL);
  changeRes (MEDRES);
  ShowWindow (hwnd, nWinMode);

  /* initialiser arrays */
  memset (pressedKeys, 1, 256);
  memset (screen, 0, 1024*256);

  /* finn katalog */
  GetCurrentDirectory (256, defaultDir);
  
  /* kjør emulator */
  if (!runEmul()) {
    MessageBox (hwnd, "Finner ikke ROM-fil!", ERROR_CAPTION, MB_OK);
  }

  /* avslutt */
  free (dsk[0]);
  free (dsk[1]);
  if (port1) CloseHandle (port1);
  if (port2) CloseHandle (port2);
  if (port3) CloseHandle (port3);

  return msg.wParam;
}

static LRESULT CALLBACK WindowFunc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      { /* sett opp memdc */
        HDC hdc = GetDC (hwnd);
        memdc = CreateCompatibleDC (hdc);
        hbit = CreateCompatibleBitmap (hdc, 1024, 1024 + STATUSBAR_HEIGHT);
        SelectObject (memdc, hbit);
        SelectObject (memdc, GetStockObject (BLACK_BRUSH));
        PatBlt (memdc, 0, 0, 1024, 1024 + STATUSBAR_HEIGHT, PATCOPY);
        ReleaseDC (hwnd, hdc);
      }
      break;
    case WM_PAINT:
      { /* kopier fra memdc til hdc */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hwnd, &ps);
        BitBlt (hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, 
                ps.rcPaint.bottom - ps.rcPaint.top, memdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint (hwnd, &ps);
      }
      break;
    case WM_SIZE:
      /* statusbar */
      SelectObject (memdc, GetSysColorBrush (COLOR_3DFACE));
      PatBlt (memdc, 0, height, width, STATUSBAR_HEIGHT, PATCOPY);
      
      /* tegn småbokser */
      draw3dBox (20, height + 5, 9, 9);
      draw3dBox (40, height + 5, 9, 9);
      draw3dBox (60, height + 5, 9, 9);
      draw3dBox (80, height + 5, 9, 9);
      
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
      pressedKeys[keyTable[(byte)wParam]] = 0;
      break;
    case WM_KEYUP:
      pressedKeys[keyTable[(byte)wParam]] = 1;
      break;
    case WM_COMMAND:
      switch (LOWORD (wParam)) {
        case IDM_RESET:
          resetEmul();
          break;
        case IDM_INNSTILLINGER:
          DialogBox (appInst, "dialog", hwnd, (DLGPROC)DialogFunc);
          break;
        case IDM_OM:
          MessageBox (hwnd, 
                      "TIKI-100_emul V1.1.1 - en freeware TIKI 100 Rev. C emulator.\n"
                      "Z80 emulering copyright (C) Marat Fayzullin 1994,1995,1996,1997.\n"
                      "Resten copyright (C) Asbjørn Djupdal 2000-2001.", 
                      "Om TIKI-100_emul",
                      MB_OK);
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
          boolean sl;
          boolean st;
          char p1[256];
          char p2[256];
          char p3[256];
          
          /* begrense hastighet */
          slowDown = SendDlgItemMessage (hdwnd, IDD_HASTIGHET, BM_GETCHECK, 0, 0);

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
                MessageBox (hwnd, "Angitt navn på P1 ikke tilgjengelig!", ERROR_CAPTION, MB_OK);
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
                MessageBox (hwnd, "Angitt navn på P2 ikke tilgjengelig!", ERROR_CAPTION, MB_OK);
                port1 = 0;
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
                MessageBox (hwnd, "Angitt navn på P3 ikke tilgjengelig!", ERROR_CAPTION, MB_OK);
                port1 = 0;
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
              break;
            case 40*1*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 40, 1, 10, 512);
              break;
            case 40*2*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 40, 2, 10, 512);
              break;
            case 80*2*10*512:
              EnableMenuItem (GetSubMenu (GetMenu (hwnd), 1), drive == 0 ? IDM_LAGRE_A : IDM_LAGRE_B,
                              MF_BYCOMMAND | MF_ENABLED);    
              insertDisk (drive, dsk[drive], 80, 2, 10, 512);
              break;
            default:
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
/* Forandre oppløsning */
void changeRes (int newRes) {
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
  GetWindowRect (hwnd, &windowRect);
  MoveWindow (hwnd, windowRect.left, windowRect.top, width + 2 * GetSystemMetrics (SM_CXFIXEDFRAME), 
              height + 2 * GetSystemMetrics (SM_CYFIXEDFRAME) + GetSystemMetrics (SM_CYCAPTION) +
              GetSystemMetrics (SM_CYMENU) + STATUSBAR_HEIGHT, 1);
  /* slett bakgrunn */
  SelectObject (memdc, GetStockObject (BLACK_BRUSH));
  PatBlt (memdc, 0, 0, width, height, PATCOPY);
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
  SetPixelV (memdc, x, y, colors[color]);
  update (x, y, 1, 1);
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
  BitBlt (memdc, 0, -distance, width, height + distance, memdc, 0, 0, SRCCOPY);
  SelectObject (memdc, GetStockObject (BLACK_BRUSH));
  /* slett resten */
  if (distance > 0) {
    PatBlt (memdc, 0, height - distance, width, distance, PATCOPY);
  } else {
    PatBlt (memdc, 0, 0, width, -distance, PATCOPY);
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
    static boolean firstTime = TRUE;
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
  /* oppdater skjerm */
  if (updateWindow) {
    RECT rect = {xmin, ymin, xmax, ymax};
    InvalidateRect (hwnd, &rect, FALSE);
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
        DispatchMessage (&msg);
      }
    }
  }
}
/* Tenn/slukk lock lys */
void lockLight (boolean status) {
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, 21, height + 6, 7, 7, PATCOPY);
  update (21, height + 6, 7, 7);
  lock = status;
}
/* Tenn/slukk grafikk lys */
void grafikkLight (boolean status) {
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, 41, height + 6, 7, 7, PATCOPY);
  update (41, height + 6, 7, 7);
  grafikk = status;
}
/* Tenn/slukk disk lys for gitt stasjon */
void diskLight (int drive, boolean status) {
  int x = (drive ? 81 : 61);
  HBRUSH brush = (status ?
                  GetSysColorBrush (COLOR_HIGHLIGHT) :
                  GetSysColorBrush (COLOR_3DFACE));
  SelectObject (memdc, brush);
  PatBlt (memdc, x, height + 6, 7, 7, PATCOPY);
  update (61, height + 6, 7, 7);
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
    /* ingen aksjon */
  }
  return;
}
/* send tegn til seriekanal */
void sendChar (int port, byte value) {
  DWORD bytesWritten;
  HANDLE portHandle = port ? port2 : port1;

  if (portHandle) {
    if (!WriteFile (portHandle, &value, 1, &bytesWritten, NULL)) {
      /* kan ikke skrive til port */
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
      /* kan ikke lese fra port */
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
      /* kan ikke skrive til port */
    }
  }
}
