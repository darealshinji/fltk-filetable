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
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tile.H>
#include <string>
#include <vector>

#include "fltk_dirtree.hpp"
#include "fltk_filetable_simple.hpp"
#include "fltk_filetable_extension.hpp"
#include "fltk_filetable_magic.hpp"
#include "xdg_dirs.hpp"

#ifndef FS_CALLBACK
#define FS_CALLBACK(FUNC) \
  static_cast<void(*)(Fl_Widget*, void*)>(\
    [](Fl_Widget*, void *v) { reinterpret_cast<fileselection *>(v)->FUNC; }\
  )
#endif


// TODO:
// * focus remains on tree_
// * random crashes on selection when using fltk::filetable_magic



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
  class filetable_sub : public T
  {
  private:
    fileselection *fs_ptr;

    void double_click_callback() override {
      fs_ptr->double_click_callback();
    }

  public:
    filetable_sub(int X, int Y, int W, int H, fileselection *fs) : T(X,Y,W,H,NULL) {
      fs_ptr = fs;
    }
  };

  std::string selection_;
  dirtree *tree_;
  filetable_sub *table_;

  typedef struct {
    fileselection *obj;
    std::string str;
  } cb_data;

  cb_data data_[xdg::LAST + 1]; // XDG dirs + $HOME

  Fl_Group *g_top;
  Fl_Tile *g_main;
  Fl_Menu_Button *b_places;
  Fl_Button *b_up, *b_reload;
  Fl_Box *g_top_dummy;

protected:

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
    if (rv) tree_->calc_tree();

    if (table_->open_directory_is_root()) {
      b_up->deactivate();
      tree_->hposition(0);
      tree_->vposition(0);
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

  static void places_cb(Fl_Widget *, void *v) {
    cb_data *dat = reinterpret_cast<cb_data *>(v);
    dat->obj->load_dir(dat->str.c_str());
  }

public:

  // c'tor
  fileselection(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Group(X,Y,W,H,L)
  {
    // top bar with buttons and such
    g_top = new Fl_Group(X, Y, W, 46);
    {
      b_places = new Fl_Menu_Button(g_top->x(), g_top->y(), 72, g_top->h() - 6, "Places");
      b_places->add("\\/", 0, FS_CALLBACK(load_dir("/")), this);

      b_up = new Fl_Button(b_places->x() + b_places->w() + 4, g_top->y(), b_places->h(), b_places->h(), "@+78->");
      b_up->callback(FS_CALLBACK(dir_up()), this);

      b_reload = new Fl_Button(b_up->x() + b_up->w() + 4, g_top->y(), b_up->h(), b_up->h(), "@+4reload");
      b_reload->callback(FS_CALLBACK(refresh()), this);

      g_top_dummy = new Fl_Box(b_reload->x() + b_reload->w(), g_top->y(), 1, 1);
    }
    g_top->end();
    g_top->resizable(g_top_dummy);

    // main part with tree and filetable
    g_main = new Fl_Tile(X, Y + g_top->h(), W, H - g_top->h());
    {
      tree_ = new dirtree(g_main->x(), g_main->y(), g_main->w()/4, g_main->h());
      tree_->callback(FS_CALLBACK(tree_callback()), this);

      table_ = new filetable_sub(tree_->x() + tree_->w(), g_main->y(), g_main->w() - tree_->w(), g_main->h(), this);

      tree_->selection_color(table_->selection_color());
    }
    g_main->end();

    // create "places" menu entries
    xdg xdg;
    int rv = xdg.get(true, true);

    // $HOME
    if (rv != -1) {
      data_[0] = { this, xdg.home() };
      b_places->add("Home", 0, places_cb, static_cast<void *>(&data_[0]));
    }

    if (rv > 0) {
      // XDG_DESKTOP_DIR
      if (xdg.dir(xdg::DESKTOP) && xdg.basename(xdg::DESKTOP)) {
        data_[1] = { this, xdg.dir(xdg::DESKTOP) };
        b_places->add(xdg.basename(xdg::DESKTOP), 0, places_cb, static_cast<void *>(&data_[1]));
      }

      // add remaining XDG directories
      std::vector<int> xdg_vec_;
      xdg_vec_.reserve(xdg::LAST - 1);

      for (int i = 1; i < xdg::LAST; ++i) {
        if (xdg.dir(i) && xdg.basename(i)) {
          xdg_vec_.push_back(i);
        }
      }

      if (xdg_vec_.size() > 0) {
        auto lambda = [xdg](const int a, const int b) {
          return strcasecmp(xdg.basename(a), xdg.basename(b)) < 0;
        };
        std::sort(xdg_vec_.begin(), xdg_vec_.end(), lambda);

        int i = 2;

        for (const int j : xdg_vec_) {
          data_[i] = { this, xdg.dir(j) };
          b_places->add(xdg.basename(j), 0, places_cb, static_cast<void *>(&data_[i]));
          i++;
        }
      }
    }

    end();
    resizable(g_main);
  }

  // d'tor
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

  // reload directory
  bool refresh() {
    if (!table_->refresh()) move_up_tree();
    return load_open_directory();
  }

  // returns selection
  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }

  void add_filter(const char *str) {table_->add_filter(str);}
  void add_filter_list(const char *list, const char *delim) {table_->add_filter_list(list, delim);}

  void load_default_icons() {tree_->load_default_icons(); table_->load_default_icons();}

  void labelsize(int i) {tree_->labelsize(i); table_->labelsize(i);}
  int labelsize() const {return table_->labelsize();}

  void color(Fl_Color c) {tree_->color(c); table_->color(c);}
  Fl_Color color() const {return table_->color();}

  void selection_color(Fl_Color c) {tree_->selection_color(c); table_->selection_color(c);}
  Fl_Color selection_color() const {return table_->selection_color();}

  void show_hidden(bool b) {tree_->show_hidden(b); table_->show_hidden(b);}
  bool show_hidden() const {return table_->show_hidden();}

  void double_click_timeout(double d) {table_->double_click_timeout(d);}
  double double_click_timeout() const {return table_->double_click_timeout();}

  void blend_w(int i) {table_->blend_w(i);}
  int blend_w() const {return table_->blend_w();}

  void autowidth_padding(int i) {table_->autowidth_padding(i);}
  int autowidth_padding() const {return table_->autowidth_padding();}

  void autowidth_max(int i) {table_->autowidth_max(i);}
  int autowidth_max() const {return table_->autowidth_max();}

  void label_header(filetable_::ECol idx, const char *l) {table_->label_header(idx, l);}
  const char *label_header(filetable_::ECol idx) const {return table_->label_header(idx);}

  void filesize_label(filetable_::EStrSize idx, const char *l) {table_->filesize_label(idx, l);}
  const char *filesize_label(filetable_::EStrSize idx) const {return table_->filesize_label(idx);}
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
