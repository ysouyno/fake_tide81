// tide.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "tide.h"
#include "scintilla.h"
#include "prop_set.h"
#include "dlog.h"
#include "comm.h"
#include "keywords.h"
#include "resource.h"
#include <Windows.h>
#include <Richedit.h>
#include <direct.h>
#include <stdio.h>
#include <process.h>

#pragma warning(disable: 4996)

const char prop_global_file_name[] = "TideGlobal.properties";
const char prop_file_name[] = "Tide.properties";
const int block_size = 32768;
HWND current_dlg = NULL;

bool get_default_properties_filename(char* path_default_props, unsigned int len) {
  GetModuleFileNameA(0, path_default_props, len);
  char* last_slash = strrchr(path_default_props, '\\');
  unsigned int name_len = strlen(prop_global_file_name);
  if (last_slash && ((last_slash + 1 - path_default_props + name_len) < len)) {
    strcpy(last_slash + 1, prop_global_file_name);
    return true;
  }
  else {
    return false;
  }
}

// default_dlg, do_dialog is a bit like something in PC Magazine May 28, 1991, page 357
// default_dlg is only used for about box
int __stdcall default_dlg(HWND hdlg, WORD msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_INITDIALOG: {
    HWND wsci = GetDlgItem(hdlg, IDABOUTSCINTILLA);
    if (wsci) {
      // TODO
    }
    return TRUE;
  }
  case WM_CLOSE:
    SendMessage(hdlg, WM_COMMAND, IDCANCEL, NULL);
    break;
  case WM_COMMAND:
    if (LOWORD(wparam) == IDOK) {
      EndDialog(hdlg, IDOK);
      return TRUE;
    }
    else if (LOWORD(wparam) == IDCANCEL) {
      EndDialog(hdlg, IDCANCEL);
      return FALSE;
    }
  }

  return FALSE;
}

int __stdcall grep_dlg(HWND hdlg, WORD msg, WPARAM wparam, LPARAM lparam) {
  static PropSet* props;
  switch (msg) {
  case WM_INITDIALOG:
    props = (PropSet*)lparam;
    SetDlgItemTextA(hdlg, IDFINDWHAT, props->get("find.what"));
    SetDlgItemTextA(hdlg, IDFILES, props->get("find.files"));
    return TRUE;
  case WM_CLOSE:
    SendMessage(hdlg, WM_COMMAND, IDCANCEL, NULL);
    break;
  case WM_COMMAND:
    if (LOWORD(wparam) == IDCANCEL) {
      EndDialog(hdlg, IDCANCEL);
      return FALSE;
    }
    else if (LOWORD(wparam) == IDOK) {
      char s[200] = { 0 };
      GetDlgItemTextA(hdlg, IDFINDWHAT, s, sizeof(s));
      props->set("find.what", s);
      GetDlgItemTextA(hdlg, IDFILES, s, sizeof(s));
      props->set("find.files", s);
      EndDialog(hdlg, IDOK);
      return TRUE;
    }
  }

  return FALSE;
}

int do_dialog(HANDLE hinst, LPSTR lp_res_name, HWND hwnd, FARPROC lpproc, DWORD dw_init_param) {
  int result = -1;

  if (!lpproc)
    lpproc = (FARPROC)default_dlg;

  DLGPROC lpfn = (DLGPROC)MakeProcInstance(lpproc, hinst);
  if (!lpfn) {
    MessageBoxA(hwnd, "Failed to create proc instance.", "Tide", MB_OK);
  }
  else {
    if (!dw_init_param)
      result = DialogBoxA((HINSTANCE)hinst, lp_res_name, hwnd, lpfn);
    else
      result = DialogBoxParamA((HINSTANCE)hinst, lp_res_name, hwnd, lpfn, dw_init_param);

    if (-1 == result) {
      DWORD dwerror = GetLastError();
      char errormsg[200] = { 0 };
      sprintf(errormsg, "Failed to create Dialog box %d.", dwerror);
      MessageBoxA(hwnd, errormsg, "Tide", MB_OK);
    }

    FreeProcInstance(lpfn);
  }

  return result;
}

class RecentFile {
public:
  RecentFile() {
    file_name[0] = '\0';
    line_number = -1;
  }

  char file_name[MAX_PATH];
  int line_number;
};

class TideWindow {
public:
  TideWindow(HINSTANCE, LPSTR);
  static void register_class(HINSTANCE);
  static LRESULT __stdcall twnd_proc(HWND, UINT, WPARAM, LPARAM);
  void process_execute();
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
  void set_window_name();
  void tw_new();
  void read_local_prop_file();
  void set_file_name(char*);
  void set_style_for(HWND, char*);
  void read_properties();
  void set_file_stack_menu();
  void add_file_to_stack(char*, int line = -1);
  void open(char* file = 0, bool initial_cmd = false);
  int length_document();
  void save_as(char* file = NULL);
  void save();
  int save_if_unsure();
  void drop_file_stack_top();
  void stack_menu(int);
  void selection_into_find();
  void find();
  void find_next();
  void add_command(char*);
  void output_append_string(char*, int len = -1);
  void execute();
  void find_in_files();
  void command(WPARAM, LPARAM);
  void move_split(POINT);
  void handle_find_replace();
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
  char full_path[MAX_PATH];
  char file_name[MAX_PATH];
  char file_ext[MAX_PATH];
  char dir_name[MAX_PATH];
  char dir_name_at_execute[MAX_PATH];
  bool is_dirty;
  bool is_built;
  POINT pt_start_drag;
  bool captured_mouse;
  int height_output_start_drag;
  char* language;
  char** keywords;
  char* keywordlist;
  bool first_properties_read;
  HMENU pyro_menu;
  enum { file_stack_max = 10 };
  RecentFile recent_file_stack[file_stack_max];
  enum { file_stack_cmd_id = 2 };
  HWND w_find_replace;
  char find_what[200];
  FINDREPLACEA fr;
  bool replacing;
  bool havefound;
  enum { command_max = 2 };
  int command_current;
  char* command_to_execute[command_max];
  long cancel_flag;
  bool executing;
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
  full_path[0] = '\0';
  file_name[0] = '\0';
  file_ext[0] = '\0';
  dir_name[0] = '\0';
  dir_name_at_execute[0] = '\0';
  props.super_ps = &props_base;
  is_dirty = false;
  is_built = false;
  pt_start_drag.x = 0;
  pt_start_drag.y = 0;
  captured_mouse = false;
  language = strdup("java");
  keywords = NULL;
  keywordlist = NULL;
  first_properties_read = true;
  pyro_menu = NULL;
  w_find_replace = NULL;
  find_what[0] = '\0';
  memset(&fr, 0, sizeof(fr));
  replacing = false;
  havefound = false;
  command_current = 0;
  for (int icmd = 0; icmd < command_max; ++icmd)
    command_to_execute[icmd] = 0;
  InterlockedExchange(&cancel_flag, 0L);
  executing = false;

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

  open(cmd, true);

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
  DWORD dwstart = timeGetTime();
  HWND win = editor ? hwnd_editor : hwnd_output;
  int length_doc = SendMessage(win, SCI_GETLENGTH, 0, 0);
  if (end == -1)
    end = length_doc;
  char* cdoc = new char[end - start + 1];
  dprintf("colourise size = %d, ptr = %x\n", length_doc, cdoc);

  // 将文件的所有内容拷贝到 cdoc 中
  get_range(win, start, end, cdoc);

  int style_start = 0;
  if (start > 0)
    style_start = SendMessage(win, SCI_GETSTYLEAT, start - 1, 0);
  if (editor)
    colourise_doc(cdoc, start, end - start, style_start, language, keywords, win);
  else
    colourise_doc(cdoc, start, end - start, 0, "errorlist", 0, win);
  InvalidateRect(win, NULL, FALSE);
  delete[] cdoc;
  DWORD dwend = timeGetTime();
  dprintf("end colourise %d\n", dwend - dwstart);
}

void TideWindow::set_window_name() {
  if (file_name[0] == '\0')
    strcpy(window_name, "(Untiled)");
  else
    strcpy(window_name, file_name);
  strcat(window_name, " - Tide");
  // TODO 为什么窗口标题会变成乱码？
  // 因为本工程属性是“Use Unicode Character Set”，所以默认的处理函数是：DefWindowProc，
  // 即 DefWindowProcW，所以会有乱码，因此处理 WM_SETTEXT 消息让它走 DefWindowProcA 即可
  SetWindowTextA(hwnd_tide, window_name);
}

void TideWindow::tw_new() {
  send_editor(SCI_CLEARALL);
  full_path[0] = '\0';
  file_name[0] = '\0';
  file_ext[0] = '\0';
  dir_name[0] = '\0';
  set_window_name();
  is_dirty = false;
  is_built = false;

  // Define markers
  // TODO
}

void TideWindow::read_local_prop_file() {
  char propfile[MAX_PATH] = { 0 };
  if (dir_name[0])
    strcpy(propfile, dir_name);
  else
    _getcwd(propfile, sizeof(propfile));
  strcat(propfile, "\\");
  strcat(propfile, prop_file_name);
  props.read(propfile);
}

void TideWindow::set_file_name(char* open_name) {
  char path_copy[MAX_PATH + 1] = { 0 };

  if (open_name[0] == '\"') {
    strncpy(path_copy, open_name + 1, MAX_PATH);
    path_copy[MAX_PATH] = '\0';
    if (path_copy[strlen(path_copy) - 1] == '\"')
      path_copy[strlen(path_copy) - 1] = '\0';
    _fullpath(full_path, path_copy, MAX_PATH);
  }
  else if (open_name[0]) {
    _fullpath(full_path, open_name, MAX_PATH);
  }
  else {
    full_path[0] = '\0';
  }

  char* cp_dir_end = strrchr(full_path, '\\');
  if (cp_dir_end) {
    strcpy(file_name, cp_dir_end + 1);
    strcpy(dir_name, full_path);
    dir_name[cp_dir_end - full_path] = '\0';
  }
  else {
    strcpy(file_name, full_path);
    strcpy(dir_name, "");
  }

  char file_base[MAX_PATH] = { 0 };
  strcpy(file_base, file_name);
  char* cp_ext = strrchr(file_base, '.');
  if (cp_ext) {
    *cp_ext = '\0';
    strcpy(file_ext, cp_ext + 1);
  }
  else {
    file_ext[0] = '\0';
  }

  read_local_prop_file();

  props.set("FilePath", full_path);
  props.set("FileDir", dir_name);
  props.set("FileName", file_base);
  props.set("FileExt", file_ext);
  props.set("FileNameExt", file_name);

  set_window_name();
}

void TideWindow::set_style_for(HWND win, char* language) {
  SendMessage(win, SCI_STYLECLEARALL, 0, 0);

  for (int style = 0; style < 32; ++style) {
    char* val = NULL;
    char key[200] = { 0 };
    sprintf_s(key, "style.%s.%0d", language, style);
    val = strdup(props.get(key));
    char* opt = val;
    while (opt) {
      char* cp_comma = strchr(opt, ',');
      if (cp_comma)
        *cp_comma = '\0';
      char* colon = strchr(opt, ':');
      if (colon)
        *colon++ = '\0';
      if (0 == strcmp(opt, "italics"))
        SendMessage(win, SCI_STYLESETITALIC, style, 1);
      if (0 == strcmp(opt, "bold"))
        SendMessage(win, SCI_STYLESETBOLD, style, 1);
      if (0 == strcmp(opt, "font"))
        SendMessage(win, SCI_STYLESETFONT, style, (LPARAM)colon);
      if (0 == strcmp(opt, "fore"))
        SendMessage(win, SCI_STYLESETFORE, style, colour_from_string(colon));
      if (0 == strcmp(opt, "back"))
        SendMessage(win, SCI_STYLESETBACK, style, colour_from_string(colon));
      if (0 == strcmp(opt, "size"))
        SendMessage(win, SCI_STYLESETSIZE, style, atoi(colon));
      if (cp_comma)
        opt = cp_comma + 1;
      else
        opt = NULL;
    }
    if (val)
      free(val);
  }
}

void TideWindow::read_properties() {
  DWORD dwstart = timeGetTime();
  free(language);
  language = props.get_new_expand("lexer.", file_name);
  if (keywords) {
    delete[] keywords;
    free(keywordlist);
  }
  keywordlist = props.get_new_expand("keywords.", file_name);
  char prev = ' ';
  int words = 0;
  for (int j = 0; keywordlist[j]; ++j) {
    if (!isspace(keywordlist[j]) && isspace(prev))
      words++;
    prev = keywordlist[j];
  }
  keywords = new char* [words + 1];
  words = 0;
  prev = '\0';
  int len = strlen(keywordlist);
  for (int k = 0; k < len; ++k) {
    if (!isspace(keywordlist[k])) {
      if (!prev) {
        keywords[words] = &keywordlist[k];
        words++;
      }
    }
    else {
      keywordlist[k] = '\0';
    }
    prev = keywordlist[k];
  }
  keywords[words] = &keywordlist[len];

  int code_page = props.get_int("code.page");
  send_editor(SCI_SETCODEPAGE, code_page);

  char* colour = NULL;

  colour = props.get("back");
  if (colour && *colour) {
    send_editor(SCI_SETBACK, colour_from_string(colour));
  }
  colour = props.get("fore");
  if (colour && *colour) {
    send_editor(SCI_SETFORE, colour_from_string(colour));
  }

  char* size = props.get("size");
  if (size && *size) {
    send_editor(SCI_SETSIZE, atoi(size));
  }

  char* font = props.get("font");
  if (font && *font) {
    send_editor(SCI_SETFONT, (WPARAM)(font));
  }

  char* selfore = props.get("selection.fore");
  if (selfore && *selfore) {
    send_editor(SCI_SETSELFORE, 1, colour_from_string(selfore));
  }
  else {
    send_editor(SCI_SETSELFORE, 0, 0);
  }
  colour = props.get("selection.back");
  if (colour && *colour) {
    send_editor(SCI_SETSELBACK, 1, colour_from_string(colour));
  }
  else {
    if (selfore && *selfore)
      send_editor(SCI_SETSELBACK, 0, 0);
    else // Have to show selection somehow
      send_editor(SCI_SETSELBACK, 1, RGB(0xc0, 0xc0, 0xc0));
  }

  set_style_for(hwnd_editor, language);
  set_style_for(hwnd_output, (char*)"errorlist");

  // As user can change vertical versus horizontal split, only read at app startup
  if (first_properties_read)
    split_vertical = props.get_int("split.vertical");

  HMENU mainm = GetMenu(hwnd_tide);
  if (0 == strcmp(language, "pyro")) {
    send_editor(SCI_MARKERADD, 7, 0);
    if (0 == pyro_menu) {
      pyro_menu = CreatePopupMenu();
      InsertMenuA(mainm, 3, MF_POPUP | MF_BYPOSITION, (int)pyro_menu, "&Pyro");
      AppendMenuA(pyro_menu, MF_STRING, IDM_TABTIMMY, "TabTimmy");
      AppendMenuA(pyro_menu, MF_STRING, IDM_SELECTIONMARGIN, "Selection Margin");
      AppendMenuA(pyro_menu, MF_STRING, IDM_BUFFEREDDRAW, "Buffered Drawing");
      AppendMenuA(pyro_menu, MF_STRING, IDM_STEP, "Step\t F8");
    }
  }
  else {
    send_editor(SCI_MARKERDELETEALL);
    // Get rid of menu
    if (pyro_menu) {
      RemoveMenu(mainm, 3, MF_BYPOSITION);
      DestroyMenu(pyro_menu);
      pyro_menu = NULL;
    }
  }

  int tabsize = props.get_int("tabsize");
  if (tabsize) {
    send_editor(SCI_SETTABWIDTH, tabsize);
  }

  first_properties_read = false;

  DWORD dwend = timeGetTime();
}

void TideWindow::set_file_stack_menu() {
  const int start_stack_menu_pos = 5;
  HMENU hmenu = GetMenu(hwnd_tide);
  HMENU hmenu_file = GetSubMenu(hmenu, 0);
  // If there is no file stack menu item and there are > 1 files in stack
  if (GetMenuState(hmenu_file, file_stack_cmd_id, MF_BYCOMMAND) == 0xffffffff &&
    recent_file_stack[1].file_name[0] != '\0') {
    InsertMenuA(hmenu_file, start_stack_menu_pos, MF_BYPOSITION | MF_SEPARATOR, file_stack_cmd_id, "-");
  }
  for (int stack_pos = 1; stack_pos < file_stack_max; ++stack_pos) {
    int item_id = file_stack_cmd_id + stack_pos;
    if (recent_file_stack[stack_pos].file_name[0]) {
      if (GetMenuState(hmenu_file, item_id, MF_BYCOMMAND) == 0xffffffff)
        InsertMenuA(hmenu_file, start_stack_menu_pos + stack_pos, MF_BYPOSITION, item_id,
          recent_file_stack[stack_pos].file_name);
      else
        ModifyMenuA(hmenu_file, item_id, MF_BYCOMMAND, item_id, recent_file_stack[stack_pos].file_name);
    }
    else {
      if (GetMenuState(hmenu_file, item_id, MF_BYCOMMAND) != 0xffffffff) {
        DeleteMenu(hmenu_file, item_id, MF_BYCOMMAND);
      }
    }
  }
}

void TideWindow::add_file_to_stack(char* file, int line) {
  if (file[strlen(file) - 1] == '\\')
    return;
  int stack_pos = 0;
  int eq_pos = file_stack_max - 1;
  for (stack_pos = 0; stack_pos < file_stack_max; ++stack_pos)
    if (strcmp(recent_file_stack[stack_pos].file_name, file) == 0)
      eq_pos = stack_pos;
  for (stack_pos = eq_pos; stack_pos > 0; stack_pos--)
    recent_file_stack[stack_pos] = recent_file_stack[stack_pos - 1];
  strcpy(recent_file_stack[0].file_name, file);
  recent_file_stack[0].line_number = line;
  set_file_stack_menu();
  DrawMenuBar(hwnd_tide);
}

void TideWindow::open(char* file, bool initial_cmd) {
  if (file) {
    tw_new();
    set_file_name(file);
    read_properties();
    if (file_name[0]) {
      DWORD dwstart = timeGetTime();
      send_editor(SCI_SETUNDOCOLLECTION, 0);
      FILE* fp = fopen(full_path, "rb");
      if (fp || initial_cmd) {
        if (fp) {
          char data[block_size] = { 0 };
          char styled_data[block_size * 2] = { 0 };
          add_file_to_stack(full_path);
          int len_file = fread(data, 1, sizeof(data), fp);
          int cur_end = 0;
          while (len_file > 0) {
            for (int i = 0; i < len_file; ++i)
              styled_data[i * 2] = data[i];
            send_editor(SCI_ADDSTYLEDTEXT, len_file * 2, (LPARAM)styled_data);
            cur_end += len_file * 2;
            len_file = fread(data, 1, sizeof(data), fp);
          }
          fclose(fp);
          is_dirty = false;
        }
        else {
          is_dirty = true;
        }
        is_built = false;
        DWORD dwend = timeGetTime();

        send_editor(SCI_SETUNDOCOLLECTION, 1);

        if (0 == strcmp(language, "pyro")) {
          send_editor(SCI_MARKERADD, 7, 0);
        }
      }
      else {
        char msg[200] = { 0 };
        strcpy(msg, "Could not open file \"");
        strcat(msg, full_path);
        strcat(msg, "\".");
        MessageBoxA(hwnd_tide, msg, "Tide", MB_OK);
        set_file_name((char*)"");
      }
      send_editor(EM_EMPTYUNDOBUFFER);
      send_editor(SCI_GOTOPOS, 0);
    }
    InvalidateRect(hwnd_editor, NULL, TRUE);
  }
  else {
    char open_name[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd_tide;
    ofn.hInstance = m_hinstance;
    ofn.lpstrFile = open_name;
    ofn.nMaxFile = sizeof(open_name);
    char* filter = NULL;
    char* open_filter = props.get("open.filter");
    if (open_filter) {
      filter = strdup(open_filter);
      for (int fc = 0; filter[fc]; ++fc)
        if (filter[fc] == '|')
          filter[fc] = '\0';
    }
    ofn.lpstrFilter = filter;
    ofn.lpstrFileTitle = (LPSTR)"Open File";
    ofn.Flags = OFN_HIDEREADONLY;

    if (GetOpenFileNameA(&ofn)) {
      free(filter);
      dprintf("Open: <%s>\n", open_name);
      open(open_name);
    }
    else {
      free(filter);
      return;
    }
  }
  set_window_name();
}

int TideWindow::length_document() {
  return send_editor(SCI_GETLENGTH);
}

void TideWindow::save_as(char* file) {
  if (file && *file) {
    strcpy(full_path, file);
    save();
    set_window_name();
  }
  else {
    char open_name[MAX_PATH] = { 0 };
    strcpy(open_name, file_name);
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwnd_tide;
    ofn.hInstance = m_hinstance;
    ofn.lpstrFile = open_name;
    ofn.nMaxFile = sizeof(open_name);
    ofn.lpstrFileTitle = (LPSTR)"Open File";
    ofn.Flags = OFN_HIDEREADONLY;

    if (GetSaveFileNameA(&ofn)) {
      dprintf("Save: <%s>\n", open_name);
      set_file_name(open_name);
      save();
      read_properties();
      colourise(); // In case extension was changed
      InvalidateRect(hwnd_editor, NULL, TRUE);
    }
  }
}

void TideWindow::save() {
  if (file_name[0]) {
    dprintf("Saving <%s>\n", file_name);
    FILE* fp = fopen(full_path, "wb");
    if (fp) {
      add_file_to_stack(full_path, get_current_line_number());
      int length_doc = length_document();
      for (int i = 0; i < length_doc; ++i) {
        char ch = (char)send_editor(SCI_GETCHARAT, i);
        fputc(ch, fp);
      }
      fclose(fp);
      is_dirty = false;
      if (0 == stricmp(file_name, prop_file_name) || 0 == stricmp(file_name, prop_global_file_name)) {
        read_global_prop_file();
        read_local_prop_file();
        read_properties();
        InvalidateRect(hwnd_editor, NULL, TRUE);
      }
    }
    else {
      char msg[200] = { 0 };
      strcpy(msg, "Could not save file \"");
      strcat(msg, full_path);
      strcat(msg, "\".");
      MessageBoxA(hwnd_tide, msg, "Tide", MB_OK);
    }
  }
  else {
    save_as();
  }
}

int TideWindow::save_if_unsure() {
  if (is_dirty) {
    if (props.get_int("are.you.sure")) {
      char msg[200] = { 0 };
      strcpy(msg, "Save changes to \"");
      strcat(msg, full_path);
      strcat(msg, "\"?");
      int decision = MessageBoxA(hwnd_tide, msg, "Tide", MB_YESNOCANCEL);
      if (decision == IDYES)
        save();
      return decision;
    }
    else {
      save();
    }
  }
  else {
    recent_file_stack[0].line_number = get_current_line_number();
    dprintf("Saving pos %d %s\n", recent_file_stack[0].line_number, recent_file_stack[0].file_name);
  }
  return IDYES;
}

void TideWindow::drop_file_stack_top() {
  for (int stack_pos = 0; stack_pos < file_stack_max; ++stack_pos)
    recent_file_stack[stack_pos] = recent_file_stack[stack_pos + 1];
  strcpy(recent_file_stack[file_stack_max - 1].file_name, "");
  recent_file_stack[file_stack_max - 1].line_number = -1;
  set_file_stack_menu();
}

void TideWindow::stack_menu(int pos) {
  if (pos == -1) {
    int last_pos = -1;
    for (int stack_pos = 1; stack_pos < file_stack_max; ++stack_pos) {
      if (recent_file_stack[stack_pos].file_name[0])
        last_pos = stack_pos;
    }
    if (last_pos > 0) {
      int line = recent_file_stack[last_pos].line_number;
      open(recent_file_stack[last_pos].file_name);
      if (line != -1)
        send_editor(SCI_GOTOLINE, line + 1);
    }
  }
  else {
    if (recent_file_stack[pos].file_name[0] == '\0') // Empty
      tw_new();
    else {
      int line = recent_file_stack[pos].line_number;
      open(recent_file_stack[pos].file_name);
      if (line != -1)
        send_editor(SCI_GOTOLINE, line + 1);
    }
  }
}

void TideWindow::selection_into_find() {
  int sel_start = 0;
  int sel_end = 0;
  int length_doc = length_document();
  send_editor(EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
  if (sel_start == sel_end) {
    // Try and find a word at the caret
    if (isalnum(send_editor(SCI_GETCHARAT, sel_start))) {
      while (sel_start > 0 && (isalnum(send_editor(SCI_GETCHARAT, sel_start - 1))))
        sel_start--;
      while (sel_end < length_doc - 1 && (isalnum(send_editor(SCI_GETCHARAT, sel_end + 1))))
        sel_end++;
      if (sel_start < sel_end)
        sel_end++; // Because normal selections end one past
    }
  }
  if (sel_start < sel_end && sel_end - sel_start < sizeof(find_what)) {
    get_range(hwnd_editor, sel_start, sel_end - 1, find_what);
    find_what[sel_end - sel_start] = '\0';
  }
}

void TideWindow::find() {
  if (w_find_replace)
    return;
  selection_into_find();

  memset(&fr, 0, sizeof(fr));
  fr.lStructSize = sizeof(fr);
  fr.hwndOwner = hwnd_tide;
  fr.hInstance = m_hinstance;
  fr.Flags = FR_NOUPDOWN;
  fr.lpstrFindWhat = find_what;
  fr.wFindWhatLen = sizeof(find_what);
  w_find_replace = FindTextA(&fr);
  replacing = false;
  current_dlg = w_find_replace;
}

void TideWindow::find_next() {
  if (0 == fr.lpstrFindWhat) {
    find();
    return;
  }
  FINDTEXTEXA ft = { 0 };
  ft.chrg.cpMin = send_editor(SCI_GETCURRENTPOS) + 1;
  ft.chrg.cpMax = length_document();
  ft.lpstrText = fr.lpstrFindWhat;
  ft.chrgText.cpMin = 0;
  ft.chrgText.cpMax = 0;
  int pos_find = send_editor(EM_FINDTEXTEX, fr.Flags & (FR_WHOLEWORD | FR_MATCHCASE), (LPARAM)&ft);
  if (pos_find == -1) {
    havefound = false;
    char msg[200] = { 0 };
    strcpy(msg, "Cannot find the string \"");
    strcat(msg, fr.lpstrFindWhat);
    strcat(msg, "\".");
    if (w_find_replace)
      MessageBoxA(w_find_replace, msg, "Tide", MB_OK | MB_ICONWARNING);
    else
      MessageBoxA(hwnd_tide, msg, "Tide", MB_OK | MB_ICONWARNING);
  }
  else {
    havefound = true;
    send_editor(EM_SETSEL, ft.chrgText.cpMax + 1, ft.chrgText.cpMin);
    if (w_find_replace && !replacing) {
      EndDialog(w_find_replace, IDCANCEL);
      w_find_replace = NULL;
      current_dlg = NULL;
    }
  }
}

void TideWindow::add_command(char* cmd) {
  if (cmd) {
    if (cmd[0]) {
      command_to_execute[command_current] = cmd;
      command_current++;
    }
    else {
      free(cmd);
    }
  }
}

void TideWindow::output_append_string(char* s, int len) {
  if (-1 == len)
    len = strlen(s);
  send_output(SCI_ADDTEXT, len, (LPARAM)s);
  send_output(SCI_GOTOPOS, send_output(WM_GETTEXTLENGTH));
}

// process_execute runs a command with redirected input and output streams
// so the output can be put in a window.
// It is based upon several usenet posts and a knowledge base article.
void TideWindow::process_execute() {
  DWORD exitcode = 0;

  send_output(SCI_GOTOPOS, send_output(WM_GETTEXTLENGTH));
  int original_end = send_output(SCI_GETCURRENTPOS);

  for (int icmd = 0; icmd < command_current && icmd < command_max && exitcode == 0; ++icmd) {
    OSVERSIONINFOA osv = { sizeof(OSVERSIONINFOA) };
    GetVersionExA(&osv);
    bool windows95 = osv.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES) };
    PROCESS_INFORMATION pi = { 0 };
    HANDLE hpipe_write = NULL;
    HANDLE hpipe_read = NULL;
    HANDLE hwrite2 = NULL;
    HANDLE hread2 = NULL;
    char buffer[256] = { 0 };
    output_append_string((char*)">");
    output_append_string(command_to_execute[icmd]);
    output_append_string((char*)"\n");

    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    SECURITY_DESCRIPTOR sd;
    // If NT make a real security thing to allow inheriting handles
    if (!windows95) {
      InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
      SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
      sa.nLength = sizeof(SECURITY_ATTRIBUTES);
      sa.lpSecurityDescriptor = &sd;
    }

    // Create pipe for output redirection
    CreatePipe(
      &hpipe_read,
      &hpipe_write,
      &sa,
      0
    );

    // Create pipe for input redirection.
    // In this code, you do not redirect the output of the child process,
    // but you need a handle to set the hStdInput field in the STARTUP_INFO
    // struct. For safety, you should not set the handles to an invalid handle.
    CreatePipe(
      &hread2,
      &hwrite2,
      &sa,
      0
    );

    // Make child process use hpipe_write as standard out, and make
    // sure it does not show on screen.
    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hwrite2;
    si.hStdOutput = hpipe_write;
    si.hStdError = hpipe_write;

    bool worked = CreateProcessA(
      NULL,
      command_to_execute[icmd],
      NULL, NULL,
      TRUE, 0,
      NULL, NULL,
      &si, &pi
    );

    if (!worked) {
      output_append_string((char*)">Failed to CreateProcess\n");
    }

    // Now that this has been inherited, close it to be safe.
    // You don't want to write to it accidentally
    CloseHandle(hpipe_write);

    bool completed = !worked;
    DWORD time_detected_death = 0;
    while (!completed) {
      Sleep(100);

      if (cancel_flag) {
        TerminateProcess(pi.hProcess, 1);
        completed = true;
        break;
      }

      bool nt_or_data = true;
      DWORD bytes_read = 0;

      if (windows95) {
        DWORD bytes_avail = 0;
        if (PeekNamedPipe(hpipe_read, buffer, sizeof(buffer), &bytes_read, &bytes_avail, NULL)) {
          if (0 == bytes_avail) {
            nt_or_data = false;
            DWORD dwexitcode = STILL_ACTIVE;
            if (GetExitCodeProcess(pi.hProcess, &dwexitcode)) {
              if (STILL_ACTIVE != dwexitcode) {
                // Process is dead, but wait a second in case there is some output in transit
                if (time_detected_death == 0)
                  time_detected_death = timeGetTime();
                else
                  if (timeGetTime() - time_detected_death > 1000)
                    completed = true; // It's a dead process
              }
            }
          }
        }
      }

      if (!completed && nt_or_data) {
        int btest = ReadFile(hpipe_read, buffer, sizeof(buffer), &bytes_read, NULL);
        if (btest && bytes_read) {
          // Display the data
          // 为什么这里输出的结果是错乱的，像是多个线程向同一个地方写入？
          output_append_string(buffer, bytes_read);
          UpdateWindow(hwnd_tide);
        }
        else
          completed = true;
      }
    }

    if (worked) {
      WaitForSingleObject(pi.hProcess, INFINITE);
      GetExitCodeProcess(pi.hProcess, &exitcode);
      char exitmessage[80] = { 0 };
      if (!exitcode)
        is_built = true;
      sprintf(exitmessage, ">Exit code: %d\n", exitcode);
      output_append_string(exitmessage);
      CloseHandle(pi.hProcess);
    }

    CloseHandle(hpipe_read);
  }

  // Move selection back to beginning of this run so that F4 will go
  // to first error of this run.
  send_output(SCI_GOTOPOS, original_end);
  SendMessage(hwnd_tide, WM_COMMAND, IDM_FINISHEDEXECUTE, 0);
}

void exec_thread(void* ptw) {
  TideWindow* tw = (TideWindow*)(ptw);
  tw->process_execute();
}

void TideWindow::execute() {
  send_output(SCI_MARKERDELETEALL, 0, 0);
  send_editor(SCI_MARKERDELETEALL, 0, 0);
  // Ensure the output pane is visible
  if (height_output < 20) {
    if (split_vertical)
      height_output = normalise_split(300);
    else
      height_output = normalise_split(100);
    size_sub_windows();
    InvalidateRect(hwnd_tide, NULL, TRUE);
    UpdateWindow(hwnd_tide);
  }

  InterlockedExchange(&cancel_flag, 0L);
  executing = true;
  chdir(dir_name);
  strcpy(dir_name_at_execute, dir_name);
  _beginthread(exec_thread, 1024 * 1024, (void*)this);
}

void TideWindow::find_in_files() {
  selection_into_find();
  props.set("find.what", find_what);
  props.set("find.directory", ".");
  if (do_dialog(m_hinstance, (LPSTR)"Grep", hwnd_tide, (FARPROC)grep_dlg, (DWORD)&props) == IDOK) {
    dprintf("asked to find %s %s %s\n", props.get("find.what"), props.get("find.files"), props.get("find.directory"));
    add_command(strdup(props.get_new_expand("find.command", "")));
    if (command_current > 0)
      execute();
  }
}

void TideWindow::command(WPARAM wparam, LPARAM lparam) {
  switch (LOWORD(wparam)) {
  case IDM_NEW:
    if (save_if_unsure() != IDCANCEL) {
      tw_new();
    }
    break;
  case IDM_OPEN:
    if (save_if_unsure() != IDCANCEL) {
      open();
      SetFocus(hwnd_editor);
    }
    break;
  case IDM_CLOSE:
    if (save_if_unsure() != IDCANCEL) {
      drop_file_stack_top();
      stack_menu(0);
      // If none left, New()
      SetFocus(hwnd_editor);
    }
    break;
  case IDM_SAVE:
    save();
    SetFocus(hwnd_editor);
    break;
  case IDM_SAVEAS:
    save_as();
    SetFocus(hwnd_editor);
    break;
  case IDM_QUIT:
    if (save_if_unsure() != IDCANCEL)
      SendMessage(hwnd_tide, WM_CLOSE, 0, 0);
    break;
  case IDM_NEXTFILE:
    if (save_if_unsure() != IDCANCEL)
      stack_menu(1);
    break;
  case IDM_UNDO:
    send_editor(WM_UNDO);
    break;
  case IDM_REDO:
    send_editor(SCI_REDO);
    break;
  case IDM_CUT:
    send_editor(WM_CUT);
    break;
  case IDM_COPY:
    send_editor(WM_COPY);
    break;
  case IDM_PASTE:
    send_editor(WM_PASTE);
    break;
  case IDM_CLEAR:
    send_editor(WM_CLEAR);
    break;
  case IDM_SELECTALL:
    send_editor(SCI_SELECTALL);
    break;
  case IDM_FIND:
    find();
    break;
  case IDM_FINDNEXT:
    find_next();
    break;
  case IDM_FINDINFILES:
    find_in_files();
    break;
  case IDM_FINISHEDEXECUTE:
    executing = false;
    for (int icmd = 0; icmd < command_max; ++icmd) {
      free(command_to_execute[icmd]);
      command_to_execute[icmd] = NULL;
    }
    command_current = 0;
    break;
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

void TideWindow::handle_find_replace() {
  if (fr.Flags & FR_DIALOGTERM) {
    w_find_replace = NULL;
    current_dlg = NULL;
    dprintf("Cancel FindReplace\n");
  }
  else if (fr.Flags & FR_FINDNEXT) {
    dprintf("FindNext <%s> %x\n", fr.lpstrFindWhat, fr.Flags);
    find_next();
  }
  else if (fr.Flags & FR_REPLACE) {
    // TODO
  }
  else if (fr.Flags & FR_REPLACEALL) {
    // TODO
  }
  else {
    dprintf("Find/replace message %x\n", fr.Flags);
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
  case WM_SETTEXT:
    return DefWindowProcA(hwnd_tide, imsg, wparam, lparam);
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
    if (imsg == RegisterWindowMessage(FINDMSGSTRING)) {
      handle_find_replace();
    }
    else {
      return DefWindowProc(hwnd_tide, imsg, wparam, lparam);
    }
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
    if (current_dlg && going) {
      if (!IsDialogMessage(current_dlg, &msg)) {
        if (TranslateAccelerator(msg.hwnd, h_acc_table, &msg) == 0) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
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
