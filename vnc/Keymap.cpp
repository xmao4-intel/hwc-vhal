//#define LOG_NDEBUG 0
#include <cutils/log.h>

#include <linux/input.h>

#include "Keymap.h"
#include "input_interface.h"

#define XK_LATIN1
#define XK_MISCELLANY
#include "keysymdef.h"

#define KeySym2ScanCode(keySym, scanCode) \
  case keySym:                            \
    return scanCode

uint32_t keySymToMask(uint32_t keySym) {
  uint32_t mask = 0;

  if (keySym >= XK_A && keySym <= XK_Z)
    mask |= KEY_STATE_MASK::Shift;

  return mask;
}

uint16_t keySymToScanCode(uint32_t keySym) {
  ALOGV("%s", __func__);

  switch (keySym) {
    // ASCII - Latin 1
    KeySym2ScanCode(XK_space, KEY_SPACE);
    KeySym2ScanCode(XK_apostrophe, KEY_APOSTROPHE);
    KeySym2ScanCode(XK_comma, KEY_COMMA);

    KeySym2ScanCode(XK_minus, KEY_MINUS);
    KeySym2ScanCode(XK_period, KEY_DOT);
    KeySym2ScanCode(XK_slash, KEY_SLASH);
    KeySym2ScanCode(XK_0, KEY_0);
    KeySym2ScanCode(XK_1, KEY_1);
    KeySym2ScanCode(XK_2, KEY_2);
    KeySym2ScanCode(XK_3, KEY_3);
    KeySym2ScanCode(XK_4, KEY_4);
    KeySym2ScanCode(XK_5, KEY_5);
    KeySym2ScanCode(XK_6, KEY_6);
    KeySym2ScanCode(XK_7, KEY_7);
    KeySym2ScanCode(XK_8, KEY_8);
    KeySym2ScanCode(XK_9, KEY_9);
    KeySym2ScanCode(XK_semicolon, KEY_SEMICOLON);
    KeySym2ScanCode(XK_equal, KEY_EQUAL);

    KeySym2ScanCode(XK_A, KEY_A);
    KeySym2ScanCode(XK_B, KEY_B);
    KeySym2ScanCode(XK_C, KEY_C);
    KeySym2ScanCode(XK_D, KEY_D);
    KeySym2ScanCode(XK_E, KEY_E);
    KeySym2ScanCode(XK_F, KEY_F);
    KeySym2ScanCode(XK_G, KEY_G);
    KeySym2ScanCode(XK_H, KEY_H);
    KeySym2ScanCode(XK_I, KEY_I);
    KeySym2ScanCode(XK_J, KEY_J);
    KeySym2ScanCode(XK_K, KEY_K);
    KeySym2ScanCode(XK_L, KEY_L);
    KeySym2ScanCode(XK_M, KEY_M);
    KeySym2ScanCode(XK_N, KEY_N);
    KeySym2ScanCode(XK_O, KEY_O);
    KeySym2ScanCode(XK_P, KEY_P);
    KeySym2ScanCode(XK_Q, KEY_Q);
    KeySym2ScanCode(XK_R, KEY_R);
    KeySym2ScanCode(XK_S, KEY_S);
    KeySym2ScanCode(XK_T, KEY_T);
    KeySym2ScanCode(XK_U, KEY_U);
    KeySym2ScanCode(XK_V, KEY_V);
    KeySym2ScanCode(XK_W, KEY_W);
    KeySym2ScanCode(XK_X, KEY_X);
    KeySym2ScanCode(XK_Y, KEY_Y);
    KeySym2ScanCode(XK_Z, KEY_Z);

    KeySym2ScanCode(XK_a, KEY_A);
    KeySym2ScanCode(XK_b, KEY_B);
    KeySym2ScanCode(XK_c, KEY_C);
    KeySym2ScanCode(XK_d, KEY_D);
    KeySym2ScanCode(XK_e, KEY_E);
    KeySym2ScanCode(XK_f, KEY_F);
    KeySym2ScanCode(XK_g, KEY_G);
    KeySym2ScanCode(XK_h, KEY_H);
    KeySym2ScanCode(XK_i, KEY_I);
    KeySym2ScanCode(XK_j, KEY_J);
    KeySym2ScanCode(XK_k, KEY_K);
    KeySym2ScanCode(XK_l, KEY_L);
    KeySym2ScanCode(XK_m, KEY_M);
    KeySym2ScanCode(XK_n, KEY_N);
    KeySym2ScanCode(XK_o, KEY_O);
    KeySym2ScanCode(XK_p, KEY_P);
    KeySym2ScanCode(XK_q, KEY_Q);
    KeySym2ScanCode(XK_r, KEY_R);
    KeySym2ScanCode(XK_s, KEY_S);
    KeySym2ScanCode(XK_t, KEY_T);
    KeySym2ScanCode(XK_u, KEY_U);
    KeySym2ScanCode(XK_v, KEY_V);
    KeySym2ScanCode(XK_w, KEY_W);
    KeySym2ScanCode(XK_x, KEY_X);
    KeySym2ScanCode(XK_y, KEY_Y);
    KeySym2ScanCode(XK_z, KEY_Z);

    KeySym2ScanCode(XK_bracketleft, KEY_LEFTBRACE);
    KeySym2ScanCode(XK_backslash, KEY_BACKSLASH);
    KeySym2ScanCode(XK_bracketright, KEY_RIGHTBRACE);
    KeySym2ScanCode(XK_grave, KEY_GRAVE);

    // extended, function, ctrl, etc
    KeySym2ScanCode(XK_BackSpace, KEY_BACKSPACE);
    KeySym2ScanCode(XK_Tab, KEY_TAB);
    KeySym2ScanCode(XK_Linefeed, KEY_LINEFEED);
    KeySym2ScanCode(XK_Clear, KEY_CLEAR);
    KeySym2ScanCode(XK_Return, KEY_ENTER);
    KeySym2ScanCode(XK_Pause, KEY_PAUSE);
    KeySym2ScanCode(XK_Scroll_Lock, KEY_SCROLLLOCK);
    KeySym2ScanCode(XK_Sys_Req, KEY_SYSRQ);
    KeySym2ScanCode(XK_Escape, KEY_ESC);
    KeySym2ScanCode(XK_Delete, KEY_DELETE);

    KeySym2ScanCode(XK_Home, KEY_HOME);
    KeySym2ScanCode(XK_Left, KEY_LEFT);
    KeySym2ScanCode(XK_Up, KEY_UP);
    KeySym2ScanCode(XK_Right, KEY_RIGHT);
    KeySym2ScanCode(XK_Down, KEY_DOWN);
    KeySym2ScanCode(XK_Page_Up, KEY_PAGEUP);
    KeySym2ScanCode(XK_Page_Down, KEY_PAGEDOWN);
    KeySym2ScanCode(XK_End, KEY_END);
    KeySym2ScanCode(XK_Begin, KEY_HOME);

    KeySym2ScanCode(XK_KP_Space, KEY_SPACE);
    KeySym2ScanCode(XK_KP_Tab, KEY_TAB);
    KeySym2ScanCode(XK_KP_Enter, KEY_ENTER);
    KeySym2ScanCode(XK_KP_Home, KEY_HOME);
    KeySym2ScanCode(XK_KP_Left, KEY_LEFT);
    KeySym2ScanCode(XK_KP_Up, KEY_UP);
    KeySym2ScanCode(XK_KP_Right, KEY_RIGHT);
    KeySym2ScanCode(XK_KP_Down, KEY_DOWN);
    KeySym2ScanCode(XK_KP_Page_Up, KEY_PAGEUP);
    KeySym2ScanCode(XK_KP_Page_Down, KEY_PAGEDOWN);
    KeySym2ScanCode(XK_KP_End, KEY_END);
    KeySym2ScanCode(XK_KP_Begin, KEY_HOME);
    KeySym2ScanCode(XK_KP_Insert, KEY_INSERT);
    KeySym2ScanCode(XK_KP_Delete, KEY_DELETE);
    KeySym2ScanCode(XK_KP_Equal, KEY_KPEQUAL);
    KeySym2ScanCode(XK_KP_Multiply, KEY_KPASTERISK);
    KeySym2ScanCode(XK_KP_Add, KEY_KPPLUS);
    KeySym2ScanCode(XK_KP_Separator, KEY_KPCOMMA);
    KeySym2ScanCode(XK_KP_Subtract, KEY_KPMINUS);
    KeySym2ScanCode(XK_KP_Decimal, KEY_KP0);
    KeySym2ScanCode(XK_KP_Divide, KEY_KPSLASH);

    KeySym2ScanCode(XK_KP_0, KEY_KP0);
    KeySym2ScanCode(XK_KP_1, KEY_KP1);
    KeySym2ScanCode(XK_KP_2, KEY_KP2);
    KeySym2ScanCode(XK_KP_3, KEY_KP3);
    KeySym2ScanCode(XK_KP_4, KEY_KP4);
    KeySym2ScanCode(XK_KP_5, KEY_KP5);
    KeySym2ScanCode(XK_KP_6, KEY_KP6);
    KeySym2ScanCode(XK_KP_7, KEY_KP7);
    KeySym2ScanCode(XK_KP_8, KEY_KP8);
    KeySym2ScanCode(XK_KP_9, KEY_KP9);

    KeySym2ScanCode(XK_F1, KEY_F1);
    KeySym2ScanCode(XK_F2, KEY_F2);
    KeySym2ScanCode(XK_F3, KEY_F3);
    KeySym2ScanCode(XK_F4, KEY_F4);
    KeySym2ScanCode(XK_F5, KEY_F5);
    KeySym2ScanCode(XK_F6, KEY_F6);
    KeySym2ScanCode(XK_F7, KEY_F7);
    KeySym2ScanCode(XK_F8, KEY_F8);
    KeySym2ScanCode(XK_F9, KEY_F9);
    KeySym2ScanCode(XK_F10, KEY_F10);
    KeySym2ScanCode(XK_F11, KEY_F11);
    KeySym2ScanCode(XK_F12, KEY_F12);

    KeySym2ScanCode(XK_Shift_L, KEY_LEFTSHIFT);
    KeySym2ScanCode(XK_Shift_R, KEY_RIGHTSHIFT);
    KeySym2ScanCode(XK_Control_L, KEY_LEFTCTRL);
    KeySym2ScanCode(XK_Control_R, KEY_RIGHTCTRL);
    KeySym2ScanCode(XK_Caps_Lock, KEY_CAPSLOCK);

    KeySym2ScanCode(XK_Meta_L, KEY_LEFTMETA);
    KeySym2ScanCode(XK_Meta_R, KEY_RIGHTMETA);
    KeySym2ScanCode(XK_Alt_L, KEY_LEFTALT);
    KeySym2ScanCode(XK_Alt_R, KEY_RIGHTALT);
  }

  return 0;
}