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

#include <algorithm>
#include <vector>
#include "xdg_dirs.hpp"


int main()
{
  std::vector<int> vec;

  xdg paths;
  int rv = paths.get(true, true);

  if (rv == -1) {
    printf("error\n");
    return 1;
  }

  printf("Found: %d of %d\n", rv, xdg::LAST);
  printf("Home: %s\n", paths.home());

  if (rv == 0) return 1;

  if (paths.dir(xdg::DESKTOP)) {
    printf("[%s] %s: %s\n", paths.varname(xdg::DESKTOP), paths.basename(xdg::DESKTOP), paths.dir(xdg::DESKTOP));
  }

  for (int i = 0; i < xdg::LAST; ++i) {
    if (i != xdg::DESKTOP && paths.dir(i)) {
      vec.push_back(i);
    }
  }

  if (vec.size() == 0) return 1;

  auto lambda = [paths](int a, int b) { return strcasecmp(paths.basename(a), paths.basename(b)) < 0; };
  std::stable_sort(vec.begin(), vec.end(), lambda);

  for (const int i : vec) {
    printf("[%s] %s: %s\n", paths.varname(i), paths.basename(i), paths.dir(i));
  }

  return 0;
}

