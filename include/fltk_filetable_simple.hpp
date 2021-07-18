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

#ifndef filetable_simple_hpp
#define filetable_simple_hpp

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <string>
#include <string.h>

#include "fltk_filetable_.hpp"


namespace fltk
{

class filetable_simple : public filetable_
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
    Fl_SVG_Image *svg;
    bool alloc;
  } svg_t;

  svg_t icn_[ICN_LAST];

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

public:
  filetable_simple(int X, int Y, int W, int H, const char *L=NULL) : filetable_(X,Y,W,H,L)
  {
    for (int i = 0; i < ICN_LAST; ++i) {
      icn_[i].svg = NULL;
      icn_[i].alloc = false;
    }
    svg_link_ = icn_[ICN_LINK].svg;
  }

  ~filetable_simple()
  {
    for (int i = 0; i < ICN_LAST; ++i) {
      if (icn_[i].alloc && icn_[i].svg) {
        delete icn_[i].svg;
      }
    }
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

    return filetable_::load_dir(dirname);
  }

  bool load_dir() {
    return load_dir(".");
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
};

} // namespace fltk

#endif  // filetable_simple_hpp


