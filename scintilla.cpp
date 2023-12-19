#include "scintilla.h"
#include "document.h"
#include "dlog.h"
#include <CommCtrl.h>
#include <Richedit.h>

#pragma warning(disable: 4996)

#define SCI_NORM 0
#define SCI_SHIFT SHIFT_PRESSED
#define SCI_CTRL LEFT_CTRL_PRESSED
#define SCI_CSHIFT (SCI_CTRL | SCI_SHIFT)

class KeyToCommand {
public:
  int key;
  int modifiers;
  int msg;
};

KeyToCommand keymap_default[] = {
  VK_DOWN, SCI_NORM, SCI_LINEDOWN,
  VK_DOWN, SCI_SHIFT, SCI_LINEDOWNEXTEND,
  VK_UP, SCI_NORM, SCI_LINEUP,
  VK_UP, SCI_SHIFT, SCI_LINEUPEXTEND,
  VK_LEFT, SCI_NORM, SCI_CHARLEFT,
  VK_LEFT, SCI_SHIFT, SCI_CHARLEFTEXTEND,
  VK_LEFT, SCI_CTRL, SCI_WORDLEFT,
  VK_LEFT, SCI_CSHIFT, SCI_WORDLEFTEXTEND,
  VK_RIGHT, SCI_NORM, SCI_CHARRIGHT,
  VK_RIGHT, SCI_SHIFT, SCI_CHARRIGHTEXTEND,
  VK_RIGHT, SCI_CTRL, SCI_WORDRIGHT,
  VK_RIGHT, SCI_CSHIFT, SCI_WORDRIGHTEXTEND,
  VK_HOME, SCI_NORM, SCI_VCHOME,
  VK_HOME, SCI_SHIFT, SCI_VCHOMEEXTEND,
  VK_HOME, SCI_CTRL, SCI_DOCUMENTSTART,
  VK_HOME, SCI_CSHIFT, SCI_DOCUMENTSTARTEXTEND,
  VK_END,  SCI_NORM, SCI_LINEEND,
  VK_END,  SCI_SHIFT, SCI_LINEENDEXTEND,
  VK_END, SCI_CTRL, SCI_DOCUMENTEND,
  VK_END, SCI_CSHIFT, SCI_DOCUMENTENDEXTEND,
  VK_PRIOR, SCI_NORM, SCI_PAGEUP,
  VK_PRIOR, SCI_SHIFT, SCI_PAGEUPEXTEND,
  VK_NEXT, SCI_NORM, SCI_PAGEDOWN,
  VK_NEXT, SCI_SHIFT, SCI_PAGEDOWNEXTEND,
  VK_DELETE, SCI_NORM, WM_CLEAR,
  VK_DELETE, SCI_SHIFT, WM_CUT,
  VK_INSERT, SCI_NORM, SCI_EDITTOGGLEOVERTYPE,
  VK_INSERT, SCI_SHIFT, WM_PASTE,
  VK_INSERT, SCI_CTRL, WM_COPY,
  VK_ESCAPE,  SCI_NORM, SCI_CANCEL,
  VK_BACK, SCI_NORM, SCI_DELETEBACK,
  'Z', SCI_CTRL, WM_UNDO,
  'Y', SCI_CTRL, SCI_REDO,
  'X', SCI_CTRL, WM_CUT,
  'C', SCI_CTRL, WM_COPY,
  'V', SCI_CTRL, WM_PASTE,
  'A', SCI_CTRL, SCI_SELECTALL,
  VK_TAB, SCI_NORM, SCI_TAB,
  VK_TAB, SCI_SHIFT, SCI_BACKTAB,
  VK_RETURN, SCI_NORM, SCI_NEWLINE,
  'L', SCI_CTRL, SCI_FORMFEED,
  0,0,0,
};

bool is_key_down(int virt_key) {
  return GetKeyState(virt_key) & 0x80000000;
}

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

  int selection_start() { return min(current_pos, anchor); }
  int selection_end() { return max(current_pos, anchor); }
  void modified_at(int pos) { if (end_styled > pos) end_styled = pos; }

  void get_text_rect(RECT* prc) { // 获取文本区域的客户区大小
    GetClientRect(m_hwnd, prc);
    prc->left += sel_margin_width;
  }

  void create_graphic_objects(HDC);
  void refresh_style_data();
  void set_scroll_bars(LPARAM* plparam = 0, WPARAM wparam = 0);
  POINT location_from_position(int);
  void move_caret(int, int);
  void ensure_markers_size();
  void paint_sel_margin(PAINTSTRUCT*);
  void paint();
  void invalidate_range(int, int);
  void set_selection(int, int);
  void delete_chars(int, int);
  void clear_selection();
  void insert_styled_string(int, char*, int);
  void insert_char(int, char);
  void insert_string(int, char*);
  void insert_string(int, char*, int);
  void scroll_to(int);
  void ensure_caret_visible();
  void notify_change();
  void notify_char(char);
  void notify_style_needed(int);
  void add_char(char);
  void drop_graphics();
  bool is_crlf(int);
  void del_char();
  void del_char_back();
  void clear();
  int key_down(WPARAM, LPARAM);
  int key_command(WORD);

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
  COLORREF sel_foreground;
  COLORREF sel_background;
  HBITMAP bitmap_line_buffer;
  HBRUSH background_brush;
  HBITMAP bitmap_sel_margin;
  COLORREF foreground;
  COLORREF background;
  HBRUSH background_sel;

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
  int anchor;
  int eol_mode;
  bool is_modified;
  bool in_call_tip_mode;
  bool in_auto_complete_mode;
  bool buffered_draw;
  bool hide_selection;
  bool selforeset;
  bool selbackset;
  bool view_whitespace;
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
  sel_foreground = RGB(0xff, 0, 0);
  sel_background = RGB(0xc0, 0xc0, 0xc0);
  bitmap_line_buffer = NULL;
  background_brush = 0;
  bitmap_sel_margin = NULL;
  foreground = RGB(0, 0, 0);
  background = RGB(0xff, 0xff, 0xff);
  background_sel = NULL;

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
  anchor = 0;
  eol_mode = SC_EOL_CRLF;
  is_modified = false;
  in_call_tip_mode = false;
  in_auto_complete_mode = false;
  buffered_draw = true;
  hide_selection = false;
  selforeset = false;
  selbackset = true;
  view_whitespace = false;
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

void Scintilla::paint_sel_margin(PAINTSTRUCT* pps) {
  RECT rc_sel_margin = { 0 };
  GetClientRect(m_hwnd, &rc_sel_margin);
  rc_sel_margin.right = sel_margin_width;

  RECT rcis = { 0 };
  BOOL intersects = IntersectRect(&rcis, &(pps->rcPaint), &rc_sel_margin);
  if (!intersects)
    return;

  HBITMAP old_bm = NULL;
  HDC hdc_show = pps->hdc;
  if (buffered_draw) {
    if (!bitmap_sel_margin) {
      bitmap_sel_margin = CreateCompatibleBitmap(pps->hdc,
        sel_margin_width, rc_sel_margin.bottom - rc_sel_margin.top);
    }
    old_bm = (HBITMAP)SelectObject(hdc_bitmap, bitmap_sel_margin);
    hdc_show = hdc_bitmap;
  }

  FillRect(hdc_show, &rc_sel_margin, sel_margin);

  if (markers_set) {
    // TODO
  }

  if (buffered_draw) {
    BitBlt(pps->hdc,
      rc_sel_margin.left,
      rc_sel_margin.top,
      rc_sel_margin.right,
      rc_sel_margin.bottom,
      hdc_show,
      0, 0, SRCCOPY);
    SelectObject(hdc_bitmap, old_bm);
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
    notify_style_needed(end_pos_paint);
  }

  int sty = 0;
  char segment[4000] = { 0 };
  int xpos = sel_margin_width;
  int ypos = 0;
  if (!buffered_draw)
    ypos += screen_line_paint_first * line_height;
  int ypos_screen = screen_line_paint_first * line_height;

  int sel_start = selection_start();
  int sel_end = selection_end();

  paint_sel_margin(&ps);

  if (ps.rcPaint.right > sel_margin_width) {
    HDC hdc_show = ps.hdc;
    if (buffered_draw) {
      SelectObject(hdc_bitmap, bitmap_sel_margin);
      hdc_show = hdc_bitmap;
    }

    HFONT font_old = (HFONT)SelectObject(ps.hdc, styles[0].font);

    SelectObject(hdc_bitmap, bitmap_line_buffer);
    HPEN pen_old = (HPEN)SelectObject(hdc_show, GetStockObject(BLACK_PEN));
    HBRUSH brush_old = (HBRUSH)SelectObject(hdc_show, background_brush);

    int line = top_line + screen_line_paint_first;

    // Remove selection margin from drawing area so text will not be drawn
    // on it in unbuffered mode.
    IntersectClipRect(ps.hdc, sel_margin_width, 0, 32000, 32000);

    while (line < doc.lc.lines && ypos_screen < ps.rcPaint.bottom) {
      HBRUSH mark_brush = NULL;
      int marks = 0;
      COLORREF mark_back = RGB(0, 0, 0);
      if (markers_set && !sel_margin_width) {
        marks = markers_set[line];
        if (marks) {
          for (int mark_bit = 0; mark_bit < 32 && marks; ++mark_bit) {
            if (marks & 1) {
              // TODO mark_back = markers[mark_bit].back;
            }
            marks >>= 1;
          }
          mark_brush = CreateSolidBrush(mark_back);
        }
        marks = markers_set[line];
      }

      segment[0] = '\0';
      int seg_pos = 0;
      int pos_line_start = doc.lc.cache[line];
      int pos_line_end = doc.lc.cache[line + 1];

      RECT rc_blank;
      rc_blank.top = ypos;
      rc_blank.bottom = ypos + line_height;

      bool in_selection = pos_line_start > sel_start && pos_line_end < sel_end;

      SetTextAlign(hdc_show, TA_BASELINE);

      int indicators_set = 0;
      if (pos_line_start < pos_line_end)
        indicators_set = doc.style_at(pos_line_start) & INDICS_MASK;
      int prev_indic = 0;
      int ind_start[INDIC_MAX] = { 0 };
      char ch_prev = '\0';
      bool visible_in_selection = false;
      int ch = ' ';
      int colour = 0;

      for (int i = pos_line_start; i <= pos_line_end; ++i) {
        if (i < pos_line_end) {
          ch = doc.char_at(i);
          colour = doc.style_at(i);
        }

        // If there is the end of a style run for any reason
        if (colour != sty || ch == '\t' || ch_prev == '\t' || i == sel_start || i == sel_end || i == pos_line_end) {
          int style_main = sty & 31;
          SetTextColor(hdc_show, styles[style_main].fore);
          SetBkColor(hdc_show, styles[style_main].back);
          if (in_selection && !hide_selection) {
            if (selbackset)
              SetBkColor(hdc_show, sel_background);
            if (selforeset)
              SetTextColor(hdc_show, sel_foreground);
            visible_in_selection = true;
          }
          else {
            if (marks)
              SetBkColor(hdc_show, mark_back);
          }
          SelectObject(hdc_show, styles[style_main].font);
          unsigned int width = 0;
          if (segment[0] == '\t') {
            int next_tab = (((xpos + 2 - sel_margin_width) / tab_width) + 1) * tab_width + sel_margin_width;
            rc_blank.left = xpos - x_offset;
            rc_blank.right = next_tab - x_offset;
            if (in_selection && !hide_selection) {
              if (selbackset)
                FillRect(hdc_show, &rc_blank, background_sel);
              else {
                HBRUSH br_tab = CreateSolidBrush(styles[style_main].back);
                FillRect(hdc_show, &rc_blank, br_tab);
                DeleteObject(br_tab);
              }
            }
            else {
              if (mark_brush) {
                FillRect(hdc_show, &rc_blank, mark_brush);
              }
              else {
                HBRUSH br_tab = CreateSolidBrush(styles[style_main].back);
                FillRect(hdc_show, &rc_blank, br_tab);
                DeleteObject(br_tab);
              }
            }
            if (view_whitespace) {
              SelectObject(hdc_show, GetStockObject(NULL_BRUSH));
              SelectObject(hdc_show, GetStockObject(BLACK_PEN));
              RoundRect(hdc_show,
                rc_blank.left + 1 - x_offset,
                rc_blank.top + 4,
                rc_blank.right - 1 - x_offset,
                rc_blank.bottom - max_descent,
                8, 8);
            }
            width = next_tab - xpos;
          }
          else {
            SIZE sz;
            GetTextExtentPoint32A(hdc_show, segment, seg_pos, &sz);
            RECT rc_show = { 0 };
            rc_show.left = xpos - x_offset;
            rc_show.right = xpos + sz.cx - x_offset;
            if (buffered_draw) {
              rc_show.top = ypos;
              rc_show.bottom = ypos + line_height;
              ExtTextOutA(hdc_show, xpos - x_offset, max_ascent, ETO_OPAQUE, &rc_show, segment, seg_pos, NULL);
            }
            else {
              rc_show.top = ypos;
              rc_show.bottom = ypos + line_height;
              ExtTextOutA(hdc_show, xpos - x_offset, ypos + max_ascent, ETO_OPAQUE, &rc_show, segment, seg_pos, NULL);
            }
            if (view_whitespace) {
              SelectObject(hdc_show, GetStockObject(BLACK_PEN));
              SIZE sz_ch;
              int xx = xpos;
              for (int cpos = 0; cpos < seg_pos; ++cpos) {
                GetTextExtentPoint32A(hdc_show, segment + cpos, 1, &sz_ch);
                if (segment[cpos] == ' ') {
                  MoveToEx(hdc_show, xx + sz_ch.cx / 2 - x_offset, ypos + line_height / 2, 0);
                  LineTo(hdc_show, xx + sz_ch.cx / 2 - x_offset, ypos + line_height / 2 + 1);
                }
                xx += sz_ch.cx;
              }
            }

            width = sz.cx;
          }
          seg_pos = 0;
          segment[seg_pos] = '\0';
          xpos += width;
          sty = colour;
        }

        if (i == sel_start)
          in_selection = true;

        if (i == sel_end && i < pos_line_end)
          in_selection = false;

        if (i == current_pos) {
          // May need to adjust caret for window to screen
          // TODO
        }

        if (i < pos_line_end) { // Do not index onto next line
          indicators_set = sty & INDICS_MASK;
        }
        if (indicators_set != prev_indic) {
          int mask = INDIC0_MASK;
          int indicnum = 0;
          for (indicnum = 0; indicnum <= INDIC_MAX; ++indicnum) {
            if (indicators_set & mask && !(prev_indic & mask)) {
              RECT rc_indic = {
                ind_start[indicnum] - x_offset,
                ypos + (LONG)max_ascent,
                xpos - x_offset,
                ypos + (LONG)max_ascent + 3 };
              // TODO indicators[indicnum].draw(hdc_show, rc_indic);
            }
            mask = mask << 1;
          }
          prev_indic = indicators_set;
        }

        if (ch != '\r' && ch != '\n')
          segment[seg_pos++] = ch;

        ch_prev = ch;
      }

      rc_blank.left = xpos - x_offset;
      rc_blank.right = xpos + ave_char_width - x_offset;
      if (in_selection && !hide_selection && visible_in_selection && selbackset)
        FillRect(hdc_show, &rc_blank, background_sel);
      else if (mark_brush)
        FillRect(hdc_show, &rc_blank, mark_brush);
      else
        FillRect(hdc_show, &rc_blank, background_brush);

      rc_blank.left = xpos + ave_char_width - x_offset;
      rc_blank.right = rc_client.right;
      if (mark_brush)
        FillRect(hdc_show, &rc_blank, mark_brush);
      else
        FillRect(hdc_show, &rc_blank, background_brush);

      if (buffered_draw) {
        BitBlt(ps.hdc, sel_margin_width, ypos_screen, rc_client.right - rc_client.left,
          line_height + 1, hdc_bitmap, sel_margin_width, 0, SRCCOPY);
      }

      if (!buffered_draw) {
        ypos += line_height;
      }

      ypos_screen += line_height;
      xpos = sel_margin_width;
      line++;
    }

    RECT rc_beyond_eof = rc_client;
    rc_beyond_eof.left = sel_margin_width;
    rc_beyond_eof.top = (doc.lc.lines - top_line) * line_height - 1;
    if (rc_beyond_eof.top < rc_beyond_eof.bottom)
      FillRect(ps.hdc, &rc_beyond_eof, background_brush);

    SelectObject(hdc_show, pen_old);
    SelectObject(hdc_show, brush_old);
    SelectObject(hdc_show, font_old);
  }

  EndPaint(m_hwnd, &ps);
  show_caret_at_current_position();
  DWORD dwend = timeGetTime();
  dprintf("Scintilla::paint(): %dms\n", dwend - dwstart);
}

void Scintilla::invalidate_range(int start, int end) {
  int minpos = min(start, end);
  int maxpos = max(start, end);
  int minline = line_from_position(minpos);
  int maxline = line_from_position(maxpos);

  RECT rc_redraw;
  rc_redraw.left = sel_margin_width;
  rc_redraw.top = (minline - top_line) * line_height;
  rc_redraw.right = 32000;
  // The +1 at end should not be needed but draws bad without
  rc_redraw.bottom = (maxline - top_line + 1) * line_height;
  // Ensure rectangle is within 16 bit space
  rc_redraw.top = clamp(rc_redraw.top, -32000, 32000);
  rc_redraw.bottom = clamp(rc_redraw.bottom, -32000, 32000);
  InvalidateRect(m_hwnd, &rc_redraw, FALSE);
}

void Scintilla::set_selection(int c, int a) {
  if (current_pos != anchor)
    invalidate_range(current_pos, anchor);
  current_pos = c;
  anchor = a;
  if (current_pos != anchor)
    invalidate_range(current_pos, anchor);
}

// Unlike Undo, Redo, and InsertStyledString, the POS is a cell number not a char number
void Scintilla::delete_chars(int pos, int len) {
  if (doc.is_read_only()) {
    // TODO NotifyModifyAttempt();
  }
  if (!doc.is_read_only()) {
    bool start_save_point = doc.is_save_point();
    doc.delete_chars(pos * 2, len * 2);
    if (start_save_point && doc.is_collecting_undo()) {
      // TODO NotifySavePoint(!startSavePoint);
    }
    modified_at(pos);
    notify_change();
    set_scroll_bars();
  }
}

void Scintilla::clear_selection() {
  int start_pos = selection_start();
  unsigned int chars = selection_end() - start_pos;
  set_selection(start_pos, start_pos);
  if (0 != chars) {
    delete_chars(start_pos, chars);
    notify_change();
  }
}

void Scintilla::insert_styled_string(int position, char* s, int insert_length) {
  if (doc.is_read_only()) {
    // TODO NotifyModifyAttempt();
  }
  if (!doc.is_read_only()) {
    bool start_save_point = doc.is_save_point();
    doc.insert_string(position, s, insert_length);
    if (start_save_point && doc.is_collecting_undo()) {
      // TODO NotifySavePoint(!start_save_point);
    }
    modified_at(position / 2);
    notify_change();
    set_scroll_bars();
  }
}

void Scintilla::insert_char(int pos, char ch) {
  char chs[2] = { ch, 0 };
  insert_styled_string(pos * 2, chs, 2);
}

// Insert a null terminated string
void Scintilla::insert_string(int position, char* s) {
  insert_string(position, s, strlen(s));
}

// Insert a string with a length
void Scintilla::insert_string(int position, char* s, int insert_length) {
  char* s_with_style = new char[insert_length * 2];
  if (s_with_style) {
    for (int i = 0; i < insert_length; ++i) {
      s_with_style[i * 2] = s[i];
      s_with_style[i * 2 + 1] = 0;
    }
    insert_styled_string(position * 2, s_with_style, insert_length * 2);
    delete[] s_with_style;
  }
}

void Scintilla::scroll_to(int line) {
  if (line < 0)
    line = 0;
  if (line > max_scroll_pos())
    line = max_scroll_pos();
  top_line = clamp(line, 0, max_scroll_pos());
  set_vert_scroll_from_top_line();
  redraw();
}

void Scintilla::ensure_caret_visible() {
  RECT rc_client = { 0 };
  get_text_rect(&rc_client);
  POINT pt = location_from_position(current_pos);
  POINT pt_bottom_caret = pt;
  pt_bottom_caret.y += line_height;
  if (!PtInRect(&rc_client, pt) || !PtInRect(&rc_client, pt_bottom_caret)) {
    if (pt.y < 0) {
      top_line += (pt.y / line_height);
    }
    else if (pt_bottom_caret.y > rc_client.bottom) {
      top_line += (pt_bottom_caret.y - rc_client.bottom) / line_height + 1;
    }
    scroll_to(top_line);
    // The 2s here are to ensure the caret is really visible
    if (pt.x < rc_client.left) {
      x_offset = x_offset - (rc_client.left - pt.x) - 2;
    }
    else if (pt.x > rc_client.right) {
      x_offset = x_offset + (pt.x - rc_client.right) + 2;
    }
    if (x_offset < 0)
      x_offset = 0;
    SetScrollPos(m_hwnd, SB_HORZ, x_offset, TRUE);
    redraw();
  }
}

void Scintilla::notify_change() {
  is_modified = true;
  SendMessage(GetParent(m_hwnd), WM_COMMAND,
    MAKELONG(GetDlgCtrlID(m_hwnd), EN_CHANGE), (LPARAM)m_hwnd);
}

void Scintilla::notify_char(char ch) {
  SendMessage(GetParent(m_hwnd), WM_COMMAND,
    MAKELONG(GetDlgCtrlID(m_hwnd), SCN_CHARADDED), ch);
}

void Scintilla::notify_style_needed(int end_style_needed) {
  SendMessage(GetParent(m_hwnd), WM_COMMAND,
    MAKELONG(GetDlgCtrlID(m_hwnd), SCN_STYLENEEDED), end_style_needed);
}

void Scintilla::add_char(char ch) {
  bool was_selection = current_pos != anchor;
  clear_selection();
  if (ch == '\r') {
    if (eol_mode == SC_EOL_CRLF || eol_mode == SC_EOL_CR)
      insert_char(current_pos, '\r');
    set_selection(current_pos + 1, current_pos + 1);
    if (eol_mode == SC_EOL_CRLF || eol_mode == SC_EOL_LF) {
      insert_char(current_pos, '\n');
      set_selection(current_pos + 1, current_pos + 1);
    }
    // TODO AutoCompleteCancel();
    // TODO CallTipCancel();
  }
  else {
    // 这里为什么要删除一个字符，in_overstrike 不是加粗的意思吗？
    if (in_overstrike && !was_selection) {
      if (current_pos < (length() - 1)) {
        char current_ch = doc.char_at(current_pos);
        if (current_ch != '\r' && current_ch != '\n')
          delete_chars(current_pos, 1);
      }
    }
    insert_char(current_pos, ch);
    set_selection(current_pos + 1, current_pos + 1);
    // TODO if (inAutoCompleteMode) AutoCompleteChanged();
  }
  ensure_caret_visible();
  notify_change();
  redraw();
  notify_char(ch);
}

void Scintilla::drop_graphics() {
  drop_caret();
  if (background_brush)
    DeleteObject(background_brush);
  background_brush = 0;
  if (background_sel)
    DeleteObject(background_sel);
  background_sel = 0;
  if (sel_margin)
    DeleteObject(sel_margin);
  sel_margin = 0;
  if (hdc_bitmap && old_bitmap)
    SelectObject(hdc_bitmap, old_bitmap);
  old_bitmap = NULL;
  if (bitmap_line_buffer)
    DeleteObject(bitmap_line_buffer);
  bitmap_line_buffer = NULL;
  if (bitmap_sel_margin)
    DeleteObject(bitmap_sel_margin);
  bitmap_sel_margin = 0;
  if (hdc_bitmap)
    DeleteDC(hdc_bitmap);
  hdc_bitmap = NULL;
}

bool Scintilla::is_crlf(int pos) {
  if (pos < 0)
    return false;
  if (pos >= (length() - 1))
    return false;
  return (doc.char_at(pos) == '\r' && doc.char_at(pos + 1) == '\n');
}

void Scintilla::del_char() {
  if (is_crlf(current_pos)) {
    delete_chars(current_pos, 2);
  }
  else if (current_pos < length()) {
    delete_chars(current_pos, 1);
  }
  notify_change();
  redraw();
}

void Scintilla::del_char_back() {
  if (current_pos > 0) {
    if (is_crlf(current_pos - 2)) {
      delete_chars(current_pos - 2, 2);
      set_selection(current_pos - 2, current_pos - 2);
    }
    else {
      delete_chars(current_pos - 1, 1);
      set_selection(current_pos - 1, current_pos - 1);
    }
  }
  notify_change();
}

void Scintilla::clear() {
  if (current_pos == anchor) {
    del_char();
  }
  else {
    clear_selection();
  }
  set_selection(current_pos, current_pos);
  redraw();
}

int Scintilla::key_down(WPARAM wparam, LPARAM lparam) {
  KeyToCommand* keymap = keymap_default;
  bool shift = is_key_down(VK_SHIFT);
  bool crtl = is_key_down(VK_CONTROL);
  int modifiers = (shift ? SCI_SHIFT : 0) | (crtl ? SCI_CTRL : 0);
  for (int key_index = 0; keymap[key_index].key; ++key_index) {
    if (wparam == keymap[key_index].key && modifiers == keymap[key_index].modifiers) {
      return wnd_proc(keymap[key_index].msg, 0, 0);
    }
  }
  return 1;
}

int Scintilla::key_command(WORD msg) {
  doc.set_line_cache();
  POINT pt = location_from_position(current_pos);
  switch (msg) {
  case SCI_DELETEBACK:
    del_char_back();
    if (in_auto_complete_mode) {
      // TODO AutoCompleteChanged();
    }
    // TODO if (inCallTipMode && (posStartCallTip > currentPos)) CallTipCancel();
    notify_change();
    break;
  }
  return 0;
}

long Scintilla::wnd_proc(WORD msg, WPARAM wparam, LPARAM lparam) {
  // dprintf("S start wnd proc %x %d %d\n", msg, wparam, lparam);
  switch (msg) {
  case WM_CREATE:
    break;
  case WM_PAINT:
    paint();
    break;
  case WM_SIZE:
    set_scroll_bars(&lparam, wparam);
    drop_graphics();
    break;
  case WM_CHAR:
    if (!iscntrl(wparam & 0xff))
      add_char(wparam & 0xff);
    return 1;
  case WM_KEYDOWN:
    return key_down(wparam, lparam);
  case WM_KEYUP:
    break;
  case WM_KILLFOCUS:
    drop_caret();
    break;
  case WM_SETFOCUS:
    show_caret_at_current_position();
    break;
  case WM_CLEAR:
    clear();
    set_scroll_bars();
    break;
  case EM_EXGETSEL:
    if (wparam)
      *(LPDWORD)wparam = selection_start();
    if (lparam)
      *(LPDWORD)lparam = selection_end();
    return MAKELONG(selection_start(), selection_end());
  case EM_LINEFROMCHAR:
    return line_from_position(wparam);
  case EM_LINELENGTH:
    doc.set_line_cache();
    return doc.lc.cache[wparam];
  case EM_REPLACESEL: {
    clear_selection();
    char* replacement = (char*)lparam;
    insert_string(current_pos, replacement);
    set_selection(current_pos + strlen(replacement), current_pos + strlen(replacement));
    notify_change();
    set_scroll_bars();
    ensure_caret_visible();
    redraw();
    break;
  }
  case EM_LINEINDEX:
    doc.set_line_cache();
    return doc.lc.cache[wparam];
  case EM_GETTEXTRANGE: {
    TEXTRANGEA* tr = (TEXTRANGEA*)lparam;
    int iplace = 0;
    for (int ichar = tr->chrg.cpMin; ichar <= tr->chrg.cpMax; ++ichar)
      tr->lpstrText[iplace++] = doc.char_at(ichar);
    return iplace;
  }
  case EM_HIDESELECTION:
    hide_selection = wparam;
    redraw();
    break;
  case SCI_GETSTYLEAT:
    if ((int)wparam >= length())
      return 0;
    else
      return doc.style_at(wparam);
  case SCI_CALLTIPACTIVE:
    return in_call_tip_mode;
  case SCI_CALLTIPCANCEL:
    // TODO CallTipCancel();
    break;
  case SCI_AUTOCACTIVE:
    return in_auto_complete_mode;
    break;
  case SCI_AUTOCCANCEL:
    // TODO AutoCompleteCancel();
    break;
  case SCI_SETMARGINWIDTH:
    if (wparam < 100) {
      sel_margin_width = wparam;
    }
    redraw();
    break;
  case SCI_GETENDSTYLED:
    return end_styled;
  case SCI_DELETEBACK:
    return key_command(msg);
  default:
    return DefWindowProc(m_hwnd, msg, wparam, lparam);
  }

  return 0l;
}

void Scintilla::register_class(HINSTANCE h) {
  m_hinstance = h;

  InitCommonControls(); // 现在 TideWindow 可以处理 IDM_SRCWIN 了

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
