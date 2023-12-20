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
