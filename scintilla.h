#pragma once

#include <Windows.h>

#define SCI_START 2000

#define SCI_SETMARGINWIDTH SCI_START + 34

#define STYLE_MAX 31

void scintilla_register_classes(HINSTANCE);
