#include "keywords.h"
#include "scintilla.h"
#include "dlog.h"

enum e_state {
  e_default = 0,
  e_comment = 1,
  e_line_comment = 2,
  e_doc_comment = 3,
  e_number = 4,
  e_word = 5,
  e_string = 6,
  e_char = 7,
  e_punct = 8,
  e_pre_proc = 9
};

inline bool iswordchar2(char ch) {
  return isalnum(ch) || ch == '.' || ch == '_';
}

inline bool iswordstart(char ch) {
  return isalnum(ch) || ch == '_';
}

bool word_in_list(char* word, char** list) {
  if (!list)
    return false;
  for (int i = 0; list[i][0]; ++i) {
    // Initial test is to mostly avoid slow function call
    if (list[i][0] == word[0] && 0 == strcmp(list[i], word))
      return true;
  }
  return false;
}

void colour_seg_hwnd(HWND hwnd, unsigned int start, unsigned int end, char ch_attr) {
  if (end < start)
    dprintf("Bad colour position %d - %d", start, end);
  SendMessage(hwnd, SCI_SETSTYLING, end - start + 1, ch_attr);
}

void classify_word_cpp(char* cdoc, unsigned int start, unsigned int end, char** keywords, HWND hwnd) {
  char s[100] = { 0 };
  bool word_is_number = isdigit(cdoc[start]) || cdoc[start] == '.';
  for (unsigned i = 0; i < end - start + 1 && i < 30; ++i) {
    s[i] = cdoc[start + i];
    s[i + 1] = '\0';
  }
  char ch_attr = e_default;
  if (word_is_number)
    ch_attr = e_number;
  else {
    if (word_in_list(s, keywords))
      ch_attr = e_word;
  }
  colour_seg_hwnd(hwnd, start, end, ch_attr);
}

static void colourise_cpp_doc(char* cdoc, int length_doc, int init_style, char** keywords, HWND hwnd) {
  e_state state = (e_state)init_style;
  char ch_prev = ' ';
  char ch_next = cdoc[0];
  int start_seg = 0;
  for (int i = 0; i < length_doc; ++i) {
    e_state state_prev = state;
    char ch = ch_next;
    ch_next = ' ';
    if (i + 1 < length_doc)
      ch_next = cdoc[i + 1];

    if (state == e_default) {
      if (iswordstart(ch)) {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_word;
        start_seg = i;
      }
      else if (ch == '/' && ch_next == '*') {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_comment;
        start_seg = i;
      }
      else if (ch == '/' && ch_next == '/') {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_line_comment;
        start_seg = i;
      }
      else if (ch == '\"') {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_string;
        start_seg = i;
      }
      else if (ch == '\'') {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_char;
        start_seg = i;
      }
      else if (ch == '#') {
        colour_seg_hwnd(hwnd, start_seg, i - 1, e_default);
        state = e_pre_proc;
        start_seg = i;
      }
    }
    else if (state == e_word) {
      if (!iswordchar2(ch)) {
        classify_word_cpp(cdoc, start_seg, i - 1, keywords, hwnd);
        state = e_default;
        start_seg = i;
        if (ch == '/' && ch_next == '*') {
          state = e_comment;
        }
        else if (ch == '/' && ch_next == '/') {
          state = e_line_comment;
        }
        else if (ch == '\"') {
          state = e_string;
        }
        else if (ch == '\'') {
          state = e_char;
        }
        else if (ch == '#') {
          state = e_pre_proc;
        }
      }
    }
    else {
      if (state == e_pre_proc) {
        if ((ch == '\r' || ch == '\n') && ch_prev != '\\') {
          state = e_default;
          colour_seg_hwnd(hwnd, start_seg, i - 1, e_pre_proc);
          start_seg = i;
        }
      }
      else if (state == e_comment) {
        if (ch == '/' && ch_prev == '*' && ((i > start_seg + 2) || ((init_style == e_comment) && (start_seg == 0)))) {
          state = e_default;
          colour_seg_hwnd(hwnd, start_seg, i, e_comment);
          start_seg = i + 1;
        }
      }
      else if (state == e_line_comment) {
        if (ch == '\r' || ch == '\n') {
          colour_seg_hwnd(hwnd, start_seg, i - 1, e_line_comment);
          state = e_default;
          start_seg = i;
        }
      }
      else if (state == e_string) {
        if (ch == '\\') {
          if (ch_next == '\"' || ch_next == '\'' || ch_next == '\\') {
            i++;
            ch = ch_next;
            ch_next = ' ';
            if (i + 1 < length_doc)
              ch_next = cdoc[i + 1];
          }
        }
        else if (ch == '\"') {
          colour_seg_hwnd(hwnd, start_seg, i, e_string);
          state = e_default;
          i++;
          ch = ch_next;
          ch_next = ' ';
          if (i + 1 < length_doc)
            ch_next = cdoc[i + 1];
          start_seg = i;
        }
      }
      else if (state == e_char) {
        if (ch == '\\') {
          if (ch_next == '\"' || ch_next == '\'' || ch_next == '\\') {
            i++;
            ch = ch_next;
            ch_next = ' ';
            if (i + 1 < length_doc)
              ch_next = cdoc[i + 1];
          }
        }
        else if (ch == '\'') {
          colour_seg_hwnd(hwnd, start_seg, i, e_char);
          state = e_default;
          i++;
          ch = ch_next;
          ch_next = ' ';
          if (i + 1 < length_doc)
            ch_next = cdoc[i + 1];
          start_seg = i;
        }
      }
      if (state == e_default) { // One of the above succeeded
        if (ch == '/' && ch_next == '*') {
          state = e_comment;
        }
        else if (ch == '/' && ch_next == '/') {
          state = e_line_comment;
        }
        else if (ch == '\"') {
          state = e_string;
        }
        else if (ch == '\'') {
          state = e_char;
        }
        else if (ch == '#') {
          state = e_pre_proc;
        }
        else if (iswordstart(ch)) {
          state = e_word;
        }
      }
    }
    ch_prev = ch;
  }
  if (start_seg < length_doc) {
    if (state == e_word)
      classify_word_cpp(cdoc, start_seg, length_doc - 1, keywords, hwnd);
    else
      colour_seg_hwnd(hwnd, start_seg, length_doc - 1, state);
  }
}

static void colourise_errorlist_line(char* line_buffer, int length_line, HWND hwnd) {
  if (line_buffer[0] == '>') {
    colour_seg_hwnd(hwnd, 0, length_line - 1, 4);
  }
  else if (strstr(line_buffer, "File \"") && strstr(line_buffer, ", line ")) {
    colour_seg_hwnd(hwnd, 0, length_line - 1, 1);
  }
  else {
    // Look for <filename>:<line>::message
    int state = 0;
    for (int i = 0; i < length_line; ++i) {
      if (state == 0 && line_buffer[i] == ':' && isdigit(line_buffer[i + 1])) {
        state = 1;
      }
      else if (state == 0 && line_buffer[i] == '(') {
        state = 10;
      }
      else if (state == 1 && isdigit(line_buffer[i])) {
        state = 2;
      }
      else if (state == 2 && line_buffer[i] == ':') {
        state = 3;
      }
      else if (state == 2 && !isdigit(line_buffer[i])) {
        state = 99;
      }
      else if (state == 10 && isdigit(line_buffer[i])) {
        state = 11;
      }
      else if (state == 11 && line_buffer[i] == ')') {
        state = 12;
      }
      else if (state == 12 && line_buffer[i] == ':') {
        state = 13;
      }
      else if (state == 11 && !isdigit(line_buffer[i])) {
        state = 99;
      }
    }
    if (state == 3) {
      colour_seg_hwnd(hwnd, 0, length_line - 1, 2);
    }
    else if (state == 13) {
      colour_seg_hwnd(hwnd, 0, length_line - 1, 3);
    }
    else {
      colour_seg_hwnd(hwnd, 0, length_line - 1, 0);
    }
  }
}

static void colourise_errorlist_doc(char* cdoc, int length_doc, int init_style, HWND hwnd) {
  char line_buffer[1024] = { 0 };
  int line_pos = 0;
  for (int i = 0; i < length_doc; ++i) {
    line_buffer[line_pos++] = cdoc[i];
    if (cdoc[i] == '\r' || cdoc[i] == '\n' || line_pos >= sizeof(line_buffer) - 1) {
      colourise_errorlist_line(line_buffer, line_pos, hwnd);
      line_pos = 0;
    }
  }
  if (line_pos > 0)
    colourise_errorlist_line(line_buffer, line_pos, hwnd);
}

void colourise_doc(char* cdoc, int start_pos, int length_doc, int init_style, const char* language, char** keywords, HWND hwnd) {
  SendMessage(hwnd, SCI_STARTSTYLING, start_pos, 31);
  if (0 == strcmp(language, "cpp"))
    colourise_cpp_doc(cdoc, length_doc, init_style, keywords, hwnd);
  else if (0 == strcmp(language, "errorlist"))
    // 为什么 errorlist 所有文本还是黑色的？因为 IDM_RUNWIN 还没有处理
    colourise_errorlist_doc(cdoc, length_doc, init_style, hwnd);
  else
    colour_seg_hwnd(hwnd, 0, length_doc, 0);
}
