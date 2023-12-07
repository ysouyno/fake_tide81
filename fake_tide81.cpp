// fake_tide81.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>

LRESULT __stdcall
wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  if (msg == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  else
    return DefWindowProcW(hwnd, msg, w, l);
}

int __stdcall
WinMain(HINSTANCE h, HINSTANCE hprev, LPSTR cmdline, int show) {
  WNDCLASSW wc = { 0 };
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wndproc;
  wc.lpszClassName = L"unknown";

  if (!RegisterClass(&wc))
    return -1;

  HWND hwnd = CreateWindowW(L"unknown", L"", WS_OVERLAPPEDWINDOW,
    0, 0, 600, 400, NULL, NULL, h, 0);
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  return (int)msg.wParam;
}
