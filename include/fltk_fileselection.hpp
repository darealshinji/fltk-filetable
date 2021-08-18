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
#include "fltk_filetable_extension.hpp"
#include "fltk_filetable_magic.hpp"


namespace fltk
{

template<class T>
class fileselection : public Fl_Group
{
  // assert if template argument was not a subclass of fltk::filetable_
  static_assert(std::is_base_of<filetable_, T>::value,
                "class was not derived from fltk::filetable_");

private:

  // create a subclass with an overloaded double_click_callback() method
  template <class T0>
  class filetable_sub : public T0
  {
  private:
    dirtree *tree_ptr;
    std::string *selection_ptr;

    void double_click_callback()
    {
      if (this->last_clicked_item_isdir()) {
        this->load_dir(this->last_clicked_item().c_str());
        tree_ptr->load_dir(this->last_clicked_item().c_str());
        // TODO: automatically scroll down tree
      } else {
        *selection_ptr = this->last_clicked_item();
        this->window()->hide();
      }
    }

  public:
    filetable_sub(int X, int Y, int W, int H, dirtree *tree, std::string *selection)
    : T0(X,Y,W,H,NULL)
    {
      tree_ptr = tree;
      selection_ptr = selection;
    }
  };

  friend class filetable_sub<T>;

  std::string selection_;
  dirtree *tree_;
  filetable_sub<T> *table_;

  static void tree_callback(Fl_Widget *, void *v) {
    reinterpret_cast<fileselection *>(v)->tree_callback2();
  }

  void tree_callback2()
  {
    switch(tree_->callback_reason()) {
      case FL_TREE_REASON_RESELECTED:
      case FL_TREE_REASON_SELECTED:
        load_dir(tree_->callback_item_path());
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
  fileselection(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Group(X,Y,W,H,L)
  {
    tree_ = new dirtree(X, Y, W * 0.25, H);
    tree_->callback(tree_callback, this);
    tree_->load_default_icons();

    table_ = new filetable_sub<T>(X + tree_->w(), Y, W + tree_->w(), H, tree_, &selection_);
    table_->load_default_icons();

    resizable(table_);
    end();
  }

  ~fileselection() {
    delete tree_;
    delete table_;
  }

  bool load_dir(const char *dirname) {
    // call tree_->load_dir() only when table_->load_dir() succeeded
    return (table_->load_dir(dirname) && tree_->load_dir(dirname));
  }

  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
