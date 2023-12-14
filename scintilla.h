#pragma once

#include <Windows.h>

#define SCI_START 2000

#define SCI_SETMARGINWIDTH SCI_START + 34

#define STYLE_MAX 31

#define SC_EOL_CRLF 0
#define SC_EOL_CR 1
#define SC_EOL_LF 2

#define SCN_CHARADDED 2001

#define SCI_GETSTYLEAT SCI_START + 10
#define SCI_AUTOCCANCEL SCI_START + 101
#define SCI_AUTOCACTIVE SCI_START + 102
#define SCI_CALLTIPCANCEL SCI_START + 201
#define SCI_CALLTIPACTIVE SCI_START + 202

void scintilla_register_classes(HINSTANCE);
