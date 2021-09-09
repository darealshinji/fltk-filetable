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
    DESKTOP     = 0,
    DOWNLOAD    = 1,
    DOCUMENTS   = 2,
    MUSIC       = 3,
    PICTURES    = 4,
    PUBLICSHARE = 5,
    TEMPLATES   = 6,
    VIDEOS      = 7,
    LAST        = 8
  };

private:
  std::string m_home;
  std::string m_dirs[LAST];
  const char *m_base[LAST] = {0};
  const char *m_var[LAST] = {0};

  // lookup XDG entry
  bool lookup(FILE *fp, unsigned int type, bool within_home, bool use_realpath)
  {
    std::string user_dir;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    if (!fp || type >= LAST) return false;

    const size_t len_type = strlen(m_var[type]);
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
      if (strncmp(p, m_var[type], len_type) != 0) continue;
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
        user_dir = m_home;
      } else {
        user_dir.clear();
      }

      user_dir.reserve(user_dir.length() + strlen(p));

      while (*p && *p != '"') {
        if (*p == '\\' && *(p+1) != 0) p++;
        user_dir.push_back(*p);
        p++;
      }
    }

    free(line);

    if (user_dir.empty()) {
      if (type == DESKTOP) {
        // Special case desktop for historical compatibility
        user_dir = m_home + "/Desktop";
      } else {
        return false;
      }
    }

    if (use_realpath) {
      char *rp = realpath(user_dir.c_str(), NULL);
      if (!rp) return false;
      user_dir = rp;
      free(rp);
    } else {
      // remove trailing '/'
      while (user_dir.back() == '/') {
        user_dir.pop_back();
      }

      if (user_dir.empty()) {
        // string was multiple '/'
        if (within_home) return false;
        m_dirs[type] = "/";
        m_base[type] = "/";
        return true;
      }
    }

    if (within_home && (user_dir.compare(m_home) == 0 ||
                        strncmp(user_dir.c_str(), m_home.c_str(), m_home.length()) != 0))
    {
      // ignore if user_dir equals m_home or
      // if user_dir doesn't begin with m_home
      return false;
    }

    m_dirs[type] = user_dir;
    m_base[type] = strrchr(m_dirs[type].c_str(), '/');

    if (!m_base[type] || *++m_base[type] == 0) {
      m_base[type] = NULL;
      m_dirs[type].clear();
      return false;
    }

    return true;
  }

public:
  xdg()
  {
    m_var[DESKTOP]     = "XDG_DESKTOP_DIR";
    m_var[DOWNLOAD]    = "XDG_DOWNLOAD_DIR";
    m_var[DOCUMENTS]   = "XDG_DOCUMENTS_DIR";
    m_var[MUSIC]       = "XDG_MUSIC_DIR";
    m_var[PICTURES]    = "XDG_PICTURES_DIR";
    m_var[PUBLICSHARE] = "XDG_PUBLICSHARE_DIR";
    m_var[TEMPLATES]   = "XDG_TEMPLATES_DIR";
    m_var[VIDEOS]      = "XDG_VIDEOS_DIR";
  }

  virtual ~xdg() {}

  // load XDG config values
  // within_home: if enabled, paths outside or equal $HOME will be ignored
  // use_realpath: use realpath() to resolve path
  // returns number of found entries (not counting $HOME) or -1 if $HOME wasn't found
  int get(bool within_home, bool use_realpath)
  {
    FILE *fp;

    /* get $HOME */
    char *p = getenv("HOME");
    if (!p) return -1;
    m_home = p;
    while (m_home.back() == '/') m_home.pop_back();
    if (m_home.empty()) return -1;
    m_home.shrink_to_fit();

    /* fopen() config file */
    p = getenv("XDG_CONFIG_HOME");

    { std::string s;

      if (p) {
        s = p;
        s += "/user-dirs.dirs";
      } else {
        s = m_home + "/.config/user-dirs.dirs";
      }

      fp = fopen(s.c_str(), "r");
    }

    if (!fp) return 0;

    /* lookup environment variables */
    int count = 0;

    for (unsigned int i=0; i < LAST; ++i) {
      if (lookup(fp, i, within_home, use_realpath)) count++;
    }

    fclose(fp);

    return count;
  }

  // return $HOME
  const char *home() const { return m_home.empty() ? NULL : m_home.c_str(); }

  // return full XDG path by type
  const char *dir(unsigned int type) const {
    return (type >= LAST || m_dirs[type].empty()) ? NULL : m_dirs[type].c_str();
  }

  // returns basename of XDG type
  const char *basename(unsigned int type) const {
    return (type >= LAST) ? NULL : m_base[type];
  }

  // returns name of XDG variable
  const char *varname(unsigned int type) const {
    return (type >= LAST) ? NULL : m_var[type];
  }
};

#endif  // xdg_dirs_hpp
