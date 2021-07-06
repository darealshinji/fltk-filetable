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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <FL/Fl.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_SVG_Image.H>
#include <algorithm>
#include <string>
#include <vector>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>


// Sort class to handle sorting column using std::sort
class fltk_dirtree_Sort {
  bool reverse_;

public:
  fltk_dirtree_Sort(bool reverse) {
    reverse_ = reverse;
  }

  bool operator() (const char *ap, const char *bp)
  {
    // skip first byte
    ap++;
    bp++;

    if (isdigit(*ap) && isdigit(*bp)) {
      // Numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
      long av = atol(ap);
      long bv = atol(bp);

      // if numbers are the same, continue to alphabetic sort
      if (av != bv) {
        return reverse_ ? bv < av : av < bv;
      }
    }

    // ignore leading dots in filenames ("bin, .config, data, .local"
    // instead of ".config, .local, bin, data")
    if (*ap == '.') { ap++; }
    if (*bp == '.') { bp++; }

    // Alphabetic sort
    return reverse_ ? strcasecmp(ap, bp) > 0 : strcasecmp(ap, bp) < 0;
  }
};


fltk_dirtree::fltk_dirtree(int X, int Y, int W, int H, const char *L)
: Fl_Tree(X, Y, W, H, L)
{
  show_hidden_ = false;
  sort_reverse_ = false;
  icon_ = NULL;
  icon_link_ = NULL;
  icon_locked_ = NULL;
  //callback_item_path_ = NULL;

  root_label("/");
  item_reselect_mode(FL_TREE_SELECTABLE_ALWAYS);

  // start with closed root ("/") dir
  add(root(), NULL);
  close(root(), 0);
}

/*
fltk_dirtree::~fltk_dirtree()
{
  if (callback_item_path_) {
    free(callback_item_path_);
  }
}
*/

/*
char *fltk_dirtree::item_path(Fl_Tree_Item *ti)
{
  if (ti->is_root()) {
    return strdup("/");
  }

  std::string s;

  while (ti && !ti->is_root()) {
    if (ti->label()) {
      s.insert(0, ti->label());
      s.insert(0, 1, '/');
    }
    ti = ti->parent();
  }

  return strdup(s.empty() ? "/" : s.c_str());
}
*/

std::string fltk_dirtree::item_path(Fl_Tree_Item *ti)
{
  std::string s;

  if (ti->is_root()) {
    return "/";
  }

  while (ti && !ti->is_root()) {
    if (ti->label()) {
      s.insert(0, ti->label());
      s.insert(0, 1, '/');
    }
    ti = ti->parent();
  }

  return s;
}

/*
const char *fltk_dirtree::callback_item_path()
{
  if (callback_item_path_) {
    free(callback_item_path_);
  }

  callback_item_path_ = item_path(callback_item());

  return callback_item_path_;
}
*/
inline std::string fltk_dirtree::callback_item_path()
{
  return item_path(callback_item());
}

void fltk_dirtree::open_callback_item()
{
  Fl_Tree_Item *ti = callback_item();

  if (!load_directory(ti)) {
    ti->clear_children();
    //ti->deactivate();

    if (icon_locked_) {
      ti->usericon(icon_locked_);
    }
  }
}

inline void fltk_dirtree::close_callback_item()
{
  Fl_Tree_Item *ti = callback_item();
  ti->clear_children();
  add(ti, NULL);
}

void fltk_dirtree::usericon_svg(Fl_SVG_Image *svg)
{
  if (svg && !svg->fail()) {
    icon_ = svg;
    icon_->resize(item_labelsize(), item_labelsize());
    usericon(icon_);
  }
}

void fltk_dirtree::usericon_link(Fl_SVG_Image *svg)
{
  if (svg && !svg->fail()) {
    icon_link_ = svg;
    icon_link_->resize(item_labelsize(), item_labelsize());
  }
}

void fltk_dirtree::usericon_locked(Fl_SVG_Image *svg)
{
  if (svg && !svg->fail()) {
    icon_locked_ = svg;
    icon_locked_->resize(item_labelsize(), item_labelsize());
  }
}

void fltk_dirtree::item_labelsize(Fl_Fontsize i)
{
  if (i == Fl_Tree::item_labelsize()) {
    return;
  }

  Fl_Tree::item_labelsize(i);
  root()->labelsize(i);

  if (icon_) {
    icon_->resize(i, i);
    usericon(icon_);
  }

  if (icon_link_) {
    icon_link_->resize(i, i);
  }

  if (icon_locked_) {
    icon_locked_->resize(i, i);
  }
}

bool fltk_dirtree::load_directory(Fl_Tree_Item *ti)
{
  struct dirent *dir;
  DIR *d;
  std::vector<char *> list;

  //char *path = item_path(ti);
  std::string path = item_path(ti);
  int fd = ::open(path.c_str(), O_CLOEXEC | O_DIRECTORY, O_RDONLY);
  //free(path);

  if (fd == -1) {
    return false;
  }

  if ((d = fdopendir(fd)) == NULL) {
    ::close(fd);
    return false;
  }

  while ((dir = readdir(d)) != NULL) {
    struct stat st, lst;

    // handle hidden files
    if (!show_hidden() && dir->d_name[0] == '.') {
      continue;
    }

    // no "." and ".." entries
    if (show_hidden() && (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)) {
      continue;
    }

    // act like lstat()
    if (fstatat(fd, dir->d_name, &lst, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW) == -1) {
      continue;
    }

    // use first byte to identify entry as a link or directory
    if (S_ISDIR(lst.st_mode)) {
      // directory: entry begins with '/'
      char *buf = reinterpret_cast<char *>(malloc(strlen(dir->d_name) + 1));
      buf[0] = '/';
      strcpy(buf + 1, dir->d_name);
      list.push_back(buf);
    }
    else if (S_ISLNK(lst.st_mode) && // act like stat()
             fstatat(fd, dir->d_name, &st, AT_NO_AUTOMOUNT) == 0 && S_ISDIR(st.st_mode))
    {
      // link to directory: does NOT begin with '/'
      list.push_back(strdup(dir->d_name));
    }
  }

  closedir(d);
  ::close(fd);
  ti->clear_children();

  if (list.size() == 0) {
    return true;
  }

  std::stable_sort(list.begin(), list.end(), fltk_dirtree_Sort(sort_reverse()));

  for (auto elem : list) {
    add(ti, elem[0] == '/' ? elem + 1 : elem);
    auto sub = ti->child(ti->children() - 1);

    if (elem[0] != '/' && icon_link_) {
      sub->usericon(icon_link_);
    }
    add(sub, NULL);

    close(sub);
    free(elem);
  }

  return true;
}

bool fltk_dirtree::load_directory(const char *path)
{
  char *s, *p;

  if (!path || path[0] != '/') {
    return false;
  }

  if ((s = fltk_filetable_::simplify_directory_path(path)) == NULL) {
    s = strdup(path);
  }
  p = s;

  open(root());

  while (*++p) {
    if (*p == '/') {
      *p = 0;

      if (!open(s)) {
        free(s);
        return false;
      }
      *p = '/';
    }
  }

  free(s);

  return open(path);
}

