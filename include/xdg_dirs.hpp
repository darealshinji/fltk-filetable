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

#ifndef xdg_dirs_hpp
#define xdg_dirs_hpp

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


class xdg
{
public:
  enum {
    DESKTOP = 0,
    DOWNLOAD = 1,
    DOCUMENTS = 2,
    MUSIC = 3,
    PICTURES = 4,
    VIDEOS = 5,
    LAST = 6
  };

private:
  std::string home_dir;
  std::string dirs[LAST];

  // lookup XDG entry
  std::string lookup(FILE *fp, const char *type)
  {
    std::string user_dir;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    if (!fp || !type || *type == 0) return "";

    const size_t len_type = strlen(type);
    rewind(fp);

    while ((nread = getline(&line, &len, fp)) != -1) {
      // Remove newline at end
      if (len > 0 && line[len-1] == '\n') {
        line[len-1] = 0;
      }

      if (*line == 0) continue;

      char *p = line;

      // get the type/key
      while (*p == ' ' || *p == '\t') p++;
      if (strncmp(p, type, len_type) != 0) continue;
      p += len_type;

      while (*p == ' ' || *p == '\t') p++;
      if (*p != '=') continue;
      p++;

      while (*p == ' ' || *p == '\t') p++;
      if (*p != '"') continue;
      p++;

      // get the path/value
      bool relative = false;

      if (strncmp(p, "$HOME/", 6) == 0) {
        p += 5;  // don't skip the path separator
        relative = true;
      } else if (*p != '/') {
        continue;
      }

      if (relative) {
        user_dir = home_dir;
      } else {
        user_dir.clear();
      }

      size_t sublen = 0;
      char *substr = p;

      for ( ; *p && *p != '"'; ++p, ++sublen) {
        if (*p == '\\' && *(p+1) != 0) {
          p++;
          sublen++;
        }
      }

      user_dir.append(substr, sublen);

      if ((p = realpath(user_dir.c_str(), NULL)) == NULL) {
        user_dir.clear();
      } else {
        user_dir = p;
        free(p);
      }
    }

    free(line);

    return user_dir;
  }

public:
  xdg() {}
  virtual ~xdg() {}

  // load XDG config values
  bool get()
  {
    FILE *fp;
    char *p;

    /* get $HOME */

    if ((p = getenv("HOME")) == NULL || (p = realpath(p, NULL)) == NULL) {
      return false;
    }

    home_dir = p;
    free(p);

    /* fopen() config file */

    p = getenv("XDG_CONFIG_HOME");

    { std::string config_file;  // used only within this scope

      if (!p) {
        config_file = home_dir + "/.config/user-dirs.dirs";
      } else {
        config_file = p;
        config_file += "/user-dirs.dirs";
      }

      if ((fp = fopen(config_file.c_str(), "r")) == NULL) {
        return false;
      }
    } // config_file end

    /* lookup environment variables */

    dirs[DESKTOP] = lookup(fp, "XDG_DESKTOP_DIR");

    if (dirs[DESKTOP].empty()) {
      /* Special case desktop for historical compatibility */
      dirs[DESKTOP] = home_dir + "/Desktop";
    }

    dirs[DOWNLOAD] = lookup(fp, "XDG_DOWNLOAD_DIR");
    dirs[DOCUMENTS] = lookup(fp, "XDG_DOCUMENTS_DIR");
    dirs[MUSIC] = lookup(fp, "XDG_MUSIC_DIR");
    dirs[PICTURES] = lookup(fp, "XDG_PICTURES_DIR");
    dirs[VIDEOS] = lookup(fp, "XDG_VIDEOS_DIR");

    fclose(fp);

    return true;
  }

  // return $HOME
  const char *home() {
    return home_dir.c_str();
  }

  // return string length of $HOME
  size_t home_length() {
    return home_dir.length();
  }

  // return full XDG path by type
  const char *dir(int type) {
    return (type >= 0 && type < LAST) ? dirs[type].c_str() : NULL;
  }

  // returns basename of XDG type or empty string on error
  // never returns NULL
  const char *basename(int type)
  {
    if (type < 0 || type >= LAST || dirs[type].empty()) {
      return "";
    }

    const char *p = strrchr(dirs[type].c_str(), '/');

    if (!p || *++p == 0) {
      return dirs[type].c_str();
    }

    return p;
  }
};

#endif  // xdg_dirs_hpp
