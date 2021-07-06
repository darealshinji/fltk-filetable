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

#ifndef fltk_filetable_extension_hpp
#define fltk_filetable_extension_hpp

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <cassert>
#include <vector>

#include "fltk_filetable_.hpp"


class fltk_filetable_extension : public fltk_filetable_
{
public:
  enum {
    ICN_DIR,   // directory
    ICN_FILE,  // regular file
    ICN_LINK,  // link (overlay)
    ICN_LAST
  };

private:
  typedef struct {
    std::vector<ext_t> list;
    Fl_SVG_Image *svg;
  } icn_t;

  Fl_SVG_Image *icn_[ICN_LAST];
  std::vector<icn_t> icn_custom_;

  void double_click_callback();
  Fl_SVG_Image *icon(fltk_filetable_Row r);

public:
  fltk_filetable_extension(int X, int Y, int W, int H, const char *L=NULL);
  ~fltk_filetable_extension();

  bool set_icon(const char *filename, const char *data, int idx);

  // svg icon and list of file extensions separated by '/' and without dots
  bool set_icon(const char *filename, const char *data, const char *list, const char *delim);

};

#endif  // fltk_filetable_extension_hpp


