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
#include "fltk_filetable_simple.hpp"


// TODO: set filetable_::double_click_callback() through a method?

namespace fltk
{

class fileselection : public Fl_Group
{
private:
  dirtree *tree_;
  filetable_ *table_;
  std::string selection_;
  bool table_alloc_;

  static void tree_callback(Fl_Widget *, void *v) {
    reinterpret_cast<fileselection *>(v)->tree_callback2();
  }

  void tree_callback2()
  {
    switch(tree_->callback_reason()) {
      case FL_TREE_REASON_RESELECTED:
      case FL_TREE_REASON_SELECTED: {
          const char *path = tree_->callback_item_path();
          if (!path) break;
          table_->load_dir(path);
          tree_->load_dir(path);
        }
        break;
      case FL_TREE_REASON_OPENED:
        tree_->open_callback_item();
        break;
      case FL_TREE_REASON_CLOSED:
        tree_->close_callback_item();
        break;
      default:
        break;
    }
  }

public:
  fileselection(int X, int Y, int W, int H, filetable_ *table, const char *L=NULL)
  : Fl_Group(X,Y,W,H,L), table_alloc_(false)
  {
    tree_ = new dirtree(X, Y, W*0.25, H);
    table_ = table;

    if (table_) {
      table_->resize(X + tree_->w(), Y, W - tree_->w(), H);
    } else {
      table_ = new filetable_simple(X + tree_->w(), Y, W - tree_->w(), H);
      table_alloc_ = true;
    }

    tree_->callback(tree_callback, this);

    const char *dir = table_->open_directory();
    tree_->load_dir(dir ? dir : "/");

    resizable(table_);
    end();
  }

  ~fileselection()
  {
    if (table_alloc_) delete table_;
    delete tree_;
  }

  void load_default_icons()
  {
    tree_->load_default_icons();
    table_->load_default_icons();
  }

  bool load_dir(const char *dirname) {
    return (table_->load_dir(dirname) && tree_->load_dir(dirname));
  }
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
