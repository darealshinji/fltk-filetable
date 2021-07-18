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

#include <dlfcn.h>

#define	MAGIC_SYMLINK           0x0000002
#define	MAGIC_MIME_TYPE         0x0000010
#define	MAGIC_PRESERVE_ATIME    0x0000080
#define	MAGIC_NO_CHECK_COMPRESS 0x0001000
#define	MAGIC_NO_CHECK_ELF      0x0010000
#define MAGIC_NO_CHECK_ENCODING 0x0200000

typedef struct magic_set *magic_t;

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

#else

#include <magic.h>

#define magic_open_XX  magic_open
#define magic_close_XX magic_close
#define magic_file_XX  magic_file
#define magic_error_XX magic_error
#define magic_load_XX  magic_load

#endif


#include "fltk_filetable_.hpp"


namespace fltk
{

class filetable_magic : public filetable_
{
public:
  enum {
    ICN_DIR,   // directory
    ICN_FILE,  // regular file
    ICN_LINK,  // symbolic link (overlay)
    ICN_BLK,   // block device
    ICN_CHR,   // character device
    ICN_PIPE,  // FIFO/pipe
    ICN_SOCK,  // socket
    ICN_LAST
  };

private:
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

  Fl_SVG_Image *icon(filetable_Row r)
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

  Fl_SVG_Image *icon_magic(filetable_Row r)
  {
    const char *p;

    // don't check 0 byte files
    if (!use_magic_ || r.bytes == 0) {
      return icn_[ICN_FILE].svg;
    }

    if (open_directory_.empty()) {
      p = magic_file_XX(cookie_, r.cols[COL_NAME]);
    } else {
      std::string file = open_directory_;
      file.push_back('/');
      file += r.cols[COL_NAME];
      p = magic_file_XX(cookie_, file.c_str());
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

  bool filter_show_entry(const char *filename) {
    return filetable_::filter_show_entry(filename);
  }

  // TODO: lock/unlock/awake isn't used correctly here
  void update_icons()
  {
    for (int i=0; i < rows(); ++i) {
      if (rowdata_.at(i).type() == 'R') {
        //Fl::lock();
        rowdata_.at(i).svg = icon_magic(rowdata_.at(i));
        redraw_range(i, i, COL_NAME, COL_NAME);
        parent()->redraw();
        //Fl::unlock();
        //Fl::awake();
      }
    }

    //Fl::lock();
    redraw_range(0, rows()-1, COL_NAME, COL_NAME);
    //Fl::unlock();
    //Fl::awake();
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

    std::string dir = open_directory_;
    dir += "/..";

    return load_dir(dir.c_str());
  }

  bool set_icon(const char *filename, const char *data, int idx)
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

  // set list of MIME types, semicolon-separated
  bool set_icon(const char *filename, const char *data, const char *list)
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
};

#ifdef DLOPEN_MAGIC
void *filetable_magic::handle_ = NULL;
bool filetable_magic::symbols_loaded_ = false;
#endif

} // namespace fltk

#endif  // fltk_filetable_magic_hpp


