#include "dlog.h"
#include <Windows.h>

void dprintf(const char* fmt, ...) {
  char buf[1024] = { 0 };
  char* args = (char*)&fmt + sizeof(fmt);
  wvsprintfA(buf, fmt, args);
  OutputDebugStringA(buf);
}
