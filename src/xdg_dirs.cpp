/*
  The line parsing code at xdg::lookup() was taken from xdg-user-dir-lookup.c
  at https://cgit.freedesktop.org/xdg/xdg-user-dirs

  Copyright (c) 2007 Red Hat, Inc.
  Copyright (c) 2021 djcj <djcj@gmx.de>

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions: 

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software. 

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xdg_dirs.hpp"


char *xdg::lookup(FILE *fp, const char *type)
{
  char *p;
  char *line = NULL;
  char *user_dir = NULL;
  size_t len = 0;
  size_t len_type = strlen(type);
  ssize_t nread;

  while ((nread = getline(&line, &len, fp)) != -1) {
    /* Remove newline at end */
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = 0;
    }

    p = line;

    while (*p == ' ' || *p == '\t') p++;

    if (strncmp(p, "XDG_", 4) != 0) continue;
    p += 4;

    if (strncmp(p, type, len_type) != 0) continue;
    p += len_type;

    if (strncmp(p, "_DIR", 4) != 0) continue;
    p += 4;

    while (*p == ' ' || *p == '\t') p++;

    if (*p != '=') continue;
    p++;

    while (*p == ' ' || *p == '\t') p++;

    if (*p != '"') continue;
    p++;

    bool relative = false;

    if (strncmp(p, "$HOME/", 6) == 0) {
      p += 5;  /* don't skip the path separator */
      relative = true;
    } else if (*p != '/') {
      continue;
    }

    if (relative) {
      user_dir = new char[home_len + strlen(p) + 2];
      strcpy(user_dir, home_dir);
    } else {
      user_dir = new char[strlen(p) + 1];
      *user_dir = 0;
    }

    char *d = user_dir + strlen(user_dir);

    while (*p && *p != '"') {
      if (*p == '\\' && *(p+1) != 0) {
        p++;
      }
      *d++ = *p++;
    }
    *d = 0;

    /* remove trailing delimiters */
    d = user_dir + strlen(user_dir) - 1;

    while (--d != user_dir) {
      if (*d != '/') {
        break;
      }
      *d = 0;
    }
  }

  free(line);

  /* small test to see if we can safely use
   * strrchr() to get the basename */
  p = strrchr(user_dir, '/');

  if (!p || p == user_dir) {
    delete user_dir;
    return NULL;
  }

  return user_dir;
}

bool xdg::get()
{
  char *config_file;

  /* get $HOME */

  char *p = getenv("HOME");

  if (!p) {
    return false;
  }

  home_dir = strdup(p);
  home_len = strlen(home_dir);

  /* remove trailing delimiters */
  p = home_dir + home_len - 1;

  while (--p != home_dir) {
    if (*p != '/') {
      break;
    }
    *p = 0;
  }

  /* fopen() config file */

  p = getenv("XDG_CONFIG_HOME");

  if (!p) {
    config_file = new char[home_len + sizeof("/.config/user-dirs.dirs")];
    strcpy(config_file, home_dir);
    strcat(config_file, "/.config/user-dirs.dirs");
  } else {
    config_file = new char[strlen(p) + sizeof("/user-dirs.dirs")];
    strcpy(config_file, p);
    strcat(config_file, "/user-dirs.dirs");
  }

  FILE *fp = fopen(config_file, "r");
  delete config_file;

  if (!fp) {
    return false;
  }

  /* lookup environment variables */

  dirs[DESKTOP] = lookup(fp, "DESKTOP");
  rewind(fp);

  if (!dirs[DESKTOP]) {
    /* Special case desktop for historical compatibility */
    dirs[DESKTOP] = new char[home_len + sizeof("/Desktop")];
    strcpy(dirs[DESKTOP], home_dir);
    strcat(dirs[DESKTOP], "/Desktop");
  }

  dirs[DOWNLOAD] = lookup(fp, "DOWNLOAD"); rewind(fp);
  dirs[DOCUMENTS] = lookup(fp, "DOCUMENTS"); rewind(fp);
  dirs[MUSIC] = lookup(fp, "MUSIC"); rewind(fp);
  dirs[PICTURES] = lookup(fp, "PICTURES"); rewind(fp);
  dirs[VIDEOS] = lookup(fp, "VIDEOS");

  fclose(fp);

  return true;
}

inline const char *xdg::home()
{
  return home_dir;
}

inline const char *xdg::dir(int type)
{
  return (type >= 0 && type < LAST) ? dirs[type] : NULL;
}

const char *xdg::basename(int type)
{
  if (type < 0 || type >= LAST || !dirs[type]) {
    return "";
  }

  const char *p = strrchr(dirs[type], '/') + 1;

  return p ? p : dirs[type];
}

xdg::xdg()
{
  home_dir = NULL;
  home_len = 0;

  for (int i = 0; i < LAST; ++i) {
    dirs[i] = NULL;
  }
}

xdg::~xdg()
{
  if (home_dir) {
    free(home_dir);  /* strdup()'ed, so use free() */
  }

  for (int i = 0; i < LAST; ++i) {
    if (dirs[i]) {
      delete dirs[i];
    }
  }
}

