/* z80info.c
 *
 * Z80 CPU information window - displays live register state
 */

#include "TIKI-100_emul.h"
#include "Z80.h"
#include "z80info.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

extern Z80 cpu;
extern tiki_bool cpuHalted;
static HWND *pHwndZ80Info; /* pointer to owner's HWND tracking variable */
static HWND hwndHaltBtn;

#define IDC_HALT_BTN 200

void z80InfoSetHwndPtr (HWND *p) {
  pHwndZ80Info = p;
}

LRESULT CALLBACK Z80InfoWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      SetTimer (hwnd, 1, 200, NULL); /* refresh 5x/sec */
      {
        HINSTANCE hInst = GetModuleHandle (NULL);
        HFONT guiFont = (HFONT)GetStockObject (DEFAULT_GUI_FONT);
        hwndHaltBtn = CreateWindow ("BUTTON", "Halt CPU",
          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
          10, 10, 100, 26,
          hwnd, (HMENU)IDC_HALT_BTN, hInst, NULL);
        SendMessage (hwndHaltBtn, WM_SETFONT, (WPARAM)guiFont, TRUE);
      }
      return 0;
    case WM_COMMAND:
      if (LOWORD (wParam) == IDC_HALT_BTN) {
        if (cpuHalted) {
          contCpu ();
          SetWindowText (hwndHaltBtn, "Halt CPU");
        } else {
          haltCpu ();
          SetWindowText (hwndHaltBtn, "Cont. CPU");
        }
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_TIMER:
      {
        /* update button text only if state changed */
        static tiki_bool lastHalted = FALSE;
        if (cpuHalted != lastHalted) {
          lastHalted = cpuHalted;
          SetWindowText (hwndHaltBtn, cpuHalted ? "Cont. CPU" : "Halt CPU");
        }
        /* only invalidate the register area below the toolbar */
        RECT rc;
        GetClientRect (hwnd, &rc);
        rc.top = 40;
        InvalidateRect (hwnd, &rc, FALSE);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint (hwnd, &ps);
        RECT rcClient;
        GetClientRect (hwnd, &rcClient);
        int cw = rcClient.right - rcClient.left;
        int ch = rcClient.bottom - rcClient.top;
        HDC hdc = CreateCompatibleDC (hdcScreen);
        HBITMAP hbmBuf = CreateCompatibleBitmap (hdcScreen, cw, ch);
        HBITMAP hbmOld = SelectObject (hdc, hbmBuf);
        FillRect (hdc, &rcClient, GetSysColorBrush (COLOR_3DFACE));
        HFONT font = CreateFont (15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT oldFont = SelectObject (hdc, font);
        SetBkMode (hdc, TRANSPARENT);
        char buf[512];
        int y = 46, dy = 18;

        /* CPU status */
        if (cpuHalted) {
          SetTextColor (hdc, RGB (200, 0, 0));
          TextOut (hdc, 130, 16, "HALTED", 6);
          SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
        }

        snprintf (buf, sizeof(buf), "  AF: %04X    AF': %04X", cpu.AF.W, cpu.AF1.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  BC: %04X    BC': %04X", cpu.BC.W, cpu.BC1.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  DE: %04X    DE': %04X", cpu.DE.W, cpu.DE1.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  HL: %04X    HL': %04X", cpu.HL.W, cpu.HL1.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        y += 5;
        snprintf (buf, sizeof(buf), "  IX: %04X    IY:  %04X", cpu.IX.W, cpu.IY.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  PC: %04X    SP:  %04X", cpu.PC.W, cpu.SP.W);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "   I: %02X      IFF: %02X", cpu.I, cpu.IFF);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        y += 10;
        /* flags */
        byte f = cpu.AF.B.l;
        snprintf (buf, sizeof(buf), "  Flags: %c%c%c%c%c%c%c%c",
          (f & 0x80) ? 'S' : '-', (f & 0x40) ? 'Z' : '-',
          (f & 0x20) ? '5' : '-', (f & 0x10) ? 'H' : '-',
          (f & 0x08) ? '3' : '-', (f & 0x04) ? 'P' : '-',
          (f & 0x02) ? 'N' : '-', (f & 0x01) ? 'C' : '-');
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        y += 10;
        snprintf (buf, sizeof(buf), "  ICount: %d", cpu.ICount);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  IPeriod: %d", cpu.IPeriod);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;
        snprintf (buf, sizeof(buf), "  IRequest: %04X", cpu.IRequest);
        TextOut (hdc, 10, y, buf, strlen(buf)); y += dy;

        SelectObject (hdc, oldFont);
        DeleteObject (font);
        BitBlt (hdcScreen, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);
        SelectObject (hdc, hbmOld);
        DeleteObject (hbmBuf);
        DeleteDC (hdc);
        EndPaint (hwnd, &ps);
      }
      return 0;
    case WM_DESTROY:
      KillTimer (hwnd, 1);
      /* if CPU was halted when window closes, resume it */
      if (cpuHalted) contCpu ();
      if (pHwndZ80Info) *pHwndZ80Info = NULL;
      return 0;
  }
  return DefWindowProc (hwnd, msg, wParam, lParam);
}
