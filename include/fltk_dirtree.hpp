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

#ifndef dirtree__hpp
#define dirtree__hpp

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <FL/Fl.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tree_Item.H>
#include <FL/Fl_SVG_Image.H>
#include <algorithm>
#include <vector>
#include <string>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include "fltk_filetable_.hpp"


namespace fltk
{

// Sort class to handle sorting column using std::sort
class dirtree_Sort {
  bool reverse_;

public:
  dirtree_Sort(bool reverse) {
    reverse_ = reverse;
  }

  bool operator() (const std::string &ap_, const std::string &bp_)
  {
    const char *ap = ap_.c_str();
    const char *bp = bp_.c_str();

    // skip first byte if it's '/' (directory instead of link to directory)
    if (*ap == '/') ap++;
    if (*bp == '/') bp++;

    // ignore leading dots in filenames ("bin, .config, data, .local"
    // instead of ".config, .local, bin, data")
    if (*ap == '.') ap++;
    if (*bp == '.') bp++;

    if (isdigit(*ap) && isdigit(*bp)) {
      // Numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
      long av = atol(ap);
      long bv = atol(bp);

      // if numbers are the same, continue to alphabetic sort
      if (av != bv) {
        return reverse_ ? bv < av : av < bv;
      }
    }

    // Alphabetic sort, case-insensitive
    return reverse_ ? strcasecmp(ap, bp) > 0 : strcasecmp(ap, bp) < 0;
  }
};

class dirtree : public Fl_Tree
{
private:
  bool show_hidden_;
  bool sort_reverse_;
  Fl_SVG_Image *icon_;
  Fl_SVG_Image *icon_link_;
  Fl_SVG_Image *icon_locked_;

  std::string callback_item_path_;

  static void default_callback(Fl_Widget *w, void *)
  {
    dirtree *o = dynamic_cast<dirtree *>(w);

    switch(o->callback_reason()) {
      /*
      case FL_TREE_REASON_RESELECTED:
      case FL_TREE_REASON_SELECTED: {
          const char *path = o->callback_item_path();
          if (path) printf("%s\n", path);
        }
        break;
      */
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

  // return the absolute path of an item, ignoring empty labels
  std::string item_path(Fl_Tree_Item *ti)
  {
    std::string s;

    if (ti->is_root()) {
      return "/";
    }

    while (ti && !ti->is_root()) {
      const char *l = ti->label();

      if (l && *l) {
        s.insert(0, l);
        s.insert(0, 1, '/');
      }
      ti = ti->parent();
    }

    return s;
  }

  // unfortunately Fl_Tree::item_labelsize() and Fl_Tree_Item::recalc_tree()
  // don't update all the present labels with the new size;
  // instead we get an array of currently open directories, close
  // everything and open it up again
  //
  // return value is true if *ti was an open empty directory; this is used
  // by the method when calling itself recursively
  bool get_open_paths(Fl_Tree_Item *ti, std::vector<std::string> &vec)
  {
    if (!ti || (ti->children() > 0 && ti->child(0)->label() == NULL)) {
      // item is NULL or isn't open
      return false;
    }

    if (ti->children() == 0) {
      // item is an open empty directory
      vec.push_back(item_path(ti));
      return true;
    }

    bool do_push_back = true;

    for (int i = 0; i < ti->children(); ++i) {
      auto child = ti->child(i);

      if (do_push_back && child && child->children() > 0 && child->child(0)->label()) {
        // child has children
        do_push_back = false;
      }

      if (get_open_paths(child, vec)) {
        // child is an open empty directory
        do_push_back = false;
      }
    }

    if (do_push_back) {
      vec.push_back(item_path(ti));
    }

    return false;
  }

protected:
  // open a directory from a selected Fl_Tree_Item
  bool load_tree(Fl_Tree_Item *ti)
  {
    struct dirent *dir;
    DIR *d;
    std::vector<std::string> list;

    int fd = ::open(item_path(ti).c_str(), O_CLOEXEC | O_DIRECTORY, O_RDONLY);

    if (fd == -1) {
      return false;
    }

    if ((d = fdopendir(fd)) == NULL) {
      ::close(fd);
      return false;
    }

    while ((dir = readdir(d)) != NULL) {
      struct stat st, lst;

      /* handle hidden files */
      if (!show_hidden() && dir->d_name[0] == '.') {
        continue;
      }

      /* no "." and ".." entries */
      if (show_hidden() && (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)) {
        continue;
      }

      /* act like lstat() */
      if (fstatat(fd, dir->d_name, &lst, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW) == -1) {
        continue;
      }

      const std::string sep("/");

      /* use first byte to identify entry as a link or directory */
      if (S_ISDIR(lst.st_mode)) {
        /* directory: entry begins with '/' */
        list.push_back(std::string(sep + dir->d_name));
      }
      else if (S_ISLNK(lst.st_mode) &&
               /* act like stat() */
               fstatat(fd, dir->d_name, &st, AT_NO_AUTOMOUNT) == 0 && S_ISDIR(st.st_mode))
      {
        /* link to directory: does NOT begin with '/' */
        list.push_back(dir->d_name);
      }
    }

    closedir(d);
    ::close(fd);
    ti->clear_children();

    if (list.size() == 0) {
      close(ti, 0);
      return true;
    }

    std::stable_sort(list.begin(), list.end(), dirtree_Sort(sort_reverse()));

    for (const std::string &elem : list) {
      add(ti, elem.front() == '/' ? elem.c_str() + 1 : elem.c_str());
      Fl_Tree_Item *sub = ti->child(ti->has_children() ? ti->children() - 1 : 0);

      if (elem.front() != '/' && icon_link_) {
        sub->usericon(icon_link_);
      }

      sub->clear_children();
      close(sub, 0);
      add(sub, NULL);  // dummy entry
    }

    return true;
  }

  /* force to accept only SVG icons (for now?) */
  Fl_Image *usericon() const { return NULL; }
  void usericon(Fl_Image *val) { Fl_Tree::usericon(val); }

public:
  dirtree(int X, int Y, int W, int H, const char *L=NULL) : Fl_Tree(X, Y, W, H, L)
  {
    show_hidden_ = false;
    sort_reverse_ = false;
    icon_ = NULL;
    icon_link_ = NULL;
    icon_locked_ = NULL;

    root_label("/");
    item_reselect_mode(FL_TREE_SELECTABLE_ALWAYS);
    connectorstyle(FL_TREE_CONNECTOR_SOLID);
    callback(default_callback, NULL);

    close(root(), 0);
    add(root(), NULL);  // dummy entry; alternatively call load_root()
  }

  virtual ~dirtree() {}

  // return the path of the last selected item or NULL
  // if nothing was selected yet
  const char *callback_item_path()
  {
    callback_item_path_ = item_path(callback_item());
    return callback_item_path_.empty() ? NULL : callback_item_path_.c_str();
  }

  // a '+' sign was clicked
  void open_callback_item()
  {
    Fl_Tree_Item *ti = callback_item();

    if (!load_tree(ti)) {
      /* don't add a dummy entry, so that the plus sign will disappear */
      ti->clear_children();
      //ti->deactivate();

      if (icon_locked_) {
        ti->usericon(icon_locked_);
      }
    }
  }

  // a '-' sign was clicked
  void close_callback_item()
  {
    Fl_Tree_Item *ti = callback_item();
    ti->clear_children();
    close(ti, 0);
    add(ti, NULL); // dummy entry so the plus sign appears
  }

  // load a directory from a given path
  bool load_directory(const char *path)
  {
    char *simp, *p;
    bool rv;

    if (!path || *path == 0) {
      return false;
    }

    if (path[0] == '/' && path[1] == 0) {
      return load_root();
    }

    if ((simp = filetable_::simplify_directory_path(path)) == NULL) {
      /* fallback */
      simp = strdup(path);
    }
    p = simp;
    //printf("DEBUG: open -> %s\n", simp);

    load_root();

    /* open subdirectories step by step */
    while (*++p) {
      if (*p != '/') continue;
      *p = 0;
      open(simp);
      *p = '/';
    }

    /* finally open the full path */
    rv = open(simp);
    free(simp);

    return rv;
  }

  // same as load_directory("/")
  bool load_root() {
    return Fl_Tree::open(root());
  }

  // basically an SVG-only wrapper for the protected
  // method dirtree::usericon()
  void usericon_svg(Fl_SVG_Image *svg)
  {
    if (svg && !svg->fail()) {
      icon_ = svg;
      icon_->resize(item_labelsize(), item_labelsize());
      usericon(icon_);
    }
  }

  Fl_SVG_Image *usericon_svg() const { return icon_; }

  // set a different icon for directory links
  void usericon_link(Fl_SVG_Image *svg)
  {
    if (svg && !svg->fail()) {
      icon_link_ = svg;
      icon_link_->resize(item_labelsize(), item_labelsize());
    }
  }

  Fl_SVG_Image *usericon_link() const { return icon_link_; }

  // set a different icon for unaccessible directories 
  void usericon_locked(Fl_SVG_Image *svg)
  {
    if (svg && !svg->fail()) {
      icon_locked_ = svg;
      icon_locked_->resize(item_labelsize(), item_labelsize());
    }
  }

  Fl_SVG_Image *usericon_locked() const { return icon_locked_; }

  // set the labelsize on items including root()
  // and adjust the icon size accordingly too
  void item_labelsize(Fl_Fontsize i)
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

    if (!root()->is_open()) {
      return;
    }

    std::vector<std::string> vec;
    get_open_paths(root(), vec);

    // close everything
    close(root(), 0);
    add(root(), NULL); // unneeded?

    // reopen directories
    for (const std::string &elem : vec) {
      //printf("DEBUG: reopening -> %s\n", elem.c_str());
      load_directory(elem.c_str());
    }

    recalc_tree(); // unneeded?
  }

  Fl_Fontsize item_labelsize() const { return Fl_Tree::item_labelsize(); }

  void show_hidden(bool b) { show_hidden_ = b; }
  bool show_hidden() const { return show_hidden_; }

  void sort_reverse(bool b) { sort_reverse_ = b; }
  bool sort_reverse() const { return sort_reverse_; }
};

} // namespace fltk

#endif  // dirtree__hpp

