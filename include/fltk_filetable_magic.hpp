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

#ifndef fltk_filetable_magic_hpp
#define fltk_filetable_magic_hpp

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <string>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <string.h>

#ifdef DLOPEN_MAGIC
# include <dlfcn.h>
#else
# include <magic.h>
#endif

#include "fltk_filetable_.hpp"


namespace fltk
{

class filetable_magic : public filetable_
{
private:

#ifdef DLOPEN_MAGIC
  enum {
    MAGIC_SYMLINK = 0x0000002,
    MAGIC_MIME_TYPE = 0x0000010,
    MAGIC_PRESERVE_ATIME = 0x0000080,
    MAGIC_NO_CHECK_COMPRESS = 0x0001000,
    MAGIC_NO_CHECK_ELF = 0x0010000,
    MAGIC_NO_CHECK_ENCODING = 0x0200000
  };

  typedef struct magic_set *magic_t;

  // magic_open()
  typedef magic_t (*magic_open_t) (int);
  static magic_open_t magic_open;

  // magic_close()
  typedef void (*magic_close_t) (magic_t);
  static magic_close_t magic_close;

  // magic_file()
  typedef const char *(*magic_file_t) (magic_t, const char *);
  static magic_file_t magic_file;

  // magic_error()
  typedef const char *(*magic_error_t) (magic_t);
  static magic_error_t magic_error;

  // magic_load()
  typedef int (*magic_load_t) (magic_t, const char *);
  static magic_load_t magic_load;
#endif

  typedef struct {
    char *list;
    Fl_SVG_Image *svg;
  } ext_t;

  typedef struct {
    Fl_SVG_Image *svg;
    bool alloc;
  } svg_t;

  svg_t icn_[ICN_LAST];
  std::vector<ext_t> icn_custom_;
  std::thread *th_;
  magic_t cookie_;
  bool use_magic_;
  char *filter_mime_;

#ifdef DLOPEN_MAGIC
  static void *handle_;
  static bool symbols_loaded_;

  static bool load_symbols()
  {
    if (symbols_loaded_) {
      return true;
    }

    if (!handle_ && (handle_ = dlopen("libmagic.so.1", RTLD_LAZY)) == NULL) {
      return false;
    }

    magic_open = reinterpret_cast<magic_open_t>(dlsym(handle_, "magic_open"));
    if (!magic_open) return false;

    magic_close = reinterpret_cast<magic_close_t>(dlsym(handle_, "magic_close"));
    if (!magic_close) return false;

    magic_file = reinterpret_cast<magic_file_t>(dlsym(handle_, "magic_file"));
    if (!magic_file) return false;

    magic_error = reinterpret_cast<magic_error_t>(dlsym(handle_, "magic_error"));
    if (!magic_error) return false;

    magic_load = reinterpret_cast<magic_load_t>(dlsym(handle_, "magic_load"));
    if (!magic_load) return false;

    symbols_loaded_ = true;

    return true;
  }
#endif

  Fl_SVG_Image *icon(Row r) const
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

  Fl_SVG_Image *icon_magic(Row r) const
  {
    const char *p;

    // don't check 0 byte files
    if (!use_magic_ || r.bytes == 0) {
      return icn_[ICN_FILE].svg;
    }

    if (open_directory_.empty()) {
      p = magic_file(cookie_, r.cols[COL_NAME]);
    } else {
      std::string file = open_directory_;
      file.push_back('/');
      file += r.cols[COL_NAME];
      p = magic_file(cookie_, file.c_str());
    }

    if (!p) {
      return icn_[ICN_FILE].svg;
    }

    const char *type = NULL;

#define STR_BEGINS(ptr,str)  (strncmp(ptr, str, sizeof(str)-1) == 0)

    switch(p[0]) {
      case 'a':
        if (STR_BEGINS(p, "application/")) {
          // check for "application/" and "application/x-" string
          const char *p2 = p + sizeof("application/") - 1;
          std::string tmp;

          if (STR_BEGINS(p2, "x-")) {
            tmp = "application/";
            tmp += p2 + 2;
          } else {
            tmp = "application/x-";
            tmp += p2;
          }

          for (const auto l : icn_custom_) {
            if (strstr(l.list, p) || strstr(l.list, tmp.c_str())) {
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

#undef STR_BEGINS

    // full check on other types than "application/"
    std::string desc = p;
    desc.push_back(';');

    for (const auto l : icn_custom_) {
      if (strstr(l.list, desc.c_str())) {
        return l.svg;
      }
    }

    // generic MIME types (check last)
    if (type) {
      for (const auto l : icn_custom_) {
        if (strstr(l.list, type)) {
          return l.svg;
        }
      }
    }

    return icn_[ICN_FILE].svg;
  }

  // TODO: stuttering when loading huge directories first time
  void update_icons()
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

public:
  filetable_magic(int X, int Y, int W, int H, const char *L=NULL) : filetable_(X,Y,W,H,L)
  {
    th_ = NULL;
    use_magic_ = false;

    for (int i = 0; i < ICN_LAST; ++i) {
      icn_[i].svg = NULL;
      icn_[i].alloc = false;
    }
    svg_link_ = icn_[ICN_LINK].svg;
    svg_noaccess_ = icn_[ICN_LOCK].svg;

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

      cookie_ = magic_open(flags);

      if (!magic_error(cookie_)) {
        if (magic_load(cookie_, NULL) != 0) {
          magic_close(cookie_);
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

  ~filetable_magic()
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

    for (const auto e : icn_custom_) {
      if (e.list) free(e.list);
      if (e.svg) delete e.svg;
    }

    if (use_magic_) {
      magic_close(cookie_);
    }

#ifdef DLOPEN_MAGIC
    if (handle_) {
      dlclose(handle_);
    }
#endif
  }

  bool load_dir(const char *dirname)
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

    if (!filetable_::load_dir(dirname)) {
      return false;
    }

    auto lambda = [] (filetable_magic *o) { o->update_icons(); };
    th_ = new std::thread(lambda, this);

    return true;
  }

  bool load_dir() {
    return load_dir(".");
  }

  bool refresh() {
    return load_dir(NULL);
  }

  bool dir_up()
  {
    if (open_directory_.empty()) {
      return load_dir("..");
    }

    if (open_directory_ == "/") {
      return false;
    }

    std::string dir(open_directory_ + "/..");

    return load_dir(dir.c_str());
  }

  // same as the parent class double_click_callback() method, but we must
  // replace it to call the child class load_dir() method,
  // or else icons won't update after double-clicking on a directory
  void double_click_callback()
  {
    std::string s(last_clicked_item());

    if (!rowdata_[last_row_clicked_].isdir()) {
      selection_ = s;
      window()->hide();
      return;
    }

    load_dir(s.c_str());
  }

  bool set_icon(const char *filename, const char *data, int idx)
  {
    if ((empty(data) && empty(filename)) || idx < 0 || idx >= ICN_LAST) {
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
    } else if (idx == ICN_LOCK) {
      svg_noaccess_ = icn_[ICN_LOCK].svg;
    }

    return true;
  }

  // set list of MIME types, semicolon-separated
  bool set_icon(const char *filename, const char *data, const char *list)
  {
    if ((empty(filename) && empty(data)) || empty(list)) {
      return false;
    }

    char *buf = reinterpret_cast<char *>(malloc(strlen(list) + 2));
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

  // load a set of default icons
  void load_default_icons()
  {
    for (int i=0; i < ICN_LAST; ++i) {
      set_icon(NULL, default_icon_data(i), i);
    }
  }
};

#ifdef DLOPEN_MAGIC
// initializing static members
void *filetable_magic::handle_ = NULL;
bool filetable_magic::symbols_loaded_ = false;

filetable_magic::magic_open_t filetable_magic::magic_open = NULL;
filetable_magic::magic_close_t filetable_magic::magic_close = NULL;
filetable_magic::magic_file_t filetable_magic::magic_file = NULL;
filetable_magic::magic_error_t filetable_magic::magic_error = NULL;
filetable_magic::magic_load_t filetable_magic::magic_load = NULL;
#endif

} // namespace fltk

#endif  // fltk_filetable_magic_hpp


