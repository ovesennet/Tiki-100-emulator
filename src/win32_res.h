/* win32_res.h V1.1.0
 *
 * Konstanter for win32 ressurser
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"

#define IDM_RESET           0
#define IDM_INNSTILLINGER   1
#define IDM_OM              2
#define IDM_AVSLUTT         3
#define IDM_HENT_A          4
#define IDM_HENT_B          5
#define IDM_LAGRE_A         6
#define IDM_LAGRE_B         7
#define IDM_FJERN_A         8
#define IDM_FJERN_B         9

#ifdef DEBUG
#define IDM_MONITOR         10
#endif

#define IDD_HASTIGHET       11
#define IDD_BEVARFORHOLD    12
#define IDD_SOLID           13
#define IDD_40SPIN          14
#define IDD_80SPIN          15
#define IDD_40EDIT          16
#define IDD_80EDIT          17
#define IDD_OK              18
#define IDD_AVBRYT          19
#define IDD_P1EDIT          20
#define IDD_P2EDIT          21
#define IDD_P3EDIT          22
#define IDD_P1              23
#define IDD_P2              24
#define IDD_P3              25
#define IDD_ST28B           26
#define IDM_TESTDLG        27
#define IDD_TESTDLG_OK     28
#define IDC_TOOLBAR         29
#define IDM_SPEED_TOGGLE    30
#define IDM_FULLSCREEN      31
#define IDM_SCREENSHOT      32
#define IDM_HELP            33
#define IDM_FPS             34
#define IDM_Z80INFO         35
#define IDM_MEMVIEW         36
#define IDM_DISKVIEW        37

/* MRU disk list menu IDs (38-45 = 8 slots) */
#define IDM_MRU_BASE        38
#define IDM_MRU_MAX         45
#define IDM_MRU_CLEAR       46

/* Hard disk menu items */
#define IDM_HENT_HDD0       50
#define IDM_HENT_HDD1       51
#define IDM_FJERN_HDD0      52
#define IDM_FJERN_HDD1      53
