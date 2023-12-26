#include "document.h"
#include "dlog.h"
#include <Windows.h>

LineCache::LineCache() :
  lines(0), size(grow_size), valid(false) {
  cache = new int[grow_size];
}

LineCache::~LineCache() {
  delete[] cache;
  cache = NULL;
}

// POS 是行号，从 1 开始，VAL 是行尾在整个文件中的位置
// 比如一个文件第一二行各有 75 个字符（包含空格）
// cache[0] = 0;
// cache[1] = 76;
// cahce[2] = 152
void LineCache::set_value(int pos, int val) {
  if (pos >= size) {
    int size_new = pos + grow_size;
    int* cache_new = new int[size_new];
    if (!cache_new) {
      dprintf("No memory available\n");
    }
    for (int i = 0; i < size; ++i) {
      cache_new[i] = cache[i];
    }
    delete[] cache;
    cache = cache_new;
    size = size_new;
  }
  cache[pos] = val;
}

int LineCache::line_from_position(int pos) {
  if (!lines)
    return 0;
  if (pos >= cache[lines])
    return lines - 1;

  int lower = 0;
  int upper = lines;
  int middle = 0;

  do {
    middle = (upper + lower + 1) / 2; // Round high
    if (pos < cache[middle]) {
      upper = middle - 1;
    }
    else {
      lower = middle;
    }
  } while (lower < upper);

  return lower;
}

void Action::create(action_type a, int p, char* d, int l) {
  delete[] data;
  at = a;
  position = p;
  data = d;
  len_data = l;
}

void Action::destroy() {
  delete[] data;
  data = NULL;
}

Document::Document(int initial_length) {
  body = new char[initial_length];
  size = initial_length;
  length = 0;
  part1len = 0;
  gaplen = initial_length;
  part2body = body + gaplen;

  actions = new Action[30000];
  len_actions = 30000;
  max_action = 0;
  current_action = 0;
  actions[current_action].create(start_action);

  read_only = false;
}

Document::~Document() {
  delete[] body;
  body = NULL;
}

void Document::gap_to(int position) {
  // 要插入的点 position 刚好位于 gap 开始处
  if (position == part1len)
    return;
  if (position < part1len) {
    int diff = part1len - position;
    for (int i = 0; i < diff; ++i)
      // gap 向左侧移动，就是将左侧的字符串向右拷贝
      body[part1len + gaplen - i - 1] = body[part1len - i - 1];
  }
  else {
    int diff = position - part1len;
    for (int i = 0; i < diff; ++i)
      // gap 向右侧移动，就是将右边的字符串向左拷贝
      body[part1len + i] = body[part1len + gaplen + i];
  }
  part1len = position;
  part2body = body + gaplen;
}

void Document::room_for(int insertion_length) {
  if (gaplen <= insertion_length) {
    gap_to(length);
    int new_size = size + insertion_length + 4000;
    char* new_body = new char[new_size];
    memcpy(new_body, body, size);
    delete[] body;
    body = new_body;
    gaplen += new_size - size;
    part2body = body + gaplen;
    size = new_size;
  }
}

void Document::append_action(action_type at, int position, char* data, int length) {
  if (current_action >= 2) {
    // See if current action can be coalesced into previous action
    // Will work if both are inserts or deletes and position is same or two different
    if (at != actions[current_action - 1].at || abs(position - actions[current_action - 1].position) > 2) {
      // 和上一个 action 不一样，则加加，条件里的 2 是因为 ch 和 style 各占一个字节
      current_action++;
    }
    else if (current_action == save_point) {
      current_action++;
    }
  }
  else {
    current_action++;
  }
  actions[current_action].create(at, position, data, length);
  current_action++;
  actions[current_action].create(start_action);
  max_action = current_action;
}

char Document::byte_at(int position) {
  if (position < 0) {
    dprintf("Bad position %d\n", position);
    return 1;
  }
  if (position >= length + 10) {
    dprintf("Very bad position %d of %d\n", position, length);
    exit(3);
  }
  if (position >= length) {
    dprintf("Bad position %d of %d\n", position, length);
    return 2;
  }

  if (position < part1len)
    return body[position];
  else
    return part2body[position];
}

char Document::set_byte_at(int position, char ch) {
  if (position < 0) {
    dprintf("Bad position %d\n", position);
    return 1;
  }
  if (position >= length + 10) {
    dprintf("Very bad position %d of %d\n", position, length);
    return 3;
  }
  if (position >= length) {
    dprintf("Bad position %d of %d\n", position, length);
    return 2;
  }

  if (position < part1len)
    body[position] = ch;
  else
    part2body[position] = ch;

  return 0;
}

void Document::set_line_cache() {
  // TODO: Maintain cache with every change so does not need to be regenerated.
  if (!lc.valid) {
    lc.set_value(0, 0);
    lc.lines = 1;
    char ch_prev = ' ';
    // length 是文件大小的两倍，来自 SCI_ADDSTYLEDTEXT 消息
    for (int i = 0; i < length; i += 2) {
      char ch = byte_at(i);
      if (ch == '\r') {
        lc.set_value(lc.lines, i / 2 + 1);
        lc.lines++;
      }
      else if (ch == '\n') {
        if (ch_prev == '\r') {
          // Patch up what was end of line
          lc.set_value(lc.lines - 1, i / 2 + 1);
        }
        else {
          lc.set_value(lc.lines, i / 2 + 1);
          lc.lines++;
        }
      }
      ch_prev = ch;
    }
    lc.set_value(lc.lines, length / 2);
    lc.valid = true;
  }
}

void Document::insert_string(int position, char* s, int insert_length) {
  if (!read_only) {
    if (collecting_undo) {
      // Save into the undo/redo stack, but only the characters - not the formatting
      char* data = new char[insert_length / 2];
      for (int i = 0; i < insert_length / 2; ++i)
        data[i] = s[i * 2];

      append_action(insert_action, position, data, insert_length / 2);
    }

    basic_insert_string(position, s, insert_length);
  }
}

void Document::insert_char_style(int position, char ch, char style) {
  char s[2] = { 0 };
  s[0] = ch;
  s[1] = style;
  insert_string(position * 2, s, 2);
}

void Document::set_style_at(int position, char style, unsigned char mask) {
  int pos = position * 2 + 1;
  set_byte_at(pos, (byte_at(pos) & ~mask) | style);
}

void Document::delete_chars(int position, int delete_length) {
  if (!read_only) {
    if (collecting_undo) {
      // Save into the undo/redo stack, but only the characters - not the formatting
      char* data = new char[delete_length / 2];
      for (int i = 0; i < delete_length / 2; ++i)
        data[i] = byte_at(position + i * 2);

      append_action(remove_action, position, data, delete_length / 2);
    }

    basic_delete_chars(position, delete_length);
  }
}

void Document::basic_insert_string(int position, char* s, int insert_length) {
  room_for(insert_length);
  gap_to(position);
  memcpy(body + part1len, s, insert_length);
  length += insert_length;
  part1len += insert_length;
  gaplen -= insert_length;
  part2body = body + gaplen;
  lc.valid = false;
}

void Document::basic_delete_chars(int position, int delete_length) {
  gap_to(position);
  length -= delete_length;
  gaplen += delete_length;
  part2body = body + gaplen;
  lc.valid = false;
}

int Document::undo(int* pos_earliest_changed) {
  int ret_position = 0; // Where the cursor should be after return
  int changed_position = 0; // Earliest byte modified
  if (pos_earliest_changed)
    *pos_earliest_changed = length;
  // start_action 就像是 dummy node 一样
  if (actions[current_action].at == start_action && current_action > 0)
    current_action--;
  while (actions[current_action].at != start_action && current_action > 0) {
    if (actions[current_action].at == insert_action) {
      basic_delete_chars(actions[current_action].position, actions[current_action].len_data * 2);
      ret_position = actions[current_action].position;
    }
    else if (actions[current_action].at == remove_action) {
      char* styled_data = new char[actions[current_action].len_data * 2];
      memset(styled_data, 0, actions[current_action].len_data * 2);
      for (int i = 0; i < actions[current_action].len_data; ++i)
        styled_data[i * 2] = actions[current_action].data[i];
      basic_insert_string(actions[current_action].position, styled_data, actions[current_action].len_data * 2);
      delete[] styled_data;
      ret_position = actions[current_action].position + actions[current_action].len_data * 2;
    }
    changed_position = actions[current_action].position;
    if (pos_earliest_changed && (*pos_earliest_changed > changed_position))
      *pos_earliest_changed = changed_position;
    current_action--;
  }
  return ret_position;
}

int Document::redo(int* pos_earliest_changed) {
  int ret_position = 0; // Where the cursor should be after return
  int changed_position = 0; // Earliest byte modified
  if (pos_earliest_changed)
    *pos_earliest_changed = length;
  if (actions[current_action].at == start_action && current_action < max_action)
    current_action++;
  while (actions[current_action].at != start_action && current_action < max_action) {
    if (actions[current_action].at == insert_action) {
      char* styled_data = new char[actions[current_action].len_data * 2];
      memset(styled_data, 0, actions[current_action].len_data * 2);
      for (int i = 0; i < actions[current_action].len_data; ++i)
        styled_data[i * 2] = actions[current_action].data[i];
      basic_insert_string(actions[current_action].position, styled_data, actions[current_action].len_data * 2);
      delete[] styled_data;
      ret_position = actions[current_action].position + actions[current_action].len_data * 2;
    }
    else if (actions[current_action].at == remove_action) {
      basic_delete_chars(actions[current_action].position, actions[current_action].len_data * 2);
      ret_position = actions[current_action].position;
    }
    changed_position = actions[current_action].position;
    if (pos_earliest_changed && (*pos_earliest_changed > changed_position))
      *pos_earliest_changed = changed_position;
    current_action++;
  }
  return ret_position;
}

bool Document::can_undo() {
  return (!read_only) && ((current_action > 0) && (max_action > 0));
}

bool Document::can_redo() {
  return (!read_only) && (max_action > current_action);
}
