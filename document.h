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
