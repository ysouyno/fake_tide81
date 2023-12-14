#pragma once

#include <Windows.h>

#define SCI_START 2000

#define SCI_SETMARGINWIDTH SCI_START + 34

#define STYLE_MAX 31

#define SC_EOL_CRLF 0
#define SC_EOL_CR 1
#define SC_EOL_LF 2

void scintilla_register_classes(HINSTANCE);
