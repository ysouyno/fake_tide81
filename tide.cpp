// tide.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "tide.h"
#include "scintilla.h"
#include "prop_set.h"
#include "dlog.h"
#include "comm.h"
#include <Windows.h>
#include <Richedit.h>

#pragma warning(disable: 4996)

const char prop_global_filename[] = "TideGlobal.properties";

bool get_default_properties_filename(char* path_default_props, unsigned int len) {
  GetModuleFileNameA(0, path_default_props, len);
  char* last_slash = strrchr(path_default_props, '\\');
  unsigned int name_len = strlen(prop_global_filename);
  if (last_slash && ((last_slash + 1 - path_default_props + name_len) < len)) {
    strcpy(last_slash + 1, prop_global_filename);
    return true;
  }
  else {
    return false;
  }
}

class TideWindow {
public:
  TideWindow(HINSTANCE, LPSTR);
  static void register_class(HINSTANCE);
  static LRESULT __stdcall twnd_proc(HWND, UINT, WPARAM, LPARAM);
  HWND get_wnd() { return hwnd_tide; }

private:
  void read_global_prop_file();
  int normalise_split(int);
  void size_sub_windows();
  void source_changed();
  int get_current_line_number();
  void char_added(char);
  void get_range(HWND, int, int, char*);
  void colourise(int start = 0, int end = -1, bool editor = true);
  void command(WPARAM, LPARAM);
  void move_split(POINT);
  long wnd_proc(WORD, WPARAM, LPARAM);

  LRESULT send_editor(UINT msg, WPARAM wparam = 0, LPARAM lparam = 0) {
    return SendMessage(hwnd_editor, msg, wparam, lparam);
  }

  LRESULT send_output(UINT msg, WPARAM wparam = 0, LPARAM lparam = 0) {
    return SendMessage(hwnd_output, msg, wparam, lparam);
  }

private:
  HINSTANCE m_hinstance;
  static const char* class_name;
  HWND hwnd_tide;
  HWND hwnd_editor;
  HWND hwnd_output;
  bool split_vertical;
  int height_output;
  PropSet props_base;
  PropSet props;
  enum { height_bar = 7 };
  char window_name[MAX_PATH];
  bool is_dirty;
  bool is_built;
  POINT pt_start_drag;
  bool captured_mouse;
  int height_output_start_drag;
};

const char* TideWindow::class_name = NULL;

TideWindow::TideWindow(HINSTANCE h, LPSTR cmd) {
  m_hinstance = h;
  hwnd_tide = NULL;
  hwnd_editor = NULL;
  hwnd_output = NULL;
  split_vertical = false;
  height_output = 0;
  window_name[0] = '\0';
  props.super_ps = &props_base;
  is_dirty = false;
  is_built = false;
  pt_start_drag.x = 0;
  pt_start_drag.y = 0;
  captured_mouse = false;

  read_global_prop_file();

  int left = props.get_int("position.left");
  int top = props.get_int("position.top");
  int width = props.get_int("position.width");
  int height = props.get_int("position.height");

  hwnd_tide = CreateWindowExA(WS_EX_CLIENTEDGE, class_name, window_name,
    WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
    WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_MAXIMIZE | WS_CLIPCHILDREN,
    left ? left : CW_USEDEFAULT,
    top ? top : CW_USEDEFAULT,
    width ? width : CW_USEDEFAULT,
    height ? height : CW_USEDEFAULT,
    NULL, NULL, h, (LPSTR)this);
  if (!hwnd_tide)
    exit(false);

  ShowWindow(hwnd_tide, SW_SHOWNORMAL);
}

void TideWindow::read_global_prop_file() {
  char propfile[MAX_PATH] = { 0 };
  if (get_default_properties_filename(propfile, sizeof(propfile))) {
    props_base.read(propfile);
  }
}

int TideWindow::normalise_split(int split_pos) {
  RECT rc_client;
  GetClientRect(hwnd_tide, &rc_client);
  int w = rc_client.right - rc_client.left;
  int h = rc_client.bottom - rc_client.top;
  if (split_pos < 20)
    split_pos = 0;
  if (split_vertical) {
    if (split_pos > w - height_bar - 20)
      split_pos = w - height_bar;
  }
  else {
    if (split_pos > h - height_bar - 20)
      split_pos = h - height_bar;
  }
  return split_pos;
}

void TideWindow::size_sub_windows() {
  RECT rc_client;
  GetClientRect(hwnd_tide, &rc_client);
  int w = rc_client.right - rc_client.left;
  int h = rc_client.bottom - rc_client.top;
  height_output = normalise_split(height_output);
  if (split_vertical) {
    SetWindowPos(hwnd_editor, 0, 0, 0, w - height_output - height_bar, h - 1, 0);
    SetWindowPos(hwnd_output, 0, w - height_output, 0, height_output - 1, h - 1, 0);
  }
  else {
    SetWindowPos(hwnd_editor, 0, 0, 0, w - 1, h - height_output - height_bar, 0);
    SetWindowPos(hwnd_output, 0, 0, h - height_output, w - 1, height_output - 1, 0);
  }
}

void TideWindow::source_changed() {
  is_dirty = true;
  is_built = false;
}

int TideWindow::get_current_line_number() {
  int sel_start = 0;
  send_editor(EM_EXGETSEL, (WPARAM)&sel_start, 0);
  return send_editor(EM_LINEFROMCHAR, sel_start);
}

void TideWindow::char_added(char ch) {
  int sel_start = 0;
  int sel_end = 0;
  send_editor(EM_EXGETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
  if (sel_end == sel_start && sel_start > 0) {
    char style = (char)send_editor(SCI_GETSTYLEAT, sel_start - 1, 0);
    if (style == 0) {
      if (send_editor(SCI_CALLTIPACTIVE)) {
        if (ch == ')') {
          send_editor(SCI_CALLTIPCANCEL);
        }
        else {
          // TODO ContinueCallTip();
        }
      }
      else if (send_editor(SCI_AUTOCACTIVE)) {
        if (ch == '(') {
          // TODO StartCallTip();
        }
        else if (!isalpha(ch)) {
          send_editor(SCI_AUTOCCANCEL);
        }
      }
      else {
        if (ch == '.') {
          // TODO StartAutoComplete();
        }
        else if (ch == '(') {
          // TODO StartCallTip();
        }
        else if (ch == '\r' || ch == '\n') {
          char linebuf[1000] = { 0 };
          int cur_line = get_current_line_number();
          int line_length = send_editor(EM_LINELENGTH, cur_line);
          dprintf("[CR] %d len = %d\n", cur_line, line_length);
          if (cur_line > 0 && line_length <= 2) {
            send_editor(EM_GETLINE, cur_line - 1, (LPARAM)linebuf);
            for (int pos = 0; linebuf[pos]; ++pos) {
              if (linebuf[pos] != ' ' && linebuf[pos] != '\t')
                linebuf[pos] = '\0';
            }
            send_editor(EM_REPLACESEL, 0, (LPARAM)linebuf);
          }
        }
      }
    }
  }
}

void TideWindow::get_range(HWND win, int start, int end, char* text) {
  TEXTRANGEA tr;
  tr.chrg.cpMin = start;
  tr.chrg.cpMax = end;
  tr.lpstrText = text;
  SendMessage(win, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
}

void TideWindow::colourise(int start, int end, bool editor) {
  // TODO
}

void TideWindow::command(WPARAM wparam, LPARAM lparam) {
  switch (LOWORD(wparam)) {
  case IDM_SRCWIN: {
    int cmd = HIWORD(wparam);
    if (cmd == EN_CHANGE) {
      source_changed();
    }
    else if (cmd == SCN_STYLENEEDED) {
      int end_pos_paint = lparam;
      int end_styled = send_editor(SCI_GETENDSTYLED);
      int line_end_styled = send_editor(EM_LINEFROMCHAR, end_styled);
      end_styled = send_editor(EM_LINEINDEX, line_end_styled);
      colourise(end_styled, end_pos_paint);
    }
    else if (cmd == SCN_CHARADDED) {
      char_added((char)lparam);
    }
    break;
  }
  default:
    break;
  }
}

void TideWindow::move_split(POINT pt_new_drag) {
  RECT rc_client = { 0 };
  GetClientRect(hwnd_tide, &rc_client);
  int new_size_output = height_output_start_drag + (pt_start_drag.y - pt_new_drag.y);
  if (split_vertical)
    new_size_output = height_output_start_drag + (pt_start_drag.x - pt_new_drag.x);
  new_size_output = normalise_split(new_size_output);
  if (height_output != new_size_output) {
    height_output = new_size_output;
    size_sub_windows();
    InvalidateRect(hwnd_tide, NULL, TRUE);
    UpdateWindow(hwnd_tide);
  }
}

long TideWindow::wnd_proc(WORD imsg, WPARAM wparam, LPARAM lparam) {
  switch (imsg) {
  case WM_CREATE:
    hwnd_editor = CreateWindowA(
      "Scintilla", "Source",
      WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN,
      0, 0, 100, 100,
      hwnd_tide,
      (HMENU)IDM_SRCWIN,
      m_hinstance, 0);
    if (!hwnd_editor)
      exit(false);
    ShowWindow(hwnd_editor, SW_SHOWNORMAL);
    SetFocus(hwnd_editor);

    hwnd_output = CreateWindowA(
      "Scintilla", "Run",
      WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN,
      0, 0, 100, 100,
      hwnd_tide,
      (HMENU)IDM_RUNWIN,
      m_hinstance, 0);
    if (!hwnd_output)
      exit(false);
    ShowWindow(hwnd_output, SW_SHOWNORMAL);
    send_output(SCI_SETMARGINWIDTH, 0, 0);
    break;
  case WM_PAINT: {
    PAINTSTRUCT ps = { 0 };
    BeginPaint(hwnd_tide, &ps);
    RECT rc_client = { 0 };
    GetClientRect(hwnd_tide, &rc_client);
    int height_client = rc_client.bottom - rc_client.top;
    int width_client = rc_client.right - rc_client.left;

    HBRUSH brush_surface = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
    FillRect(ps.hdc, &rc_client, brush_surface);
    DeleteObject(brush_surface);

    HPEN pen_hilight = CreatePen(0, 1, GetSysColor(COLOR_3DHILIGHT));
    HPEN pen_old = (HPEN)SelectObject(ps.hdc, pen_hilight);
    HPEN pen_shadow = CreatePen(0, 1, GetSysColor(COLOR_3DSHADOW));
    HPEN pen_dark = CreatePen(0, 1, GetSysColor(COLOR_3DDKSHADOW));

    if (split_vertical) {
      MoveToEx(ps.hdc, width_client - (height_output + height_bar - 1), 0, 0);
      LineTo(ps.hdc, width_client - (height_output + height_bar - 1), height_client);

      SelectObject(ps.hdc, pen_shadow);
      MoveToEx(ps.hdc, width_client - (height_output + 2), 0, 0);
      LineTo(ps.hdc, width_client - (height_output + 2), height_client);

      SelectObject(ps.hdc, pen_dark);
      MoveToEx(ps.hdc, width_client - (height_output + 1), 0, 0);
      LineTo(ps.hdc, width_client - (height_output + 1), height_client);
    }
    else {
      MoveToEx(ps.hdc, 0, height_client - (height_output + height_bar - 1), 0);
      LineTo(ps.hdc, width_client, height_client - (height_output + height_bar - 1));

      SelectObject(ps.hdc, pen_shadow);
      MoveToEx(ps.hdc, 0, height_client - (height_output + 2), 0);
      LineTo(ps.hdc, width_client, height_client - (height_output + 2));

      SelectObject(ps.hdc, pen_dark);
      MoveToEx(ps.hdc, 0, height_client - (height_output + 1), 0);
      LineTo(ps.hdc, width_client, height_client - (height_output + 1));
    }

    SelectObject(ps.hdc, pen_old);
    DeleteObject(pen_dark);
    DeleteObject(pen_shadow);
    DeleteObject(pen_hilight);
    EndPaint(hwnd_tide, &ps);
    break;
  }
  case WM_SIZE:
    size_sub_windows();
    break;
  case WM_COMMAND:
    command(wparam, lparam);
    break;
  case WM_LBUTTONDOWN:
    pt_start_drag = point_from_lparam(lparam);
    captured_mouse = true;
    height_output_start_drag = height_output;
    SetCapture(hwnd_tide);
    break;
  case WM_MOUSEMOVE:
    if (captured_mouse) {
      POINT pt_new_drag = point_from_lparam(lparam);
      move_split(pt_new_drag);
    }
    break;
  case WM_LBUTTONUP:
    if (captured_mouse) {
      POINT pt_new_drag = point_from_lparam(lparam);
      move_split(pt_new_drag);
      captured_mouse = false;
      ReleaseCapture();
    }
    break;
  case WM_DESTROY:
    if (hwnd_editor)
      DestroyWindow(hwnd_editor);
    hwnd_editor = 0;
    if (hwnd_output)
      DestroyWindow(hwnd_output);
    hwnd_output = 0;
    PostQuitMessage(0);
    break;
  case WM_ACTIVATEAPP:
    send_editor(EM_HIDESELECTION, !wparam);
    break;
  case WM_ACTIVATE:
    SetFocus(hwnd_editor);
    break;
  default:
    return DefWindowProc(hwnd_tide, imsg, wparam, lparam);
  }

  return 0l;
}

void TideWindow::register_class(HINSTANCE h) {
  const char* app_name = "Tide";
  class_name = "TideWindow";

  WNDCLASSA wc = { 0 };
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = TideWindow::twnd_proc;
  wc.cbWndExtra = sizeof(TideWindow*);
  wc.hInstance = h;
  wc.hIcon = LoadIconA(h, app_name);
  wc.lpszMenuName = app_name;
  wc.lpszClassName = class_name;

  if (!RegisterClassA(&wc))
    exit(false);
}

LRESULT __stdcall
TideWindow::twnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  TideWindow* tide = (TideWindow*)GetWindowLong(hwnd, 0);
  if (!tide) {
    if (msg == WM_CREATE) {
      LPCREATESTRUCT cs = (LPCREATESTRUCT)lparam;
      tide = (TideWindow*)cs->lpCreateParams;
      tide->hwnd_tide = hwnd;
      SetWindowLong(hwnd, 0, (LONG)tide);
      return tide->wnd_proc(msg, wparam, lparam);
    }
    else
      return DefWindowProc(hwnd, msg, wparam, lparam);
  }
  else
    return tide->wnd_proc(msg, wparam, lparam);
}

int __stdcall WinMain(
  _In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPSTR lpCmdLine,
  _In_ int nShowCmd
) {
  HACCEL h_acc_table = LoadAcceleratorsA(hInstance, "ACCELS");

  TideWindow::register_class(hInstance);
  scintilla_register_classes(hInstance);

  TideWindow main_wnd(hInstance, lpCmdLine);

  bool going = true;
  MSG msg;
  msg.wParam = 0;
  while (going) {
    going = GetMessage(&msg, NULL, 0, 0);
    if (going) {
      if (TranslateAccelerator(msg.hwnd, h_acc_table, &msg) == 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    else if (going) {
      if (TranslateAccelerator(main_wnd.get_wnd(), h_acc_table, &msg) == 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }

  return (int)msg.wParam;
}
