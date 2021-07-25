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

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>

#include "fltk_dirtree.hpp"

#include "Folder_generic.svg.h"
#include "Folder_noaccess.svg.h"
#include "Folder_link.svg.h"


static void myCallback(Fl_Widget *w, void *)
{
  fltk::dirtree *o = dynamic_cast<fltk::dirtree *>(w);

  switch(o->callback_reason()) {
    case FL_TREE_REASON_RESELECTED:
    case FL_TREE_REASON_SELECTED: {
        const char *path = o->callback_item_path();
        if (path) printf("%s\n", path);
      }
      break;
    case FL_TREE_REASON_OPENED:
      o->open_callback_item();
      break;
    case FL_TREE_REASON_CLOSED:
      o->close_callback_item();
      break;
    default:
      break;
  }
}

int main()
{
  Fl_Double_Window win(300, 600, "Tree");

  fltk::dirtree tree(5, 5, win.w()-10, win.h()-10);
  tree.callback(myCallback, NULL);
  //tree.item_labelsize(16);

  Fl_SVG_Image icon(NULL, folder_generic_svg);
  Fl_SVG_Image icon_link(NULL, folder_link_svg);
  Fl_SVG_Image icon_locked(NULL, folder_noaccess_svg);

  tree.usericon_svg(&icon);
  tree.usericon_link(&icon_link);
  tree.usericon_locked(&icon_locked);

  tree.load_directory("/boot/grub/../grub/fonts/////");
  tree.load_directory(".");
  tree.load_directory("../fltk");
  tree.load_directory("/var");

  tree.item_labelsize(16);  // reopens all open tree items

  win.end();
  win.resizable(tree);
  win.show();

  return Fl::run();
}
