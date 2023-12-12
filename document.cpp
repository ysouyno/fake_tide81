#include <Windows.h>
#include "document.h"
#include "dlog.h"

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
