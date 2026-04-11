/* keyboard.c V1.1.0
 *
 * Tastatur emulering for TIKI-100_emul
 * Copyright (C) Asbjørn Djupdal 2000-2001
 */

#include "TIKI-100_emul.h"
#include "protos.h"

/* variabler */

static byte keyMatrix[12][8] = {
  {KEY_CTRL, KEY_SHIFT, KEY_BRYT, KEY_CR, KEY_SPACE, KEY_NUMDIV, KEY_SLETT, KEY_NONE},
  {KEY_GRAFIKK, '1', KEY_ANGRE, 'a', '<', 'z', 'q', KEY_LOCK},
  {'2', 'w', 's', 'x', '3', 'e', 'd', 'c'},
  {'4', 'r', 'f', 'v', '5', 't', 'g', 'b'},
  {'6', 'y', 'h', 'n', '7', 'u', 'j', 'm'},
  {'8', 'i', 'k', ',', '9', 'o', 'l', '.'},
  {'0', 'p', (byte)'ø', '-', '+', (byte)'å', (byte)'æ', KEY_HJELP},
  {'@', '^', '\'', KEY_LEFT, KEY_UTVID, KEY_F1, KEY_F4, KEY_PGUP},
  {KEY_F2, KEY_F3, KEY_F5, KEY_F6, KEY_UP, KEY_PGDOWN, KEY_TABLEFT, KEY_DOWN},
  {KEY_NUMPLUS, KEY_NUMMINUS, KEY_NUMMULT, KEY_NUM7, KEY_NUM8, KEY_NUM9, KEY_NUMPERCENT, KEY_NUMEQU},
  {KEY_NUM4, KEY_NUM5, KEY_NUM6, KEY_TABRIGHT, KEY_NUM1, KEY_NUM0, KEY_NUMDOT, KEY_NONE},
  {KEY_HOME, KEY_RIGHT, KEY_NUM2, KEY_NUM3, KEY_ENTER, KEY_NONE, KEY_NONE, KEY_NONE},
};

static int column = 0;

/*****************************************************************************/

/* skriv til tastatur-register */
void resetKeyboard (void) {
  column = 0;
}
/* les tastatur-register */
byte readKeyboard (void) {
  return testKey (keyMatrix[column++]);
}

