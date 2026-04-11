/* unix.c V1.1.1
 *
 * Unix (med X11) systemspesifikk kode for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2001
 */

#include "TIKI-100_emul.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#ifndef FIONREAD
#include <sys/filio.h>  /* antar at FIONREAD er definert i sys/filio.h */
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>

#define STATUSBAR_HEIGHT 19

/* protos */
int main (int argc, char *argv[]);
static void update (int x, int y, int w, int h);
static void createStatusBar (void);
static byte keysymToTikikey (KeySym keysym);
static void commandline (void);
static void readDiskImage (int drive, char *filename);
static void setParam (int portfd, struct serParams *params);
static void setPrintCmd (char *p);
static void setPrintFile (char *p);

/* variabler */

static byte *dsk[2] = {NULL, NULL};
static int dsksize[2] = {0, 0};
static int resolution = MEDRES;
static byte pressedKeys[256];
static boolean updateWindow = FALSE;
static unsigned int xmin = (unsigned)~0;
static unsigned int ymin = (unsigned)~0;
static unsigned int xmax = 0;
static unsigned int ymax = 0;
static boolean scanlines;
static boolean slowDown;
static int width[3];
static int height[3];
static int times40;
static int times80;
static byte *screen;
static FILE *printCmd = 0;
static FILE *printFile = 0;
static int port1 = 0;
static int port2 = 0;
static boolean lock = FALSE;
static boolean grafikk = FALSE;
static boolean disk[2] = {FALSE, FALSE};
static boolean dyn_cmap;
static Colormap default_cmap;
static unsigned long pixels[16];
static unsigned long blackpix, whitepix, backpix, diodepix;
static Display *display;
static Window mainwindow;
static Pixmap mainpixmap;
static GC colorGC[16];
static GC blackGC, backGC, diodeGC;
static XrmDatabase options_db = NULL;
static XSizeHints xsh;

static XrmOptionDescRec options[] = {
  {"-display", "*display", XrmoptionSepArg, NULL},
  {"-d", "*display", XrmoptionSepArg, NULL},
  {"-geometry", "*geometry", XrmoptionSepArg, NULL},
  {"-g", "*geometry", XrmoptionSepArg, NULL},
  {"-diska", "*diska", XrmoptionSepArg, NULL},
  {"-diskb", "*diskb", XrmoptionSepArg, NULL},
  {"-bevarforhold", "*bevarforhold", XrmoptionNoArg, "on"},
  {"-ikkebevarforhold", "*bevarforhold", XrmoptionNoArg, "off"},
  {"-40x", "*40x", XrmoptionSepArg, NULL},
  {"-80x", "*80x", XrmoptionSepArg, NULL},
  {"-begrens", "*begrens", XrmoptionNoArg, "on"},
  {"-ikkebegrens", "*begrens", XrmoptionNoArg, "off"},
  {"-port1", "*port1", XrmoptionSepArg, NULL},
  {"-port2", "*port2", XrmoptionSepArg, NULL},
  {"-pk", "*pk", XrmoptionSepArg, NULL},
  {"-pf", "*pf", XrmoptionSepArg, NULL},
  {"-st28b", "*st28b", XrmoptionNoArg, "on"},
  {"-ikkest28b", "*st28b", XrmoptionNoArg, "off"}
};

static int num_opts = (sizeof options / sizeof options[0]);

typedef struct KeyConv {
  byte tikikey;
  KeySym keysym;
} KeyConv;

/* Tabell som konverterer X11-keysym til TIKI-100 tasteverdier */
static const KeyConv keyConvTable[] = {
  {KEY_CTRL,    XK_Control_L},   {KEY_CTRL,       XK_Control_R}, {KEY_SHIFT,    XK_Shift_L},
  {KEY_SHIFT,   XK_Shift_R},     {KEY_BRYT,       XK_Break},     {KEY_BRYT,     XK_F10},
  {KEY_CR,      XK_Return},      {KEY_SPACE,      XK_space},     {KEY_SLETT,    XK_BackSpace},
  {KEY_SLETT,   XK_Delete},      {KEY_GRAFIKK,    XK_F8},        {KEY_ANGRE,    XK_Redo},
  {KEY_ANGRE,   XK_F9},          {KEY_LOCK,       XK_Caps_Lock}, {KEY_LOCK,     XK_Shift_Lock},
  {KEY_LOCK,    XK_F7},          {KEY_HJELP,      XK_Help},      {KEY_HJELP,    XK_F11},
  {KEY_LEFT,    XK_Left},        {KEY_UTVID,      XK_Insert},    {KEY_UTVID,    XK_F12},
  {KEY_F1,      XK_F1},          {KEY_F4,         XK_F4},        {KEY_RIGHT,    XK_Right},
  {KEY_F2,      XK_F2},          {KEY_F3,         XK_F3},        {KEY_F5,       XK_F5},
  {KEY_F6,      XK_F6},          {KEY_DOWN,       XK_Down},      {KEY_PGUP,     XK_Page_Up},
  {KEY_PGDOWN,  XK_Page_Down},   {KEY_UP,         XK_Up},        {KEY_HOME,     XK_Home},
  {KEY_TABLEFT, XK_Prior},       {KEY_TABRIGHT,   XK_Tab},       {KEY_TABRIGHT, XK_Next},
  {KEY_NUMDIV,  XK_KP_Divide},   {KEY_NUMPLUS,    XK_KP_Add},    {KEY_NUMMINUS, XK_KP_Subtract},
  {KEY_NUMMULT, XK_KP_Multiply}, {KEY_NUMPERCENT, XK_percent},   {KEY_NUMEQU,   XK_KP_Equal},
  {KEY_ENTER,   XK_KP_Enter},    {KEY_NUM0,       XK_KP_0},      {KEY_NUM1,     XK_KP_1},
  {KEY_NUM2,    XK_KP_2},        {KEY_NUM3,       XK_KP_3},      {KEY_NUM4,     XK_KP_4},
  {KEY_NUM5,    XK_KP_5},        {KEY_NUM6,       XK_KP_6},      {KEY_NUM7,     XK_KP_7},
  {KEY_NUM8,    XK_KP_8},        {KEY_NUM9,       XK_KP_9},      {KEY_NUMDOT,   XK_KP_Separator},
  {KEY_NUMDOT,  XK_KP_Decimal},  {(byte)'a',      XK_a},         {(byte)'b',    XK_b},
  {(byte)'c',   XK_c},           {(byte)'d',      XK_d},         {(byte)'e',    XK_e},
  {(byte)'f',   XK_f},           {(byte)'g',      XK_g},         {(byte)'h',    XK_h},
  {(byte)'i',   XK_i},           {(byte)'j',      XK_j},         {(byte)'k',    XK_k},
  {(byte)'l',   XK_l},           {(byte)'m',      XK_m},         {(byte)'n',    XK_n},
  {(byte)'o',   XK_o},           {(byte)'p',      XK_p},         {(byte)'q',    XK_q},
  {(byte)'r',   XK_r},           {(byte)'s',      XK_s},         {(byte)'t',    XK_t},
  {(byte)'u',   XK_u},           {(byte)'v',      XK_v},         {(byte)'w',    XK_w},
  {(byte)'x',   XK_x},           {(byte)'y',      XK_y},         {(byte)'z',    XK_z},
  {(byte)'1',   XK_1},           {(byte)'2',      XK_2},         {(byte)'3',    XK_3},
  {(byte)'4',   XK_4},           {(byte)'5',      XK_5},         {(byte)'6',    XK_6},
  {(byte)'7',   XK_7},           {(byte)'8',      XK_8},         {(byte)'9',    XK_9},
  {(byte)'0',   XK_0},           {(byte)'<',      XK_less},      {(byte)',',    XK_comma},
  {(byte)'.',   XK_period},      {(byte)'-',      XK_minus},     {(byte)'æ',    XK_ae},
  {(byte)'ø',   XK_oslash},      {(byte)'å',      XK_aring},     {(byte)'\'',   XK_apostrophe},
  {(byte)'^',   XK_asciicircum}, {(byte)'+',      XK_plus},      {(byte)'@',    XK_at}
};

/*****************************************************************************/

int main (int argc, char *argv[]) {
  char *resType;     /* for resourcekall */
  XrmValue resValue; /* -- " -- */
  XrmDatabase cline_db = NULL;
  
  XrmInitialize();

  /* parse kommandolinjeopsoner */
  XrmParseCommand (&cline_db, options, num_opts, "tikiemul", &argc, argv);

  /* feil i argumenter */
  if (argc > 1) {
    printf ("TIKI-100_emul V1.1.1 brukes:\n"
            "    tikiemul [-opsjoner...]\n"
            "\n"
            "Opsjoner:\n"
            "    -display displaynavn      X-server som skal benyttes\n"
            "    -d displaynavn            Forkortelse for den over\n"
            "    -geometry geom            Vindusposisjon\n"
            "    -g geom                   Forkortelse for den over\n"
            "    -diska diskettfilnavn     Diskettfil i stasjon a\n"
            "    -diskb diskettfilnavn     Diskettfil i stasjon b\n"
            "    -bevarforhold             Emulatorvindu formlik TIKI-100 skjerm\n"
            "    -ikkebevarforhold         Negasjonen av den over\n"
            "    -40x forstørrelsesfaktor  Forstørring av 40-modus vindu\n"
            "    -80x forstørrelsesfaktor  Forstørring av 80-modus vindu\n"
            "    -begrens                  Begrens hastighet til vanlig TIKI-hastighet\n"
            "    -ikkebegrens              Negasjonen av den over\n"
            "    -port1 devicenavn         Skru på serieport P1\n"
            "    -port2 devicenavn         Skru på serieport P2\n"
            "    -pk utskriftskommando     Send utskrift til kommando\n"
            "    -pf utskriftsfil          Send utskrift til fil\n"
            "    -st28b                    Sett bøyle ST 28 b\n"
            "    -ikkest28b                Negasjonen av den over\n"
            "\n"
            "Underveis i emulering kan du trykke på Escapetasten for å gå inn i\n"
            "kommandomodus. Kommandoen 'hjelp' gir en liste over gyldige kommandoer.\n"
            "\n");
    exit (1);
  }

  /* åpne display */
  XrmGetResource (cline_db, "tikiemul.display", "Tikiemul.display", &resType, &resValue);
  if ((display = XOpenDisplay (resValue.addr)) == NULL) {
    fprintf (stderr, "Kan ikke åpne display %s\n", XDisplayName (resValue.addr));
    exit (1);
  }

  { /* hent defaults fra server */
    char *xrms;

    xrms = XResourceManagerString (display);
    if (xrms != NULL) {
      options_db = XrmGetStringDatabase (xrms);
      XrmMergeDatabases (cline_db, &options_db);
    } else {
      options_db = cline_db;
    }
  }
  
  /* finn geometry */
  XrmGetResource (options_db, "tikiemul.geometry", "Tikiemul.geometry", &resType, &resValue);
  XGeometry (display, DefaultScreen (display), resValue.addr, "+0+0", 1,
             1, 1, 0, 0, &(xsh.x), &(xsh.y), &(xsh.width), &(xsh.height));
  
  /* skal forhold bevares? */
  XrmGetResource (options_db, "tikiemul.bevarforhold", "Tikiemul.bevarforhold",
                  &resType, &resValue);
  scanlines = FALSE;
  if (resValue.addr) {
    if (!strcmp (resValue.addr, "on")) {
      scanlines = TRUE;
    }
  }

  /* finn forstørring og vindusstørrelse */
  XrmGetResource (options_db, "tikiemul.40x", "Tikiemul.40x", &resType, &resValue);
  if (resValue.addr) times40 = atoi (resValue.addr);
  if (times40 > 4) times40 = 4;
  if (times40 < 1) times40 = 1;
  width[0] = 256 * times40;
  height[0] = 256 * times40;
  XrmGetResource (options_db, "tikiemul.80x", "Tikiemul.80x", &resType, &resValue);
  if (resValue.addr) times80 = atoi (resValue.addr);
  if (times80 > 2) times80 = 2;
  if (times80 < 1) times80 = 1;
  width[1] = 512 * times80;
  height[1] = 256 * times80 * (scanlines ? 2 : 1);
  width[2] = 1024;
  height[2] = 256 * (scanlines ? 4 : 1);

  /* skal hastighet begrenses? */
  XrmGetResource (options_db, "tikiemul.begrens", "Tikiemul.begrens", &resType, &resValue);
  slowDown = TRUE;
  if (resValue.addr) {
    if (!strcmp (resValue.addr, "off")) {
      slowDown = FALSE;
    }
  }

  /* hent inn eventuelle diskfiler */
  XrmGetResource (options_db, "tikiemul.diska", "Tikiemul.diska", &resType, &resValue);
  if (resValue.addr) readDiskImage (0, resValue.addr);
  XrmGetResource (options_db, "tikiemul.diskb", "Tikiemul.diskb", &resType, &resValue);
  if (resValue.addr) readDiskImage (1, resValue.addr);
  
  /* sett opp farger */
  default_cmap = DefaultColormap (display, DefaultScreen (display));
  {
    int visclass = DefaultVisual (display, DefaultScreen (display))->class;

    whitepix = WhitePixel (display, DefaultScreen (display));
    blackpix = BlackPixel (display, DefaultScreen (display));
    { /* farger for statusbar */
      XColor color;
      XColor exact;

      if (!XAllocNamedColor (display, default_cmap, "grey80", &exact, &color)) {
        backpix = blackpix;
      } else {
        backpix = color.pixel;
      }
      if (!XAllocNamedColor (display, default_cmap, "green", &exact, &color)) {
        diodepix = whitepix;
      } else {
        diodepix = color.pixel;
      }
    }

    if (visclass == PseudoColor || visclass == DirectColor) {
      /* dynamisk colormap - skaff r/w-fargeceller */
      if (!XAllocColorCells (display, default_cmap, False, NULL, 0, pixels, 16)) {
        printf ("For få ledige fargeceller\n");
        free (dsk[0]);
        free (dsk[1]);
        XCloseDisplay (display);
        exit (1);
      }
      dyn_cmap = TRUE;
    } else if (visclass == TrueColor || visclass == StaticColor) {
      /* statisk colormap - skaff ny pixel hver gang farge forandres */
      int i;

      if (!(screen = malloc (1024 * 256))) {
        fprintf (stderr, "Ikke nok minne\n");
        free (dsk[0]);
        free (dsk[1]);
        XCloseDisplay (display);
        exit (1);
      }
      memset (screen, 0, 1024 * 256);

      for (i = 0; i < 16; i++) {
        pixels[i] = blackpix;
      }
      dyn_cmap = FALSE;
    } else {
      /* svart/hvitt display, kan ikke brukes */
      fprintf (stderr, "Display ikke tilstrekkelig!\n");
      free (dsk[0]);
      free (dsk[1]);
      XCloseDisplay (display);
      exit (1);
    }
  }

  { /* graphics context, en for hver farge */
    int i;
    XGCValues gcv;

    gcv.foreground = blackpix;
    blackGC = XCreateGC (display, DefaultRootWindow (display),
                        GCForeground | GCBackground, &gcv);
    gcv.foreground = backpix;
    backGC = XCreateGC (display, DefaultRootWindow (display),
                        GCForeground | GCBackground, &gcv);
    gcv.foreground = diodepix;
    diodeGC = XCreateGC (display, DefaultRootWindow (display),
                         GCForeground | GCBackground, &gcv);
    
    for (i = 0; i < 16; i++) {
      gcv.foreground = pixels[i];
      colorGC[i] = XCreateGC (display, DefaultRootWindow (display),
                              GCForeground | GCBackground, &gcv);
    }
  }

  /* lag vindu */
  mainwindow =
    XCreateSimpleWindow (display, DefaultRootWindow (display),
                         xsh.x, xsh.y, width[1], height[1] + STATUSBAR_HEIGHT,
                         1, pixels[0], pixels[0]);

  { /* gi hint til window manager */
    XWMHints xwmh;
    
    xsh.flags = (PPosition | PSize | PMinSize | PMaxSize);
    xsh.width = width[1];
    xsh.height = height[1] + STATUSBAR_HEIGHT;
    xsh.min_width = xsh.width;
    xsh.min_height = xsh.height;
    xsh.max_width = xsh.width;
    xsh.max_height = xsh.height;
    XSetStandardProperties (display, mainwindow, "tikiemul", "tikiemul", None,
                            argv, argc, &xsh);
    xwmh.flags = (InputHint | StateHint);
    xwmh.input = True;
    xwmh.initial_state = NormalState;
    XSetWMHints (display, mainwindow, &xwmh);
  }
    
  { /* lag pixmap. Alt tegnes til pixmap, kopieres til vindu hver loopEmul() */
    int maxheight = height[0];

    if (height[1] > maxheight) maxheight = height[1];
    if (height[2] > maxheight) maxheight = height[2];

    mainpixmap = XCreatePixmap (display, mainwindow, 1024, maxheight + STATUSBAR_HEIGHT,
                                DefaultDepth (display, DefaultScreen (display)));
    XFillRectangle (display, mainpixmap, colorGC[0], 0, 0, xsh.width, xsh.height);
    update (0, 0, xsh.width, xsh.height);
  }

  /* lag statusbar */
  createStatusBar();

  /* gi beskjed om hvilke events som trengs */
  XSelectInput (display, mainwindow,
                ExposureMask | KeyPressMask | KeyReleaseMask);
  
  /* gjør vindu synlig */
  XMapWindow (display, mainwindow);

  /* init pressedKeys tabell */
  memset (pressedKeys, 1, 256);

  /* bøyle st28b? */
  XrmGetResource (options_db, "tikiemul.st28b", "Tikiemul.st28b", &resType, &resValue);
  if (resValue.addr) {
    if (!strcmp (resValue.addr, "on")) {
      setST28b (TRUE);
    } else {
      setST28b (FALSE);
    }
  }

  /* sett opp printfil/kommando */
  XrmGetResource (options_db, "tikiemul.pk", "Tikiemul.pk", &resType, &resValue);
  if (resValue.addr) {
    setPrintCmd (resValue.addr);
  }
  XrmGetResource (options_db, "tikiemul.pf", "Tikiemul.pf", &resType, &resValue);
  if (resValue.addr) {
    setPrintFile (resValue.addr);
  }

  /* sett opp serieporter */
  XrmGetResource (options_db, "tikiemul.port1", "Tikiemul.port1", &resType, &resValue);
  if (resValue.addr) {
    if ((port1 = open (resValue.addr, O_RDWR | O_NOCTTY | O_NDELAY)) == -1) {
      fprintf (stderr, "Kan ikke åpne port %s\n", resValue.addr);
      port1 = 0;
    } else {
      fcntl (port1, F_SETFL, 0);
    }
  }
  XrmGetResource (options_db, "tikiemul.port2", "Tikiemul.port2", &resType, &resValue);
  if (resValue.addr) {
    if ((port2 = open (resValue.addr, O_RDWR | O_NOCTTY | O_NDELAY)) == -1) {
      fprintf (stderr, "Kan ikke åpne port %s\n", resValue.addr);
      port2 = 0;
    } else {
      fcntl (port2, F_SETFL, 0);
    }
  }

  /* kjør emulator */
  if (!runEmul()) {
    fprintf (stderr, "Finner ikke ROM-fil\n");
  }
  
  /* avslutt */
  if (port2) close (port2);
  if (port1) close (port1);
  if (printFile) fclose (printFile);
  if (printCmd) fclose (printCmd);
  XFreePixmap (display, mainpixmap);
  {
    int i;

    XFreeGC (display, blackGC);
    XFreeGC (display, backGC);
    XFreeGC (display, diodeGC);

    for (i = 0; i < 16; i++) {
      XFreeGC (display, colorGC[i]);
    }
  }
  XDestroyWindow (display, mainwindow);
  if (dyn_cmap) {
    XFreeColors (display, default_cmap, pixels, 16, 0);
  } else {
    free (screen);
  }
  free (dsk[0]);
  free (dsk[1]);
  XCloseDisplay (display);
  
  exit (0);
}
/* oppdater gitt rektangel ved neste loopEmul() */
static void update (int x, int y, int w, int h) {
  if (xmin > x) xmin = x;
  if (ymin > y) ymin = y;
  if (xmax < (x + w)) xmax = x + w;
  if (ymax < (y + h)) ymax = y + h;
  updateWindow = TRUE;  
}
/* tegner statusbar */
static void createStatusBar (void) {
  /* statuslinje */
  XFillRectangle (display, mainpixmap, backGC,
                  0, xsh.height - STATUSBAR_HEIGHT, xsh.width, STATUSBAR_HEIGHT);

  /* "hull" til dioder */
  XFillRectangle (display, mainpixmap, blackGC,
                  20, xsh.height - STATUSBAR_HEIGHT + 3, 10, 10);
  XFillRectangle (display, mainpixmap, blackGC,
                  40, xsh.height - STATUSBAR_HEIGHT + 3, 10, 10);
  XFillRectangle (display, mainpixmap, blackGC,
                  60, xsh.height - STATUSBAR_HEIGHT + 3, 10, 10);
  XFillRectangle (display, mainpixmap, blackGC,
                  80, xsh.height - STATUSBAR_HEIGHT + 3, 10, 10);

  /* oppdater diodelys */
  lockLight (lock);
  grafikkLight (grafikk);
  diskLight (0, disk[0]);
  diskLight (1, disk[1]);

  update (0, xsh.height - STATUSBAR_HEIGHT, xsh.width, STATUSBAR_HEIGHT);
}
/* Forandre oppløsning */
void changeRes (int newRes) {
  resolution = newRes;

  switch (resolution) {
    case LOWRES:
      xsh.width = width[0];
      xsh.height = height[0];
      break;
    case MEDRES:
      xsh.width = width[1];
      xsh.height = height[1];
      break;
    case HIGHRES:
      xsh.width = width[2];
      xsh.height = height[2];
      break;
  }
  xsh.height += STATUSBAR_HEIGHT;

  /* slett gammelt innhold */
  XFillRectangle (display, mainpixmap, colorGC[0], 0, 0, xsh.width, xsh.height - STATUSBAR_HEIGHT);
  update (0, 0, xsh.width, xsh.height - STATUSBAR_HEIGHT);

  /* nye hints til window manager */
  xsh.flags = (PSize | PMinSize | PMaxSize);
  xsh.min_width = xsh.width;
  xsh.min_height = xsh.height;
  xsh.max_width = xsh.width;
  xsh.max_height = xsh.height;
  XSetNormalHints (display, mainwindow, &xsh);

  /* forandre vindusstørrelse */
  XResizeWindow (display, mainwindow, xsh.width, xsh.height);

  if (!dyn_cmap) {
    /* slett gammelt innhold i screen */
    memset (screen, 0, 1024 * 256);
  }

  /* lag statusbar */
  createStatusBar();
}
/* Plotter en pixel med farge tatt fra pallett */
void plotPixel (int x, int y, int color) {
  if (!dyn_cmap) {
    screen[x + y * 1024] = color;
  }

  switch (resolution) {
    case LOWRES:
      x = x * times40;
      y = y * times40;
      break;
    case MEDRES:
      x = x * times80;
      y = y * times80 * (scanlines ? 2 : 1);
      break;
    case HIGHRES:
      y = y * (scanlines ? 4 : 1);
      break;
  }

  XDrawPoint (display, mainpixmap, colorGC[color], x, y);

  update (x, y, 1, 1);
}
/* Scroller skjerm 'distance' linjer oppover */
void scrollScreen (int distance) {
  if (!dyn_cmap) {
    /* scroll screen */
    if (distance > 0) {
      memmove (screen, screen + distance * 1024, (256 - distance) * 1024);
      memset (screen + (256 - distance) * 1024, 0, distance * 1024);
    } else {
      memmove (screen + (-distance * 1024), screen, (byte)distance * 1024);
      memset (screen, 0, -distance * 1024);
    }      
  }
  switch (resolution) {
    case LOWRES:
      distance *= times40;
      break;
    case MEDRES:
      distance *= times80 * (scanlines ? 2 : 1);
      break;
    case HIGHRES:
      distance *= (scanlines ? 4 : 1);
      break;
  }
  /* scroll pixmap */
  if (distance > 0) {
    XCopyArea (display, mainpixmap, mainpixmap, colorGC[1],
               0, distance, xsh.width, xsh.height - STATUSBAR_HEIGHT - distance, 0, 0);
    XFillRectangle (display, mainpixmap, colorGC[0], 0, xsh.height - STATUSBAR_HEIGHT - distance,
                    xsh.width, distance);
  } else {
    XCopyArea (display, mainpixmap, mainpixmap, colorGC[1],
               0, 0, xsh.width, xsh.height - STATUSBAR_HEIGHT - (-distance), 0, -distance);
    XFillRectangle (display, mainpixmap, colorGC[0], 0, 0,
                    xsh.width, -distance);
  }
  /* oppdater hele vinduet */
  update (0, 0, xsh.width, xsh.height - STATUSBAR_HEIGHT);
}
/* Ny farge, gitt pallett nummer og intensitet 0-255 */
void changePalette (int colornumber, byte red, byte green, byte blue) {
  XColor color;

  color.red = red << 8;
  color.green = green << 8;
  color.blue = blue << 8;
  if (dyn_cmap) {
    color.flags = DoRed | DoGreen | DoBlue;
    color.pixel = pixels[colornumber];
    XStoreColor (display, default_cmap, &color);
  } else {
    XGCValues gcv;

    /* skaff pixel */
    if (!XAllocColor (display, default_cmap, &color)) {
      fprintf (stderr, "Farge %x %x %x ikke tilgjengelig, bruker hvit!\n", red, green, blue);
      color.pixel = whitepix;
    }
    pixels[colornumber] = color.pixel;
    /* forandre GC */
    gcv.foreground = pixels[colornumber];
    XChangeGC (display, colorGC[colornumber], GCForeground, &gcv);
    { /* oppdater skjerm */
      int x, y;

      for (y = 0; y < 256; y++) {
        for (x = 0; x < 1024; x++) {
          if (screen[x + y * 1024]  == colornumber) {
            plotPixel (x, y, colornumber);
          }
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
  int events;
  XEvent event;
  
  /* senk hastighet */
  if (slowDown) {
    static boolean firsttime = TRUE; /* brukes for å initialisere lastTOD */
    static struct timeval lastTOD;
    struct timeval currentTOD;
    
    if (firsttime) { /* initialiser lastTOD */
      gettimeofday (&lastTOD, NULL);
      firsttime = FALSE;
    } else {
      long sleepPeriod;
      
      gettimeofday (&currentTOD, NULL);

      /* finner soveintervall */
      if ((currentTOD.tv_sec - lastTOD.tv_sec) != 0) { /* har krysset sekundgrense */
        sleepPeriod = (ms * 1000) - (currentTOD.tv_usec + (1000000 - lastTOD.tv_usec));
      } else {
        sleepPeriod = (ms * 1000) - (currentTOD.tv_usec - lastTOD.tv_usec);
      }

      lastTOD = currentTOD;

      /* ta en liten lur */
      if (sleepPeriod > 0) {
        usleep (sleepPeriod);

        /* lastTOD += sleepPeriod */
        lastTOD.tv_usec = lastTOD.tv_usec + sleepPeriod;
        if (lastTOD.tv_usec > 999999) {
          lastTOD.tv_sec++;
          lastTOD.tv_usec -= 1000000;
        }
      }
    }
  }

  /* oppdater skjerm */
  if (updateWindow) {
    XCopyArea (display, mainpixmap, mainwindow, colorGC[1],
               xmin, ymin, xmax - xmin, ymax - ymin, xmin, ymin);
    xmin = (unsigned)~0; ymin = (unsigned)~0; xmax = 0; ymax = 0;
    updateWindow = FALSE;
  }
  
  XFlush (display);

  /* sjekk events */
  if ((events = XEventsQueued (display, QueuedAfterReading))) {
    while (events--) {
      XNextEvent (display, &event);
      switch (event.type) {
        case MappingNotify:
          XRefreshKeyboardMapping ((XMappingEvent *)&event);
          break;
        case Expose:
          XCopyArea (display, mainpixmap, mainwindow, colorGC[1],
                     event.xexpose.x, event.xexpose.y,
                     event.xexpose.width, event.xexpose.height,
                     event.xexpose.x, event.xexpose.y);
          break;
        case KeyPress: {
          if (XKeycodeToKeysym (display, event.xkey.keycode, 0) == XK_Escape) {
            commandline();
          } else {
            pressedKeys[keysymToTikikey (XKeycodeToKeysym (display, event.xkey.keycode, 0))]
              = 0;
          }
          break;
        }
        case KeyRelease: {
          pressedKeys[keysymToTikikey (XKeycodeToKeysym (display, event.xkey.keycode, 0))]
            = 1;
          break;
        }
      }
    }
  }

  /* sjekk serieporter */
  if (port1) {
    int bytes;

    ioctl (port1, FIONREAD, &bytes);
    if (bytes) {
      charAvailable (0);
    }
  }
  if (port2) {
    int bytes;

    ioctl (port2, FIONREAD, &bytes);
    if (bytes) {
      charAvailable (1);
    }
  }
}
/* oversetter fra keysym til tikitaster */
static byte keysymToTikikey (KeySym keysym) {
  int i;

  /* lineærsøk gjennom tabell */
  for (i = 0; i < (sizeof keyConvTable / sizeof (struct KeyConv)); i++) {
    if (keyConvTable[i].keysym == keysym)
      return keyConvTable[i].tikikey;
  }
  return KEY_NONE;
}
/* tar imot kommandoer via konsoll */
static void commandline (void) {
  while (TRUE) {
    char cmdline[256];
    char *cmd;
    
    printf ("Kommando [h for hjelp]> ");
    fflush (stdout);

    fgets (cmdline, 256, stdin);

    if ((cmd = strtok (cmdline, " \t\n"))) {
      if (!strcmp (cmd, "h") || !strcmp (cmd, "hjelp")) {
        /* vis hjelpetekst */
        puts ("\n***** Kommandoer *****");
        puts ("hjelp                      : Vis denne hjelpeteksten");
        puts ("h                          : ");
        puts ("disk <stasjon> <filnavn>   : Hent inn diskettfil");
        puts ("d <stasjon> <filnavn>      : ");        
        puts ("lagre <stasjon> <filnavn>  : Lagre diskettfil");
        puts ("l <stasjon> <filnavn>      : ");
        puts ("fjern <stasjon>            : Fjern diskettfil");
        puts ("f <stasjon>                : ");
        puts ("pk [kommandonavn]          : Send utskrift til print-kommando");
        puts ("pf [filnavn]               : Send utskrift til fil");
        puts ("reset                      : Reset emulator");
        puts ("fortsett                   : Fortsett emulering");
        puts ("c                          : ");
        puts ("om                         : Om emulator");
        puts ("avslutt                    : Avslutt emulator");
        puts ("q                          : ");
#ifdef DEBUG
        puts ("monitor                    : Z80-monitor");
        puts ("m                          : ");
#endif
        puts ("");
      } else if (!strcmp (cmd, "d") || !strcmp (cmd, "disk")) {
        /* hent diskfil */
        char *driveString = strtok (NULL, " \t\n");
        char *filename = strtok (NULL, " \t\n");

        if (!filename) {
          fprintf (stderr, "For få argumenter\n");
          continue;
        }

        if (!strcmp (driveString, "a") || !strcmp (driveString, "a:")) {
          readDiskImage (0, filename);
        } else if (!strcmp (driveString, "b") || !strcmp (driveString, "b:")) {
          readDiskImage (1, filename);
        } else fprintf (stderr, "Ugyldig stasjon\n");
      } else if (!strcmp (cmd, "l") || !strcmp (cmd, "lagre")) {
        /* lagre diskfil */
        int drive;
        char *driveString = strtok (NULL, " \t\n");
        char *filename = strtok (NULL, " \t\n");

        if (!filename) {
          fprintf (stderr, "For få argumenter\n");
          continue;
        }

        if (!strcmp (driveString, "a") || !strcmp (driveString, "a:")) {
          drive = 0;
        } else if (!strcmp (driveString, "b") || !strcmp (driveString, "b:")) {
          drive = 1;
        } else {
          fprintf (stderr, "Ugyldig stasjon\n");
          continue;
        }
        if (dsksize[drive] != 0) {
          int fd;

          if ((fd = creat (filename, 0640)) != -1) {
            if (write (fd, dsk[drive], dsksize[drive]) == -1) {
              fprintf (stderr, "Kan ikke skrive til fil %s\n", filename);
            }
            close (fd);
          } else fprintf (stderr, "Kan ikke skrive til fil %s\n", filename);
        }
      } else if (!strcmp (cmd, "f") || !strcmp (cmd, "fjern")) {
        /* fjern diskfil */
        int drive;
        char *driveString = strtok (NULL, " \t\n");

        if (!driveString) {
          fprintf (stderr, "For få argumenter\n");
          continue;
        }

        if (!strcmp (driveString, "a") || !strcmp (driveString, "a:")) {
          drive = 0;
        } else if (!strcmp (driveString, "b") || !strcmp (driveString, "b:")) {
          drive = 1;
        } else {
          fprintf (stderr, "Ugyldig stasjon\n");
          continue;
        }
        removeDisk (drive);
        free (dsk[drive]);
        dsk[drive] = NULL;
        dsksize[drive] = 0;
      } else if (!strcmp (cmd, "pk")) {
        char *pc = strtok (NULL, " \t\n");

        if (!pc) {
          if (printCmd) {
            pclose (printCmd);
            printCmd = 0;
          }
        } else {
          setPrintCmd (pc);
        }
      } else if (!strcmp (cmd, "pf")) {
        char *pf = strtok (NULL, " \t\n");

        if (!pf) {
          if (printFile) {
            fclose (printFile);
            printFile = 0;
          }
        } else {
          setPrintFile (pf);
        }
      } else if (!strcmp (cmd, "reset")) {
        /* reset */
        resetEmul();
      } else if (!strcmp (cmd, "c") || !strcmp (cmd, "fortsett")) {
        /* fortsett emulering */
        return;
      } else if (!strcmp (cmd, "om")) {
        /* vis "om"-tekst */
        puts ("\nTIKI-100_emul V1.1.1 - en freeware TIKI 100 Rev. C emulator.");
        puts ("Z80 emulering copyright (C) Marat Fayzullin 1994,1995,1996,1997.");
        puts ("Resten copyright (C) Asbjørn Djupdal 2000-2001.\n");
      } else if (!strcmp (cmd, "q") || !strcmp (cmd, "avslutt")) {
        /* avslutt */
        quitEmul();
        return;
#ifdef DEBUG
      } else if (!strcmp (cmd, "m") || !strcmp (cmd, "monitor")) {
        /* z80-debugger */
        trace();
        return;
#endif
      } else {
        fprintf (stderr, "Ugyldig kommando\n");
      }
    }
  }
}
/* setter opp printkommando/fil */
static void setPrintCmd (char *p) {
  if (printCmd) pclose (printCmd);
  if (!(printCmd = popen (p, "w"))) {
    fprintf (stderr, "Kan ikke starte printkommando\n");
  }
}
static void setPrintFile (char *p) {
  if (printFile) fclose (printFile);
  if (!(printFile = fopen (p, "wb"))) {
    fprintf (stderr, "Kan ikke lage printerfil %s\n", p);
  }
}
/* henter inn diskbilde fra fil */
static void readDiskImage (int drive, char *filename) {
  int fd;
    
  if ((fd = open (filename, O_RDONLY)) != -1) {
    struct stat fileInfo;

    removeDisk (drive);
    free (dsk[drive]);
    fstat (fd, &fileInfo);
    if ((dsk[drive] = malloc (fileInfo.st_size))) {
      dsksize[drive] = read (fd, dsk[drive], fileInfo.st_size);

      switch (dsksize[drive]) {
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
        case -1:
          fprintf (stderr, "Klarer ikke lese fra fil %s\n", filename);
          break;
        default:
          fprintf (stderr, "Kjenner ikke diskformatet\n");
          dsksize[drive] = 0;
          free (dsk[drive]);
          dsk[drive] = NULL;
          break;
      }
    } else {
      fprintf (stderr, "Ikke nok minne til diskettfil\n");
      dsksize[drive] = 0;
    }
    close (fd);
  } else {
    fprintf (stderr, "Klarer ikke lese fra fil %s\n", filename);
  }
}

/* Tenn/slukk lock lys */
void lockLight (boolean status) {
  GC gc = (status ? diodeGC : blackGC);

  XFillRectangle (display, mainpixmap, gc,
                  21, xsh.height - STATUSBAR_HEIGHT + 4, 8, 8);

  update (21, xsh.height - STATUSBAR_HEIGHT + 4, 9, 9);
  lock = status;
}
/* Tenn/slukk grafikk lys */
void grafikkLight (boolean status) {
  GC gc = (status ? diodeGC : blackGC);

  XFillRectangle (display, mainpixmap, gc,
                  41, xsh.height - STATUSBAR_HEIGHT + 4, 8, 8);

  update (41, xsh.height - STATUSBAR_HEIGHT + 4, 9, 9);
  grafikk = status;
}
/* Tenn/slukk disk lys for gitt stasjon */
void diskLight (int drive, boolean status) {
  GC gc = (status ? diodeGC : blackGC);
  int x = (drive ? 81 : 61);

  XFillRectangle (display, mainpixmap, gc,
                  x, xsh.height - STATUSBAR_HEIGHT + 4, 8, 8);

  update (x, xsh.height - STATUSBAR_HEIGHT + 4, 9, 9);
  disk[drive] = status;
}
/* Sjekk status til hver av de gitte tastene
 * Sett bit n i returkode hvis tast n IKKE er nedtrykt
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
static void setParam (int portfd, struct serParams *params) {
  struct termios options;

  tcgetattr (portfd, &options);

  options.c_iflag = 0;
  options.c_oflag = 0;
  options.c_lflag = 0;
  memset (options.c_cc, 0, sizeof (options.c_cc));

  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_cflag |= (CLOCAL | CREAD);

  /* rec/send bits */
  options.c_cflag &= ~CSIZE;
  switch (params->sendBits) {
    case 5:
      options.c_cflag |= CS5;
      break;
    case 6:
      options.c_cflag |= CS6;
      break;
    case 7:
      options.c_cflag |= CS7;
      break;
    default:
    case 8:
      options.c_cflag |= CS8;
      break;
  }
  /* parity */
  switch (params->parity) {
    case PAR_NONE:
      options.c_cflag &= ~PARENB;
      options.c_iflag &= ~(INPCK | ISTRIP);
      break;
    case PAR_EVEN:
      options.c_cflag |= PARENB;
      options.c_cflag &= ~PARODD;
      options.c_iflag |= (INPCK | ISTRIP);
      break;
    case PAR_ODD:
      options.c_cflag |= PARENB;
      options.c_cflag |= PARODD;
      options.c_iflag |= (INPCK | ISTRIP);
      break;
  }
  /* stopbits */
  switch (params->stopBits) {
    case ONE_SB:
      options.c_cflag &= ~CSTOPB;
      break;
    case ONE_PT_FIVE_SB:
    case TWO_SB:
      options.c_cflag |= CSTOPB;
      break;
  }
  /* baud */
  if (params->baud < 62) {
    params->baud = B50;
  } else if (params->baud < 92) {
    params->baud = B75;
  } else if (params->baud < 122) {
    params->baud = B110;
  } else if (params->baud < 142) {
    params->baud = B134;
  } else if (params->baud < 175) {
    params->baud = B150;
  } else if (params->baud < 250) {
    params->baud = B200;
  } else if (params->baud < 450) {
    params->baud = B300;
  } else if (params->baud < 900) {
    params->baud = B600;
  } else if (params->baud < 1500) {
    params->baud = B1200;
  } else if (params->baud < 2100) {
    params->baud = B1800;
  } else if (params->baud < 3600) {
    params->baud = B2400;
  } else if (params->baud < 7200) {
    params->baud = B4800;
  } else if (params->baud < 14400) {
    params->baud = B9600;
  } else {
    params->baud = B19200;
  }
  cfsetispeed (&options, params->baud);
  cfsetospeed (&options, params->baud);

  tcsetattr (portfd, TCSADRAIN, &options);
}
/* send tegn til seriekanal */
void sendChar (int port, byte value) {
  int portfd = port ? port2 : port1;

  if (portfd) {
    if (write (portfd, &value, 1) == -1) {
      /* klarer ikke sende tegn */
    }
  }
}
/* hent tegn fra seriekanal */
byte getChar (int port) {
  int portfd = port ? port2 : port1;
  byte value;
  
  if (portfd) {
    read (portfd, &value, 1);

    { /* flere tegn? */
      int bytes;

      ioctl (portfd, FIONREAD, &bytes);
      if (bytes) {
        charAvailable (port);
      }
    }
  }
  return value;
}
/* send tegn til skriver */
void printChar (byte value) {
  if (printCmd) {
    if (fputc (value, printCmd) == EOF) {
      /* klarer ikke skrive til printkommando */
    }
  }
  if (printFile) {
    if (fputc (value, printFile) == EOF) {
      /* klarer ikke skrive til printfil */
    }
  }
}
