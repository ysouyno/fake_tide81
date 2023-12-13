#pragma once

class LineCache {
public:
  enum { grow_size = 4000 };
  int lines;
  int* cache;
  int size;
  bool valid;

  LineCache();
  ~LineCache();

  void set_value(int, int);
  int line_from_position(int);
};

enum action_type {
  insert_action,
  remove_action,
  start_action
};

class Action {
public:
  action_type at;
  int position;
  char* data;
  int len_data;

  Action() : at(start_action), position(0), data(0), len_data(0) {}
  ~Action() {}

  void create(action_type, int p = 0, char* d = 0, int l = 0);
  void destroy();
};

// Holder for an expandable array of characters
// Based on article "Data Structures in a Bit-Mapped Text Editor"
// by Wilfred J. Hansen, Byte January 1987, page 183
class Document {
public:
  Document(int initial_length = 4000);
  ~Document();

  char byte_at(int);
  char set_byte_at(int, char);

  char char_at(int position) { return byte_at(position * 2); }
  char style_at(int position) { return byte_at(position * 2 + 1); }
  int byte_length() { return length; }
  int get_length() { return byte_length() / 2; }
  int lines() { set_line_cache(); return lc.lines; }

  bool is_read_only() { return read_only; }
  void set_read_only(bool b) { read_only = b; }

  void set_save_point() { save_point = current_action; }
  bool is_save_point() { return save_point == current_action; }

  bool set_undo_collection(bool b) {
    collecting_undo = b;
    return collecting_undo;
  }

  bool is_collecting_undo() { return collecting_undo; }

  void delete_undo_history() {
    for (int i = 1; i < max_action; ++i)
      actions[i].destroy();
    max_action = 0;
    current_action = 0;
  }

  void set_line_cache();
  void insert_string(int, char*, int);
  void insert_char_style(int, char, char);
  void set_style_at(int, char, unsigned char mask = 0xff);
  void delete_chars(int, int);
  void basic_insert_string(int, char*, int);
  void basic_delete_chars(int, int);
  int undo(int* pos_earliest_changed = 0);
  int redo(int* pos_earliest_changed = 0);
  bool can_undo();
  bool can_redo();

public:
  LineCache lc;

private:
  void append_action(action_type, int, char*, int);

  char* body;
  int size;
  int length;
  int part1len;
  int gaplen;
  char* part2body;
  bool read_only;

  void gap_to(int);
  void room_for(int);

  Action* actions;
  int len_actions;
  int max_action;
  int current_action;
  int collecting_undo;
  int save_point;
};
