#pragma once

#include <Windows.h>

POINT point_from_lparam(LPARAM);
bool iswordchar(char);
bool pt_close(POINT, POINT);
COLORREF colour_from_string(const char*);
