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

#ifndef filetable_extension_hpp
#define filetable_extension_hpp

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <string>
#include <vector>
#include <string.h>

#include "fltk_filetable_.hpp"


namespace fltk
{

class filetable_extension : public filetable_
{
private:
  typedef struct {
    std::vector<std::string> list;
    const char *desc;
    Fl_SVG_Image *svg;
  } icn_t;

  Fl_SVG_Image *icn_[ICN_LOCK + 1] = {0};
  std::vector<icn_t> icn_custom_;

  Fl_SVG_Image *icon(Row_t &r) const override
  {
    if (r.isdir()) {
      return icn_[ICN_DIR];
    }

    // check if filename has an extension
    char *p = strrchr(r.cols[COL_NAME], '.');

    // error || dot is first char || dot is last char
    if (!p || p == r.cols[COL_NAME] || *(p+1) == 0) {
      return icn_[ICN_FILE];
    }

    size_t len = strlen(r.cols[COL_NAME]);

    for (const auto &icn : icn_custom_) {
      for (const auto &ext : icn.list) {
        if (ext.size() >= len) {
          continue;
        }

        if (strcasecmp(r.cols[COL_NAME] + (len - ext.size()), ext.c_str()) == 0) {
          r.cols[COL_TYPE] = const_cast<char *>(icn.desc);
          return icn.svg;
        }
      }
    }

    return icn_[ICN_FILE];
  }

public:
  // c'tor
  filetable_extension(int X, int Y, int W, int H, const char *L=NULL)
  : filetable_(X,Y,W,H,L)
  {
    svg_link_ = icn_[ICN_LINK];
    svg_noaccess_ = icn_[ICN_LOCK];
  }

  virtual ~filetable_extension()
  {
    for (size_t i=0; i < (sizeof(icn_)/sizeof(*icn_)); ++i) {
      if (icn_[i]) delete icn_[i];
    }

    for (const auto &e : icn_custom_) {
      if (e.svg) delete e.svg;
    }
  }

  bool set_icon(const char *filename, const char *data, EIcn idx)
  {
    if (empty(filename) && empty(data)) return false;

    if (icn_[idx]) delete icn_[idx];
    icn_[idx] = new Fl_SVG_Image(filename, data);

    if (icn_[idx]->fail()) {
      delete icn_[idx];
      icn_[idx] = NULL;
      return false;
    }

    icn_[idx]->proportional = false;
    icn_[idx]->resize(labelsize() + 4, labelsize() + 4);

    if (idx == ICN_LINK) {
      svg_link_ = icn_[ICN_LINK];
    } else if (idx == ICN_LOCK) {
      svg_noaccess_ = icn_[ICN_LOCK];
    }

    return true;
  }

  // svg icon and list of file extensions separated by a delimiter and without dots
  bool set_icon(const char *filename, const char *data, const char *description, const char *list, const char *delim)
  {
    std::vector<std::string> filter_list;
    char *tok;

    if ((empty(filename) && empty(data)) || empty(list) || empty(delim)) {
      return false;
    }

    Fl_SVG_Image *svg = new Fl_SVG_Image(filename, data);

    if (svg->fail()) {
      delete svg;
      return false;
    }

    svg->proportional = false;
    svg->resize(labelsize() + 4, labelsize() + 4);

    char *copy = strdup(list);

    for (char *str = copy; ; str = NULL) {
      if ((tok = strtok(str, delim)) == NULL) {
        break;
      }

      std::string ext;
      if (tok[0] != '.') ext = ".";
      ext += tok;

      filter_list.push_back(ext);
    }

    free(copy);

    icn_t icn;
    icn.list = filter_list;
    icn.svg = svg;
    icn.desc = description;
    icn_custom_.push_back(icn);

    return true;
  }

  // load a set of default icons
  void load_default_icons() override
  {
#ifdef SVG_DATA_H
    set_icon(NULL, default_icon_data(ICN_DIR), ICN_DIR);
    set_icon(NULL, default_icon_data(ICN_FILE), ICN_FILE);
    set_icon(NULL, default_icon_data(ICN_LINK), ICN_LINK);
    set_icon(NULL, default_icon_data(ICN_LOCK), ICN_LOCK);
#endif
  }
};

} // namespace fltk


#endif  // filetable_extension_hpp


