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
    TYPE_LINK        = 0x01,
    TYPE_LOCKED      = 0x02,
    TYPE_LOCKED_LINK = 0x03
  };

  enum {
    ICN_DIR = 0,  // directory
    ICN_LNK = 1,  // link overlay
    ICN_LCK = 2,  // lock overlay
    ICN_NUM = 3
  };

  enum {
    RGB_LNK = 0,  // directory + link
    RGB_LCK = 1,  // directory + lock
    RGB_LLK = 2,  // directory + lock + link
    RGB_NUM = 3
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
  std::string callback_item_path_;

  Fl_RGB_Image *rgb_[RGB_NUM] = {0};
  Fl_SVG_Image *def_[ICN_NUM] = {0};
  Fl_SVG_Image *icn_[ICN_NUM] = {0};

  static void default_callback(Fl_Widget *w, void *)
  {
    dirtree *o = reinterpret_cast<dirtree *>(w);

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

  static inline bool same_image_format(Fl_RGB_Image *a, Fl_RGB_Image *b)
  {
    return (a && b && !a->fail() && !b->fail() &&
            a->w() == b->w() && a->h() == b->h() &&
            a->d() == b->d() && a->ld() == b->ld());
  }

  // set icon by index number
  void usericon(Fl_SVG_Image *svg, int idx)
  {
    if (!svg || svg->fail()) return;
    svg->proportional = false;
    svg->resize(item_labelsize(), item_labelsize());
    icn_[idx] = svg;

    // it's important to set this here
    if (idx == ICN_DIR) Fl_Tree::usericon(icn_[idx]);
  }

protected:
  inline bool empty(const char *val) {
    return (!val || *val == 0) ? true : false;
  }

  // blend/overlay up to 3 Fl_RGB_Image* images in RGBA format
  static Fl_RGB_Image *blend_rgba(Fl_RGB_Image *inBg, Fl_RGB_Image *inFg, Fl_RGB_Image *inFg2=NULL)
  {
    // background image is needed
    if (!inBg || inBg->fail()) {
      return NULL;
    }

    if (!inFg || inFg->fail()) {
      if (inFg2 && !inFg2->fail()) {
        // take over pointer
        inFg = inFg2;
        inFg2 = NULL;
      } else {
        // no foreground images provided
        return NULL;
      }
    }

    const int w = inBg->w();
    int ld = inBg->ld();

    // d() must be 4, ld() must be formatted correctly
    if (inBg->d() != 4 || (ld > 0 && ld < w*4) || ld < 0) {
      return NULL;
    }

    // compare image specs

    if (!same_image_format(inBg, inFg)) {
      return NULL;
    }

    if (!same_image_format(inFg, inFg2)) {
      inFg2 = NULL;
    }

    // don't calculate with the full line data
    if (ld != 0) {
      ld -= w*4;
    }

    const int h = inBg->h();
    uchar *out = new uchar[w*h*4];

    // http://de.voidcc.com/question/p-shsavrtq-bz.html
    if (inFg2) {
      for (int x=0, i=0; x < w; ++x) {
        for (int y=0; y < h; ++y, i+=4) {
          const int off = i + y*ld;
          const uchar *bg = inBg->array + off;
          const uchar *fg = inFg->array + off;
          const uchar *fg2 = inFg2->array + off;
          uint alpha = fg[3] + 1;
          uchar *px = out + i;

          if (alpha == 1) {
            px[0] = bg[0];
            px[1] = bg[1];
            px[2] = bg[2];
            px[3] = bg[3];
          } else {
            const uint inv_alpha = 256 - fg[3];
            px[0] = (alpha * fg[0] + inv_alpha * bg[0]) >> 8;
            px[1] = (alpha * fg[1] + inv_alpha * bg[1]) >> 8;
            px[2] = (alpha * fg[2] + inv_alpha * bg[2]) >> 8;
            px[3] = (alpha * fg[3] + inv_alpha * bg[3]) >> 8;
          }

          if ((alpha = fg2[3] + 1) != 1) {
            const uint inv_alpha = 256 - fg2[3];
            px[0] = (alpha * fg2[0] + inv_alpha * px[0]) >> 8;
            px[1] = (alpha * fg2[1] + inv_alpha * px[1]) >> 8;
            px[2] = (alpha * fg2[2] + inv_alpha * px[2]) >> 8;
            px[3] = (alpha * fg2[3] + inv_alpha * px[3]) >> 8;
          }
        }
      }
    } else {
      for (int x=0, i=0; x < w; ++x) {
        for (int y=0; y < h; ++y, i+=4) {
          const int off = i + y*ld;
          const uchar *bg = inBg->array + off;
          const uchar *fg = inFg->array + off;
          const uint alpha = fg[3] + 1;
          uchar *px = out + i;

          if (alpha == 1) {
            px[0] = bg[0];
            px[1] = bg[1];
            px[2] = bg[2];
            px[3] = bg[3];
          } else {
            const uint inv_alpha = 256 - fg[3];
            px[0] = (alpha * fg[0] + inv_alpha * bg[0]) >> 8;
            px[1] = (alpha * fg[1] + inv_alpha * bg[1]) >> 8;
            px[2] = (alpha * fg[2] + inv_alpha * bg[2]) >> 8;
            px[3] = (alpha * fg[3] + inv_alpha * bg[3]) >> 8;
          }
        }
      }
    }

    Fl_RGB_Image *rgba = new Fl_RGB_Image(out, w, h, 4);
    rgba->alloc_array = 1;

    return rgba;
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
      ti->usericon(icn_[ICN_DIR]);
    }

    for (int i = 0; i < ti->children(); ++i) {
      auto child = ti->child(i);
      child->labelsize(item_labelsize());
      child->labelfont(item_labelfont());
      child->labelfgcolor(item_labelfgcolor());
      child->labelbgcolor(item_labelbgcolor());

      void *ptr = child->user_data();

      // careful: the enum values are stored as address pointers
      if (ptr == reinterpret_cast<void *>(TYPE_LINK)) {
        child->usericon(rgb_[RGB_LNK]);
      } else if (ptr == reinterpret_cast<void *>(TYPE_LOCKED)) {
        child->usericon(rgb_[RGB_LCK]);
      } else if (ptr == reinterpret_cast<void *>(TYPE_LOCKED_LINK)) {
        child->usericon(rgb_[RGB_LLK]);
      } else {
        child->usericon(icn_[ICN_DIR]);
      }

      // recursive call of method
      update_items(child);
    }

    recalc_tree();
  }

  void update_items(bool regen=false)
  {
    // don't regenerate RGB icons
    if (!regen) {
      update_items(root());
      return;
    }

    // delete RGB data
    for (int i=0; i < RGB_NUM; ++i) {
      if (rgb_[i]) delete rgb_[i];
      rgb_[i] = NULL;
    }

    // no folder icon means no icons at all
    if (!icn_[ICN_DIR]) {
      update_items(root());
      return;
    }

    // generate icons (svg images were already resized properly)
    if (icn_[ICN_LNK]) rgb_[RGB_LNK] = blend_rgba(icn_[ICN_DIR], icn_[ICN_LNK]);
    if (icn_[ICN_LCK]) rgb_[RGB_LCK] = blend_rgba(icn_[ICN_DIR], icn_[ICN_LCK]);

    if (icn_[ICN_LNK] && icn_[ICN_LCK]) {
      rgb_[RGB_LLK] = blend_rgba(icn_[ICN_DIR], icn_[ICN_LCK], icn_[ICN_LNK]);
    }

    update_items(root());
  }

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

    for (const auto e : list) {
      add(ti, e.name);
      auto sub = ti->child(ti->has_children() ? ti->children() - 1 : 0);

      if (e.is_link) {
        sub->user_data(reinterpret_cast<void *>(TYPE_LINK));
        sub->usericon(rgb_[RGB_LNK]);
      }

      close(sub, 0);
      add(sub, NULL);  // dummy entry
      free(e.name);
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
    for (int i=0; i < ICN_NUM; ++i) {
      if (def_[i]) delete def_[i];
    }

    for (int i=0; i < RGB_NUM; ++i) {
      if (rgb_[i]) delete rgb_[i];
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
        ti->usericon(rgb_[RGB_LLK]);
        ti->user_data(reinterpret_cast<void *>(TYPE_LOCKED_LINK));
      } else {
        ti->usericon(rgb_[RGB_LCK]);
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
    // Fl_Tree::open(root()) will "reset" the entire root tree!
    return (open(root()) != -1);
  }

  // close the tree
  void close_root() {
    close(root(), 0);
    add(root(), NULL);  // dummy entry
  }

  // load a set of default icons
  void load_default_icons()
  {
    const char *data[ICN_NUM] = {
      FOLDER_GENERIC_SVG_DATA,
      OVERLAY_LINK_BIG_SVG_DATA,
      OVERLAY_PADLOCK_BIG_SVG_DATA
    };

    for (int i=0; i < ICN_NUM; ++i) {
      if (def_[i]) delete def_[i];
      def_[i] = new Fl_SVG_Image(NULL, data[i]);

      if (!def_[i]) continue;

      if (def_[i]->fail()) {
        delete def_[i];
        def_[i] = NULL;
        continue;
      }
      usericon(def_[i], i);
    }

    update_items(true);
  }

  // set/get icons
  void usericon(Fl_SVG_Image *svg) {
    usericon(svg, ICN_DIR);
    update_items(true);
  }

  void overlay_link(Fl_SVG_Image *svg) {
    usericon(svg, ICN_LNK);
    update_items(true);
  }

  void overlay_lock(Fl_SVG_Image *svg) {
    usericon(svg, ICN_LCK);
    update_items(true);
  }

  Fl_SVG_Image *usericon() const { return icn_[ICN_DIR]; }
  Fl_SVG_Image *overlay_link() const { return icn_[ICN_LNK]; }
  Fl_SVG_Image *overlay_lock() const { return icn_[ICN_LCK]; }

  // set the labelsize on items including root()
  // and adjust the icon size accordingly too
  void item_labelsize(Fl_Fontsize val)
  {
    Fl_Tree::item_labelsize(val);

    if (icn_[ICN_DIR]) {
      icn_[ICN_DIR]->resize(val, val);
      Fl_Tree::usericon(icn_[ICN_DIR]);
    }

    if (icn_[ICN_LNK]) icn_[ICN_LNK]->resize(val, val);
    if (icn_[ICN_LCK]) icn_[ICN_LCK]->resize(val, val);

    update_items(true);
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

