#include "scintilla.h"

class Scintilla {
public:
  static void register_class(HINSTANCE);
  static LRESULT __stdcall swnd_proc(HWND, UINT, WPARAM, LPARAM);

private:
  Scintilla();

  void redraw();

  long wnd_proc(WORD, WPARAM, LPARAM);

private:
  static HINSTANCE m_hinstance;
  HWND hwnd;

  int sel_margin_width;
};

HINSTANCE Scintilla::m_hinstance = 0;

Scintilla::Scintilla() {
  hwnd = NULL;

  sel_margin_width = 20;
}

void Scintilla::redraw() {
  InvalidateRect(hwnd, NULL, FALSE);
}

long Scintilla::wnd_proc(WORD msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_CREATE:
    break;
  case SCI_SETMARGINWIDTH:
    if (wparam < 100) {
      sel_margin_width = wparam;
    }
    redraw();
    break;
  default:
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }

  return 0l;
}

void Scintilla::register_class(HINSTANCE h) {
  m_hinstance = h;

  WNDCLASSA wc = { 0 };
  wc.style = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Scintilla::swnd_proc;
  // Reserve extra bytes for each instance of the window, we will use
  // these bytes to store a pointer to the c++ (Scintilla) object
  // corresponding to the window.
  wc.cbWndExtra = sizeof(Scintilla*);
  wc.hInstance = h;
  wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
  wc.lpszClassName = "Scintilla";

  if (!RegisterClassA(&wc)) {
    exit(false);
  }
}

LRESULT __stdcall Scintilla::swnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  Scintilla* sci = (Scintilla*)GetWindowLong(hwnd, 0);
  if (!sci) {
    if (msg == WM_CREATE) {
      sci = new Scintilla();
      sci->hwnd = hwnd;
      SetWindowLong(hwnd, 0, (LONG)sci);
      return sci->wnd_proc(msg, wparam, lparam);
    }
    else {
      return DefWindowProc(hwnd, msg, wparam, lparam);
    }
  }
  else {
    if (msg == WM_DESTROY) {
      delete sci;
      SetWindowLong(hwnd, 0, 0);
      return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    else {
      return sci->wnd_proc(msg, wparam, lparam);
    }
  }
}

void scintilla_register_classes(HINSTANCE h) {
  Scintilla::register_class(h);
}
