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

#ifndef FLTK_EXPERIMENTAL
#error You need to define FLTK_EXPERIMENTAL
#else

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <string>
#include <thread>
#include <vector>
#include <stdlib.h>
#include <string.h>

#if DLOPEN_MAGIC != 0
# include <dlfcn.h>
#else
# include <magic.h>
#endif

#include "fltk_filetable_.hpp"


// add second libmagic thread?
// Bug: crashes on very fast directory switching

namespace fltk
{

class filetable_magic : public filetable_
{
private:

#if DLOPEN_MAGIC != 0
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
  typedef magic_t (*open_t) (int);
  static open_t magic_open;

  // magic_close()
  typedef void (*close_t) (magic_t);
  static close_t magic_close;

  // magic_file()
  typedef const char *(*file_t) (magic_t, const char *);
  static file_t magic_file;

  // magic_error()
  typedef const char *(*error_t) (magic_t);
  static error_t magic_error;

  // magic_load()
  typedef int (*load_t) (magic_t, const char *);
  static load_t magic_load;
#endif

  typedef struct {
    char *list;
    const char *desc;
    Fl_SVG_Image *svg;
  } ext_t;

  typedef struct {
    Fl_SVG_Image *svg;
    bool alloc;
  } svg_t;

  svg_t icn_[ICN_LAST] = {0};
  std::vector<ext_t> icn_custom_;
  std::thread *th_ = NULL;
  magic_t cookie_;
  bool use_magic_ = false;
  bool show_mime_ = false;
  char *filter_mime_;

  // set this true to stop the thread immediately
  static bool request_stop_;

#if DLOPEN_MAGIC != 0
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

    magic_open = reinterpret_cast<open_t>(dlsym(handle_, "magic_open"));
    if (!magic_open) return false;

    magic_close = reinterpret_cast<close_t>(dlsym(handle_, "magic_close"));
    if (!magic_close) return false;

    magic_file = reinterpret_cast<file_t>(dlsym(handle_, "magic_file"));
    if (!magic_file) return false;

    magic_error = reinterpret_cast<error_t>(dlsym(handle_, "magic_error"));
    if (!magic_error) return false;

    magic_load = reinterpret_cast<load_t>(dlsym(handle_, "magic_load"));
    if (!magic_load) return false;

    symbols_loaded_ = true;

    return true;
  }
#endif

  Fl_SVG_Image *icon(Row_t &r) const override
  {
    switch (r.type) {
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

  Fl_SVG_Image *icon_magic(Row_t &r) const
  {
    const char *p;

    // don't check 0 byte files
    if (!use_magic_ || r.bytes == 0) {
      return icn_[ICN_FILE].svg;
    }

    if (open_directory_.empty()) {
      p = magic_file(cookie_, r.cols[COL_NAME]);
    } else {
      std::string s;
      s.reserve(open_directory_.size() + strlen(r.cols[COL_NAME]) + 1);
      s = open_directory_ + "/" + r.cols[COL_NAME];
      p = magic_file(cookie_, s.c_str());
    }

    if (!p) return icn_[ICN_FILE].svg;

    const char *type = NULL;
    const char *mime = p;

#define STR_BEGINS(ptr,str)  (strncmp(ptr, str, sizeof(str)-1) == 0)

    switch(p[0]) {
      case 'a':
        if (STR_BEGINS(p, "application/")) {
          // check for "application/" and "application/x-" string
          const char *p2 = p + sizeof("application/") - 1;
          std::string s;
          s.reserve(strlen(p) + 2);

          if (STR_BEGINS(p2, "x-")) {
            s = "application/";
            s += p2 + 2;
          } else {
            s = "application/x-";
            s += p2;
          }

          for (const auto l : icn_custom_) {
            if (strstr(l.list, p) || strstr(l.list, s.c_str())) {
              if (show_mime()) {
                r.cols[COL_TYPE] = strdup(mime);
                r.type = ENTRY_ALLOCATED;
              } else {
                r.cols[COL_TYPE] = const_cast<char *>(l.desc);
              }
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
        if (show_mime()) {
          r.cols[COL_TYPE] = strdup(mime);
          r.type = ENTRY_ALLOCATED;
        }
        return icn_[ICN_FILE].svg;
    }

#undef STR_BEGINS

    // full check on other types than "application/"
    std::string s;
    s.reserve(strlen(p) + 2);
    s.push_back(';');
    s.append(p);
    s.push_back(';');

    for (const auto l : icn_custom_) {
      if (strstr(l.list, s.c_str())) {
        if (show_mime()) {
          r.cols[COL_TYPE] = strdup(mime);
          r.type = ENTRY_ALLOCATED;
        } else {
          r.cols[COL_TYPE] = const_cast<char *>(l.desc);
        }
        return l.svg;
      }
    }

    // generic MIME types (check last)
    if (type) {
      for (const auto l : icn_custom_) {
        if (strstr(l.list, type)) {
          if (show_mime()) {
            r.cols[COL_TYPE] = strdup(mime);
            r.type = ENTRY_ALLOCATED;
          } else {
            r.cols[COL_TYPE] = const_cast<char *>(l.desc);
          }
          return l.svg;
        }
      }
    }

    if (show_mime()) {
      r.cols[COL_TYPE] = strdup(mime);
      r.type = ENTRY_ALLOCATED;
    }
    return icn_[ICN_FILE].svg;
  }

  // multi-threading: update icons "on the fly"
  void update_icons()
  {
    for (int i=0; i < rows(); ++i) {
      // stop immediately
      if (request_stop_) {
        Fl::unlock();
        Fl::awake();
        return;
      }

      if (rowdata_.at(i).type == 'R' || (show_mime() && rowdata_.at(i).type != 'D')) {
        rowdata_.at(i).svg = icon_magic(rowdata_.at(i));
        redraw_range(i, i, COL_NAME, COL_NAME);
        parent()->redraw();
        Fl::awake();
      }
    }

/*
    Fl::lock();
    redraw_range(0, rows()-1, COL_NAME, COL_NAME);
    Fl::unlock();
    Fl::awake();
    */
  }

public:
  // c'tor
  filetable_magic(int X, int Y, int W, int H, const char *L=NULL) : filetable_(X,Y,W,H,L)
  {
    svg_link_ = icn_[ICN_LINK].svg;
    svg_noaccess_ = icn_[ICN_LOCK].svg;

#if DLOPEN_MAGIC != 0
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
#if DLOPEN_MAGIC != 0
    } else if (handle_) {
      // dlopen() succeeded but dlsym() failed
      dlclose(handle_);
      handle_ = NULL;
    }
#endif
  }

  // d'tor
  virtual ~filetable_magic()
  {
    if (th_) {
      request_stop_ = true;
      th_->join();
      delete th_;
      //th_ = NULL;
      //request_stop_ = false;
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

#if DLOPEN_MAGIC != 0
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
      request_stop_ = true;
      th_->join();
      delete th_;
      th_ = NULL;
      request_stop_ = false;
    }

    if (!filetable_::load_dir(dirname)) {
      return false;
    }

    th_ = new std::thread([this](){ this->update_icons(); });

    return true;
  }

  bool load_dir() override {
    return load_dir(".");
  }

  bool refresh() override {
    return load_dir(NULL);
  }

  bool dir_up() override
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
  virtual void double_click_callback() override
  {
    if (rowdata_.at(last_row_clicked_).isdir()) {
      load_dir(last_clicked_item().c_str());
      return;
    }

    selection_ = last_clicked_item();
    window()->hide();
  }

  bool set_icon(const char *filename, const char *data, EIcn idx)
  {
    if (empty(data) && empty(filename)) return false;

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
  bool set_icon(const char *filename, const char *data, const char *description, const char *list)
  {
    if ((empty(filename) && empty(data)) || empty(list)) {
      return false;
    }

    char *buf = static_cast<char *>(malloc(strlen(list) + 2));
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
    ext.desc = description;
    ext.list = static_cast<char *>(realloc(buf, strlen(buf)));
    ext.svg = svg;

    icn_custom_.emplace_back(ext);

    return true;
  }

  // load a set of default icons
  void load_default_icons() override
  {
#ifdef SVG_DATA_H
    // https://en.wikipedia.org/wiki/List_of_archive_formats
    const char *archive_mime = ";"
      // more common (?) ones first
      "application/x-gtar;"
      "application/gzip;"
      "application/x-bzip2;"
      "application/x-xz;"
      "application/x-tar;"
      "application/zip;"
      "application/x-7z-compressed;"
      "application/x-rar-compressed;"
      "application/x-lzma;"
      "application/x-iso9660-image;"
      "application/x-cpio;"
      "application/x-archive;"
      "application/vnd.ms-cab-compressed;"
      "application/x-apple-diskimage;"
      "application/java-archive;"
      // less common?
      "application/x-lzip;"
      "application/x-lzop;"
      "application/x-snappy-framed;"
      "application/x-compress;"
      "application/x-zstd;"
      "application/x-shar;"
      "application/x-sbx;"
      "application/x-ace-compressed;"
      "application/x-astrotite-afa;"
      "application/x-alz-compressed;"
      "application/vnd.android.package-archive;"
      "application/x-freearc;"
      "application/x-arj;"
      "application/x-b1;"
      "application/x-cfs-compressed;"
      "application/x-dar;"
      "application/x-dgc-compressed;"
      "application/x-gca-compressed;"
      "application/x-lzh;"
      "application/x-lzx;"
      "application/x-stuffit;"
      "application/x-stuffitx;"
      "application/x-ms-wim;"
      "application/x-xar;"
      "application/x-zoo;"
      "application/x-par2;";

    for (int i=0; i < static_cast<int>(ICN_LAST); ++i) {
      EIcn e = static_cast<EIcn>(i);
      set_icon(NULL, default_icon_data(e), e);
    }

    // clear custom icons
    for (const auto e : icn_custom_) {
      if (e.list) free(e.list);
      if (e.svg) delete e.svg;
    }

    icn_custom_.clear();
    icn_custom_.reserve(8);

    set_icon(NULL, FILE_TEXT_SVG_DATA, "Text", ";text;");
    set_icon(NULL, FILE_IMAGE_3_SVG_DATA, "Image", ";image;");
    set_icon(NULL, FILE_VIDEO_SVG_DATA, "Video", ";video;");
    set_icon(NULL, FILE_AUDIO_2_SVG_DATA, "Audio", ";audio;");
    set_icon(NULL, FILE_FONT_SVG_DATA, "Font", ";font;");
    set_icon(NULL, FILE_PDF_SVG_DATA, "Portable Document Format", ";application/x-pdf;");
    set_icon(NULL, APP_GENERIC_SVG_DATA, "Executable / shared library", ";application/x-sharedlib;application/x-executable;");
    set_icon(NULL, FILE_ARCHIVE_SVG_DATA, "Archive", archive_mime);
#endif  // SVG_DATA_H
  }

  // show MIME type or custom description
  void show_mime(bool b) { show_mime_ = b; }
  bool show_mime() const { return show_mime_; }
};

// initializing static members
bool filetable_magic::request_stop_ = false;

#if DLOPEN_MAGIC != 0
void *filetable_magic::handle_ = NULL;
bool filetable_magic::symbols_loaded_ = false;

filetable_magic::open_t filetable_magic::magic_open = NULL;
filetable_magic::close_t filetable_magic::magic_close = NULL;
filetable_magic::file_t filetable_magic::magic_file = NULL;
filetable_magic::error_t filetable_magic::magic_error = NULL;
filetable_magic::load_t filetable_magic::magic_load = NULL;
#endif

} // namespace fltk

#endif  // FLTK_EXPERIMENTAL

#endif  // fltk_filetable_magic_hpp

