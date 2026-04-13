/* diskview.c
 *
 * Disk directory viewer window - shows CP/M directory for disk A and B
 * Parses CP/M 2.2 directory structure from raw disk images
 */

#include "TIKI-100_emul.h"
#include "diskview.h"
#include "log.h"

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_DIR_ENTRIES 256
#define MAX_FILES       128
#define CPM_EMPTY       0xE5
#define HEADER_HEIGHT    28   /* space for drive label + separator */
#define LINE_HEIGHT      15   /* pixels per file line */
#define SUMMARY_HEIGHT   26   /* gap + summary line */

static HWND *pHwndDiskView;
static HWND hwndDiskViewWnd;
static int scrollPos = 0;     /* scroll position in pixels */
static int contentHeight = 0; /* total content height in pixels */

/* selection state */
static int selDrive = -1;     /* selected drive (0 or 1), -1 = none */
static int selIndex = -1;     /* selected file index, -1 = none */

/* button hit areas (in content coords, set during paint) */
#define BTN_W  36
#define BTN_H  14
static RECT btnView = {0,0,0,0};
static RECT btnDump = {0,0,0,0};
static int  btnDrive = -1;     /* drive for which buttons are drawn */

/* "Add file" button hit areas (one per drive, in content coords) */
#define ADDBTN_W 58
#define ADDBTN_H 18
static RECT btnAddFile[2] = {{0,0,0,0}, {0,0,0,0}};

/* disk image data provided by win32.c (mutable for Add-file writes) */
static byte *diskData[2] = {NULL, NULL};
static unsigned long diskSize[2] = {0, 0};
static char diskFilePath[2][MAX_PATH] = {"", ""};

/* raw directory entry as read from disk */
typedef struct {
  int  user;
  char name[13];       /* "FILENAME.EXT" trimmed */
  int  extent;         /* logical extent = s2*32 + extent_low */
  int  recordCount;    /* RC field */
  int  blockCount;     /* number of non-zero blocks in allocation map */
} RawEntry;

/* final file info for display */
typedef struct {
  char name[13];
  unsigned long size;
  int user;
} FileInfo;

void diskViewSetHwndPtr (HWND *p) {
  pHwndDiskView = p;
}

void diskViewSetDisk (int drive, byte *data, unsigned long size,
                      const char *filePath) {
  if (drive >= 0 && drive <= 1) {
    diskData[drive] = data;
    diskSize[drive] = size;
    if (filePath && filePath[0]) {
      strncpy (diskFilePath[drive], filePath, MAX_PATH - 1);
      diskFilePath[drive][MAX_PATH - 1] = '\0';
    } else {
      diskFilePath[drive][0] = '\0';
    }
  }
}

/* TIKI-100 disk geometry parameters */
typedef struct {
  int dirOffset;         /* byte offset to directory area */
  int blockSize;         /* CP/M block size in bytes */
  int maxDir;            /* max directory entries */
  int singleByteAlloc;   /* 1 = 1-byte block numbers, 0 = 2-byte */
} DiskGeometry;

static tiki_bool getDiskGeometry (unsigned long size, DiskGeometry *geo) {
  switch (size) {
    case 40UL*1*18*128:  /* 90KB */
      geo->dirOffset = 3 * 1 * 18 * 128;     /* 3 reserved tracks, 1 side */
      geo->blockSize = 1024;
      geo->maxDir = 64;
      geo->singleByteAlloc = 1;
      return TRUE;
    case 40UL*1*10*512:  /* 200KB */
      geo->dirOffset = 2 * 1 * 10 * 512;     /* 2 reserved tracks, 1 side */
      geo->blockSize = 1024;
      geo->maxDir = 64;
      geo->singleByteAlloc = 1;
      return TRUE;
    case 40UL*2*10*512:  /* 400KB */
      geo->dirOffset = 1 * 2 * 10 * 512;     /* 1 reserved track, 2 sides */
      geo->blockSize = 2048;
      geo->maxDir = 128;
      geo->singleByteAlloc = 1;
      return TRUE;
    case 80UL*2*10*512:  /* 800KB */
      geo->dirOffset = 1 * 2 * 10 * 512;     /* 1 reserved track, 2 sides */
      geo->blockSize = 2048;
      geo->maxDir = 256;
      geo->singleByteAlloc = 0;              /* >255 blocks, 2-byte alloc */
      return TRUE;
    default:
      return FALSE;
  }
}

/* count non-zero blocks in a directory entry's allocation map */
static int countBlocks (const byte *alloc, int singleByte) {
  int count = 0, i;
  if (singleByte) {
    for (i = 0; i < 16; i++) {
      if (alloc[i]) count++;
    }
  } else {
    for (i = 0; i < 16; i += 2) {
      unsigned short blk = alloc[i] | ((unsigned short)alloc[i+1] << 8);
      if (blk) count++;
    }
  }
  return count;
}

/* check if a character is valid for a CP/M filename (printable ASCII) */
static tiki_bool isValidCpmChar (byte b) {
  b &= 0x7F;
  return (b >= 0x20 && b <= 0x7E);
}

/* convert TIKI-100 charset (Nordic ISO 646) to Windows-1252.
 * TIKI-100 uses [ \ ] for the Norwegian letters \xC6 \xD8 \xC5
 * and { | } for \xE6 \xF8 \xE5 */
static char tikiCharToWin (char c) {
  switch ((unsigned char)c) {
    case 0x5B: return (char)0xC6;  /* [ -> AE */
    case 0x5C: return (char)0xD8;  /* \ -> O-stroke */
    case 0x5D: return (char)0xC5;  /* ] -> A-ring */
    case 0x7B: return (char)0xE6;  /* { -> ae */
    case 0x7C: return (char)0xF8;  /* | -> o-stroke */
    case 0x7D: return (char)0xE5;  /* } -> a-ring */
    default:   return c;
  }
}

/* Norwegian alphabetical sort order: A-Z, then AE, O-stroke, A-ring */
static int sortOrder (unsigned char c) {
  if (c >= 'a' && c <= 'z') c -= 32;          /* to uppercase */
  if (c == 0xE6) c = 0xC6;                    /* ae -> AE */
  if (c == 0xF8) c = 0xD8;                    /* o-stroke -> O-stroke */
  if (c == 0xE5) c = 0xC5;                    /* a-ring -> A-ring */
  switch (c) {
    case 0xC6: return 'Z' + 1;                /* AE after Z */
    case 0xD8: return 'Z' + 2;                /* O-stroke */
    case 0xC5: return 'Z' + 3;                /* A-ring */
    default:   return c;
  }
}

static int compareFiles (const void *a, const void *b) {
  const FileInfo *fa = (const FileInfo *)a;
  const FileInfo *fb = (const FileInfo *)b;
  const unsigned char *sa = (const unsigned char *)fa->name;
  const unsigned char *sb = (const unsigned char *)fb->name;
  while (*sa && *sb) {
    int oa = sortOrder (*sa);
    int ob = sortOrder (*sb);
    if (oa != ob) return oa - ob;
    sa++; sb++;
  }
  return *sa - *sb;
}

/* parse CP/M directory and build file list.
 * returns number of files found, or -1 if no disk / unknown format */
static int parseCpmDirectory (int drive, FileInfo *files, int maxFiles) {
  DiskGeometry geo;
  RawEntry entries[MAX_DIR_ENTRIES];
  int entryCount = 0;
  int i, j;

  if (!diskData[drive] || diskSize[drive] == 0) return -1;
  if (!getDiskGeometry (diskSize[drive], &geo)) return -1;

  /* pass 1: read all valid directory entries */
  for (i = 0; i < geo.maxDir; i++) {
    unsigned long offset = geo.dirOffset + (unsigned long)i * 32;
    if (offset + 32 > diskSize[drive]) break;

    const byte *raw = diskData[drive] + offset;
    byte userNum = raw[0];

    /* skip deleted/empty entries */
    if (userNum == CPM_EMPTY) continue;
    if (userNum > 0x0F) continue;

    /* validate filename characters */
    tiki_bool valid = TRUE;
    for (j = 1; j <= 11; j++) {
      if (!isValidCpmChar (raw[j])) { valid = FALSE; break; }
    }
    if (!valid) continue;

    /* build trimmed "NAME.EXT" string */
    RawEntry *e = &entries[entryCount];
    e->user = userNum;
    int k = 0;
    for (j = 1; j <= 8; j++) {
      char c = raw[j] & 0x7F;
      if (c != ' ') e->name[k++] = tikiCharToWin (c);
    }
    /* check if there's an extension */
    tiki_bool hasExt = FALSE;
    for (j = 9; j <= 11; j++) {
      if ((raw[j] & 0x7F) != ' ') { hasExt = TRUE; break; }
    }
    if (hasExt) {
      e->name[k++] = '.';
      for (j = 9; j <= 11; j++) {
        char c = raw[j] & 0x7F;
        if (c != ' ') e->name[k++] = tikiCharToWin (c);
      }
    }
    e->name[k] = '\0';
    if (k == 0) continue;  /* empty name */

    e->extent = raw[14] * 32 + raw[12];    /* s2*32 + extent_low */
    e->recordCount = raw[15];               /* RC */
    e->blockCount = countBlocks (raw + 16, geo.singleByteAlloc);

    if (e->blockCount == 0) continue;       /* skip entries with no blocks */
    entryCount++;
    if (entryCount >= MAX_DIR_ENTRIES) break;
  }

  /* pass 2: group extents by user+name, compute file sizes */
  int fileCount = 0;
  tiki_bool used[MAX_DIR_ENTRIES];
  memset (used, 0, sizeof(used));

  for (i = 0; i < entryCount && fileCount < maxFiles; i++) {
    if (used[i]) continue;
    used[i] = TRUE;

    /* collect all extents for this file */
    RawEntry *extents[MAX_DIR_ENTRIES];
    int extentCount = 0;
    extents[extentCount++] = &entries[i];

    for (j = i + 1; j < entryCount; j++) {
      if (used[j]) continue;
      if (entries[j].user == entries[i].user &&
          strcmp (entries[j].name, entries[i].name) == 0) {
        used[j] = TRUE;
        extents[extentCount++] = &entries[j];
      }
    }

    /* sort extents by extent number (simple insertion sort) */
    {
      int a, b;
      for (a = 1; a < extentCount; a++) {
        RawEntry *tmp = extents[a];
        for (b = a; b > 0 && extents[b-1]->extent > tmp->extent; b--)
          extents[b] = extents[b-1];
        extents[b] = tmp;
      }
    }

    /* calculate disk space used: total allocated blocks * blockSize
     * this matches CP/M's KAT command output */
    int totalBlocks = 0;
    for (j = 0; j < extentCount; j++) {
      totalBlocks += extents[j]->blockCount;
    }
    unsigned long totalSize = (unsigned long)totalBlocks * geo.blockSize;

    strncpy (files[fileCount].name, entries[i].name, 12);
    files[fileCount].name[12] = '\0';
    files[fileCount].user = entries[i].user;
    files[fileCount].size = totalSize;
    fileCount++;
  }

  /* sort files alphabetically (Norwegian order: ..., Z, AE, O-stroke, A-ring) */
  if (fileCount > 1)
    qsort (files, fileCount, sizeof (FileInfo), compareFiles);

  return fileCount;
}

/* format a file size in KB (always block-aligned, matching CP/M KAT output) */
static void formatSize (unsigned long size, char *buf, int bufLen) {
  unsigned long kb = (size + 1023) / 1024; /* round up to nearest KB */
  snprintf (buf, bufLen, "%4luKB", kb);
}

/* ------------ file data extraction ------------ */

/* extract raw file data from CP/M disk image.
 * caller must free() the returned buffer. */
static byte *extractFileData (int drive, int user, const char *fileName,
                              unsigned long *outSize) {
  DiskGeometry geo;
  int i, j;

  *outSize = 0;
  if (!diskData[drive] || !getDiskGeometry (diskSize[drive], &geo)) return NULL;

  typedef struct { int extent, rc; byte alloc[16]; } ExtInfo;
  ExtInfo exts[64];
  int extCount = 0;

  for (i = 0; i < geo.maxDir; i++) {
    unsigned long off = geo.dirOffset + (unsigned long)i * 32;
    if (off + 32 > diskSize[drive]) break;
    const byte *raw = diskData[drive] + off;
    if (raw[0] == CPM_EMPTY || raw[0] != (byte)user) continue;

    /* build name for comparison */
    char name[13];
    int k = 0;
    for (j = 1; j <= 8; j++) {
      char c = raw[j] & 0x7F;
      if (c != ' ') name[k++] = tikiCharToWin (c);
    }
    tiki_bool hasExt = FALSE;
    for (j = 9; j <= 11; j++)
      if ((raw[j] & 0x7F) != ' ') { hasExt = TRUE; break; }
    if (hasExt) {
      name[k++] = '.';
      for (j = 9; j <= 11; j++) {
        char c = raw[j] & 0x7F;
        if (c != ' ') name[k++] = tikiCharToWin (c);
      }
    }
    name[k] = '\0';
    if (strcmp (name, fileName) != 0) continue;

    ExtInfo *e = &exts[extCount];
    e->extent = raw[14] * 32 + raw[12];
    e->rc = raw[15];
    memcpy (e->alloc, raw + 16, 16);
    extCount++;
    if (extCount >= 64) break;
  }

  if (extCount == 0) return NULL;

  /* sort by extent number */
  for (i = 1; i < extCount; i++) {
    ExtInfo tmp = exts[i];
    for (j = i; j > 0 && exts[j-1].extent > tmp.extent; j--)
      exts[j] = exts[j-1];
    exts[j] = tmp;
  }

  int numAlloc = geo.singleByteAlloc ? 16 : 8;
  unsigned long maxSize = (unsigned long)extCount * numAlloc * geo.blockSize;
  byte *data = (byte *)malloc (maxSize);
  if (!data) return NULL;

  unsigned long pos = 0;
  for (i = 0; i < extCount; i++) {
    unsigned long extStart = pos;
    for (j = 0; j < numAlloc; j++) {
      unsigned int blk;
      if (geo.singleByteAlloc)
        blk = exts[i].alloc[j];
      else
        blk = exts[i].alloc[j*2] | ((unsigned int)exts[i].alloc[j*2+1] << 8);
      if (blk == 0) continue;
      unsigned long diskOff = geo.dirOffset + (unsigned long)blk * geo.blockSize;
      if (diskOff + geo.blockSize <= diskSize[drive]) {
        memcpy (data + pos, diskData[drive] + diskOff, geo.blockSize);
        pos += geo.blockSize;
      }
    }
    /* trim last extent to RC * 128 bytes */
    if (i == extCount - 1 && exts[i].rc > 0) {
      unsigned long extBytes = (unsigned long)exts[i].rc * 128;
      if (extStart + extBytes < pos) pos = extStart + extBytes;
    }
  }

  *outSize = pos;
  return data;
}

/* convert raw bytes to readable text, applying TIKI-100 charset conversion.
 * stops at CP/M EOF (0x1A). caller must free() the returned buffer. */
static char *convertToReadableText (const byte *data, unsigned long size,
                                    int *outLen, int *outLines) {
  char *text = (char *)malloc (size + 1);
  if (!text) { *outLen = 0; *outLines = 1; return NULL; }
  int len = 0, lines = 1;
  unsigned long i;
  for (i = 0; i < size; i++) {
    if (data[i] == 0x1A) break;            /* CP/M EOF */
    char c = tikiCharToWin ((char)data[i]);
    unsigned char uc = (unsigned char)c;
    if (c == '\n') { text[len++] = c; lines++; }
    else if (c == '\r') { /* skip CR */ }
    else if (c == '\t') { text[len++] = c; }
    else if (uc >= 0x20) { text[len++] = c; }
  }
  text[len] = '\0';
  *outLen = len;
  *outLines = lines;
  return text;
}

/* ------------ file viewer window ------------ */

#define FV_LINE_HEIGHT  15
#define FV_MARGIN        8

typedef struct {
  char *text;
  int   textLen;
  int   lineCount;
  int  *lineOffsets;
  int   scrollPos;      /* first visible line */
  int   viewLines;      /* lines visible in viewport */
  HFONT font;
} FileViewData;

static void buildLineOffsets (FileViewData *fvd) {
  fvd->lineOffsets = (int *)malloc (sizeof(int) * (fvd->lineCount + 1));
  if (!fvd->lineOffsets) { fvd->lineCount = 0; return; }
  fvd->lineOffsets[0] = 0;
  int line = 1, i;
  for (i = 0; i < fvd->textLen && line < fvd->lineCount; i++) {
    if (fvd->text[i] == '\n')
      fvd->lineOffsets[line++] = i + 1;
  }
}

static void fvUpdateScrollbar (HWND hwnd, FileViewData *fvd) {
  SCROLLINFO si = {0};
  si.cbSize = sizeof (SCROLLINFO);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = fvd->lineCount > 0 ? fvd->lineCount - 1 : 0;
  si.nPage = fvd->viewLines;
  if (fvd->scrollPos > si.nMax - (int)si.nPage + 1)
    fvd->scrollPos = si.nMax - (int)si.nPage + 1;
  if (fvd->scrollPos < 0) fvd->scrollPos = 0;
  si.nPos = fvd->scrollPos;
  SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
}

static LRESULT CALLBACK FileViewWndProc (HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam) {
  FileViewData *fvd = (FileViewData *)GetWindowLongPtr (hwnd, GWLP_USERDATA);
  switch (msg) {
    case WM_CREATE:
      {
        CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
        fvd = (FileViewData *)cs->lpCreateParams;
        SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)fvd);
        fvd->font = CreateFont (13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
      }
      return 0;
    case WM_SIZE:
      if (fvd) {
        fvd->viewLines = HIWORD (lParam) / FV_LINE_HEIGHT;
        fvUpdateScrollbar (hwnd, fvd);
      }
      return 0;
    case WM_VSCROLL:
      if (fvd) {
        SCROLLINFO si = {0};
        si.cbSize = sizeof (SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo (hwnd, SB_VERT, &si);
        int pos = si.nPos;
        switch (LOWORD (wParam)) {
          case SB_LINEUP:     pos--; break;
          case SB_LINEDOWN:   pos++; break;
          case SB_PAGEUP:     pos -= si.nPage; break;
          case SB_PAGEDOWN:   pos += si.nPage; break;
          case SB_THUMBTRACK: pos = si.nTrackPos; break;
        }
        if (pos < 0) pos = 0;
        if (pos > si.nMax - (int)si.nPage + 1) pos = si.nMax - (int)si.nPage + 1;
        if (pos < 0) pos = 0;
        fvd->scrollPos = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_MOUSEWHEEL:
      if (fvd) {
        int delta = GET_WHEEL_DELTA_WPARAM (wParam);
        fvd->scrollPos -= delta / WHEEL_DELTA * 3;
        if (fvd->scrollPos < 0) fvd->scrollPos = 0;
        int maxPos = fvd->lineCount - fvd->viewLines;
        if (maxPos < 0) maxPos = 0;
        if (fvd->scrollPos > maxPos) fvd->scrollPos = maxPos;
        fvUpdateScrollbar (hwnd, fvd);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_KEYDOWN:
      if (fvd) {
        int maxPos = fvd->lineCount - fvd->viewLines;
        if (maxPos < 0) maxPos = 0;
        switch (wParam) {
          case VK_UP:    fvd->scrollPos--; break;
          case VK_DOWN:  fvd->scrollPos++; break;
          case VK_PRIOR: fvd->scrollPos -= fvd->viewLines; break;
          case VK_NEXT:  fvd->scrollPos += fvd->viewLines; break;
          case VK_HOME:  fvd->scrollPos = 0; break;
          case VK_END:   fvd->scrollPos = maxPos; break;
          default: return DefWindowProc (hwnd, msg, wParam, lParam);
        }
        if (fvd->scrollPos < 0) fvd->scrollPos = 0;
        if (fvd->scrollPos > maxPos) fvd->scrollPos = maxPos;
        fvUpdateScrollbar (hwnd, fvd);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint (hwnd, &ps);
        RECT rc;
        GetClientRect (hwnd, &rc);
        int cw = rc.right, ch = rc.bottom;
        HDC hdc = CreateCompatibleDC (hdcScreen);
        HBITMAP hbm = CreateCompatibleBitmap (hdcScreen, cw, ch);
        HBITMAP hbmOld = SelectObject (hdc, hbm);
        FillRect (hdc, &rc, (HBRUSH)GetStockObject (WHITE_BRUSH));
        SetBkMode (hdc, TRANSPARENT);

        if (fvd && fvd->text && fvd->font) {
          HFONT oldFont = SelectObject (hdc, fvd->font);
          SetTextColor (hdc, RGB (0, 0, 0));
          int y = FV_MARGIN, i;
          for (i = fvd->scrollPos; i < fvd->lineCount && y < ch; i++) {
            int start = fvd->lineOffsets[i];
            int end = (i + 1 < fvd->lineCount) ?
                      fvd->lineOffsets[i + 1] : fvd->textLen;
            if (end > start && fvd->text[end - 1] == '\n') end--;
            int lineLen = end - start;
            if (lineLen > 0)
              TabbedTextOut (hdc, FV_MARGIN, y, fvd->text + start,
                             lineLen, 0, NULL, FV_MARGIN);
            y += FV_LINE_HEIGHT;
          }
          SelectObject (hdc, oldFont);
        }

        BitBlt (hdcScreen, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);
        SelectObject (hdc, hbmOld);
        DeleteObject (hbm);
        DeleteDC (hdc);
        EndPaint (hwnd, &ps);
      }
      return 0;
    case WM_DESTROY:
      if (fvd) {
        if (fvd->font) DeleteObject (fvd->font);
        if (fvd->lineOffsets) free (fvd->lineOffsets);
        if (fvd->text) free (fvd->text);
        free (fvd);
      }
      return 0;
  }
  return DefWindowProc (hwnd, msg, wParam, lParam);
}

static void openFileViewer (int drive, const FileInfo *file) {
  unsigned long rawSize = 0;
  byte *rawData = extractFileData (drive, file->user, file->name, &rawSize);
  if (!rawData || rawSize == 0) { if (rawData) free (rawData); return; }

  int textLen = 0, lineCount = 0;
  char *text = convertToReadableText (rawData, rawSize, &textLen, &lineCount);
  free (rawData);
  if (!text) return;

  FileViewData *fvd = (FileViewData *)calloc (1, sizeof (FileViewData));
  fvd->text = text;
  fvd->textLen = textLen;
  fvd->lineCount = lineCount;
  buildLineOffsets (fvd);

  static tiki_bool registered = FALSE;
  if (!registered) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = FileViewWndProc;
    wc.hInstance = GetModuleHandle (NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject (WHITE_BRUSH);
    wc.lpszClassName = "FileViewClass";
    wc.hCursor = LoadCursor (NULL, IDC_ARROW);
    RegisterClass (&wc);
    registered = TRUE;
  }

  char title[32];
  snprintf (title, sizeof(title), "%c:%s", 'A' + drive, file->name);

  HWND fv = CreateWindowEx (WS_EX_TOOLWINDOW, "FileViewClass", title,
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL | WS_THICKFRAME,
    CW_USEDEFAULT, CW_USEDEFAULT, 550, 500,
    hwndDiskViewWnd, NULL, GetModuleHandle (NULL), fvd);
  ShowWindow (fv, SW_SHOW);
}

static void dumpFileData (int drive, const FileInfo *file) {
  unsigned long rawSize = 0;
  byte *rawData = extractFileData (drive, file->user, file->name, &rawSize);
  if (!rawData || rawSize == 0) { if (rawData) free (rawData); return; }

  char filename[MAX_PATH];
  strncpy (filename, file->name, sizeof(filename) - 1);
  filename[sizeof(filename) - 1] = '\0';

  OPENFILENAME ofn;
  memset (&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwndDiskViewWnd;
  ofn.lpstrFilter = "All Files\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrTitle = "Save file";

  if (GetSaveFileName (&ofn)) {
    FILE *fp = fopen (filename, "wb");
    if (fp) {
      fwrite (rawData, 1, rawSize, fp);
      fclose (fp);
    }
  }
  free (rawData);
}

/* ------------ CP/M file writer ------------ */

/* convert a Windows-1252 character to TIKI-100 charset (ISO 646 Norwegian) */
static char winToTikiChar (unsigned char c) {
  switch (c) {
    case 0xC6: case 0xE6: return '[';   /* AE/ae -> [ */
    case 0xD8: case 0xF8: return '\\';  /* O-stroke -> \ */
    case 0xC5: case 0xE5: return ']';   /* A-ring -> ] */
    default:
      if (c >= 'a' && c <= 'z') return (char)(c - 32);
      if (c >= 0x20 && c <= 0x7E) return (char)c;
      return '_';
  }
}

/* convert a host filename to CP/M 8.3 format (11 bytes, space-padded).
 * returns TRUE on success, FALSE if the name is empty or invalid. */
static tiki_bool toCpmFilename (const char *hostPath, byte *cpm11) {
  const char *base = strrchr (hostPath, '\\');
  if (!base) base = strrchr (hostPath, '/');
  base = base ? base + 1 : hostPath;

  const char *dot = strrchr (base, '.');
  int nameLen = dot ? (int)(dot - base) : (int)strlen (base);
  const char *ext = dot ? dot + 1 : "";
  int extLen = (int)strlen (ext);
  if (nameLen > 8) nameLen = 8;
  if (extLen > 3) extLen = 3;
  if (nameLen == 0) return FALSE;

  memset (cpm11, ' ', 11);
  { int i; for (i = 0; i < nameLen; i++) cpm11[i] = winToTikiChar ((unsigned char)base[i]); }
  { int i; for (i = 0; i < extLen; i++) cpm11[8 + i] = winToTikiChar ((unsigned char)ext[i]); }
  return TRUE;
}

/* save the in-memory disk image back to the host file */
static tiki_bool saveDiskImageToFile (int drive) {
  FILE *fp;
  if (!diskFilePath[drive][0] || !diskData[drive] || diskSize[drive] == 0)
    return FALSE;
  fp = fopen (diskFilePath[drive], "wb");
  if (!fp) {
    MessageBox (hwndDiskViewWnd, "Could not write disk image file.",
                "Add file", MB_OK | MB_ICONERROR);
    return FALSE;
  }
  fwrite (diskData[drive], 1, diskSize[drive], fp);
  fclose (fp);
  LOG_I ("Saved disk %c to %s", 'A' + drive, diskFilePath[drive]);
  return TRUE;
}

/* add a host file to a CP/M disk image */
static void handleAddFile (int drive) {
  DiskGeometry geo;
  byte cpm11[11];
  int numAllocs, bytesPerEntry, totalBlocks, dirBlocks;
  int blocksNeeded, entriesNeeded, freeBlocks, freeDirEntries;
  byte *blockBitmap;
  int *allocBlocks;
  int allocated, exm, blockIdx, dirEntryIdx;
  unsigned long bytesRemaining;
  DWORD hostFileSize, dwRead;
  byte *fileData;
  HANDLE hFile;
  int i, j, e;
  char hostPath[MAX_PATH] = "";
  OPENFILENAME ofn;

  if (!diskData[drive] || diskSize[drive] == 0) {
    MessageBox (hwndDiskViewWnd, "No disk loaded in this drive.",
                "Add file", MB_OK | MB_ICONWARNING);
    return;
  }
  if (!diskFilePath[drive][0]) {
    MessageBox (hwndDiskViewWnd, "No file path for this disk image — cannot save changes.",
                "Add file", MB_OK | MB_ICONWARNING);
    return;
  }
  if (!getDiskGeometry (diskSize[drive], &geo)) {
    MessageBox (hwndDiskViewWnd, "Unknown disk format.",
                "Add file", MB_OK | MB_ICONERROR);
    return;
  }

  /* open file dialog */
  memset (&ofn, 0, sizeof (ofn));
  ofn.lStructSize = sizeof (ofn);
  ofn.hwndOwner = hwndDiskViewWnd;
  ofn.lpstrFilter = "All Files\0*.*\0";
  ofn.lpstrFile = hostPath;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  ofn.lpstrTitle = drive ? "Add file to Drive B:" : "Add file to Drive A:";
  if (!GetOpenFileName (&ofn)) return;

  /* convert filename to CP/M 8.3 */
  if (!toCpmFilename (hostPath, cpm11)) {
    MessageBox (hwndDiskViewWnd, "Invalid filename for CP/M.",
                "Add file", MB_OK | MB_ICONERROR);
    return;
  }

  /* check for duplicate filename */
  for (i = 0; i < geo.maxDir; i++) {
    unsigned long off = geo.dirOffset + (unsigned long)i * 32;
    tiki_bool match;
    if (off + 32 > diskSize[drive]) break;
    if (diskData[drive][off] == CPM_EMPTY || diskData[drive][off] > 0x0F) continue;
    match = TRUE;
    for (j = 0; j < 11; j++) {
      if ((diskData[drive][off + 1 + j] & 0x7F) != cpm11[j]) { match = FALSE; break; }
    }
    if (match) {
      MessageBox (hwndDiskViewWnd,
                  "A file with this name already exists on the disk.",
                  "Add file", MB_OK | MB_ICONWARNING);
      return;
    }
  }

  /* read the host file */
  hFile = CreateFile (hostPath, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    MessageBox (hwndDiskViewWnd, "Cannot open the selected file.",
                "Add file", MB_OK | MB_ICONERROR);
    return;
  }
  hostFileSize = GetFileSize (hFile, NULL);
  if (hostFileSize == 0) {
    CloseHandle (hFile);
    MessageBox (hwndDiskViewWnd, "Cannot add an empty file.",
                "Add file", MB_OK | MB_ICONWARNING);
    return;
  }
  fileData = (byte *)malloc (hostFileSize);
  if (!fileData) {
    CloseHandle (hFile);
    MessageBox (hwndDiskViewWnd, "Out of memory.", "Add file", MB_OK | MB_ICONERROR);
    return;
  }
  ReadFile (hFile, fileData, hostFileSize, &dwRead, NULL);
  CloseHandle (hFile);

  /* geometry calculations */
  numAllocs    = geo.singleByteAlloc ? 16 : 8;
  bytesPerEntry = numAllocs * geo.blockSize;
  totalBlocks  = (int)((diskSize[drive] - geo.dirOffset) / geo.blockSize);
  dirBlocks    = (geo.maxDir * 32 + geo.blockSize - 1) / geo.blockSize;
  blocksNeeded = (int)((hostFileSize + geo.blockSize - 1) / geo.blockSize);
  entriesNeeded = (blocksNeeded + numAllocs - 1) / numAllocs;
  if (entriesNeeded < 1) entriesNeeded = 1;

  /* build used-block bitmap */
  blockBitmap = (byte *)calloc (totalBlocks, 1);
  if (!blockBitmap) { free (fileData); return; }
  for (i = 0; i < dirBlocks && i < totalBlocks; i++)
    blockBitmap[i] = 1;
  for (i = 0; i < geo.maxDir; i++) {
    unsigned long off = geo.dirOffset + (unsigned long)i * 32;
    if (off + 32 > diskSize[drive]) break;
    if (diskData[drive][off] == CPM_EMPTY || diskData[drive][off] > 0x0F) continue;
    if (geo.singleByteAlloc) {
      for (j = 0; j < 16; j++) {
        int blk = diskData[drive][off + 16 + j];
        if (blk > 0 && blk < totalBlocks) blockBitmap[blk] = 1;
      }
    } else {
      for (j = 0; j < 16; j += 2) {
        int blk = diskData[drive][off + 16 + j] |
                  (diskData[drive][off + 16 + j + 1] << 8);
        if (blk > 0 && blk < totalBlocks) blockBitmap[blk] = 1;
      }
    }
  }

  /* check free space */
  freeBlocks = 0;
  for (i = 0; i < totalBlocks; i++)
    if (!blockBitmap[i]) freeBlocks++;
  if (blocksNeeded > freeBlocks) {
    free (fileData);
    free (blockBitmap);
    {
      char msg[256];
      snprintf (msg, sizeof (msg),
                "Not enough space on disk.\n\n"
                "File needs %d KB, only %d KB available.",
                (int)((hostFileSize + 1023) / 1024),
                freeBlocks * geo.blockSize / 1024);
      MessageBox (hwndDiskViewWnd, msg, "Add file", MB_OK | MB_ICONERROR);
    }
    return;
  }

  /* check free directory entries */
  freeDirEntries = 0;
  for (i = 0; i < geo.maxDir; i++) {
    unsigned long off = geo.dirOffset + (unsigned long)i * 32;
    if (off + 32 > diskSize[drive]) break;
    if (diskData[drive][off] == CPM_EMPTY) freeDirEntries++;
  }
  if (entriesNeeded > freeDirEntries) {
    free (fileData);
    free (blockBitmap);
    MessageBox (hwndDiskViewWnd, "Not enough free directory entries on the disk.",
                "Add file", MB_OK | MB_ICONERROR);
    return;
  }

  /* allocate blocks */
  allocBlocks = (int *)malloc (blocksNeeded * sizeof (int));
  if (!allocBlocks) { free (fileData); free (blockBitmap); return; }
  allocated = 0;
  for (i = 0; i < totalBlocks && allocated < blocksNeeded; i++) {
    if (!blockBitmap[i]) allocBlocks[allocated++] = i;
  }
  free (blockBitmap);

  /* write file data into disk image blocks */
  for (i = 0; i < blocksNeeded; i++) {
    unsigned long diskOff = geo.dirOffset + (unsigned long)allocBlocks[i] * geo.blockSize;
    unsigned long remain  = hostFileSize - (unsigned long)i * geo.blockSize;
    unsigned long toWrite = (remain < (unsigned long)geo.blockSize) ? remain : (unsigned long)geo.blockSize;
    memcpy (diskData[drive] + diskOff, fileData + (unsigned long)i * geo.blockSize, toWrite);
    if (toWrite < (unsigned long)geo.blockSize)
      memset (diskData[drive] + diskOff + toWrite, 0x1A, geo.blockSize - toWrite);
  }
  free (fileData);

  /* create directory entries */
  exm = bytesPerEntry / 16384 - 1;
  if (exm < 0) exm = 0;
  blockIdx = 0;
  dirEntryIdx = 0;
  bytesRemaining = hostFileSize;

  for (e = 0; e < entriesNeeded; e++) {
    unsigned long dirOff;
    byte *entry;
    int blocksThisEntry, logExtent;
    unsigned long bytesThisEntry;
    int rc;

    /* find next free directory slot */
    while (dirEntryIdx < geo.maxDir) {
      dirOff = geo.dirOffset + (unsigned long)dirEntryIdx * 32;
      if (dirOff + 32 <= diskSize[drive] && diskData[drive][dirOff] == CPM_EMPTY) break;
      dirEntryIdx++;
    }
    if (dirEntryIdx >= geo.maxDir) break;

    dirOff = geo.dirOffset + (unsigned long)dirEntryIdx * 32;
    entry = diskData[drive] + dirOff;
    memset (entry, 0, 32);

    entry[0] = 0;                         /* user 0 */
    memcpy (entry + 1, cpm11, 11);        /* filename */

    logExtent = e * (exm + 1);
    entry[12] = (byte)(logExtent & 0x1F); /* EX */
    entry[13] = 0;                        /* S1 */
    entry[14] = (byte)((logExtent >> 5) & 0x3F); /* S2 */

    blocksThisEntry = blocksNeeded - blockIdx;
    if (blocksThisEntry > numAllocs) blocksThisEntry = numAllocs;
    bytesThisEntry = (unsigned long)blocksThisEntry * geo.blockSize;
    if (bytesThisEntry > bytesRemaining) bytesThisEntry = bytesRemaining;

    /* RC (record count) */
    if (e < entriesNeeded - 1) {
      rc = 0x80;  /* 128 records — full extent */
      if (exm > 0) entry[12] = (byte)((logExtent + exm) & 0x1F);
    } else {
      if (exm > 0 && bytesThisEntry > 16384) {
        entry[12] = (byte)((logExtent + 1) & 0x1F);
        rc = (int)((bytesThisEntry - 16384 + 127) / 128);
      } else {
        rc = (int)((bytesThisEntry + 127) / 128);
      }
      if (rc > 0x80) rc = 0x80;
    }
    entry[15] = (byte)rc;

    /* allocation map */
    for (j = 0; j < blocksThisEntry; j++) {
      int blk = allocBlocks[blockIdx + j];
      if (geo.singleByteAlloc) {
        entry[16 + j] = (byte)blk;
      } else {
        entry[16 + j * 2]     = (byte)(blk & 0xFF);
        entry[16 + j * 2 + 1] = (byte)((blk >> 8) & 0xFF);
      }
    }

    blockIdx += blocksThisEntry;
    bytesRemaining -= bytesThisEntry;
    dirEntryIdx++;
  }
  free (allocBlocks);

  /* save modified image back to the host file */
  saveDiskImageToFile (drive);

  /* force the viewer to refresh immediately */
  if (hwndDiskViewWnd) {
    selDrive = -1; selIndex = -1;
    InvalidateRect (hwndDiskViewWnd, NULL, FALSE);
  }

  {
    char fmtName[14];
    int k = 0;
    for (i = 0; i < 8 && cpm11[i] != ' '; i++) fmtName[k++] = tikiCharToWin (cpm11[i]);
    fmtName[k++] = '.';
    for (i = 8; i < 11 && cpm11[i] != ' '; i++) fmtName[k++] = tikiCharToWin (cpm11[i]);
    fmtName[k] = '\0';
    LOG_I ("Added file %s to drive %c", fmtName, 'A' + drive);
  }
}

/* paint a drive section, returns the y position after the last drawn line */
static int paintDrive (HDC hdc, int drive, int x, int y, int w, HFONT monoFont, HFONT headerFont) {
  char label[64];
  FileInfo files[MAX_FILES];

  /* header */
  SelectObject (hdc, headerFont);
  SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
  if (diskData[drive] && diskSize[drive] > 0)
    snprintf (label, sizeof(label), "Drive %c: (%luKB)", 'A' + drive, diskSize[drive] / 1024);
  else
    snprintf (label, sizeof(label), "Drive %c:", 'A' + drive);
  TextOut (hdc, x, y, label, (int)strlen (label));

  /* "Add file" button — right-aligned in header, only if a disk is loaded */
  if (diskData[drive] && diskSize[drive] > 0) {
    RECT ab;
    HFONT oldF;
    ab.left = x + w - ADDBTN_W - 14;
    ab.top  = y - 1;
    ab.right = ab.left + ADDBTN_W;
    ab.bottom = ab.top + ADDBTN_H;
    DrawFrameControl (hdc, &ab, DFC_BUTTON, DFCS_BUTTONPUSH);
    oldF = SelectObject (hdc, monoFont);
    SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
    DrawText (hdc, "Add file", 8, &ab, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject (hdc, oldF);
    btnAddFile[drive] = ab;
  } else {
    SetRectEmpty (&btnAddFile[drive]);
  }
  y += 22;

  /* separator line */
  MoveToEx (hdc, x, y, NULL);
  LineTo (hdc, x + w - 10, y);
  y += 6;

  SelectObject (hdc, monoFont);

  int fileCount = parseCpmDirectory (drive, files, MAX_FILES);
  if (fileCount < 0) {
    SetTextColor (hdc, RGB (160, 0, 0));
    TextOut (hdc, x, y, "NO DISK", 7);
    y += LINE_HEIGHT;
  } else if (fileCount == 0) {
    SetTextColor (hdc, RGB (100, 100, 100));
    TextOut (hdc, x, y, "(empty)", 7);
    y += LINE_HEIGHT;
  } else {
    /* column headers */
    SetTextColor (hdc, RGB (80, 80, 80));
    TextOut (hdc, x, y, "Name            Size", 20);
    y += 16;

    SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
    int i;
    for (i = 0; i < fileCount; i++) {
      char line[48];
      char sizeBuf[12];
      formatSize (files[i].size, sizeBuf, sizeof(sizeBuf));
      snprintf (line, sizeof(line), "%-12.12s  %s", files[i].name, sizeBuf);

      /* highlight selected row */
      if (drive == selDrive && i == selIndex) {
        RECT hlrc;
        hlrc.left = x - 2;
        hlrc.top = y;
        hlrc.right = x + w - 12;
        hlrc.bottom = y + LINE_HEIGHT;
        HBRUSH hlBr = CreateSolidBrush (GetSysColor (COLOR_HIGHLIGHT));
        FillRect (hdc, &hlrc, hlBr);
        DeleteObject (hlBr);
        SetTextColor (hdc, GetSysColor (COLOR_HIGHLIGHTTEXT));
      } else {
        SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
      }

      TextOut (hdc, x, y, line, (int)strlen (line));

      /* draw View / Dump buttons for selected row */
      if (drive == selDrive && i == selIndex) {
        int bx = x + 123;  /* after the size column */
        int by = y;
        RECT rv = {bx, by, bx + BTN_W, by + BTN_H};
        RECT rd = {bx + BTN_W + 4, by, bx + 2*BTN_W + 4, by + BTN_H};
        DrawFrameControl (hdc, &rv, DFC_BUTTON, DFCS_BUTTONPUSH);
        DrawFrameControl (hdc, &rd, DFC_BUTTON, DFCS_BUTTONPUSH);
        SetTextColor (hdc, GetSysColor (COLOR_BTNTEXT));
        SetBkMode (hdc, TRANSPARENT);
        DrawText (hdc, "View", 4, &rv, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawText (hdc, "Save", 4, &rd, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        /* store button positions in content coords for hit testing */
        btnView = rv;
        btnDump = rd;
        btnDrive = drive;
      }

      y += LINE_HEIGHT;
    }

    /* summary */
    y += 6;
    unsigned long totalSize = 0;
    for (i = 0; i < fileCount; i++) totalSize += files[i].size;
    SetTextColor (hdc, RGB (80, 80, 80));
    snprintf (label, sizeof(label), "%d file(s), %luKB total", fileCount, totalSize / 1024);
    TextOut (hdc, x, y, label, (int)strlen (label));
    y += LINE_HEIGHT;
  }
  return y;
}

/* compute total content height for both drives */
static int computeContentHeight (void) {
  FileInfo files[MAX_FILES];
  int maxY = 0;
  int drive;
  for (drive = 0; drive < 2; drive++) {
    int y = 10;  /* top margin */
    y += HEADER_HEIGHT;
    int fc = parseCpmDirectory (drive, files, MAX_FILES);
    if (fc <= 0) {
      y += LINE_HEIGHT; /* "NO DISK" or "(empty)" */
    } else {
      y += 16;  /* column header */
      y += fc * LINE_HEIGHT;
      y += SUMMARY_HEIGHT;
    }
    if (y > maxY) maxY = y;
  }
  return maxY + 10; /* bottom margin */
}

static void updateScrollbar (HWND hwnd) {
  RECT rc;
  GetClientRect (hwnd, &rc);
  int viewH = rc.bottom - rc.top;
  contentHeight = computeContentHeight ();
  SCROLLINFO si = {0};
  si.cbSize = sizeof (SCROLLINFO);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = contentHeight > viewH ? contentHeight - 1 : 0;
  si.nPage = viewH;
  if (scrollPos > si.nMax - (int)si.nPage + 1) scrollPos = si.nMax - (int)si.nPage + 1;
  if (scrollPos < 0) scrollPos = 0;
  si.nPos = scrollPos;
  SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
}

LRESULT CALLBACK DiskViewWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      hwndDiskViewWnd = hwnd;
      scrollPos = 0;
      SetTimer (hwnd, 1, 1000, NULL); /* refresh 1x/sec */
      return 0;
    case WM_SIZE:
      updateScrollbar (hwnd);
      return 0;
    case WM_TIMER:
      {
        updateScrollbar (hwnd);
        RECT rc;
        GetClientRect (hwnd, &rc);
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
          case SB_LINEUP:     pos -= LINE_HEIGHT; break;
          case SB_LINEDOWN:   pos += LINE_HEIGHT; break;
          case SB_PAGEUP:     pos -= si.nPage; break;
          case SB_PAGEDOWN:   pos += si.nPage; break;
          case SB_THUMBTRACK: pos = si.nTrackPos; break;
        }
        if (pos < 0) pos = 0;
        if (pos > si.nMax - (int)si.nPage + 1) pos = si.nMax - (int)si.nPage + 1;
        if (pos < 0) pos = 0;
        scrollPos = pos;
        si.fMask = SIF_POS;
        si.nPos = pos;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_MOUSEWHEEL:
      {
        int delta = GET_WHEEL_DELTA_WPARAM (wParam);
        scrollPos -= delta / WHEEL_DELTA * LINE_HEIGHT * 3;
        if (scrollPos < 0) scrollPos = 0;
        RECT rc;
        GetClientRect (hwnd, &rc);
        int viewH = rc.bottom - rc.top;
        int maxScroll = contentHeight - viewH;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollPos > maxScroll) scrollPos = maxScroll;
        SCROLLINFO si = {0};
        si.cbSize = sizeof (SCROLLINFO);
        si.fMask = SIF_POS;
        si.nPos = scrollPos;
        SetScrollInfo (hwnd, SB_VERT, &si, TRUE);
        InvalidateRect (hwnd, NULL, FALSE);
      }
      return 0;
    case WM_LBUTTONDOWN:
      {
        int mx = (int)(short)LOWORD (lParam);
        int my = (int)(short)HIWORD (lParam);
        int contentY = my + scrollPos;
        RECT rc;
        int halfW, d;
        GetClientRect (hwnd, &rc);
        halfW = (rc.right - rc.left) / 2;

        /* check "Add file" buttons */
        for (d = 0; d < 2; d++) {
          if (!IsRectEmpty (&btnAddFile[d]) &&
              mx >= btnAddFile[d].left && mx < btnAddFile[d].right &&
              contentY >= btnAddFile[d].top && contentY < btnAddFile[d].bottom) {
            handleAddFile (d);
            return 0;
          }
        }

        /* check if a View/Save button was clicked (in content coords) */
        if (selDrive >= 0 && selIndex >= 0 && btnDrive == selDrive) {
          int btnY = contentY;
          if (btnY >= btnView.top && btnY < btnView.bottom) {
            FileInfo files[MAX_FILES];
            int fc = parseCpmDirectory (selDrive, files, MAX_FILES);
            if (fc > 0 && selIndex < fc) {
              /* view button */
              if (mx >= btnView.left && mx < btnView.right) {
                openFileViewer (selDrive, &files[selIndex]);
                return 0;
              }
              /* dump button */
              if (mx >= btnDump.left && mx < btnDump.right) {
                dumpFileData (selDrive, &files[selIndex]);
                return 0;
              }
            }
          }
        }

        /* otherwise, select a file row */
        {
          int drive = (mx < halfW) ? 0 : 1;
          FileInfo files[MAX_FILES];
          int fileCount = parseCpmDirectory (drive, files, MAX_FILES);
          if (fileCount <= 0) {
            selDrive = -1; selIndex = -1;
            InvalidateRect (hwnd, NULL, FALSE);
            break;
          }
          {
            int firstFileY = 10 + HEADER_HEIGHT + 16;
            int idx = (contentY - firstFileY) / LINE_HEIGHT;
            if (contentY >= firstFileY && idx >= 0 && idx < fileCount) {
              selDrive = drive;
              selIndex = idx;
            } else {
              selDrive = -1;
              selIndex = -1;
            }
          }
          InvalidateRect (hwnd, NULL, FALSE);
        }
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
        SetBkMode (hdc, TRANSPARENT);

        /* apply scroll offset */
        SetViewportOrgEx (hdc, 0, -scrollPos, NULL);

        HFONT monoFont = CreateFont (13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT headerFont = CreateFont (16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

        int halfW = cw / 2;
        paintDrive (hdc, 0, 10, 10, halfW, monoFont, headerFont);
        paintDrive (hdc, 1, halfW + 5, 10, halfW, monoFont, headerFont);

        /* reset viewport before drawing divider (full height) */
        SetViewportOrgEx (hdc, 0, 0, NULL);
        /* vertical divider */
        HPEN pen = CreatePen (PS_SOLID, 1, GetSysColor (COLOR_GRAYTEXT));
        HPEN oldPen = SelectObject (hdc, pen);
        MoveToEx (hdc, halfW, 0, NULL);
        LineTo (hdc, halfW, ch);
        SelectObject (hdc, oldPen);
        DeleteObject (pen);

        DeleteObject (monoFont);
        DeleteObject (headerFont);
        BitBlt (hdcScreen, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);
        SelectObject (hdc, hbmOld);
        DeleteObject (hbmBuf);
        DeleteDC (hdc);
        EndPaint (hwnd, &ps);
      }
      return 0;
    case WM_DESTROY:
      KillTimer (hwnd, 1);
      hwndDiskViewWnd = NULL;
      if (pHwndDiskView) *pHwndDiskView = NULL;
      return 0;
  }
  return DefWindowProc (hwnd, msg, wParam, lParam);
}
