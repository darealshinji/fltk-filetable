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
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Tile.H>
#include <string>

#include "fltk_dirtree.hpp"
#include "fltk_filetable_simple.hpp"
#include "fltk_filetable_extension.hpp"
#include "fltk_filetable_magic.hpp"

#ifndef FILESELECTION_CALLBACK
#define FILESELECTION_CALLBACK(FUNC) \
  static_cast<void(*)(Fl_Widget*, void*)>(\
    [](Fl_Widget*, void *v) { reinterpret_cast<fileselection *>(v)->FUNC(); }\
  )
#endif


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
    fileselection *fs_ptr;

    void double_click_callback() override {
      fs_ptr->double_click_callback();
    }

  public:
    filetable_sub(int X, int Y, int W, int H, fileselection *fs) : T0(X,Y,W,H,NULL) {
      fs_ptr = fs;
    }
  };

  std::string selection_;
  dirtree *tree_;
  filetable_sub<T> *table_;

  Fl_Group *g_top;
  Fl_Tile *g_main;
  Fl_Button *b_up, *b_reload;
  Fl_Box *g_top_dummy;

  virtual void double_click_callback()
  {
    if (table_->last_clicked_item_isdir()) {
      load_dir(table_->last_clicked_item().c_str());
    } else {
      selection_ = table_->last_clicked_item();
      window()->hide();
    }
  }

  void tree_callback()
  {
    switch(tree_->callback_reason()) {
      case FL_TREE_REASON_RESELECTED:
      case FL_TREE_REASON_SELECTED:
        if (!load_dir(tree_->callback_item_path())) {
          tree_->open_callback_item();  // sets the item as "no access"
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

  // load currently open directory for tree_ and table_
  bool load_open_directory()
  {
    bool rv = tree_->load_dir(table_->open_directory());

    if (table_->open_directory_is_root()) {
      b_up->deactivate();
    } else {
      b_up->activate();
      // scroll down tree
      if (rv) tree_->show_item(tree_->find_item(table_->open_directory()));
    }

    return rv;
  }

  // move up the tree until we can open a directory, otherwise open root
  void move_up_tree()
  {
    std::string s = filetable_::simplify_directory_path(table_->open_directory());
    char * const path = const_cast<char *>(s.data());
    char *p = path + s.length();

    while (p-- != path) {
      if (*p == '/') {
        *p = 0;
        if (table_->load_dir(path)) break;
      }
    }

    if (*path == 0) {
      // moved all the way up to the root directory
      tree_->close_root();
      table_->load_dir("/");
    } else {
      // close path
      tree_->close(path, 0);
    }
  }

public:
  fileselection(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Group(X,Y,W,H,L)
  {
    // top bar with buttons and such
    g_top = new Fl_Group(X, Y, W, 46);
    {
      b_up = new Fl_Button(X, Y, 42, 42, "@+68->");
      b_up->callback(FILESELECTION_CALLBACK(dir_up), this);

      b_reload = new Fl_Button(X + b_up->w() + 4, Y, 42, 42, "@+4reload");
      b_reload->callback(FILESELECTION_CALLBACK(refresh), this);

      g_top_dummy = new Fl_Box(b_reload->x() + b_reload->w(), Y, 1, 1);
    }
    g_top->end();
    g_top->resizable(g_top_dummy);

    int nx = X;
    int ny = Y + g_top->h();
    int nw = W;
    int nh = H - g_top->h();

    // main part with tree and filetable
    g_main = new Fl_Tile(nx,ny,nw,nh);
    {
      nw *= 0.25;
      tree_ = new dirtree(nx,ny,nw,nh);
      tree_->callback(FILESELECTION_CALLBACK(tree_callback), this);
      tree_->load_default_icons();

      nx += tree_->w();
      nw = g_main->w() - nw;
      table_ = new filetable_sub<T>(nx,ny,nw,nh, this);
      table_->load_default_icons();
    }
    g_main->end();

    end();
    resizable(g_main);
  }

  virtual ~fileselection() {
    delete tree_;
    delete table_;
  }

  // calls load_dir() on table_ and tree_
  bool load_dir(const char *dirname) {
    if (!table_->load_dir(dirname)) return false;
    return load_open_directory();
  }

  // move directory up (table_ and tree_)
  bool dir_up() {
    if (!table_->dir_up()) move_up_tree();
    return load_open_directory();
  }

  bool refresh() {
    if (!table_->refresh()) move_up_tree();
    return load_open_directory();
  }

  // returns selection
  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
