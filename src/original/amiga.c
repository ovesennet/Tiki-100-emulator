/* amiga.c V1.1.0
 *
 * Amiga systemspesifikk kode for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"

#include <stdio.h>
#include <stdlib.h>

#include <graphics/gfxbase.h>
#include <clib/intuition_protos.h>
#include <clib/asl_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/locale_protos.h>

#define CATCOMP_NUMBERS
#define CATCOMP_STRINGS
#define CATCOMP_BLOCK
#define CATCOMP_CODE
#include "amiga_strings.h"

#define DEFAULT_DIR       "PROGDIR:plater"
#define STATUSBAR_HEIGHT  15

/* protos */

int main (int argc, char *argv[]);
static struct Menu *CreateLocMenus(struct NewMenu *newMenus, ULONG tag, ...);
static void createStatusBar (void);
static void draw3dBox (int x, int y, int w, int h, boolean raise);
static void doIDCMP (void);
static void loadDiskImage (int drive);
static void saveDiskImage (int drive);

/* variabler */

static byte keyTable[0x80] = {
  KEY_GRAFIKK, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x30, 0x2b, 0x40, KEY_UTVID, KEY_NONE, KEY_NUM0,
  0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69,
  0x6f, 0x70, 0xe5, 0x5e, KEY_NONE, KEY_NUM1, KEY_NUM2, KEY_NUM3,
  0x61, 0x73, 0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b,
  0x6c, 0xf8, 0xe6, 0x27, KEY_NONE, KEY_NUM4, KEY_NUM5, KEY_NUM6,
  0x3c, 0x7a, 0x78, 0x63, 0x76, 0x62, 0x6e, 0x6d,
  0x2c, 0x2e, 0x2d, KEY_NONE, KEY_NUMDOT, KEY_NUM7, KEY_NUM8, KEY_NUM9,
  KEY_SPACE, KEY_SLETT, KEY_BRYT, KEY_ENTER, KEY_CR, KEY_ANGRE, KEY_HOME, KEY_NONE,
  KEY_NONE, KEY_NONE, KEY_NUMMINUS, KEY_NONE, KEY_UP, KEY_DOWN, KEY_RIGHT, KEY_LEFT,
  KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_PGUP, KEY_PGDOWN,
  KEY_TABLEFT, KEY_TABRIGHT, KEY_NUMPERCENT, KEY_NUMEQU, KEY_NUMDIV, KEY_NUMMULT, KEY_NUMPLUS, KEY_HJELP,
  KEY_SHIFT, KEY_SHIFT, KEY_NONE, KEY_CTRL, KEY_LOCK, KEY_NONE, KEY_NONE, KEY_NONE,
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE,
  KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE, KEY_NONE
};

static struct NewMenu mn[] = {
  {NM_TITLE, (char *)MSG_EMULATOR_MENU,       0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_EMULATOR_RESET,      0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_EMULATOR_ABOUT,      0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_EMULATOR_QUIT,       0, 0, 0, 0, },

  {NM_TITLE, (char *)MSG_DISKDRIVE_MENU,      0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_LOADA,     0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_LOADB,     0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_SAVEA,     0, NM_ITEMDISABLED, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_SAVEB,     0, NM_ITEMDISABLED, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_REMOVEA,   0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_DISKDRIVE_REMOVEB,   0, 0, 0, 0, },

  {NM_TITLE, (char *)MSG_OPTIONS_MENU,        0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_OPTIONS_SLOWDOWN,    0, NM_ITEMDISABLED | CHECKIT | MENUTOGGLE, 0, 0, },
  {NM_ITEM,  (char *)MSG_OPTIONS_SCANLINES,   0, CHECKIT | MENUTOGGLE, 0, 0, },
  {NM_ITEM,  (char *)MSG_OPTIONS_SIZE40,      0, 0, 0, 0, },
  {NM_SUB,   (char *)MSG_OPTIONS_SIZE40_ONE,  0, CHECKIT | CHECKED, ~1, 0, },
  {NM_SUB,   (char *)MSG_OPTIONS_SIZE40_TWO,  0, CHECKIT, ~2, 0, },
  {NM_SUB,   (char *)MSG_OPTIONS_SIZE40_FOUR, 0, CHECKIT, ~4, 0, },
  {NM_ITEM,  (char *)MSG_OPTIONS_SIZE80,      0, 0, 0, 0, },
  {NM_SUB,   (char *)MSG_OPTIONS_SIZE80_ONE,  0, CHECKIT | CHECKED, ~1, 0, },
  {NM_SUB,   (char *)MSG_OPTIONS_SIZE80_TWO,  0, CHECKIT, ~2, 0, },

#ifdef DEBUG
  {NM_TITLE, (char *)MSG_DEBUG_MENU,          0, 0, 0, 0, },
  {NM_ITEM,  (char *)MSG_DEBUG_MONITOR,       0, 0, 0, 0, },
#endif

  {NM_END,   NULL,                            0, 0, 0, 0, },
};

static byte pressedKeys[0x100];
static struct Screen *pubScreen;
static struct Window *window;
static struct Menu *menuStrip;
static long pens[16];
static struct LocaleInfo localeInfo;
struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;
struct Library *AslBase;
struct Library *LocaleBase;
static int topOffset, leftOffset;
static int borderX, borderY;
static int resolution = MEDRES;
static BOOL scanlines = FALSE;
static int times40 = 1;
static int times80 = 1;
static byte *dsk[2];
static ULONG fileSize[2];
static int width;
static int height;
static boolean changingWinSize;
static boolean lock = FALSE;
static boolean grafikk = FALSE;
static boolean disk[2] = {FALSE, FALSE};

/*****************************************************************************/

int main (int argc, char *argv[]) {
  memset (pressedKeys, 1, 0x100);

  /* åpne bibliotek */
  if ((AslBase = (struct Library *)OpenLibrary ("asl.library", 37L))) {
    if ((IntuitionBase = (struct Library *)OpenLibrary ("intuition.library", 39L))) {
      if ((GfxBase = (struct Library *)OpenLibrary ("graphics.library", 39L))) {
        if ((GadToolsBase = (struct Library *)OpenLibrary ("gadtools.library", 39L))) {
          if ((LocaleBase = (struct Library *)OpenLibrary ("locale.library", 38L))) {

            /* finn katalog */
            localeInfo.li_LocaleBase = LocaleBase;
            localeInfo.li_Catalog = OpenCatalog (NULL, "tikiemul.catalog",
                                                 OC_BuiltInLanguage, "english", TAG_DONE);

            /* skaff public screen */
            if ((pubScreen = LockPubScreen (NULL))) {

              /* finn vindusrammetykkelse */
              topOffset = pubScreen->WBorTop + pubScreen->Font->ta_YSize + 1;
              leftOffset = pubScreen->WBorLeft;
              borderX = leftOffset + pubScreen->WBorRight;
              borderY = topOffset + pubScreen->WBorBottom + STATUSBAR_HEIGHT;

              /* finn vindusstørrelse */
              width = 512 * times80;
              height = 256 * times80 * (scanlines ? 2 : 1);

              /* åpne vindu */
              if ((window = (struct Window *)OpenWindowTags (NULL, WA_Left, 0, WA_Top, 0,
                                                             WA_Width, width + borderX,
                                                             WA_Height, height + borderY,
                                                             WA_Activate, TRUE, WA_SmartRefresh, TRUE,
                                                             WA_Title, "TIKI-100_emul",
                                                             WA_PubScreen, pubScreen,
                                                             WA_NewLookMenus, TRUE, WA_DepthGadget, TRUE,
                                                             WA_CloseGadget, TRUE, WA_DragBar, TRUE,
                                                             WA_IDCMP, IDCMP_RAWKEY | IDCMP_MENUPICK |
                                                             IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
                                                             TAG_DONE))) {

                /* skaff 16 pens */
                {
                  BOOL cont = TRUE;
                  int i;

                  for (i = 0; i < 16; i++) {
                    if ((pens[i] = ObtainPen (pubScreen->ViewPort.ColorMap, -1, 0, 0, 0, PEN_EXCLUSIVE))
                        == -1) {
                      cont = FALSE;
                    }
                  }
                  if (cont) {

                    /* sett bakgrunnsfarge til farge 0 */
                    SetAPen (window->RPort, pens[0]);
                    SetBPen (window->RPort, pens[0]);
                    RectFill (window->RPort, leftOffset,
                              topOffset, width - 1 + leftOffset,
                              height - 1 + topOffset);

                    createStatusBar();

                    /* lag menystripe */
                    {
                      APTR *visInfo;
                      if ((visInfo = (APTR)GetVisualInfo (window->WScreen, TAG_END))) {
                        if ((menuStrip = (struct Menu *)CreateLocMenus (mn, TAG_END))) {
                          if (LayoutMenus (menuStrip, visInfo, GTMN_NewLookMenus, TRUE, TAG_END)) {
                            if (SetMenuStrip (window, menuStrip)) {

                              /* kjør emulator */
                              runEmul();

                              /* avlutt */
                              ClearMenuStrip (window);
                            }
                            FreeMenus (menuStrip);
                          }
                        }
                        FreeVisualInfo (visInfo);
                      }
                    }
                  }
                  for (i = 0; i < 16; i++) {
                    ReleasePen (pubScreen->ViewPort.ColorMap, pens[i]);
                  }
                }
                CloseWindow (window);
              }
              UnlockPubScreen (NULL, pubScreen);
            }
            CloseCatalog (localeInfo.li_Catalog);
            CloseLibrary (LocaleBase);
          }
          CloseLibrary (GadToolsBase);
        }
        CloseLibrary (GfxBase);
      }
      CloseLibrary (IntuitionBase);
    }
    CloseLibrary (AslBase);
  }
  free (dsk[0]);
  free (dsk[1]);
  return 0;
}
/* Tatt (nesten) direkte fra NDK
 * Lokalisert CreateMenus()
 */
static struct Menu *CreateLocMenus(struct NewMenu *newMenus, ULONG tag, ...) {
  UWORD i = 0;
  struct NewMenu *nm;
  struct Menu *menus;

  /* gå til slutten */
  while (newMenus[i++].nm_Type != NM_END) {}

  /* alloker minne til menystruktur */
  if (!(nm = AllocVec (sizeof (struct NewMenu) * i, MEMF_CLEAR | MEMF_PUBLIC)))
    return NULL;

  /* sett alle strenger i menystruktur */
  while (i--) {
    nm[i] = newMenus[i];

    if (nm[i].nm_Label != NM_BARLABEL) {
      nm[i].nm_CommKey = GetString (&localeInfo, (LONG)nm[i].nm_Label);

      nm[i].nm_Label = nm[i].nm_CommKey + 2;

      if (nm[i].nm_CommKey[0] == ' ')
        nm[i].nm_CommKey = NULL;
    }
  }

  /* lag meny */
  menus = CreateMenusA (nm, (struct TagItem *)&tag);

  FreeVec (nm);

  return (menus);
}
/* lager statusbar */
static void createStatusBar (void) {
  SetAPen (window->RPort, 0);
  RectFill (window->RPort, leftOffset, height + topOffset,
            width - 1 + leftOffset, height - 1 + STATUSBAR_HEIGHT + topOffset);

  draw3dBox (0, height, width, STATUSBAR_HEIGHT, TRUE);

  draw3dBox (20, height + 3, 9, 9, FALSE);
  draw3dBox (40, height + 3, 9, 9, FALSE);
  draw3dBox (60, height + 3, 9, 9, FALSE);
  draw3dBox (80, height + 3, 9, 9, FALSE);

  lockLight (lock);
  grafikkLight (grafikk);
  diskLight (0, disk[0]);
  diskLight (1, disk[1]);
}
/* lager en 3d-boks */
static void draw3dBox (int x, int y, int w, int h, boolean raise) {
  long pen1 = (raise ? 2 : 1);
  long pen2 = (raise ? 1 : 2);

  SetAPen (window->RPort, pen1);
  Move (window->RPort, x + leftOffset, y + h - 1 + topOffset);
  Draw (window->RPort, x + leftOffset, y + topOffset);
  Draw (window->RPort, x + w - 1 + leftOffset, y + topOffset);
  SetAPen (window->RPort, pen2);
  Draw (window->RPort, x + w - 1 + leftOffset, y + h - 1 + topOffset);
  Draw (window->RPort, x + leftOffset, y + h - 1 + topOffset);
}

/* Forandre oppløsning */
void changeRes (int newRes) {
  resolution = newRes;
  switch (resolution) {
    case LOWRES:
      width = 256 * times40;
      height = 256 * times40;
      break;
    case MEDRES:
      width = 512 * times80;
      height = 256 * times80 * (scanlines ? 2 : 1);
      break;
    case HIGHRES:
      width = 1024;
      height = 256 * (scanlines ? 4 : 1);
      break;
  }
  ChangeWindowBox (window, 0, 0, width + borderX, height + borderY);
  /* må vente til vindu har fått ny størrelse */
  changingWinSize = TRUE;
  while (changingWinSize) {
    Wait (1L << window->UserPort->mp_SigBit);
    doIDCMP();
  }
  /* rensk vindu */
  SetAPen (window->RPort, pens[0]);
  RectFill (window->RPort, leftOffset, topOffset, width - 1 + leftOffset, height - 1 + topOffset);

  createStatusBar();
}
/* Plotter en pixel med farge tatt fra pallett */
void plotPixel (int x, int y, int color) {
  SetAPen (window->RPort, pens[color]);
  switch (resolution) {
    case LOWRES:
      x *= times40;
      y *= times40;
      break;
    case MEDRES:
      x *= times80;
      y *= times80 * (scanlines ? 2 : 1);
      break;
    case HIGHRES:
      y *= scanlines ? 4 : 1;
      break;
  }
  WritePixel (window->RPort, x + leftOffset, y + topOffset);
}
/* Scroller skjerm 'distance' linjer oppover */
void scrollScreen (int distance) {
  switch (resolution) {
    case LOWRES:
      distance *= times40;
      break;
    case MEDRES:
      distance *= times80 * (scanlines ? 2 : 1);
      break;
    case HIGHRES:
      distance *= scanlines ? 4 : 1;
      break;
  }
  ScrollRaster (window->RPort, 0, distance, leftOffset, topOffset,
                width - 1 + leftOffset, height - 1 + topOffset);
}
/* Ny farge, gitt pallett nummer og intensitet 0-255 */
void changePalette (int colornumber, byte red, byte green, byte blue) {
  SetRGB32 (&(pubScreen->ViewPort), pens[colornumber], red << 24, green << 24, blue << 24);
}
/* Kalles periodisk. Lar system kode måle / senke emuleringshastighet
 * Kan også brukes til sjekk av brukeraktivitet
 * ms er antall "emulerte" millisekunder siden forrige gang loopEmul ble kalt
 */
void loopEmul (int ms) {
  doIDCMP();
}
/* Prosesserer alle nye IDCMP-events */
static void doIDCMP (void) {
  struct IntuiMessage *msg;

  while ((msg = (struct IntuiMessage *)GetMsg (window->UserPort))) {
    struct MenuItem *item;
    UWORD menuNumber;

    switch (msg->Class) {
      case IDCMP_MENUPICK:
        menuNumber = msg->Code;
        while (menuNumber != MENUNULL) {
          item = (struct MenuItem *)ItemAddress (menuStrip, menuNumber);
          switch (MENUNUM (menuNumber)) {
            case 0:  /* emulator */
              switch (ITEMNUM (menuNumber)) {
                case 0:  /* reset */
                  resetEmul();
                  break;
                case 1: { /* om */
                  struct EasyStruct omReq = {
                    sizeof (struct EasyStruct), 0,
                    GetString (&localeInfo, MSG_ABOUTBOX_TITLE),
                    GetString (&localeInfo, MSG_ABOUTBOX_TEXT),
                    GetString (&localeInfo, MSG_ABOUTBOX_BUTTON)
                  };
                  EasyRequest (NULL, &omReq, NULL);
                  break;
                }
                case 2:  /* avslutt */
                  quitEmul();
                  break;
              }
              break;
            case 1:  /* diskettstasjon */
              switch (ITEMNUM (menuNumber)) {
                case 0:  /* hent plate a */
                  loadDiskImage (0);
                  break;
                case 1:  /* hent plate b */
                  loadDiskImage (1);
                  break;
                case 2:  /* lagre plate a */
                  saveDiskImage (0);
                  break;
                case 3:  /* lagre plate b */
                  saveDiskImage (1);
                  break;
                case 4:  /* fjern plate a */
                  removeDisk (0);
                  OffMenu (window, FULLMENUNUM (1, 2, 0));
                  break;
                case 5:  /* fjern plate b */
                  removeDisk (1);
                  OffMenu (window, FULLMENUNUM (1, 3, 0));
                  break;
              }
              break;
            case 2:  /* innstillinger */
              switch (ITEMNUM (menuNumber)) {
                case 0:  /* reduser hastighet */
                  /* ikke implementert, har for treg maskin :-/ */
                  break;
                case 1:  /* bevar forhold */
                  scanlines = item->Flags & CHECKED;
                  changeRes (resolution);
                  resetEmul();
                  break;
                case 2:  /* forstørr 40 modus */
                  switch (SUBNUM (menuNumber)) {
                    case 0:  /* 1 */
                      times40 = 1;
                      break;
                    case 1:  /* 2 */
                      times40 = 2;
                      break;
                    case 2:  /* 4 */
                      times40 = 4;
                      break;
                  }
                  changeRes (resolution);
                  resetEmul();
                  break;
                case 3:  /* forstærr 80 modus */
                  switch (SUBNUM (menuNumber)) {
                    case 0:  /* 1 */
                      times80 = 1;
                      break;
                    case 1:  /* 2 */
                      times80 = 2;
                      break;
                  }
                  changeRes (resolution);
                  resetEmul();
                  break;
              }
              break;
#ifdef DEBUG
            case 3:  /* debug */
              switch (ITEMNUM (menuNumber)) {
                case 0:  /* monitor */
                  trace();
                  break;
              }
              break;
#endif
          }
          menuNumber = item->NextSelect;
        }
        break;
      case IDCMP_RAWKEY:
        if (msg->Code < 0x80) {
          pressedKeys[keyTable[msg->Code]] = 0;
        } else {
          pressedKeys[keyTable[msg->Code - 0x80]] = 1;
        }
        break;
      case IDCMP_CLOSEWINDOW:
        quitEmul();
        break;
      case IDCMP_NEWSIZE:
        changingWinSize = FALSE;
        break;
    }
    ReplyMsg ((struct Message *)msg);
  }
}
/* Hent inn diskbilde fra fil */
static void loadDiskImage (int drive) {
  struct FileRequester *fr;
  static char drawer[256] = DEFAULT_DIR;   /* hvilken skuffe filreq. skal åpne i */

  if ((fr = (struct FileRequester *)AllocAslRequestTags (ASL_FileRequest, ASLFR_DoPatterns,
                                                         TRUE, ASLFR_InitialPattern, "~(#?.info)",
                                                         ASLFR_InitialDrawer, drawer, TAG_DONE))) {
    if (AslRequest (fr, NULL)) {
      char fileName[256];
      strcpy (fileName, fr->rf_Dir);
      strcpy (drawer, fr->rf_Dir);
      if (AddPart (fileName, fr->rf_File, 256)) {
        BPTR fp;
        if ((fp = Open ((STRPTR)&fileName, MODE_OLDFILE))) {
          struct FileInfoBlock *fileInfo = 0L;
          if ((fileInfo = (struct FileInfoBlock *)AllocDosObject (DOS_FIB, TAG_DONE))) {
            if (ExamineFH (fp, fileInfo)) {
              FILE *f;
              fileSize[drive] = fileInfo->fib_Size;
              free (dsk[drive]);
              OnMenu (window, FULLMENUNUM (1, (drive + 2), 0));
              if ((dsk[drive] = (byte *)malloc (fileSize[drive]))) {
                if ((f = fopen (fileName, "r"))) {
                  if (fread (dsk[drive], 1, fileSize[drive], f)) {
                    switch (fileSize[drive]) {
                      case 40*1*18*128:
                        insertDisk (drive, dsk[drive], 40, 1, 18, 128);
                        break;
                      case 40*1*10*512:
                        insertDisk (drive, dsk[drive], 40, 1, 10, 512);
                        break;
                      case 40*2*10*512:
                        insertDisk (drive, dsk[drive], 40, 2, 10, 512);
                        break;
                      case 80*2*10*512:
                        insertDisk (drive, dsk[drive], 80, 2, 10, 512);
                        break;
                      default:
                        removeDisk (drive);
                        OffMenu (window, FULLMENUNUM (1, (drive + 2), 0));
                    }
                  } else {
                    removeDisk (drive);
                    OffMenu (window, FULLMENUNUM (1, (drive + 2), 0));
                  }
                  fclose (f);
                } else {
                  removeDisk (drive);
                  OffMenu (window, FULLMENUNUM (1, (drive + 2), 0));
                }
              } else {
                removeDisk (drive);
                OffMenu (window, FULLMENUNUM (1, (drive + 2), 0));
              }
            }
            FreeDosObject (DOS_FIB, fileInfo);
          }
          Close (fp);
        }
      }
    }
    FreeAslRequest (fr);
  }
}
/* Lagre diskbilde til fil */
static void saveDiskImage (int drive) {
  struct FileRequester *fr;
  FILE *fp;
  static char drawer[256] = DEFAULT_DIR;   /* hvilken skuffe filreq. skal åpne i */

  if ((fr = (struct FileRequester *)AllocAslRequestTags (ASL_FileRequest, ASL_FuncFlags, FILF_SAVE,
                                                         ASLFR_DoPatterns, TRUE, ASLFR_InitialPattern,
                                                         "~(#?.info)", ASLFR_InitialDrawer, drawer,
                                                         TAG_DONE))) {
    if (AslRequest (fr, NULL)) {
      char fileName[256];
      strcpy (fileName, fr->rf_Dir);
      strcpy (drawer, fr->rf_Dir);
      if (AddPart (fileName, fr->rf_File, 256)) {
        if ((fp = fopen (fileName, "w"))) {
          fwrite (dsk[drive], 1, fileSize[drive], fp);
          fclose (fp);
        }
      }
    }
    FreeAslRequest (fr);
  }
}
/* Tenn/slukk lock lys */
void lockLight (boolean status) {
  lock = status; 
  SetAPen (window->RPort, status ? 3 : 0);
  RectFill (window->RPort, 21 + leftOffset, height + 4 + topOffset,
            27 + leftOffset, height + 10 + topOffset);
}
/* Tenn/slukk grafikk lys */
void grafikkLight (boolean status) {
  grafikk = status;
  SetAPen (window->RPort, status ? 3 : 0);
  RectFill (window->RPort, 41 + leftOffset, height + 4 + topOffset,
            47 + leftOffset, height + 10 + topOffset);
}
/* Tenn/slukk disk lys for gitt stasjon */
void diskLight (int drive, boolean status) {
  int x = (drive ? 80 : 60);
  disk[drive] = status;
  SetAPen (window->RPort, status ? 3 : 0);
  RectFill (window->RPort, x + 1 + leftOffset, height + 4 + topOffset,
            x + 7 + leftOffset, height + 10 + topOffset);
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
/* Setter seriekanalparametre */
void setParams (struct serParams *p1Params, struct serParams *p2Params) {
  /* ikke implementert */
}
/* Send tegn til seriekanal */
void sendChar (int port, byte value) {
  /* ikke implementert */
}
/* Hent tegn fra seriekanal */
byte getChar (int port) {
  /* ikke implementert */
  return 0;
}
/* Send tegn til skriver */
void printChar (byte value) {
  /* ikke implementert */
}
