// fake_tide81.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include "tide.h"
#include "scintilla.h"

class TideWindow {
public:
  TideWindow(HINSTANCE, LPSTR);
  static void register_class(HINSTANCE);
  static LRESULT __stdcall twnd_proc(HWND, UINT, WPARAM, LPARAM);
  HWND get_wnd() { return hwnd_tide; }

private:
  long wnd_proc(WORD, WPARAM, LPARAM);

private:
  HINSTANCE m_hinstance;
  static const char* class_name;
  HWND hwnd_tide;
  HWND hwnd_editor;
  HWND hwnd_output;

  char window_name[MAX_PATH];
};

const char* TideWindow::class_name = NULL;

TideWindow::TideWindow(HINSTANCE h, LPSTR cmd) {
  m_hinstance = h;
  hwnd_tide = NULL;
  hwnd_editor = NULL;
  hwnd_output = NULL;

  window_name[0] = '\0';

  int left = 0;
  int top = 0;
  int width = 600;
  int height = 400;

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
    // TODO
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
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
