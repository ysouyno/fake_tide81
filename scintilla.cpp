#include "scintilla.h"
#include "dlog.h"

class Scintilla {
public:
  static void register_class(HINSTANCE);
  static LRESULT __stdcall swnd_proc(HWND, UINT, WPARAM, LPARAM);

private:
  Scintilla();

  void redraw();

  void create_graphic_objects(HDC);
  void paint();

  long wnd_proc(WORD, WPARAM, LPARAM);

private:
  static HINSTANCE m_hinstance;
  HWND m_hwnd;

  int sel_margin_width;

  HDC hdc_bitmap;
  HBITMAP old_bitmap;
  HBRUSH sel_margin;
  COLORREF sel_background;
  HBITMAP bitmap_line_buffer;
  int line_height;
  HBRUSH background_brush;
  COLORREF background;
};

HINSTANCE Scintilla::m_hinstance = 0;

Scintilla::Scintilla() {
  m_hwnd = NULL;

  sel_margin_width = 20;

  hdc_bitmap = NULL;
  old_bitmap = NULL;
  sel_margin = 0;
  sel_background = RGB(0xc0, 0xc0, 0xc0);
  bitmap_line_buffer = NULL;
  line_height = 1;
  background_brush = 0;
  background = RGB(0xff, 0xff, 0xff);
}

void Scintilla::redraw() {
  InvalidateRect(m_hwnd, NULL, FALSE);
}

void Scintilla::create_graphic_objects(HDC hdc) {
  hdc_bitmap = CreateCompatibleDC(hdc);

  HBITMAP sel_map = CreateCompatibleBitmap(hdc, 8, 8);
  old_bitmap = (HBITMAP)SelectObject(hdc_bitmap, sel_map);

  COLORREF highlight = GetSysColor(COLOR_3DHILIGHT);
  if (highlight == RGB(0xff, 0xff, 0xff)) {
    RECT rc_pattern = { 0, 0, 8, 8 };
    FillRect(hdc_bitmap, &rc_pattern, (HBRUSH)GetStockObject(WHITE_BRUSH));
    HPEN pen_sel = CreatePen(0, 1, GetSysColor(COLOR_3DFACE));
    HPEN pen_old = (HPEN)SelectObject(hdc_bitmap, pen_sel);
    for (int stripe = 0; stripe < 8; ++stripe) {
      MoveToEx(hdc_bitmap, 0, stripe * 2, 0);
      LineTo(hdc_bitmap, 8, stripe * 2 - 8);
    }
    sel_margin = CreatePatternBrush(sel_map);

    SelectObject(hdc_bitmap, pen_old);
    DeleteObject(pen_sel);
  }
  else {
    sel_margin = CreateSolidBrush(sel_background);

    RECT rc_client = { 0 };
    GetClientRect(m_hwnd, &rc_client);

    bitmap_line_buffer = CreateCompatibleBitmap(hdc,
      rc_client.right - rc_client.left, line_height);
    SelectObject(hdc_bitmap, bitmap_line_buffer);
    DeleteObject(sel_map);
    background_brush = CreateSolidBrush(background);
  }
}

void Scintilla::paint() {
  DWORD dwstart = timeGetTime();
  // TODO doc.SetLineCache();
  // TODO RefreshStyleData();
  PAINTSTRUCT ps;
  BeginPaint(m_hwnd, &ps);

  if (!hdc_bitmap) {
    create_graphic_objects(ps.hdc);
  }

  EndPaint(m_hwnd, &ps);
  // TODO ShowCaretAtCurrentPosition();
  DWORD dwend = timeGetTime();
  dprintf("Scintilla::paint(): %dms\n", dwend - dwstart);
}

long Scintilla::wnd_proc(WORD msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_CREATE:
    break;
  case WM_PAINT:
    paint();
    break;
  case SCI_SETMARGINWIDTH:
    if (wparam < 100) {
      sel_margin_width = wparam;
    }
    redraw();
    break;
  default:
    return DefWindowProc(m_hwnd, msg, wparam, lparam);
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

LRESULT __stdcall
Scintilla::swnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  Scintilla* sci = (Scintilla*)GetWindowLong(hwnd, 0);
  if (!sci) {
    if (msg == WM_CREATE) {
      sci = new Scintilla();
      sci->m_hwnd = hwnd;
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
