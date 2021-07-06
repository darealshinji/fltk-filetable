/*
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

#include "fltk_filetable_magic.hpp"

#include <string.h>

#define STR_BEGINS(ptr,str)  (strncmp(ptr, str, sizeof(str)-1) == 0)


#ifdef DLOPEN_MAGIC

#define	MAGIC_SYMLINK           0x0000002
#define	MAGIC_MIME_TYPE         0x0000010
#define	MAGIC_PRESERVE_ATIME    0x0000080
#define	MAGIC_NO_CHECK_COMPRESS 0x0001000
#define	MAGIC_NO_CHECK_ELF      0x0010000
#define MAGIC_NO_CHECK_ENCODING 0x0200000

// magic_open()
typedef magic_t (*magic_open_XX_t) (int);
static magic_open_XX_t magic_open_XX = NULL;

// magic_close()
typedef void (*magic_close_XX_t) (magic_t);
static magic_close_XX_t magic_close_XX = NULL;

// magic_file()
typedef const char *(*magic_file_XX_t) (magic_t, const char *);
static magic_file_XX_t magic_file_XX = NULL;

// magic_error()
typedef const char *(*magic_error_XX_t) (magic_t);
static magic_error_XX_t magic_error_XX = NULL;

// magic_load()
typedef int (*magic_load_XX_t) (magic_t, const char *);
static magic_load_XX_t magic_load_XX = NULL;

void *fltk_filetable_magic::handle_ = NULL;
bool fltk_filetable_magic::symbols_loaded_ = false;

#else

#define magic_open_XX  magic_open
#define magic_close_XX magic_close
#define magic_file_XX  magic_file
#define magic_error_XX magic_error
#define magic_load_XX  magic_load

#endif  // DLOPEN_MAGIC


fltk_filetable_magic::fltk_filetable_magic(int X, int Y, int W, int H, const char *L)
: fltk_filetable_(X,Y,W,H,L)
{
  th_ = NULL;
  use_magic_ = false;

  for (int i = 0; i < ICN_LAST; ++i) {
    icn_[i].svg = NULL;
    icn_[i].alloc = false;
  }
  svg_link_ = icn_[ICN_LINK].svg;

#ifdef DLOPEN_MAGIC
  if (load_symbols()) {
#endif
    const int flags =
        MAGIC_SYMLINK
      | MAGIC_MIME_TYPE
      | MAGIC_PRESERVE_ATIME
      | MAGIC_NO_CHECK_COMPRESS
      | MAGIC_NO_CHECK_ELF
      | MAGIC_NO_CHECK_ENCODING;

    cookie_ = magic_open_XX(flags);

    if (!magic_error_XX(cookie_)) {
      if (magic_load_XX(cookie_, NULL) != 0) {
        magic_close_XX(cookie_);
      } else {
        use_magic_ = true;
      }
    }
#ifdef DLOPEN_MAGIC
  } else if (handle_) {
    // dlopen() succeeded but dlsym() failed
    dlclose(handle_);
    handle_ = NULL;
  }
#endif
}

fltk_filetable_magic::~fltk_filetable_magic()
{
  if (th_) {
    th_->detach();
    delete th_;
  }

  for (int i = 0; i < ICN_LAST; ++i) {
    if (icn_[i].alloc && icn_[i].svg) {
      delete icn_[i].svg;
    }
  }

  while (icn_custom_.size() > 0) {
    if (icn_custom_.back().list) {
      free(icn_custom_.back().list);
    }

    if (icn_custom_.back().svg) {
      delete icn_custom_.back().svg;
    }
    icn_custom_.pop_back();
  }

  if (use_magic_) {
    magic_close_XX(cookie_);
  }

#ifdef DLOPEN_MAGIC
  if (handle_) {
    dlclose(handle_);
  }
#endif
}

#ifdef DLOPEN_MAGIC
bool fltk_filetable_magic::load_symbols()
{
  if (symbols_loaded_) {
    return true;
  }

  if (!handle_ && (handle_ = dlopen("libmagic.so.1", RTLD_LAZY)) == NULL) {
    return false;
  }

  dlerror();  // clear error messages

  magic_open_XX = reinterpret_cast<magic_open_XX_t>(dlsym(handle_, "magic_open"));
  if (dlerror()) return false;

  magic_close_XX = reinterpret_cast<magic_close_XX_t>(dlsym(handle_, "magic_close"));
  if (dlerror()) return false;

  magic_file_XX = reinterpret_cast<magic_file_XX_t>(dlsym(handle_, "magic_file"));
  if (dlerror()) return false;

  magic_error_XX = reinterpret_cast<magic_error_XX_t>(dlsym(handle_, "magic_error"));
  if (dlerror()) return false;

  magic_load_XX = reinterpret_cast<magic_load_XX_t>(dlsym(handle_, "magic_load"));
  if (dlerror()) return false;

  symbols_loaded_ = true;

  return true;
}
#endif

void fltk_filetable_magic::double_click_callback()
{
  const char *name = rowdata_[last_row_clicked_].cols[COL_NAME];
  char *buf = NULL;

  if (open_directory_) {
    size_t len = strlen(open_directory_);
    const char *format = (open_directory_[len - 1] == '/') ? "%s%s" : "%s/%s";
    buf = new char[len + strlen(name) + 1];
    sprintf(buf, format, open_directory_, name);
    name = buf;
  }

  if (rowdata_[last_row_clicked_].isdir()) {
    if (!load_dir(name)) {
      // refresh current directory if we cannot access
      load_dir(NULL);
    }
  } else {
    selection_ = strdup(name);
    window()->hide();  // keep this?
  }

  if (buf) {
    delete buf;
  }
}

bool fltk_filetable_magic::load_dir(const char *dirname)
{
  if (icn_[ICN_FILE].svg) {
    for (const auto i : { ICN_BLK, ICN_CHR, ICN_PIPE, ICN_SOCK }) {
      if (!icn_[i].svg) {
        icn_[i].svg = icn_[ICN_FILE].svg;
        icn_[i].alloc = false;
      }
    }
  }

  if (th_) {
    th_->detach();
    delete th_;
    th_ = NULL;
  }

  if (!fltk_filetable_::load_dir(dirname)) {
    return false;
  }

  auto lambda = [] (fltk_filetable_magic *o) { o->update_icons(); };
  th_ = new std::thread(lambda, this);

  return true;
}

inline bool fltk_filetable_magic::load_dir()
{
  return load_dir(".");
}

inline bool fltk_filetable_magic::refresh()
{
  return load_dir(NULL);
}

bool fltk_filetable_magic::dir_up()
{
  if (!open_directory_) {
    return load_dir("..");
  }

  if (strcmp(open_directory_, "/") == 0) {
    return false;
  }

  size_t len = strlen(open_directory_);
  char buf[len + 3];
  strcpy(buf, open_directory_);
  strcpy(buf + len, "/..");

  return load_dir(buf);
}

bool fltk_filetable_magic::filter_show_entry(const char *filename)
{
  return fltk_filetable_::filter_show_entry(filename);
}

Fl_SVG_Image *fltk_filetable_magic::icon(fltk_filetable_Row r)
{
  switch (r.type()) {
    case 'D':
      return icn_[ICN_DIR].svg;
    case 'B':
      return icn_[ICN_BLK].svg;
    case 'C':
      return icn_[ICN_CHR].svg;
    case 'F':
      return icn_[ICN_PIPE].svg;
    case 'S':
      return icn_[ICN_SOCK].svg;
    case 'R':
    default:
      break;
  }

  return icn_[ICN_FILE].svg;
}

// I'm not 100% sure if lock/unlock/awake is used correctly here
void fltk_filetable_magic::update_icons()
{
  for (int i=0; i < rows(); ++i) {
    if (rowdata_.at(i).type() == 'R') {
      rowdata_.at(i).svg = icon_magic(rowdata_.at(i));
      redraw_range(i, i, COL_NAME, COL_NAME);
      parent()->redraw();
      Fl::awake();
    }
  }

  Fl::lock();
  redraw_range(0, rows()-1, COL_NAME, COL_NAME);
  Fl::unlock();
  Fl::awake();
}

Fl_SVG_Image *fltk_filetable_magic::icon_magic(fltk_filetable_Row r)
{
  const char *p;
  size_t len;

  // don't check 0 byte files
  if (!use_magic_ || r.bytes == 0) {
    return icn_[ICN_FILE].svg;
  }

  if (!open_directory_) {
    p = magic_file_XX(cookie_, r.cols[COL_NAME]);
  } else {
    len = strlen(open_directory_);
    char tmp[len + strlen(r.cols[COL_NAME]) + 1];
    strcpy(tmp, open_directory_);
    tmp[len] = '/';
    strcpy(tmp + len + 1, r.cols[COL_NAME]);

    p = magic_file_XX(cookie_, tmp);
  }

  if (!p) {
    return icn_[ICN_FILE].svg;
  }

  const char *type = NULL;
  len = strlen(p);

  switch(p[0]) {
    case 'a':
      if (STR_BEGINS(p, "application/")) {
        // check for "application/" and "application/x-" string
        char tmp[len + 2];
        strcpy(tmp, "application/");

        if (STR_BEGINS(p + 12, "x-")) {
          strcpy(tmp + 12, p + 14);
        } else {
          tmp[12] = 'x';
          tmp[13] = '-';
          strcpy(tmp + 14, p + 12);
        }

        for (auto l : icn_custom_) {
          if (strstr(l.list, p) || strstr(l.list, tmp)) {
            return l.svg;
          }
        }

        // no generic icon for type "application/"
        return icn_[ICN_FILE].svg;
      }
      else if (STR_BEGINS(p, "audio/")) {
        type = ";audio;";
      }
      break;

    case 'f':
      if (STR_BEGINS(p, "font/")) {
        type = ";font;";
      }
      break;

    case 'i':
      if (STR_BEGINS(p, "image/")) {
        type = ";image;";
      }
      break;

    case 'm':
      if (STR_BEGINS(p, "model/")) {
        type = ";model;";
      }
      else if (STR_BEGINS(p, "message/")) {
        type = ";message;";
      }
      else if (STR_BEGINS(p, "multipart/")) {
        type = ";multipart;";
      }
      break;

    case 't':
      if (STR_BEGINS(p, "text/")) {
        type = ";text;";
      }
      break;

    case 'v':
      if (STR_BEGINS(p, "video/")) {
        type = ";video;";
      }
      break;

    default:
      // unsupported type or message like "regular file"
      return icn_[ICN_FILE].svg;
      break;
  }

  // full check on other types than "application/"
  char desc[len + 1];
  strcpy(desc, p);
  desc[len] = ';';
  desc[len + 1] = 0;

  for (auto l : icn_custom_) {
    if (strstr(l.list, desc)) {
      return l.svg;
    }
  }

  // generic MIME types (check last)
  if (type) {
    for (auto l : icn_custom_) {
      if (strstr(l.list, type)) {
        return l.svg;
      }
    }
  }

  return icn_[ICN_FILE].svg;
}

bool fltk_filetable_magic::set_icon(const char *filename, const char *data, int idx)
{
  if ((!data && !filename) || idx < 0 || idx >= ICN_LAST) {
    return false;
  }

  if (icn_[idx].svg && icn_[idx].alloc) {
    delete icn_[idx].svg;
  }

  icn_[idx].svg = new Fl_SVG_Image(filename, data);

  if (icn_[idx].svg->fail()) {
    delete icn_[idx].svg;
    icn_[idx].svg = NULL;
    icn_[idx].alloc = false;
    return false;
  }

  icn_[idx].svg->proportional = false;
  icn_[idx].svg->resize(labelsize() + 4, labelsize() + 4);
  icn_[idx].alloc = true;

  if (idx == ICN_LINK) {
    svg_link_ = icn_[ICN_LINK].svg;
  }

  return true;
}

bool fltk_filetable_magic::set_icon(const char *filename, const char *data, const char *list)
{
  size_t len;

  if ((!filename && !data) || !list || (len = strlen(list)) < 1) {
    return false;
  }

  char *buf = reinterpret_cast<char *>(malloc(len + 2));
  char *p = buf;

  if (*list != ';') {
    *p = ';';
    p++;
  }

  // convert to lowercase, so we don't have to use case-insensitive functions later
  for ( ; *list; ++list, ++p) {
    switch (*list) {
      case 'A' ... 'Z':
        *p = std::tolower(*list);
        break;
      case 'a' ... 'z':
      case '0' ... '9':
      case '/':
      case '-':
      case '+':
      case '.':
      case ';': // separator
        *p = *list;
        break;
      default:
        // unwanted character found!
        free(buf);
        return false;
    }
  }

  *p = 0;

  if (*--p != ';') {
    *++p = ';';
    *++p = 0;
  }

  Fl_SVG_Image *svg = new Fl_SVG_Image(filename, data);

  if (svg->fail()) {
    free(buf);
    delete svg;
    return false;
  }

  svg->proportional = false;
  svg->resize(labelsize() + 4, labelsize() + 4);

  ext_t ext;
  ext.list = reinterpret_cast<char *>(realloc(buf, strlen(buf)));
  ext.svg = svg;

  icn_custom_.push_back(ext);

  return true;
}

