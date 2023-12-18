#include "comm.h"

POINT point_from_lparam(LPARAM lparam) {
  POINT pt;
  pt.x = (int)(short)LOWORD(lparam);
  pt.y = (int)(short)HIWORD(lparam);
  return pt;
}
