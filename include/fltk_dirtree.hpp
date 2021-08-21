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
  // return the absolute path of an item, ignoring empty labels
  std::string item_path(Fl_Tree_Item *ti)
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

  /* force to accept only SVG icons (for now?) */
  Fl_Image *usericon() const { return Fl_Tree::usericon(); }
  void usericon(Fl_Image *val) { Fl_Tree::usericon(val); }

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

  ~dirtree()
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
  bool load_dir(const char *path)
  {
    if (!path || *path == 0) {
      return false;
    }

    if (path[0] == '/' && path[1] == 0) {
      return load_root();
    }

    char *s = filetable_::simplify_directory_path(path);
    if (!s) s = strdup(path);  // fallback
    char *p = s;

    load_root();

    // open subdirectories step by step
    while (*++p) {
      if (*p != '/') continue;
      *p = 0;

      if (open(s) == -1) {
        free(s);
        return false;
      }
      *p = '/';
    }

    // finally open the full path
    int rv = open(s);
    free(s);

    return (rv == -1) ? false : true;
  }

  // same as load_dir("/")
  bool load_root() {
    return Fl_Tree::open(root());
  }

  // load a set of default icons
  void load_default_icons()
  {
    // Icons were taken from https://git.haiku-os.org/haiku/tree/data/artwork/icons
    // exported to SVG, minimized with Scour and manually edited
    // (scour --no-renderer-workaround --strip-xml-prolog --remove-descriptive-elements --enable-comment-stripping
    //  --disable-embed-rasters --indent=none --strip-xml-space --enable-id-stripping --shorten-ids)

    const char *data[TYPE_NUM] =
    {
      // Folder_generic.svg
      "<svg width='64' height='64'>"
        "<path d='m42 61h6l4-4h4l6-6-14-4-6 14z' fill='#010101' fill-opacity='.396'/>"
        "<path d='m3 15 10 26 29 18 18-47-12-2-4 2-18-4-2 5-8-2-.09 8.2-12.91-4.2z' fill='none' stroke='#000' stroke-width='4'/>"
        "<linearGradient id='d' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#face79' offset='0'/>"
          "<stop stop-color='#bc4105' offset='1'/>"
        "</linearGradient>"
        "<path d='m26 8-12 33 28 17 18-46-12-2-4 2-18-4z' fill='url(#d)'/>"
        "<linearGradient id='c' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#fff' offset='0'/>"
          "<stop stop-color='#8e8e8e' offset='1'/>"
        "</linearGradient>"
        "<path d='m16 11v30l26 16 7-35-33-11z' fill='url(#c)'/>"
        "<linearGradient id='b' x1='78.34' x2='101.46' y1='-26.66' y2='12.94' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#9a9a9a' offset='0'/>"
          "<stop stop-color='#505050' offset='1'/>"
        "</linearGradient>"
        "<path d='M16 11L49 22L42 57L52 20.37L16 11z' fill='url(#b)'/>"
        "<linearGradient id='a' x1='88.52' x2='97.54' y1='9.59' y2='51.29' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#ffe2ac' offset='0'/>"
          "<stop stop-color='#f49806' offset='1'/>"
        "</linearGradient>"
        "<path d='m3 15 10 26 29 18-4-29-35-15z' fill='url(#a)'/>"
        "<path d='M3 15L38 30L42 59L40.5 28L3 15z' fill='#a03d03'/>"
      "</svg>",

      // Folder_link.svg
      "<svg width='64' height='64'>"
        "<path d='m42 61h6l4-4h4l6-6-14-4-6 14z' fill='#010101' fill-opacity='.396'/>"
        "<path d='m3 15 10 26 29 18 18-47-12-2-4 2-18-4-2 5-8-2-.09 8.2-12.91-4.2z' fill='none' stroke='#000' stroke-width='4'/>"
        "<linearGradient id='d' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#face79' offset='0'/>"
          "<stop stop-color='#bc4105' offset='1'/>"
        "</linearGradient>"
        "<path d='m26 8-12 33 28 17 18-46-12-2-4 2-18-4z' fill='url(#d)'/>"
        "<linearGradient id='c' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#fff' offset='0'/>"
          "<stop stop-color='#8e8e8e' offset='1'/>"
        "</linearGradient>"
        "<path d='m16 11v30l26 16 7-35-33-11z' fill='url(#c)'/>"
        "<linearGradient id='b' x1='78.34' x2='101.46' y1='-26.66' y2='12.94' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#9a9a9a' offset='0'/>"
          "<stop stop-color='#505050' offset='1'/>"
        "</linearGradient>"
        "<path d='M16 11L49 22L42 57L52 20.37L16 11z' fill='url(#b)'/>"
        "<linearGradient id='a' x1='88.52' x2='97.54' y1='9.59' y2='51.29' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#ffe2ac' offset='0'/>"
          "<stop stop-color='#f49806' offset='1'/>"
        "</linearGradient>"
        "<path d='m3 15 10 26 29 18-4-29-35-15z' fill='url(#a)'/>"
        "<path d='M3 15L38 30L42 59L40.5 28L3 15z' fill='#a03d03'/>"
        "<g transform='matrix(1.6466 0 0 1.6466 1.084 .86265)' color-interpolation='linearRGB'>"
          "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z'"
            " fill='none' stroke='#000' stroke-linecap='round' stroke-linejoin='round' stroke-width='3.6416'/>"
          "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z'"
            " fill='#0ff' stroke-width='.69364'/>"
        "</g>"
      "</svg>",

      // Folder_noaccess.svg
      "<svg width='64' height='64'>"
        "<defs>"
          "<linearGradient id='d' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse' xlink:href='#b'/>"
          "<linearGradient id='c' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse' xlink:href='#a'/>"
        "</defs>"
        "<path d='m42 61h6l4-4h4l6-6-14-4-6 14z' fill='#010101' fill-opacity='.396'/>"
        "<path d='m3 15 10 26 29 18 18-47-12-2-4 2-18-4-2 5-8-2-.09 8.2-12.91-4.2z' fill='none' stroke='#000' stroke-width='4'/>"
        "<linearGradient id='b' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#face79' offset='0'/>"
          "<stop stop-color='#bc4105' offset='1'/>"
        "</linearGradient>"
        "<path d='m26 8-12 33 28 17 18-46-12-2-4 2-18-4z' fill='url(#b)'/>"
        "<linearGradient id='a' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#fff' offset='0'/>"
          "<stop stop-color='#8e8e8e' offset='1'/>"
        "</linearGradient>"
        "<path d='m16 11v30l26 16 7-35-33-11z' fill='url(#a)'/>"
        "<linearGradient id='f' x1='78.34' x2='101.46' y1='-26.66' y2='12.94' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#9a9a9a' offset='0'/>"
          "<stop stop-color='#505050' offset='1'/>"
        "</linearGradient>"
        "<path d='M16 11L49 22L42 57L52 20.37L16 11z' fill='url(#f)'/>"
        "<linearGradient id='e' x1='88.52' x2='97.54' y1='9.59' y2='51.29' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#ffe2ac' offset='0'/>"
          "<stop stop-color='#f49806' offset='1'/>"
        "</linearGradient>"
        "<path d='m3 15 10 26 29 18-4-29-35-15z' fill='url(#e)'/>"
        "<path d='M3 15L38 30L42 59L40.5 28L3 15z' fill='#a03d03'/>"
        "<g transform='matrix(1.4805 0 0 1.4805 -30.621 -29.375)'>"
          "<path transform='matrix(1 0 0 .9473 0 3.2114)' d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='none' stroke='#000' stroke-width='4'/>"
          "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='#000' stroke-width='8'/>"
          "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='url(#d)' stroke-width='4'/>"
          "<path d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='url(#c)'/>"
          "<path d='m50 49c-1 0-2 1-2 2s0 1 1 2c0 2 0 3 1 3s1-1 1-3c1-1 1-1 1-2s-1-2-2-2z'/>"
          "<path d='m40 44h20' fill='none' stroke='#000'/>"
        "</g>"
      "</svg>",

      // Folder_link_noaccess.svg
      "<svg width='64' height='64'>"
        "<defs>"
          "<linearGradient id='d' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse' xlink:href='#b'/>"
          "<linearGradient id='c' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse' xlink:href='#a'/>"
        "</defs>"
        "<path d='m42 61h6l4-4h4l6-6-14-4-6 14z' fill='#010101' fill-opacity='.396'/>"
        "<path d='m3 15 10 26 29 18 18-47-12-2-4 2-18-4-2 5-8-2-.09 8.2-12.91-4.2z' fill='none' stroke='#000' stroke-width='4'/>"
        "<linearGradient id='b' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#face79' offset='0'/>"
          "<stop stop-color='#bc4105' offset='1'/>"
        "</linearGradient>"
        "<path d='m26 8-12 33 28 17 18-46-12-2-4 2-18-4z' fill='url(#b)'/>"
        "<linearGradient id='a' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#fff' offset='0'/>"
          "<stop stop-color='#8e8e8e' offset='1'/>"
        "</linearGradient>"
        "<path d='m16 11v30l26 16 7-35-33-11z' fill='url(#a)'/>"
        "<linearGradient id='f' x1='78.34' x2='101.46' y1='-26.66' y2='12.94' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#9a9a9a' offset='0'/>"
          "<stop stop-color='#505050' offset='1'/>"
        "</linearGradient>"
        "<path d='M16 11L49 22L42 57L52 20.37L16 11z' fill='url(#f)'/>"
        "<linearGradient id='e' x1='88.52' x2='97.54' y1='9.59' y2='51.29' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#ffe2ac' offset='0'/>"
          "<stop stop-color='#f49806' offset='1'/>"
        "</linearGradient>"
        "<path d='m3 15 10 26 29 18-4-29-35-15z' fill='url(#e)'/>"
        "<path d='M3 15L38 30L42 59L40.5 28L3 15z' fill='#a03d03'/>"
        "<g transform='matrix(1.4805 0 0 1.4805 -30.621 -29.375)'>"
          "<path transform='matrix(1 0 0 .9473 0 3.2114)' d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='none' stroke='#000' stroke-width='4'/>"
          "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='#000' stroke-width='8'/>"
          "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='url(#d)' stroke-width='4'/>"
          "<path d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='url(#c)'/>"
          "<path d='m50 49c-1 0-2 1-2 2s0 1 1 2c0 2 0 3 1 3s1-1 1-3c1-1 1-1 1-2s-1-2-2-2z'/>"
          "<path d='m40 44h20' fill='none' stroke='#000'/>"
        "</g>"
        "<g transform='matrix(1.6466 0 0 1.6466 1.084 .86265)' color-interpolation='linearRGB'>"
          "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z'"
            " fill='none' stroke='#000' stroke-linecap='round' stroke-linejoin='round' stroke-width='3.6416'/>"
          "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z'"
            " fill='#0ff' stroke-width='.69364'/>"
        "</g>"
      "</svg>"
    };

    for (int i=0; i < TYPE_NUM; ++i) {
      if (!def_icon_[i]) {
        def_icon_[i] = new Fl_SVG_Image(NULL, data[i]);
      }
      usericon(def_icon_[i], i);
    }
  }

  // set usericon by index/type
  void usericon(Fl_SVG_Image *svg, int idx)
  {
    if (!svg || svg->fail()) return;

    switch(idx) {
      case TYPE_DEFAULT:
        icon_[idx] = svg;
        icon_[idx]->resize(item_labelsize(), item_labelsize());
        usericon(icon_[idx]);  // set default usericon
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
      usericon(icon_[TYPE_DEFAULT]);
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

