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

#include "fltk_fileselection.hpp"


int main()
{
  Fl_Double_Window win(800, 600, "Test");

  fltk::fileselection<> sel(4, 4, win.w() - 8, win.h() - 8, false);
  sel.set_dir("/usr/local");  // set directory but don't load it
  sel.load_default_icons();
  //sel.use_iec(false);
  //sel.show_hidden(true);
  //sel.sort_mode(sel.sort_mode() | fltk::filetable_::SORT_DIRECTORY_AS_FILE);
  sel.load_dir();

  win.end();
  win.resizable(sel);
  win.show();

  int rv = Fl::run();

  const char *p = sel.selection();
  if (p) printf("%s\n", p);

  return rv;
}
