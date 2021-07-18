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

#ifndef xdg_dirs_hpp
#define xdg_dirs_hpp

#include <stdio.h>


class xdg
{
public:
  enum {
    DESKTOP = 0,
    DOWNLOAD = 1,
    DOCUMENTS = 2,
    MUSIC = 3,
    PICTURES = 4,
    VIDEOS = 5,
    LAST = 6
  };

private:
  char *home_dir;
  char *dirs[LAST];
  size_t home_len;

  char *lookup(FILE *fp, const char *type);

public:
  xdg();
  ~xdg();

  bool get();

  const char *home();
  const char *dir(int type);
  const char *basename(int type);
};

#endif  // xdg_dirs_hpp
