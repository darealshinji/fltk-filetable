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

#ifndef fltk_dirtree__hpp
#define fltk_dirtree__hpp

#include <FL/Fl.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_SVG_Image.H>
#include <string>


class fltk_dirtree : public Fl_Tree
{
private:
  bool show_hidden_;
  bool sort_reverse_;
  Fl_SVG_Image *icon_;
  Fl_SVG_Image *icon_link_;
  Fl_SVG_Image *icon_locked_;

  std::string callback_item_path_;
  std::string item_path(Fl_Tree_Item *ti);

protected:
  bool load_tree(Fl_Tree_Item *ti);

  /* force to accept only SVG icons (for now?) */
  Fl_Image *usericon() const { return NULL; }
  void usericon(Fl_Image *val) { Fl_Tree::usericon(val); }

public:
  fltk_dirtree(int X, int Y, int W, int H, const char *L=NULL);
  virtual ~fltk_dirtree() {}

  const char *callback_item_path();
  void open_callback_item();
  void close_callback_item();

  bool load_directory(const char *path);

  /* same as load_directory("/") */
  bool load_root() { return Fl_Tree::open(root()); }

  void usericon_svg(Fl_SVG_Image *svg);
  Fl_SVG_Image *usericon_svg() const { return icon_; }

  void usericon_link(Fl_SVG_Image *svg);
  Fl_SVG_Image *usericon_link() const { return icon_link_; }

  void usericon_locked(Fl_SVG_Image *svg);
  Fl_SVG_Image *usericon_locked() const { return icon_locked_; }

  void item_labelsize(Fl_Fontsize i);
  Fl_Fontsize item_labelsize() { return Fl_Tree::item_labelsize(); }

  void show_hidden(bool b) { show_hidden_ = b; }
  bool show_hidden() const { return show_hidden_; }

  void sort_reverse(bool b) { sort_reverse_ = b; }
  bool sort_reverse() const { return sort_reverse_; }
};

#endif  // fltk_dirtree__hpp

