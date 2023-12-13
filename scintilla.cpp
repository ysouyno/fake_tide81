#include "scintilla.h"
#include "document.h"
#include "dlog.h"

#pragma warning(disable: 4996)

static int clamp(int val, int min_val, int max_val) {
  if (val > max_val)
    val = max_val;
  if (val < min_val)
    val = min_val;
  return val;
}

class Style {
public:
  Style() : font(0) { clear(); }

  ~Style() {
    if (font)
      DeleteObject(font);
    font = NULL;
  }

  COLORREF fore;
  COLORREF back;
  bool bold;
  bool italic;
  int size;
  char font_name[100];

  HFONT font;
  unsigned int line_height;
  unsigned int ascent;
  unsigned int descent;
  unsigned int external_leading;
  unsigned int ave_char_width;
  unsigned int space_width;

  void clear(COLORREF f0 = RGB(0, 0, 0), COLORREF b0 = RGB(0xff, 0xff, 0xff),
    int s = 8, const char* f1 = "Verdana", bool b1 = false, bool i = false) {
    fore = f0;
    back = b0;
    bold = b1;
    italic = i;
    size = s;
    strcpy(font_name, f1);
    if (font)
      DeleteObject(font);
    font = NULL;
  }

  void realise() {
    if (font)
      DeleteObject(font);
    font = NULL;

    HDC hdc = CreateCompatibleDC(NULL);

    LOGFONTA lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -(abs(size) * GetDeviceCaps(hdc, LOGPIXELSY)) / 72;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = italic ? 1 : 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, font_name);

    font = CreateFontIndirectA(&lf);
    HFONT font_old = (HFONT)SelectObject(hdc, font);
    // 将 CreateFontIndirectA 新创建的字体选中，调用 GetTextMetricsA，
    // GetTextExtentPointA 设置类成员（ascent，descent 等）的初始值
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    ascent = tm.tmAscent;
    descent = tm.tmDescent;
    external_leading = tm.tmExternalLeading;
    line_height = tm.tmHeight;
    ave_char_width = tm.tmAveCharWidth;

    SIZE sz;
    char ch_space = ' ';
    GetTextExtentPointA(hdc, &ch_space, 1, &sz);
    space_width = sz.cx;

    SelectObject(hdc, font_old);
    DeleteDC(hdc);
  }
};

class Scintilla {
public:
  static void register_class(HINSTANCE);
  static LRESULT __stdcall swnd_proc(HWND, UINT, WPARAM, LPARAM);

private:
  Scintilla();

  void redraw() { InvalidateRect(m_hwnd, NULL, FALSE); }
  int lines_total() { return doc.lines(); }

  int lines_on_screen() {
    RECT rc_client = { 0 };
    GetClientRect(m_hwnd, &rc_client);
    int ht_client = rc_client.bottom - rc_client.top - GetSystemMetrics(SM_CYHSCROLL);
    return ht_client / line_height + 1;
  }

  int max_scroll_pos() {
    int ret_val = lines_total() - lines_on_screen();
    if (ret_val < 0)
      return 0;
    else
      return ret_val;
  }

  void set_vert_scroll_from_top_line() {
    SetScrollPos(m_hwnd, SB_VERT, top_line, TRUE);
  }

  int line_from_position(int pos) {
    return doc.lc.line_from_position(pos);
  }

  void show_caret_at_current_position() {
    POINT pt_caret = location_from_position(current_pos);
    move_caret(pt_caret.x, pt_caret.y);
  }

  void drop_caret() {
    if (caret) {
      HideCaret(m_hwnd);
      DestroyCaret();
      caret = false;
    }
  }

  int length() { return doc.get_length(); }

  void create_graphic_objects(HDC);
  void refresh_style_data();
  void set_scroll_bars(LPARAM* plparam = 0, WPARAM wparam = 0);
  POINT location_from_position(int);
  void move_caret(int, int);
  void ensure_markers_size();
  void paint();

  long wnd_proc(WORD, WPARAM, LPARAM);

private:
  static HINSTANCE m_hinstance;
  HWND m_hwnd;
  Document doc;

  Style styles[STYLE_MAX + 1];
  bool styles_valid;

  int sel_margin_width;
  int top_line;
  HDC hdc_bitmap;
  HBITMAP old_bitmap;
  HBRUSH sel_margin;
  COLORREF sel_background;
  HBITMAP bitmap_line_buffer;
  HBRUSH background_brush;
  COLORREF background;

  int line_height;
  unsigned int max_ascent;
  unsigned int max_descent;
  unsigned int ave_char_width;
  unsigned int space_width;
  unsigned int tab_width;
  unsigned int tab_in_chars;
  int x_offset;
  int current_pos;
  bool caret;
  bool in_overstrike;

  int* markers_set;
  int len_markers_set;

  int end_styled;
};

HINSTANCE Scintilla::m_hinstance = 0;

Scintilla::Scintilla() {
  m_hwnd = NULL;

  styles_valid = false;

  sel_margin_width = 20;
  top_line = 0;
  hdc_bitmap = NULL;
  old_bitmap = NULL;
  sel_margin = 0;
  sel_background = RGB(0xc0, 0xc0, 0xc0);
  bitmap_line_buffer = NULL;
  background_brush = 0;
  background = RGB(0xff, 0xff, 0xff);

  line_height = 1;
  max_ascent = 1;
  max_descent = 1;
  ave_char_width = 1;
  space_width = 1;
  tab_width = 1;
  tab_in_chars = 4;
  x_offset = 0;
  current_pos = 0;
  caret = false;
  in_overstrike = false;

  markers_set = NULL;
  len_markers_set = 0;

  end_styled = 0;
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

void Scintilla::refresh_style_data() {
  if (!styles_valid) {
    styles_valid = true;
    max_ascent = 1;
    max_descent = 1;
    for (unsigned int i = 0; i < (sizeof(styles) / sizeof(styles[0])); ++i) {
      styles[i].realise();
      if (max_ascent < styles[i].ascent)
        max_ascent = styles[i].ascent;
      if (max_descent < styles[i].descent)
        max_descent = styles[i].descent;
    }
    line_height = max_ascent + max_descent;
    ave_char_width = styles[0].ave_char_width;
    space_width = styles[0].space_width;
    tab_width = space_width * tab_in_chars;
    set_scroll_bars();
  }
}

void Scintilla::set_scroll_bars(LPARAM* plparam, WPARAM wparam) {
  doc.set_line_cache();
  refresh_style_data();
  RECT rc_client = { 0 };

  if (plparam) {
    rc_client.right = LOWORD(*plparam);
    rc_client.bottom = HIWORD(*plparam);
  }
  else
    GetClientRect(m_hwnd, &rc_client);

  SCROLLINFO sci = { sizeof(sci) };
  sci.fMask = SIF_PAGE | SIF_RANGE;
  sci.nMin = 0;
  sci.nMax = lines_total();
  sci.nPage = lines_total() - max_scroll_pos() + 1;
  sci.nPos = 0;
  sci.nTrackPos = 1;
  SetScrollInfo(m_hwnd, SB_VERT, &sci, TRUE);
  SetScrollRange(m_hwnd, SB_HORZ, 0, 2000, TRUE);

  if (top_line > max_scroll_pos()) {
    top_line = clamp(top_line, 0, max_scroll_pos());
    set_vert_scroll_from_top_line();
    redraw();
  }

  if (!plparam)
    redraw();
}

POINT Scintilla::location_from_position(int pos) {
  doc.set_line_cache();
  refresh_style_data();
  POINT pt = { 0 };
  int line = line_from_position(pos);
  HDC hdc = GetDC(m_hwnd);
  HFONT font_old = (HFONT)SelectObject(hdc, styles[0].font);
  pt.y = (line - top_line) * line_height;
  int pos_line_start = doc.lc.cache[line];
  int pos_line_end = doc.lc.cache[line + 1];
  int xpos = sel_margin_width;
  for (int i = pos_line_start; i < pos && i <= pos_line_end; ++i) {
    char ch = doc.char_at(i);
    int colour = doc.style_at(i) & 31;
    unsigned int width = 0;
    SIZE sz;
    if (ch == '\t') {
      width = (((xpos + 2 - sel_margin_width) / tab_width) + 1) * tab_width + sel_margin_width - xpos;
    }
    else {
      SelectObject(hdc, styles[colour].font);
      GetTextExtentPoint32A(hdc, &ch, 1, &sz);
      width = sz.cx;
    }
    xpos += width;
  }
  pt.x = xpos;
  SelectObject(hdc, font_old);
  ReleaseDC(m_hwnd, hdc);
  pt.x -= x_offset;
  return pt;
}

void Scintilla::move_caret(int x, int y) {
  RECT rc_client = { 0 };
  GetClientRect(m_hwnd, &rc_client);
  POINT pt_top = { x, y };
  POINT pt_bottom = { x, y + line_height };
  bool caret_visible = PtInRect(&rc_client, pt_top) || PtInRect(&rc_client, pt_bottom);
  if (GetFocus() != m_hwnd)
    caret_visible = false;
  if (caret) {
    if (caret_visible) {
      POINT pt;
      GetCaretPos(&pt);
      if (pt.x != x || pt.y != y) {
        if (in_overstrike) {
          SetCaretPos(x, y + line_height - 2);
        }
        else {
          SetCaretPos(x, y);
        }
      }
    }
    else {
      drop_caret();
    }
  }
  else {
    if (caret_visible) {
      if (in_overstrike) {
        CreateCaret(m_hwnd, 0, ave_char_width - 1, 2);
        SetCaretPos(x, y + line_height - 2);
      }
      else {
        CreateCaret(m_hwnd, 0, 1, line_height);
        SetCaretPos(x, y);
      }
      ShowCaret(m_hwnd);
      caret = true;
    }
  }
}

void Scintilla::ensure_markers_size() {
  doc.set_line_cache();
  if (len_markers_set < lines_total()) {
    int* new_markers_set = new int[lines_total()];
    if (new_markers_set) {
      for (int i = 0; i < lines_total(); ++i) {
        if (markers_set && i < len_markers_set)
          new_markers_set[i] = markers_set[i];
        else
          new_markers_set[i] = 0;
      }
      delete[] markers_set;
      markers_set = new_markers_set;
      len_markers_set = lines_total();
    }
    else {
      delete[] markers_set;
      markers_set = NULL;
    }
  }
}

void Scintilla::paint() {
  DWORD dwstart = timeGetTime();
  doc.set_line_cache();
  refresh_style_data();
  PAINTSTRUCT ps;
  BeginPaint(m_hwnd, &ps);

  if (!hdc_bitmap) {
    create_graphic_objects(ps.hdc);
  }

  if (markers_set)
    ensure_markers_size();

  RECT rc_client = { 0 };
  GetClientRect(m_hwnd, &rc_client);

  int screen_line_paint_first = ps.rcPaint.top / line_height;
  int line_paint_last = top_line + ps.rcPaint.bottom / line_height;
  int end_pos_paint = length();
  if (line_paint_last < doc.lc.lines)
    end_pos_paint = doc.lc.cache[line_paint_last + 1] - 1;
  if (end_pos_paint > end_styled) {
    // Notify container to do some more styling
    // TODO NotifyStyleNeeded(end_pos_paint);
  }

  // TODO

  EndPaint(m_hwnd, &ps);
  show_caret_at_current_position();
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
