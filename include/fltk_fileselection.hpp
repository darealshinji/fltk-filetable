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

#ifndef fltk_fileselection_hpp
#define fltk_fileselection_hpp

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <string>

#include "fltk_dirtree.hpp"
#include "fltk_filetable_.hpp"


namespace fltk
{

class fileselection : public Fl_Group
{
private:
  dirtree *tree_;
  filetable_ *table_;
  std::string selection_;

public:
  fileselection(int X, int Y, int W, int H, filetable_ *table, const char *L=NULL) : Fl_Group(X,Y,W,H,L)
  {
    table_ = table;
    tree_ = new dirtree(X, Y, W*0.2, H);

    if (table_) {
      table_->resize(X + tree_->w(), Y, W - tree_->w(), H);
    }
  }

  ~fileselection()
  {
    if (tree_) delete tree_;
  }
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
