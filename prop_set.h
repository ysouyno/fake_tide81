#pragma once

class PropSet {
public:
  PropSet* super_ps;
  PropSet();
  ~PropSet();

  void ensure_can_add_entry();
  void set(const char*, const char*);
  void set(char*);
  char* get(const char*);
  int get_int(const char*);
  char* get_wild(const char*, const char*);
  char* get_new_expand(const char*, const char*);
  void clear();
  void read(const char*);

private:
  char** vals;
  int size;
  int used;
};
