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
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Tile.H>
#include <string>
#include <vector>
#include <string.h>

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
// * focus issues
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

    void set_dir(const char *dirname) {
      if (dirname) T::open_directory_ = dirname;
    }
  };

  class addressline : public Fl_Input
  {
  private:
    Fl_Menu_Item menu_[4] = {
      { "Copy selection", 0, copy_selection_cb, this, FL_MENU_INACTIVE },
      { "Copy line",      0, copy_cb,           this, FL_MENU_DIVIDER },
      { "Dismiss" },
      {0}
    };

    int handle(int event)
    {
      switch (event) {
        case FL_UNFOCUS:
          position(0);
          break;

        case FL_PUSH:
          if (Fl::event_button() == FL_RIGHT_MOUSE) {
            // change cursor back to default
            if (active_r() && window()) window()->cursor(FL_CURSOR_DEFAULT);

            // activate "Copy selection" entry if text was selected
            if (position() == mark()) {
              menu_[0].deactivate();
            } else {
              menu_[0].activate();
            }

            // popup menu and do callback
            const Fl_Menu_Item *m = menu_->popup(Fl::event_x(), Fl::event_y());
            if (m && m->callback_) m->do_callback(NULL);
          }
          break;

        default:
          break;
      }
      return Fl_Input::handle(event);
    }

    static void copy_selection_cb(Fl_Widget *, void *v) {
      reinterpret_cast<addressline *>(v)->copy(1);
    }

    static void copy_cb(Fl_Widget *, void *v) {
      const char *text = reinterpret_cast<addressline *>(v)->value();
      if (text) Fl::copy(text, strlen(text), 1);
    }

  public:
    addressline(int X, int Y, int W, int H) : Fl_Input(X,Y,W,H, NULL) {
      readonly(1);
    }
  };

  std::string selection_;
  dirtree *tree_;
  filetable_sub *table_;
  addressline *addr_;

  typedef struct {
    fileselection *obj;
    std::string str;
  } cb_data;

  cb_data data_[xdg::LAST + 1]; // XDG dirs + $HOME

  Fl_Group *g_but;
  Fl_Tile *g_main;
  Fl_Menu_Button *b_places;
  Fl_Button *b_up, *b_reload;
  Fl_Toggle_Button *b_hidden;
  Fl_Box *g_but_dummy;

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

  // load currently open directory in tree_
  bool load_open_directory()
  {
    const char *dir = table_->open_directory();

    addr_->value(dir);
    tree_->close_root();
    if (!dir) return false;

    bool rv = tree_->load_dir(dir);
    if (rv) tree_->calc_tree();

    if (table_->open_directory_is_root()) {
      b_up->deactivate();
      tree_->hposition(0);
      tree_->vposition(0);
      tree_->set_item_focus(tree_->root());
    } else {
      b_up->activate();
      // scroll down tree
      if (rv) {
        auto item = tree_->find_item(dir);
        tree_->show_item(item);
        tree_->set_item_focus(item);
      }
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

  void toggle_hidden() {
    show_hidden(show_hidden() ? false : true);
    refresh();
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
    addr_ = new addressline(X, Y, W, FL_NORMAL_SIZE + 12);

    // top bar with buttons and such
    g_but = new Fl_Group(X, addr_->y() + addr_->h() + 4, W, 46);
    {
      b_places = new Fl_Menu_Button(g_but->x(), g_but->y(), 72, g_but->h() - 4, "Places");
      b_places->add("\\/", 0, FS_CALLBACK(load_dir("/")), this);

      b_up = new Fl_Button(b_places->x() + b_places->w() + 4, g_but->y(), b_places->h(), b_places->h(), "@+78->");
      b_up->callback(FS_CALLBACK(dir_up()), this);

      b_reload = new Fl_Button(b_up->x() + b_up->w() + 4, g_but->y(), b_up->h(), b_up->h(), "@+4reload");
      b_reload->callback(FS_CALLBACK(refresh()), this);

      b_hidden = new Fl_Toggle_Button(b_reload->x() + b_reload->w() + 4, g_but->y(), 60, b_reload->h(), "Hidden\nfiles");
      b_hidden->callback(FS_CALLBACK(toggle_hidden()), this);

      g_but_dummy = new Fl_Box(b_hidden->x() + b_hidden->w(), g_but->y(), 1, 1);
    }
    g_but->end();
    g_but->resizable(g_but_dummy);

    // main part with tree and filetable
    g_main = new Fl_Tile(X, g_but->y() + g_but->h(), W, H - g_but->h() - g_but->y());
    {
      tree_ = new dirtree(g_main->x(), g_main->y(), g_main->w()/4, g_main->h());
      tree_->callback(FS_CALLBACK(tree_callback()), this);
      tree_->selection_color(FL_WHITE);

      table_ = new filetable_sub(tree_->x() + tree_->w(), g_main->y(), g_main->w() - tree_->w(), g_main->h(), this);

      //tree_->selection_color(table_->selection_color());
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

  // set directory without loading it; use refresh() to load it
  void set_dir(const char *dirname) {
    table_->set_dir(dirname);
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

  // alternative to refresh()
  bool load_dir() { return refresh(); }

  // returns selection
  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }

  void add_filter(const char *str) {table_->add_filter(str);}
  void add_filter_list(const char *list, const char *delim) {table_->add_filter_list(list, delim);}

  void load_default_icons() {tree_->load_default_icons(); table_->load_default_icons();}

  void labelsize(int i) {tree_->labelsize(i); table_->labelsize(i);}
  int labelsize() const {return table_->labelsize();}

  void color(Fl_Color c) {tree_->color(c); tree_->selection_color(c); table_->color(c);}
  Fl_Color color() const {return table_->color();}

  void selection_color(Fl_Color c) {/*tree_->selection_color(c);*/ table_->selection_color(c);}
  Fl_Color selection_color() const {return table_->selection_color();}

  void show_hidden(bool b) {tree_->show_hidden(b); table_->show_hidden(b); b_hidden->value(b);}
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
