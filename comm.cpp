#include "comm.h"

POINT point_from_lparam(LPARAM lparam) {
  POINT pt;
  pt.x = (int)(short)LOWORD(lparam);
  pt.y = (int)(short)HIWORD(lparam);
  return pt;
}

bool iswordchar(char ch) {
  return isalnum(ch) || ch == '_';
}

bool pt_close(POINT pt1, POINT pt2) {
  if (abs(pt1.x - pt2.x) > 3)
    return false;
  if (abs(pt1.y - pt2.y) > 3)
    return false;
  return true;
}

int int_from_hex_digit(const char ch) {
  if (isdigit(ch))
    return ch - '0';
  else if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  else if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  else
    return 0;
}

COLORREF colour_from_string(const char* val) {
  int r = int_from_hex_digit(val[1]) * 16 + int_from_hex_digit(val[2]);
  int g = int_from_hex_digit(val[3]) * 16 + int_from_hex_digit(val[4]);
  int b = int_from_hex_digit(val[5]) * 16 + int_from_hex_digit(val[6]);
  return RGB(r, g, b);
}
