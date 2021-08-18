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

#if DLOPEN_MAGIC != 0
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

  Fl_SVG_Image *icon(Row_t r) const
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

  Fl_SVG_Image *icon_magic(Row_t r) const
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

  // multi-threading: update icons "on the fly"
  void update_icons()
  {
    for (int i=0; i < rows(); ++i) {
      if (rowdata_.at(i).type == 'R') {
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
  virtual void double_click_callback()
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
    // Icons were taken from https://git.haiku-os.org/haiku/tree/data/artwork/icons
    // exported to SVG, minimized with Scour and manually edited
    // (scour --no-renderer-workaround --strip-xml-prolog --remove-descriptive-elements --enable-comment-stripping
    //  --disable-embed-rasters --indent=none --strip-xml-space --enable-id-stripping --shorten-ids)

    const char *svg_data_app_generic =
    "<svg width='64' height='64'>"
     "<path d='m19 60h6l8-8 14 8h6l11-11-7-4-27 4-11 11z' fill='#010101' fill-opacity='.4549'/>"
     "<path transform='translate(-16 20)' d='m18 14v16l16 8 10-10v-16l-15-6-11 8z' fill='none' stroke='#000' stroke-width='4'/>"
     "<linearGradient id='a' x1='26.68' x2='34.48' y1='46.52' y2='36.39' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#c6d7f5' offset='0'/>"
      "<stop stop-color='#6b94dd' offset='1'/>"
     "</linearGradient>"
     "<path transform='translate(-16 20)' d='m18 14v16l16 8v-17l-16-7z' fill='url(#a)'/>"
     "<path transform='translate(-16 20)' d='M34 21V38L44 28V12L34 21z' fill='#003cb0'/>"
     "<path transform='translate(0 1)' d='m24 34 4 11v-9l-4-2z' fill='#0d2964'/>"
     "<path d='m2 34 16 7 10-9-15-6-11 8z' fill='url(#a)'/>"
     "<path transform='translate(12 20)' d='m18 14v16l16 8 10-10v-16l-15-6-11 8z' fill='none' stroke='#000' stroke-width='4'/>"
     "<path d='M34 43L33.99 51.99L46 58V41L41.07 38.84L34 43z' fill='#ec6666'/>"
     "<path d='M30 34V50L33.99 51.99L34 43L41.07 38.84L30 34z' fill='#cd4d4d'/>"
     "<linearGradient id='f' x1='70.4' x2='71.08' y1='34.52' y2='50.83' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#a3043c' offset='0'/>"
      "<stop stop-color='#ffdce6' offset='1'/>"
     "</linearGradient>"
     "<path transform='translate(12 20)' d='M34 21V38L44 28V12L34 21z' fill='url(#f)'/>"
     "<path d='m41.07 38.84 4.93 2.16 10-9-5.01-2.01-9.92 8.85z' fill='#ffacac'/>"
     "<linearGradient id='e' x1='138.84' x2='86.24' y1='49.93' y2='116.1' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#c13e3e' offset='0'/>"
      "<stop stop-color='#e27a7a' offset='1'/>"
     "</linearGradient>"
     "<path d='m30 34 11.07 4.84 9.92-8.85-9.99-3.99-11 8z' fill='url(#e)'/>"
     "<path d='m18 14v16l16 8 10-10v-16l-15-6-11 8z' fill='none' stroke='#000' stroke-width='4'/>"
     "<linearGradient id='d' x1='-19.87' x2='5.48' y1='-1.53' y2='-26.29' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#ffec4b' offset='0'/>"
      "<stop stop-color='#f0a506' offset='1'/>"
     "</linearGradient>"
     "<path d='m18 14v16l16 8v-17l-16-7z' fill='url(#d)'/>"
     "<linearGradient id='c' x1='30.5' x2='56.01' y1='-22.81' y2='-9.13' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#fff' offset='0'/>"
      "<stop stop-color='#fff289' offset='1'/>"
     "</linearGradient>"
     "<path d='m18 14 16 7 10-9-15-6-11 8z' fill='url(#c)'/>"
     "<linearGradient id='b' x1='53.01' x2='69.94' y1='-9.55' y2='3.43' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#f99b05' offset='0'/>"
      "<stop stop-color='#fcb23d' offset='1'/>"
     "</linearGradient>"
     "<path d='M34 21V38L44 28V12L34 21z' fill='url(#b)'/>"
    "</svg>";

    const char *svg_data_text =
    "<svg width='64' height='64'>"
     "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
     "<radialGradient id='c' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#fffcc0' offset='1'/>"
      "<stop stop-color='#f1b706' offset='.4886'/>"
     "</radialGradient>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#c)'/>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
     "<linearGradient id='b' x1='107.04' x2='118.33' y1='-17.09' y2='27.99' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#ffefa5' offset='0'/>"
      "<stop stop-color='#fffcc0' offset='.7424'/>"
      "<stop stop-color='#fff890' offset='1'/>"
     "</linearGradient>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#b)'/>"
     "<linearGradient id='a' x1='-47.14' x2='-28.46' y1='-15.78' y2='-37.85' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#010101' offset='0'/>"
      "<stop stop-color='#9e9e9e' offset='1'/>"
     "</linearGradient>"
     "<path d='m20 36 14.87-15.13m-20.87 11.13 12.87-13m-.87 21 12.75-13.13m-6.75 17.13 10.75-11.63' fill='none' stroke='url(#a)'/>"
     "<path d='m29 42 15-15.63m-21 11.63 11.75-11.75m-17.75 7.75 11.12-11.38' fill='none' stroke='url(#a)'/>"
    "</svg>";

    const char *svg_data_image =
    "<svg width='64' height='64'>"
     "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
     "<radialGradient id='c' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#eaeaea' offset='1'/>"
      "<stop stop-color='#a3a3a3' offset='.4886'/>"
     "</radialGradient>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#c)'/>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
     "<linearGradient id='b' x1='107.04' x2='118.33' y1='-17.09' y2='27.99' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#d6d6d6' offset='0'/>"
      "<stop stop-color='#f5f5f5' offset='.7424'/>"
      "<stop stop-color='#e6e6e6' offset='1'/>"
     "</linearGradient>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#b)'/>"
     "<linearGradient id='a' x1='-32' x2='48' y1='-16' y2='-16' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#1467ff' offset='1'/>"
      "<stop stop-color='#06f8b7' offset='.4035'/>"
     "</linearGradient>"
     "<path d='M8 30L30 44L48 24L26 14L8 30z' fill='url(#a)'/>"
    "</svg>";

    const char *svg_data_video =
    "<svg width='64' height='64'>"
     "<path d='m38 58h5l21-22-8-4h-18v26z' fill='#010000' fill-opacity='.4431'/>"
     "<path d='M2 30L21 14L58 28V36L38 56L2 38V30z' fill='none' stroke='#010000' stroke-width='4'/>"
     "<linearGradient id='f' x1='35.39' x2='50.49' y1='14.68' y2='42.9' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#686868' offset='0'/>"
      "<stop stop-color='#393939' offset='1'/>"
     "</linearGradient>"
     "<path d='M38 56L2 38V30L38 46V56z' fill='url(#f)'/>"
     "<linearGradient id='e' x1='39.81' x2='30.43' y1='66.3' y2='59.24' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#505050' offset='0'/>"
      "<stop stop-color='#222' offset='1'/>"
     "</linearGradient>"
     "<path d='M38 46V56L58 36V28L38 46z' fill='url(#e)'/>"
     "<linearGradient id='d' x1='49.05' x2='61.23' y1='4.66' y2='34.25' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#aeaeae' offset='0'/>"
      "<stop stop-color='#7c7c7c' offset='1'/>"
     "</linearGradient>"
     "<path d='M4.23 28.11L21 14L58 28L40.37 43.86L4.23 28.11z' fill='url(#d)'/>"
     "<linearGradient id='c' x1='33.55' x2='57.12' y1='5.08' y2='26.73' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#8e8e8e' offset='0'/>"
      "<stop stop-color='#5a5a5a' offset='1'/>"
     "</linearGradient>"
     "<path d='M2 30L4.23 28.11L40.37 43.86L38 46L2 30z' fill='url(#c)'/>"
     "<path d='m39 36c-2-1-21-9-23-10s-2-3 0-5 5-3 7-2 22 8 24 9 4 3 1 6-7 3-9 2z' fill='none' stroke='#010000' stroke-width='2'/>"
     "<linearGradient id='b' x1='17.54' x2='49.01' y1='4.52' y2='6.15' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#3c3b3b' offset='0'/>"
      "<stop stop-color='#737373' offset='.2681'/>"
      "<stop stop-color='#3a3a3a' offset='1'/>"
     "</linearGradient>"
     "<path d='m39 36c-2-1-21-9-23-10s-2-3 0-5 5-3 7-2 22 8 24 9 4 3 1 6-7 3-9 2z' fill='url(#b)'/>"
     "<path d='m23 22c-1.66 0-3 .89-3 2 0 1.1 1.34 2 3 2 1.65 0 3-.9 3-2 0-1.11-1.35-2-3-2z' fill='#828282'/>"
     "<path transform='translate(17 7.375)' d='m23 22c-1.66 0-3 .89-3 2 0 1.1 1.34 2 3 2 1.65 0 3-.9 3-2 0-1.11-1.35-2-3-2z' fill='#7c7c7c'/>"
     "<linearGradient id='a' x1='41.84' x2='63.9' y1='1.06' y2='24.25' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#fff' offset='0'/>"
      "<stop stop-color='#e8e8e8' offset='1'/>"
     "</linearGradient>"
     "<path d='m34.62 33.37-13.62-5.37 7.12-6.13 13.88 5-7.38 6.5z' fill='url(#a)'/>"
     "<path d='m4 28 37 16 1.5-1.5v3l-3.5 10.5' fill='none' stroke='#090909'/>"
    "</svg>";

    const char *svg_data_audio =
    "<svg width='64' height='64'>"
     "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
     "<radialGradient id='d' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#d8e4fa' offset='1'/>"
      "<stop stop-color='#7794c9' offset='.4886'/>"
     "</radialGradient>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#d)'/>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
     "<linearGradient id='c' x1='107.04' x2='118.33' y1='-17.09' y2='27.99' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#a5c5ff' offset='0'/>"
      "<stop stop-color='#eaf2ff' offset='.7424'/>"
      "<stop stop-color='#d5e3ff' offset='1'/>"
     "</linearGradient>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#c)'/>"
     "<radialGradient id='b' cx='0' cy='0' r='64' gradientTransform='matrix(.1558 -.1915 .4544 .3697 27.88 27.496)' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#485c85' offset='0'/>"
      "<stop stop-color='#bbcff7' offset='1'/>"
     "</radialGradient>"
     "<path d='m33 49 20-22m-25 18 19-21m-25 17 20-20m-25 16 19-19m-25 15 20-20' fill='none' stroke='url(#b)'/>"
     "<linearGradient id='a' x1='-16.88' x2='2.32' y1='12.97' y2='-12.12' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#010101' offset='0'/>"
      "<stop stop-color='#272727' offset='1'/>"
     "</linearGradient>"
     "<path d='m27 42c4.16 2.77 10-6 6-9s-11-5-11-5l8.87-7.38 12.13 7.38c-3 1-5 7-2 9 4.16 2.77 10-6 6-9s-20-12-20-12l-13 10 15 7c-3 1-5 7-2 9z' fill='url(#a)'/>"
    "</svg>";

    const char *svg_data_pdf =
    "<svg width='64' height='64'>"
     "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
     "<radialGradient id='e' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#f4f4f4' offset='1'/>"
      "<stop stop-color='#ffc0c0' offset='.4886'/>"
     "</radialGradient>"
     "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#e)'/>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
     "<linearGradient id='d' x1='107.04' x2='118.33' y1='-17.09' y2='27.99' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#e7e7e7' offset='0'/>"
      "<stop stop-color='#fff' offset='.7424'/>"
      "<stop stop-color='#ebebeb' offset='1'/>"
     "</linearGradient>"
     "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#d)'/>"
     "<linearGradient id='a' x1='-47.14' x2='-28.46' y1='-15.78' y2='-37.85' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#010101' offset='0'/>"
      "<stop stop-color='#9e9e9e' offset='1'/>"
     "</linearGradient>"
     "<path d='m33 25 5-5m-14-1 5-5m-3 26 16-17m-8 23 13-15' fill='none' stroke='url(#a)'/>"
     "<path d='m30 43 16-18m-17-3 4-4' fill='none' stroke='url(#a)'/>"
     "<linearGradient id='c' x1='57.5' x2='71.09' y1='-34.42' y2='46.71' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#1467ff' offset='1'/>"
      "<stop stop-color='#06f8b7' offset='.4212'/>"
     "</linearGradient>"
     "<path transform='matrix(.4749 0 0 .5 8.1999 15)' d='M8 30L30 44L48 24L26 14L8 30z' fill='url(#c)'/>"
     "<path transform='matrix(.4749 0 0 .5 8.1999 15)' d='M8 30L30 44L48 24L26 14L8 30z' fill='none' stroke='#010101'/>"
     "<path d='m30 16v-14s6 2 11 2c4 0 12-2 17-2v14c-5 0-13 2-17 2-5 0-11-2-11-2z' fill='none' stroke='#010101' stroke-linejoin='round' stroke-width='4'/>"
     "<linearGradient id='b' x1='38.95' x2='66' y1='-23.75' y2='-12.14' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#ff6d6d' offset='0'/>"
      "<stop stop-color='#da1c1c' offset='1'/>"
     "</linearGradient>"
     "<path d='m30 16v-14s6 2 11 2c4 0 12-2 17-2v14c-5 0-13 2-17 2-5 0-11-2-11-2z' fill='url(#b)'/>"
     "<path d='m50 14v-7h4m-21 7v-7s4-1 4 2-3 2-3 2m7-4v7s5 1 5-2v-3c0-3-5-2-5-2zm9 4h3' fill='none' stroke='#fff' stroke-linecap='round' "
        "stroke-linejoin='round' stroke-width='2'/>"
    "</svg>";

    const char *svg_data_archive =
    "<svg width='64' height='64'>"
     "<path d='m36 59h5s4.5-3.38 7.12-6.92c2.91-3.91 3.96-8.03 3.96-8.03s4.98-4.01 7.96-8.03c2.98-4.01 2.96-8.02 2.96-8.02l-4-2-2 5-21 28z' "
        "fill='#010101' fill-opacity='.5333'/>"
     "<path d='m3 28s-1 2.47-1 5.25c0 3.61.62 7.37.62 7.37s2.35 1.95 5.61 3.68c3.97 2.1 8.38 3.43 8.38 3.43l.01-13.86 "
        "12.88-13.12s-4.05-1.65-7.84-3.2c-3.37-1.38-6.55-1.67-6.55-1.67s-4.07 2.07-7.24 5.24c-3.38 3.38-4.87 6.88-4.87 6.88zm27-8.25 "
        "8.93-9.69s-3.86-1.4-7.51-2.73c-3.29-1.2-6.42-1.33-6.42-1.33s-1.73.72-3.63 2.62l-4.88 4.88 13.51 6.25zm1.5 1.62 16.37 5.63.05 "
        "14.96s3.92-3.85 6.44-7.18c1.93-2.54 2.64-5.16 2.64-5.16s1-3.67 1-7.25c0-2.84-1.75-4.87-1.75-4.87s-4.04-2.61-8.92-4.38c-3.54-1.29-6.61-1.91-6.61-1.91s-2.05 "
        "1.44-4.32 3.58c-2.79 2.64-4.9 6.58-4.9 6.58zm15.12 7.63s-4.2-2.15-8.65-4.06c-3.97-1.7-7.47-2.69-7.47-2.69s-2.91 2.24-5.72 5.29c-3.16 "
        "3.43-6.16 7.08-6.16 7.08s-.38 2.97-.38 6.26c0 4.1.63 8.59.63 8.59s3.28 2.19 7.43 4.39c4.69 2.49 9.7 3.39 9.7 3.39s3.12-2.38 "
        "6.12-6.33c2.76-3.63 4.65-7.25 4.65-7.25s.46-3.59.43-7.31c-.04-3.74-.58-7.36-.58-7.36z' fill='none' stroke='#010000' stroke-width='4'/>"
     "<linearGradient id='d' x1='13.84' x2='58.34' y1='-38.14' y2='-27.7' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#fff8ea' offset='0'/>"
      "<stop stop-color='#f5deac' offset='1'/>"
     "</linearGradient>"
     "<path d='m46.62 29s-4.2-2.15-8.65-4.06c-3.97-1.7-7.47-2.69-7.47-2.69s-2.91 2.24-5.72 5.29c-3.16 3.43-6.16 7.08-6.16 7.08l17.38 7.38s3.67-3.28 "
        "6.34-6.54c2.64-3.24 4.28-6.46 4.28-6.46zm-43.62-1 13.62 5.87 12.88-13.12s-4.05-1.65-7.84-3.2c-3.37-1.38-6.55-1.67-6.55-1.67s-4.07 2.07-7.24 "
        "5.24c-3.38 3.38-4.87 6.88-4.87 6.88zm27-8.25 8.93-9.69s-3.86-1.4-7.51-2.73c-3.29-1.2-6.42-1.33-6.42-1.33s-1.73.72-3.63 2.62l-4.88 4.88 13.51 "
        "6.25zm1.5 1.62 16.37 5.63s3.47-2.81 5.73-5.37c1.9-2.15 2.65-4.13 2.65-4.13s-4.04-2.61-8.92-4.38c-3.54-1.29-6.61-1.91-6.61-1.91s-2.05 "
        "1.44-4.32 3.58c-2.79 2.64-4.9 6.58-4.9 6.58z' fill='url(#d)'/>"
     "<linearGradient id='c' x1='44.72' x2='86.1' y1='-7.9' y2='10.51' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#983f04' offset='0'/>"
      "<stop stop-color='#e6c276' offset='1'/>"
     "</linearGradient>"
     "<path d='m46.62 29s-1.64 3.22-4.28 6.46c-2.67 3.26-6.34 6.54-6.34 6.54v15.25s3.12-2.38 6.12-6.33c2.76-3.63 4.65-7.25 "
        "4.65-7.25s.46-3.59.43-7.31c-.04-3.74-.58-7.36-.58-7.36zm1.25-2 .05 14.96s3.92-3.85 6.44-7.18c1.93-2.54 2.64-5.16 2.64-5.16s1-3.67 "
        "1-7.25c0-2.84-1.75-4.87-1.75-4.87s-.75 1.98-2.65 4.13c-2.26 2.56-5.73 5.37-5.73 5.37z' fill='url(#c)'/>"
     "<linearGradient id='b' x1='17.72' x2='-10.6' y1='90.92' y2='71.85' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#eebf5a' offset='0'/>"
      "<stop stop-color='#ffebc0' offset='1'/>"
     "</linearGradient>"
     "<path d='m36 42-17.38-7.38s-.38 2.97-.38 6.26c0 4.1.63 8.59.63 8.59s3.28 2.19 7.43 4.39c4.69 2.49 9.7 3.39 9.7 3.39v-15.25z' fill='url(#b)'/>"
     "<path d='m37 31s-6-14-10-16-16 5-9 6 24-7 28-4 5 4 1 4c-1.58 0-16-1-16-1s-2.63.67-7 3.25c-4.12 2.42-9 4-9 4' fill='none' stroke='#010000' stroke-width='2'/>"
     "<linearGradient id='a' x1='2.85' x2='21.14' y1='-64' y2='-64' gradientUnits='userSpaceOnUse'>"
      "<stop stop-color='#ffefce' offset='0'/>"
      "<stop stop-color='#ffd16e' offset='1'/>"
     "</linearGradient>"
     "<path d='m3 28s-1 2.47-1 5.25c0 3.61.62 7.37.62 7.37s2.35 1.95 5.61 3.68c3.97 2.1 8.38 3.43 8.38 3.43l.01-13.86-13.62-5.87z' fill='url(#a)'/>"
    "</svg>";

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

    for (int i=0; i < ICN_LAST; ++i) {
      set_icon(NULL, default_icon_data(i), i);
    }
    set_icon(NULL, svg_data_text, ";text;");
    set_icon(NULL, svg_data_image, ";image;");
    set_icon(NULL, svg_data_video, ";video;");
    set_icon(NULL, svg_data_audio, ";audio;");
    set_icon(NULL, svg_data_pdf, ";application/x-pdf;");
    set_icon(NULL, svg_data_app_generic, ";application/x-sharedlib;application/x-executable;");
    set_icon(NULL, svg_data_archive, archive_mime);
  }
};

#if DLOPEN_MAGIC != 0
// initializing static members
void *filetable_magic::handle_ = NULL;
bool filetable_magic::symbols_loaded_ = false;

filetable_magic::open_t filetable_magic::magic_open = NULL;
filetable_magic::close_t filetable_magic::magic_close = NULL;
filetable_magic::file_t filetable_magic::magic_file = NULL;
filetable_magic::error_t filetable_magic::magic_error = NULL;
filetable_magic::load_t filetable_magic::magic_load = NULL;
#endif

} // namespace fltk

#endif  // fltk_filetable_magic_hpp


