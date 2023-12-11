#include "prop_set.h"
#include <Windows.h>
#include <stdio.h>

#pragma warning(disable: 4996)

const char* ZEROSTR = "";

// Get a line of input. If end of line escaped with '\\' then continue reading.
static bool get_full_line(FILE* fp, char* s, int len) {
  while (len > 0) {
    char* cp = fgets(s, len, fp);
    if (!cp)
      return false;
    int last = strlen(s);
    // Remove probable trailing line terminator characters
    if ((last > 0) && ((s[last - 1] == '\n') || (s[last - 1] == '\r'))) {
      s[last - 1] = '\0';
      last--;
    }
    if ((last > 0) && ((s[last - 1] == '\n') || (s[last - 1] == '\r'))) {
      s[last - 1] = '\0';
      last--;
    }
    if (last == 0) // Empty line so do not need to check for trailing '\\'
      return true;
    if (s[last - 1] != '\\')
      return true;
    // Trailing '\\' so read another line
    s[last - 1] = '\0';
    last--;
    s += last;
    len -= last;
  }

  return false;
}

PropSet::PropSet() :
  super_ps(NULL), size(10), used(0) {
  vals = new char* [size];
}

PropSet::~PropSet() {
  super_ps = 0;
  clear();
  delete[] vals;
}

void PropSet::ensure_can_add_entry() {
  if (used >= size - 2) {
    int newsize = size + 10;
    char** newvals = new char* [newsize];
    for (int i = 0; i < used; ++i)
      newvals[i] = vals[i];
    delete[] vals;
    vals = newvals;
    size = newsize;
  }
}

void PropSet::set(const char* key, const char* val) {
  ensure_can_add_entry();
  for (int i = 0; i < used; i += 2) {
    if (0 == _stricmp(vals[i], key)) {
      // Replace current value
      free(vals[i + 1]);
      vals[i + 1] = _strdup(val);
      return;
    }
  }
  // Not found
  vals[used++] = _strdup(key);
  vals[used++] = _strdup(val);
}

void PropSet::set(char* keyval) {
  char* eqat = strchr(keyval, '=');
  if (eqat) {
    *eqat = '\0';
    set(keyval, eqat + 1);
    *eqat = '=';
  }
}

char* PropSet::get(const char* key) {
  for (int i = 0; i < used; i += 2) {
    if (0 == _stricmp(vals[i], key))
      return vals[i + 1];
  }

  if (super_ps) {
    // Failed here, so try in base property set
    return super_ps->get(key);
  }
  else {
    return (char*)ZEROSTR;
  }
}

int PropSet::get_int(const char* key) {
  char* val = get(key);
  if (*val)
    return atoi(val);
  else
    return 0;
}

bool isprefix(const char* target, const char* prefix) {
  while (*target && *prefix) {
    if (toupper(*target) != toupper(*prefix))
      return false;
    target++;
    prefix++;
  }
  if (*prefix)
    return false;
  else
    return true;
}

bool issuffix(const char* target, const char* suffix) {
  int lentarget = strlen(target);
  int lensuffix = strlen(suffix);
  if (lensuffix > lentarget)
    return false;
  for (int i = lensuffix - 1; i >= 0; --i) {
    if (toupper(target[i + lentarget - lensuffix]) != toupper(suffix[i]))
      return false;
  }
  return true;
}

char* PropSet::get_wild(const char* keybase, const char* filename) {
  int lenbase = strlen(keybase);
  int lenfile = strlen(filename);
  for (int i = 0; i < used; i += 2) {
    if (isprefix(vals[i], keybase)) {
      const char* keyfile = vals[i] + strlen(keybase);
      if (*keyfile == '*') {
        if (issuffix(filename, keyfile + 1)) {
          return vals[i + 1];
        }
      }
      else if (0 == _stricmp(keyfile, filename)) {
        return vals[i + 1];
      }
      else if (0 == _stricmp(vals[i], keybase)) {
        return vals[i + 1];
      }
    }
  }
  if (super_ps) {
    return super_ps->get_wild(keybase, filename);
  }
  else {
    return (char*)ZEROSTR;
  }
}

char* PropSet::get_new_expand(const char* keybase, const char* filename) {
  char* base = _strdup(get_wild(keybase, filename));
  char* cpvar = strstr(base, "$(");
  while (cpvar) {
    char* cpendvar = strchr(cpvar, ')');
    if (cpendvar) {
      int lenvar = cpendvar - cpvar - 2; // Subtract the $()
      char* var = (char*)malloc(lenvar + 1);
      strncpy(var, cpvar + 2, lenvar);
      var[lenvar] = '\0';
      char* val = get_wild(var, filename);
      int newlenbase = strlen(base) + strlen(val) - lenvar;
      char* newbase = (char*)malloc(newlenbase);
      strncpy(newbase, base, cpvar - base);
      strcpy(newbase + (cpvar - base), val);
      strcpy(newbase + (cpvar - base) + strlen(val), cpendvar + 1);
      free(var);
      free(base);
      base = newbase;
    }
    cpvar = strstr(base, "$(");
  }
  return base;
}

void PropSet::clear() {
  for (int i = 0; i < used; ++i) {
    free(vals[i]);
    vals[i] = NULL;
  }
  used = 0;
}

void PropSet::read(const char* filename) {
  clear();
  FILE* rcfile = fopen(filename, "rt");
  if (rcfile) {
    char linebuf[4000] = { 0 };
    while (get_full_line(rcfile, linebuf, sizeof(linebuf))) {
      if (isalpha(linebuf[0]))
        set(linebuf);
    }
    fclose(rcfile);
  }
}
