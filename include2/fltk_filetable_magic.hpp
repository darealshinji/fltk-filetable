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
#include <cassert>
#include <thread>
#include <vector>

#ifdef DLOPEN_MAGIC
# include <dlfcn.h>
typedef struct magic_set *magic_t;
#else
# include <magic.h>
#endif

#include "fltk_filetable_.hpp"


class fltk_filetable_magic : public fltk_filetable_
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
  static bool load_symbols();
#endif

  void double_click_callback();
  Fl_SVG_Image *icon(fltk_filetable_Row r);
  Fl_SVG_Image *icon_magic(fltk_filetable_Row r);

  bool filter_show_entry(const char *filename);
  void update_icons();

public:
  fltk_filetable_magic(int X, int Y, int W, int H, const char *L=NULL);
  ~fltk_filetable_magic();

  bool load_dir(const char *dirname);
  bool load_dir();
  bool refresh();
  bool dir_up();

  bool set_icon(const char *filename, const char *data, int idx);

  // set list of MIME types, semicolon-separated
  bool set_icon(const char *filename, const char *data, const char *list);
};

#endif  // fltk_filetable_magic_hpp


