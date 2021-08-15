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

#ifndef DLOPEN_MAGIC
# define DLOPEN_MAGIC
#endif
#include "fltk_filetable_magic.hpp"


namespace fltk
{

class fileselection : public Fl_Group
{
public:
  enum {
    SIMPLE,
    EXTENSION,
    MAGIC
  };

private:
  static std::string selection_;
  int type_;
  dirtree *tree_;

// Can this be done with templates?
// Set double_click_callback() as a callback?
#define MAKE_CLASS(CLASS) \
  class CLASS##2 : public CLASS { \
  private: \
    dirtree *tree_;\
    \
    void double_click_callback() { \
      if (rowdata_[last_row_clicked_].isdir()) { \
        load_dir(last_clicked_item().c_str()); \
        tree_->load_dir(last_clicked_item().c_str()); \
      } else { \
        fileselection::selection_ = last_clicked_item(); \
        window()->hide(); \
      } \
    } \
  \
  public: \
    CLASS##2 (int X, int Y, int W, int H, dirtree *tree, const char *L=NULL) \
    : CLASS (X,Y,W,H,L) { tree_ = tree; } \
  }; \
  \
  friend class CLASS##2;

  MAKE_CLASS(filetable_simple);
  MAKE_CLASS(filetable_extension);
  MAKE_CLASS(filetable_magic);

#undef MAKE_CLASS

  filetable_simple2 *stable_ = NULL;
  filetable_extension2 *etable_ = NULL;
  filetable_magic2 *mtable_ = NULL;

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
  fileselection(int X, int Y, int W, int H, int type, const char *L=NULL)
  : Fl_Group(X,Y,W,H,L)
  {
    type_ = type;

    tree_ = new dirtree(X, Y, W * 0.25, H);
    tree_->callback(tree_callback, this);
    tree_->load_default_icons();

    X += tree_->w();
    W -= tree_->w();

    switch (type_) {
      case MAGIC:
        mtable_ = new filetable_magic2(X, Y, W, H, tree_, L);
        mtable_->load_default_icons();
        resizable(mtable_);
        break;
      case EXTENSION:
        etable_ = new filetable_extension2(X, Y, W, H, tree_, L);
        etable_->load_default_icons();
        resizable(etable_);
        break;
      case SIMPLE:
      default:
        stable_ = new filetable_simple2(X, Y, W, H, tree_, L);
        stable_->load_default_icons();
        resizable(stable_);
        break;
    }

    end();
  }

  ~fileselection()
  {
    if (stable_) delete stable_;
    if (etable_) delete etable_;
    if (mtable_) delete mtable_;
    delete tree_;
  }

  bool load_dir(const char *dirname)
  {
    bool rv;

    switch (type_) {
      case MAGIC:
        rv = mtable_->load_dir(dirname);
        break;
      case EXTENSION:
        rv = etable_->load_dir(dirname);
        break;
      case SIMPLE:
      default:
        rv = stable_->load_dir(dirname);
        break;
    }

    return (rv && tree_->load_dir(dirname));
  }

  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }
};

std::string fileselection::selection_;

} // namespace fltk

#endif  // fltk_fileselection_hpp
