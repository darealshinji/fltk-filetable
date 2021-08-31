/*
  Copyright (c) 2021 djcj <djcj@gmx.de>

  The icons used are
  Copyright (c) 2007-2020 Haiku, Inc.

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

class dirtree : public Fl_Tree
{
private:
  enum {
    TYPE_DEFAULT,
    TYPE_LINK,
    TYPE_LOCKED,
    TYPE_LOCKED_LINK,
    TYPE_NUM
  };

  typedef struct {
    char *name;
    bool is_link;
  } dir_entry_t;

  // Sort class to handle sorting column using std::sort
  class sort
  {
  private:
    bool reverse_;

  public:
    sort(bool reverse) {
      reverse_ = reverse;
    }

    bool operator() (const dir_entry_t ap_, const dir_entry_t bp_)
    {
      const char *ap = ap_.name;
      const char *bp = bp_.name;

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

  bool show_hidden_ = false;
  bool sort_reverse_ = false;
  Fl_SVG_Image *icon_[TYPE_NUM] = {0};
  Fl_SVG_Image *def_icon_[TYPE_NUM] = {0};
  std::string callback_item_path_;

  static void default_callback(Fl_Widget *w, void *)
  {
    dirtree *o = dynamic_cast<dirtree *>(w);
    //if (!o) return;

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

protected:
  inline bool empty(const char *val) {
    return (!val || *val == 0) ? true : false;
  }

  // appends data from "filename" to "data"
  static bool append_file(std::string &data, const char *filename, const size_t max)
  {
    FILE *fp = fopen(filename, "r");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);

    if (fsize >= max) {
      fclose(fp);
      return false;
    }
    rewind(fp);

    data.reserve(data.size() + fsize);
    char c = fgetc(fp);

    while (c != EOF) {
      data.push_back(c);
      c = fgetc(fp);
    }

    fclose(fp);
    return true;
  }

  // return the absolute path of an item, ignoring empty labels
  std::string item_path(const Fl_Tree_Item *ti)
  {
    if (ti->is_root()) {
      return "/";
    }

    std::string s;

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

  // update all items
  void update_items(Fl_Tree_Item *ti)
  {
    if (!ti || ti->is_close()) {
      return;
    }

    if (ti == root()) {
      ti->labelsize(item_labelsize());
      ti->labelfont(item_labelfont());
      ti->labelfgcolor(item_labelfgcolor());
      ti->labelbgcolor(item_labelbgcolor());
      ti->usericon(usericon(TYPE_DEFAULT));
    }

    for (int i = 0; i < ti->children(); ++i) {
      auto child = ti->child(i);
      child->labelsize(item_labelsize());
      child->labelfont(item_labelfont());
      child->labelfgcolor(item_labelfgcolor());
      child->labelbgcolor(item_labelbgcolor());

      void *ptr = child->user_data();

      // careful: the enum values are stored as address pointers;
      // do not attempt to access the addresses!
      if (ptr == reinterpret_cast<void *>(TYPE_LINK)) {
        child->usericon(usericon(TYPE_LINK));
      } else if (ptr == reinterpret_cast<void *>(TYPE_LOCKED)) {
        child->usericon(usericon(TYPE_LOCKED));
      } else if (ptr == reinterpret_cast<void *>(TYPE_LOCKED_LINK)) {
        child->usericon(usericon(TYPE_LOCKED_LINK));
      } else {
        child->usericon(usericon(TYPE_DEFAULT));
      }

      // recursive call of method
      update_items(child);
    }

    recalc_tree();
  }

  void update_items() { update_items(root()); }

  // open a directory from a selected Fl_Tree_Item
  bool load_tree(Fl_Tree_Item *ti)
  {
    struct dirent *dir;
    DIR *d;
    std::vector<dir_entry_t> list;

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

      // store directories and links to directories
      if (S_ISDIR(lst.st_mode)) {
        dir_entry_t ent;
        ent.name = strdup(dir->d_name);
        ent.is_link = false;
        list.push_back(ent);
      }
      else if (S_ISLNK(lst.st_mode) &&
               fstatat(fd, dir->d_name, &st, AT_NO_AUTOMOUNT) == 0 &&  // act like stat()
               S_ISDIR(st.st_mode))
      {
        dir_entry_t ent;
        ent.name = strdup(dir->d_name);
        ent.is_link = true;
        list.push_back(ent);
      }
    }

    closedir(d);
    ::close(fd);

    // remove dummy entry
    ti->clear_children();

    if (list.size() == 0) {
      close(ti, 0);
      return true;
    }

    std::stable_sort(list.begin(), list.end(), sort(sort_reverse()));

    for (const auto elem : list) {
      add(ti, elem.name);
      auto sub = ti->child(ti->has_children() ? ti->children() - 1 : 0);

      if (elem.is_link) {
        sub->user_data(reinterpret_cast<void *>(TYPE_LINK));
        sub->usericon(usericon(TYPE_LINK));
      }

      close(sub, 0);
      add(sub, NULL);  // dummy entry
      free(elem.name);
    }

    return true;
  }

public:
  dirtree(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Tree(X, Y, W, H, L)
  {
    root_label("/");
    item_reselect_mode(FL_TREE_SELECTABLE_ALWAYS);
    connectorstyle(FL_TREE_CONNECTOR_SOLID);
    callback(default_callback, NULL);

    close(root(), 0);
    add(root(), NULL);  // dummy entry
  }

  virtual ~dirtree()
  {
    for (int i=0; i < TYPE_NUM; ++i) {
      if (def_icon_[i]) delete def_icon_[i];
    }
  }

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
      // don't add a dummy entry, so that the plus sign will disappear
      ti->clear_children();

      if (ti->user_data() == reinterpret_cast<void *>(TYPE_LINK)) {
        ti->usericon(usericon(TYPE_LOCKED_LINK));
        ti->user_data(reinterpret_cast<void *>(TYPE_LOCKED_LINK));
      } else {
        ti->usericon(usericon(TYPE_LOCKED));
        ti->user_data(reinterpret_cast<void *>(TYPE_LOCKED));
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
  bool load_dir(const char *inPath)
  {
    if (empty(inPath)) {
      return false;
    }

    if (inPath[0] == '/' && inPath[1] == 0) {
      return load_root();
    }

    std::string s = filetable_::simplify_directory_path(inPath);
    if (s.empty()) s = inPath;
    char *p = const_cast<char *>(s.data());

    load_root();

    // open subdirectories step by step
    while (*++p) {
      if (*p != '/') continue;
      *p = 0;
      if (open(s.data()) == -1) return false;
      *p = '/';
    }

    // finally open the full path
    return (open(s.data()) != -1);
  }

  // same as load_dir("/")
  bool load_root() {
    return Fl_Tree::open(root());
  }

  // load a set of default icons
  void load_default_icons()
  {
    const char *data[TYPE_NUM] = {0};
    data[TYPE_DEFAULT] = FOLDER_GENERIC_SVG_DATA;
    data[TYPE_LINK] = FOLDER_LINK_SVG_DATA;
    data[TYPE_LOCKED] = FOLDER_NOACCESS_SVG_DATA;
    data[TYPE_LOCKED_LINK] = FOLDER_LINK_NOACCESS_SVG_DATA;

    for (int i=0; i < TYPE_NUM; ++i) {
      if (!def_icon_[i]) {
        def_icon_[i] = new Fl_SVG_Image(NULL, data[i]);
      }
      usericon(def_icon_[i], i);
    }
  }

  // set usericon by index/type;
  // force any call to usericon() method to accept only Fl_SVG_Image* as argument
  void usericon(Fl_SVG_Image *svg, int idx=TYPE_DEFAULT)
  {
    if (!svg || svg->fail()) return;

    switch(idx) {
      case TYPE_DEFAULT:
        icon_[idx] = svg;
        icon_[idx]->resize(item_labelsize(), item_labelsize());
        Fl_Tree::usericon(icon_[idx]);  // set default usericon
        update_items();
        break;
      case TYPE_LINK:
      case TYPE_LOCKED:
      case TYPE_LOCKED_LINK:
        icon_[idx] = svg;
        icon_[idx]->resize(item_labelsize(), item_labelsize());
        update_items();
        break;
      default:
        break;
    }
  }

  // get usericon by index/type
  Fl_SVG_Image *usericon(int idx) const
  {
    switch(idx) {
      case TYPE_LINK:
      case TYPE_LOCKED:
        if (icon_[idx]) return icon_[idx];
        break;
      case TYPE_LOCKED_LINK:
        if (icon_[idx]) return icon_[idx];
        if (icon_[TYPE_LOCKED]) return icon_[TYPE_LOCKED];
        break;
      default:
        break;
    }

    return icon_[TYPE_DEFAULT];
  }

  Fl_SVG_Image *usericon() const {
    return icon_[TYPE_DEFAULT];
  }

  // set the labelsize on items including root()
  // and adjust the icon size accordingly too
  void item_labelsize(Fl_Fontsize val)
  {
    Fl_Tree::item_labelsize(val);

    for (int i=0; i < TYPE_NUM; ++i) {
      if (icon_[i]) {
        icon_[i]->resize(val, val);
      }
    }

    if (icon_[TYPE_DEFAULT]) {
      Fl_Tree::usericon(icon_[TYPE_DEFAULT]);
    }

    update_items();
  }

  Fl_Fontsize item_labelsize() const { return Fl_Tree::item_labelsize(); }

  // labelfont
  Fl_Font item_labelfont() const { return Fl_Tree::item_labelfont(); }

  void item_labelfont(Fl_Font val) {
    Fl_Tree::item_labelfont(val);
    update_items();
  }

  // labelfgcolor
  Fl_Color item_labelfgcolor(void) const { return Fl_Tree::item_labelfgcolor(); }

  void item_labelfgcolor(Fl_Color val) {
    Fl_Tree::item_labelfgcolor(val);
    update_items();
  }

  // labelbgcolor
  Fl_Color item_labelbgcolor(void) const { return Fl_Tree::item_labelbgcolor(); }

  void item_labelbgcolor(Fl_Color val) {
    Fl_Tree::item_labelbgcolor(val);
    update_items();
  }

  void show_hidden(bool b) { show_hidden_ = b; }
  bool show_hidden() const { return show_hidden_; }

  void sort_reverse(bool b) { sort_reverse_ = b; }
  bool sort_reverse() const { return sort_reverse_; }
};

} // namespace fltk

#endif  // dirtree__hpp

