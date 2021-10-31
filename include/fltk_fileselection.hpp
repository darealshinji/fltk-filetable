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

#ifndef fltk_fileselection_hpp
#define fltk_fileselection_hpp

#include <FL/Fl.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Toggle_Button.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Tile.H>
#include <FL/filename.H>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <mntent.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fltk_dirtree.hpp"
#include "fltk_filetable_simple.hpp"
#include "fltk_filetable_extension.hpp"
#include "xdg_dirs.hpp"

#ifdef FLTK_EXPERIMENTAL
#include "fltk_filetable_magic.hpp"
#endif

#define CALLBACK(x) \
  [] (Fl_Widget *o, void *) { \
    static_cast<fileselection *>(o->parent()->parent())->x; \
  }

#define PRINT_DEBUG(fmt, ...)  //printf("DEBUG: " fmt, ##__VA_ARGS__)


// TODO:
// * focus issues
// * make partition/gio stuff optional


namespace fltk
{

template<class T=filetable_simple>
class fileselection : public Fl_Group
{
  // assert if template argument was not a subclass of fltk::filetable_
  static_assert(std::is_base_of<filetable_, T>::value,
                "class was not derived from fltk::filetable_");

public:

  // set file sort behavior
  enum {
    // numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
    SORT_NUMERIC = T::SORT_NUMERIC,

    // case-insensitive sort
    SORT_IGNORE_CASE = T::SORT_IGNORE_CASE,

    // ignore leading dots in filenames ("a, b, .c" instead of ".c, a, b")
    SORT_IGNORE_LEADING_DOT = T::SORT_IGNORE_LEADING_DOT,

    // don't list directories and files separated
    SORT_DIRECTORY_AS_FILE = T::SORT_DIRECTORY_AS_FILE
  };

private:

  // create a subclass with an overloaded double_click_callback() method
  class filetable_sub : public T
  {
  private:
    fileselection *fs_ptr;

    void double_click_callback() override {
      fs_ptr->double_click_callback();
    }

  public:
    filetable_sub(int X, int Y, int W, int H, fileselection *fs) : T(X,Y,W,H,NULL) {
      fs_ptr = fs;
    }
  };

  class addressline : public Fl_Input
  {
  private:
    Fl_Menu_Item menu_[4] = {
      { "Copy selection", 0, copy_selection_cb, this, FL_MENU_INACTIVE },
      { "Copy line",      0, copy_cb,           this, FL_MENU_DIVIDER },
      { "Dismiss" },
      {0}
    };

    int handle(int event)
    {
      switch (event) {
        case FL_UNFOCUS:
          position(0);
          break;

        case FL_PUSH:
          if (Fl::event_button() == FL_RIGHT_MOUSE) {
            // change cursor back to default
            if (active_r() && window()) window()->cursor(FL_CURSOR_DEFAULT);

            // activate "Copy selection" entry if text was selected
            if (position() == mark()) {
              menu_[0].deactivate();
            } else {
              menu_[0].activate();
            }

            // popup menu and do callback
            const Fl_Menu_Item *m = menu_->popup(Fl::event_x(), Fl::event_y());
            if (m && m->callback_) m->do_callback(NULL);
          }
          break;

        default:
          break;
      }
      return Fl_Input::handle(event);
    }

    static void copy_selection_cb(Fl_Widget *, void *v) {
      static_cast<addressline *>(v)->copy(1);
    }

    static void copy_cb(Fl_Widget *, void *v) {
      const char *text = static_cast<addressline *>(v)->value();
      if (text) Fl::copy(text, strlen(text), 1);
    }

  public:
    addressline(int X, int Y, int W, int H) : Fl_Input(X,Y,W,H, NULL) {
      readonly(1);
    }
  };

  std::string data_[xdg::LAST + 1]; // XDG dirs + $HOME
  uint sort_mode_ = SORT_NUMERIC|SORT_IGNORE_CASE|SORT_IGNORE_LEADING_DOT;

  enum { HISTORY_MAX = 10 };
  Fl_Menu_Item mprev_[HISTORY_MAX + 1] = {0};
  Fl_Menu_Item mnext_[HISTORY_MAX + 1] = {0};

  enum { UUID_MAX_SIZE = 36 };

  typedef struct {
    std::string dev;
    std::string label;
    char uuid[UUID_MAX_SIZE + 1];
    long long size;  // bytes
    std::string path;
    fileselection *fs;
  } partition_t;

  std::vector<partition_t> partitions_;

  typedef struct {
    std::string base;
    std::string path;
  } path_t;
  std::vector<path_t> vprev_, vnext_;

  std::string selection_, load_dir_, prev_;
  char *user_ = NULL;

  std::thread *th_ = NULL;
  pid_t pid_ = -1;

  dirtree *tree_;
  filetable_sub *table_;
  addressline *addr_;

  Fl_Group *g_top, *g_bot;
  Fl_Tile *g_main;
  Fl_Menu_Button *b_devices = NULL;
  Fl_Menu_Button *b_places, *b_list_prev, *b_list_next;
  Fl_Button *b_up, *b_reload, *b_cancel, *b_prev, *b_next;
  Fl_Return_Button *b_ok;
  Fl_Toggle_Button *b_hidden;
  Fl_Box *g_top_dummy, *g_bot_dummy;

protected:

  virtual void double_click_callback()
  {
    if (!table_->selected()) {
      // called by other means than a double click (i.e. button)
      // and nothing was yet selected -> quit
      window()->hide();
    } else if (table_->last_clicked_item_isdir()) {
      // double click on directory -> load directory
      load_dir(table_->last_clicked_item());
    } else {
      // set selection and quit
      selection_ = table_->last_clicked_item();
      window()->hide();
    }
  }

private:

  // check if const char * is empty
  inline static bool empty(const char *val) {
    return (!val || *val == 0) ? true : false;
  }

  void tree_callback()
  {
    switch(tree_->callback_reason())
    {
      case FL_TREE_REASON_RESELECTED:
      case FL_TREE_REASON_SELECTED:
        if (!load_dir(tree_->callback_item_path())) {
          tree_->open_callback_item();  // sets the item as "no access"
        }
        break;

      case FL_TREE_REASON_OPENED:
        tree_->open_callback_item();
        break;

      case FL_TREE_REASON_CLOSED:
        tree_->close_callback_item();
        break;

      default:
        break;
    }
  }

  // add entry to history
  void add_to_history(const char *path, std::vector<path_t> &vec)
  {
    if (empty(path)) return;
    path_t entry;
    const char *base = basename(path);
    entry.base = empty(base) ? path : base;
    entry.path = path;
    vec.insert(vec.begin(), entry);
  }

  void update_history()
  {
    int i = 0;

    // keep list at a maximum size
    while (vprev_.size() >= HISTORY_MAX + 1) {
      vprev_.pop_back();
    }

    while (vnext_.size() >= HISTORY_MAX + 1) {
      vnext_.pop_back();
    }

    // clear old menus
    for ( ; i <= HISTORY_MAX; ++i) {
      mprev_[i] = mnext_[i] = {0};
    }

    // update menus

    auto it = vprev_.begin();

    for (i=0; it != vprev_.end(); ++i, ++it) {
      const path_t &p = *it;
      mprev_[i].text = p.base.c_str();
      mprev_[i].user_data_ = reinterpret_cast<void *>(i);
      mprev_[i].callback_ = reinterpret_cast<Fl_Callback *>(previous_cb);
    }

    for (i=0, it=vnext_.begin(); it != vnext_.end(); ++i, ++it) {
      const path_t &p = *it;
      mnext_[i].text = p.base.c_str();
      mnext_[i].user_data_ = reinterpret_cast<void *>(i);
      mnext_[i].callback_ = reinterpret_cast<Fl_Callback *>(next_cb);
    }
  }

  // get available partitions
  std::vector<partition_t> get_partitions()
  {
    FILE *fp;
    DIR *dp;
    struct mntent *mnt;
    struct dirent *dir;
    std::vector<partition_t> devices;
    std::vector<std::string> automount, ignored;


    // read /etc/fstab for ignored automounted partitions

    if ((fp = fopen("/etc/fstab", "r")) == NULL) {
      return {};
    }

    while ((mnt = getmntent(fp)) != NULL) {
      if (mnt->mnt_dir[0] == '/') {
        automount.push_back(mnt->mnt_dir);
        PRINT_DEBUG("automount -> %s\n", mnt->mnt_dir);
      }
    }

    fclose(fp);


    // get disk UUIDs

    if ((dp = opendir("/dev/disk/by-uuid")) == NULL) {
      return {};
    }

    while ((dir = readdir(dp)) != NULL) {
      const char *p = dir->d_name;

      if (p[0] == '.' || strlen(p) > UUID_MAX_SIZE) {
        continue;
      }

      std::string path = "/dev/disk/by-uuid/";
      path += p;

      char *rp = realpath(path.c_str(), NULL);
      if (!rp) continue;

      if (strncmp(rp, "/dev/", 5) != 0) {
        free(rp);
        continue;
      }

      partition_t dev;
      dev.dev = rp;
      strcpy(dev.uuid, p);
      dev.size = 0;
      dev.fs = this;
      PRINT_DEBUG("disk by uuid -> %s\n", p);

      free(rp);
      devices.push_back(dev);
    }

    closedir(dp);


    // get disk labels

    if ((dp = opendir("/dev/disk/by-label")) != NULL) {
      while ((dir = readdir(dp)) != NULL) {
        const char *p = dir->d_name;
        if (p[0] == '.') continue;

        std::string path = "/dev/disk/by-label/";
        path += p;

        char *rp = realpath(path.c_str(), NULL);
        if (!rp) continue;

        if (strncmp(rp, "/dev/", 5) != 0) {
          free(rp);
          continue;
        }

        for (auto &e : devices) {
          if (e.dev.compare(rp) != 0) continue;

          e.label = e.path = p;
          PRINT_DEBUG("label -> %s\n", p);

          // resolve escape sequences

          // label
          for (size_t pos=0; (pos = e.label.find("\\x", pos)) != std::string::npos; ++pos) {
            std::string s = e.label.substr(pos + 2, 2);
            if (s.size() != 2) continue;

            if (s == "0a") {
              // replace newline with "\n" escape sequence
              e.label.replace(pos, 4, "\\n");
            } else {
              std::istringstream iss(s);
              int n = 0;
              iss >> std::hex >> n;
              if (n < 1 || n > 255) continue;
              char c = static_cast<char>(n);

              if (c == '/') {
                // escape slash in a label
                e.label.replace(pos, 4, "\\/");
              } else {
                e.label.replace(pos, 4, 1, static_cast<char>(n));
              }
            }
          }

          // path
          for (size_t pos=0; (pos = e.path.find("\\x", pos)) != std::string::npos; ++pos) {
            std::string s = e.path.substr(pos + 2, 2);
            if (s.size() != 2) continue;

            if (s == "2f") {
              // replace slash with underscore
              e.path.replace(pos, 4, 1, '_');
            } else {
              std::istringstream iss(s);
              int n = 0;
              iss >> std::hex >> n;

              if (n > 0 && n < 256) {
                e.path.replace(pos, 4, 1, static_cast<char>(n));
              }
            }
          }
        }

        free(rp);
      }

      closedir(dp);
    }


    // get "/dev" address of ignored partitions
    if (automount.size() > 0) {
      if ((fp = fopen("/etc/mtab", "r")) == NULL) {
        return {};
      }

      while ((mnt = getmntent(fp)) != NULL) {
        for (const auto &e : automount) {
          if (e.compare(mnt->mnt_dir) == 0) {
            ignored.push_back(mnt->mnt_fsname);
            PRINT_DEBUG("ignored -> %s\n", mnt->mnt_fsname);
          }
        }
      }

      fclose(fp);
    }


    // remove ignored devices
    for (auto it = ignored.begin(); it != ignored.end(); ++it) {
      const std::string &s_ignored = *it;

      for (auto it2 = devices.begin(); it2 != devices.end(); ) {
        partition_t &e = *it2;

        if (s_ignored == e.dev) {
          devices.erase(it2);
          continue;
        }

        it2++;
      }
    }


    // remove leading "/dev/" from device paths
    for (auto &e : devices) {
      e.dev.erase(0, 5);
      e.dev.shrink_to_fit();
    }


    // sort by name
    auto lambda = [] (const partition_t &a, const partition_t &b) {
      const char *label_a = a.label.empty() ? a.dev.c_str() : a.label.c_str();
      const char *label_b = b.label.empty() ? b.dev.c_str() : b.label.c_str();
      return strcasecmp(label_a, label_b) < 0;
    };
    std::sort(devices.begin(), devices.end(), lambda);


    // get partition sizes
    for (partition_t &e : devices) {
      std::string s = e.dev;
      PRINT_DEBUG("dev -> %s\n", e.dev.c_str());

      while (isdigit(s.back())) s.pop_back();
      if (s.empty()) continue;

      std::string line;
      std::string file = "/sys/block/" + s + "/" + e.dev + "/size";
      std::ifstream ifs(file);

      if (ifs.is_open() && std::getline(ifs, line) && !line.empty()) {
        // Linux always considers sectors to be 512 bytes long independently of the devices real block size.
        // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/linux/types.h?id=v4.4-rc6#n121
        e.size = atoll(line.c_str()) * 512;
      }
    }

    return devices;
  }

  // add partitions to menu
  void add_partitions()
  {
    if (!user_ || !b_devices) return;

    b_devices->deactivate();
    b_devices->clear();
    partitions_.clear();

    partitions_ = get_partitions();

    for (size_t i = 0; i < partitions_.size(); ++i) {
      partition_t &e = partitions_.at(i);

      std::string s = "/media/";
      s += user_;
      s += '/';

      if (e.path.empty()) {
        e.path = s + e.uuid;
      } else {
        e.path.insert(0, s);
      }

      std::string label = s = e.label.empty() ? e.dev : e.label;

      s += " [";
      s += table_->human_readable_filesize_iec(e.size, false);
      s += " \\/ ";
      s += table_->human_readable_filesize_iec(e.size, true);
      s += ']';
      PRINT_DEBUG("devices -> %s\n", s.c_str());

      std::string unmount = "Unmount/" + label;
      int flags = FL_MENU_INACTIVE;

      if (fl_filename_isdir(e.path.c_str())) {
        s.insert(0, "*");
        flags = 0;
      }

      void *arg = reinterpret_cast<void *>(&partitions_.at(i));
      b_devices->add(unmount.c_str(), 0, unmount_cb, arg, flags);
      b_devices->add(s.c_str(), 0, partition_cb, arg);
    }

    if (partitions_.size() > 0) b_devices->activate();
  }

  // load currently open directory in tree_
  // or load the directory that was set with set_dir()
  // in tree_ and table_
  bool load_open_directory(bool keep_history=false)
  {
    // update menu first
    add_partitions();

    // a directory was set with set_dir()
    if (!load_dir_.empty()) {
      // will set open_directory_ on success
      bool rv = table_->load_dir(load_dir_.c_str());
      load_dir_.clear();

      if (!rv) {
        addr_->value(NULL);
        tree_->close_root();
        return false;
      }
    }

    const char *dir = table_->open_directory();
    addr_->value(dir);
    tree_->close_root();

    if (!dir) return false;

    // update tree_
    bool rv = tree_->load_dir(dir);
    if (rv) tree_->calc_tree();

    if (table_->open_directory_is_root()) {
      b_up->deactivate();
      tree_->hposition(0);
      tree_->vposition(0);
      tree_->set_item_focus(tree_->root());
    } else {
      b_up->activate();
      // scroll down tree
      if (rv) {
        auto item = tree_->find_item(dir);
        tree_->show_item(item);
        tree_->set_item_focus(item);
      }
    }

    if (vprev_.size() == 0 || prev_.compare(dir) == 0) {
      return rv;
    }

    if (keep_history) return rv;

    // add previous directory to the history list

    b_prev->activate();
    b_list_prev->activate();

    b_next->deactivate();
    b_list_next->deactivate();
    vnext_.clear();

    const std::string &s = (*vprev_.begin()).path;

    // delete duplicates and current directory
    for (auto it = vprev_.begin()+1; it != vprev_.end(); ++it) {
      const path_t &p = *it;

      if (p.path == s || p.path.compare(dir) == 0) {
        vprev_.erase(it);
        it--;
      }
    }

    update_history();

    return rv;
  }

  // move up the tree until we can open a directory, otherwise open root
  void move_up_tree(const char *dirname=NULL)
  {
    if (!dirname) dirname = table_->open_directory();
    if (!dirname) return;

    std::string s = filetable_::simplify_directory_path(dirname);
    char * const path = const_cast<char *>(s.data());
    char *p = path + s.length();

    while (p-- != path) {
      if (*p == '/') {
        *p = 0;
        if (table_->load_dir(path)) break;
      }
    }

    if (*path == 0) {
      // moved all the way up to the root directory
      tree_->close_root();
      table_->load_dir("/");
    } else {
      // close path
      tree_->close(path, 0);
    }
  }

  void toggle_hidden() {
    show_hidden(show_hidden() ^ 1);
    refresh();
  }

  void kill_pid()
  {
    if (pid_ > getpid() && kill(pid_, 0) == 0) {
      kill(pid_, 1);
      pid_ = -1;
    }
  }

  void stop_thread()
  {
    if (th_) {
      if (th_->joinable()) th_->join();
      delete th_;
    }
    th_ = NULL;
  }

  // thread waiting for child to exit and return status
  static void wait_for_child_process(pid_t pid, fileselection *o, const char *path)
  {
    int status = 0;
    int rv = -1;

    if (waitpid(pid, &status, 0) > 0 && WIFEXITED(status)) {
      rv = WEXITSTATUS(status);
    }

    if (rv != 0) return;

    // successfully mounted
    Fl::lock();

    if (empty(path))  {
      o->refresh();
    } else {
      o->load_dir(path);
    }

    Fl::unlock();
  }

  void load_partition(partition_t *part)
  {
    kill_pid();
    if (!part) return;

    const char *path = part->path.c_str();

    if (fl_filename_isdir(path)) {
      load_dir(path);
      return;
    }

    // fork process
    if ((pid_ = fork()) == 0) {
      // redirect output instead of using close()
      // https://stackoverflow.com/a/33268567/5687704
      FILE *fp = freopen("/dev/null", "w", stdout);
      fp = freopen("/dev/null", "w", stderr);
      static_cast<void>(fp);  // silence -Wunused-result

      execlp("gio", "gio", "mount", "-f", "-a", "-d", part->uuid, NULL);
      _exit(127);
    }

    // wait for child process in a separate thread
    if (pid_ > 0) {
      th_ = new std::thread(wait_for_child_process, pid_, this, path);
    }
  }

  void unmount_partition(partition_t *part)
  {
    kill_pid();
    if (!part) return;

    const char *path = part->path.c_str();

    if (!fl_filename_isdir(path)) return;

    // fork process
    if ((pid_ = fork()) == 0) {
      // redirect output instead of using close()
      // https://stackoverflow.com/a/33268567/5687704
      FILE *fp = freopen("/dev/null", "w", stdout);
      fp = freopen("/dev/null", "w", stderr);
      static_cast<void>(fp);  // silence -Wunused-result

      execlp("gio", "gio", "mount", "-f", "-a", "-u", path, NULL);
      _exit(127);
    }

    // wait for child process in a separate thread
    if (pid_ > 0) {
      th_ = new std::thread(wait_for_child_process, pid_, this, static_cast<const char *>(NULL));
    }
  }

  static void previous_cb(Fl_Widget *o, long n) {
    static_cast<fileselection *>(o->parent()->parent())->history(n);
  }

  static void next_cb(Fl_Widget *o, long n) {
    static_cast<fileselection *>(o->parent()->parent())->history(n, true);
  }

  static void places_cb(Fl_Widget *o, void *v) {
    static_cast<fileselection *>(o->parent()->parent())->load_dir(static_cast<const char *>(v));
  }

  static void partition_cb(Fl_Widget *o, void *v) {
    static_cast<fileselection *>(o->parent()->parent())->load_partition(static_cast<partition_t *>(v));
  }

  static void unmount_cb(Fl_Widget *o, void *v) {
    static_cast<fileselection *>(o->parent()->parent())->unmount_partition(static_cast<partition_t *>(v));
  }

  int handle(int event)
  {
    if (event == FL_NO_EVENT) return 1;

    // OK button activation
    if (table_->selected()) {
      b_ok->activate();
    } else {
      b_ok->deactivate();
    }

    return Fl_Group::handle(event);
  }

public:

  // c'tor
  fileselection(int X, int Y, int W, int H, int spacing=4)
  : Fl_Group(X,Y,W,H, NULL)
  {
    vprev_.reserve(HISTORY_MAX);
    vnext_.reserve(HISTORY_MAX);

    if (spacing < 0) spacing = 0;
    if (spacing > 24) spacing = 24;

    // get username
    passwd *pw = getpwuid(geteuid());

    if (pw && pw->pw_name) {
      user_ = pw->pw_name;
    } else {
      user_ = getenv("USER");
    }

    // top buttons
    const int addr_h = FL_NORMAL_SIZE + 12;
    const int but_h = 42;

    g_top = new Fl_Group(X, Y, W, addr_h + but_h + spacing*2);
    {
      addr_ = new addressline(X, Y, W, addr_h);
      const int but_y = addr_->y() + addr_->h() + spacing;

      b_places = new Fl_Menu_Button(X, but_y, 72, but_h, "Places");

      b_devices = new Fl_Menu_Button(b_places->x() + b_places->w(), but_y, 82, but_h, "Devices");
      b_devices->deactivate();

      b_up = new Fl_Button(b_devices->x() + b_devices->w(), but_y, but_h, but_h, "@+78->");
      b_up->callback(CALLBACK(dir_up()));

      b_reload = new Fl_Button(b_up->x() + b_up->w(), but_y, but_h, but_h, "@+4reload");
      b_reload->callback(CALLBACK(refresh()));

      b_hidden = new Fl_Toggle_Button(b_reload->x() + b_reload->w(), but_y, 60, but_h, "Show\nhidden");
      b_hidden->callback(CALLBACK(toggle_hidden()));

      b_prev = new Fl_Button(b_hidden->x() + b_hidden->w(), but_y, but_h, but_h, "@<|");
      b_prev->callback(previous_cb, 0);
      b_prev->deactivate();

      b_list_prev = new Fl_Menu_Button(b_prev->x() + b_prev->w(), but_y, 21, but_h);
      b_list_prev->menu(reinterpret_cast<Fl_Menu_Item *>(&mprev_));
      b_list_prev->deactivate();

      b_next = new Fl_Button(b_list_prev->x() + b_list_prev->w(), but_y, but_h, but_h, "@|>");
      b_next->callback(next_cb, 0);
      b_next->deactivate();

      b_list_next = new Fl_Menu_Button(b_next->x() + b_next->w(), but_y, 21, but_h);
      b_list_next->menu(reinterpret_cast<Fl_Menu_Item *>(&mnext_));
      b_list_next->deactivate();

      g_top_dummy = new Fl_Box(b_list_prev->x() + b_list_prev->w(), but_y, 1, 1);
    }
    g_top->end();
    g_top->resizable(g_top_dummy);

    // main part with tree and filetable
    const int g_bot_h = 34 + spacing;
    const int main_h = H - g_top->h() - g_bot_h;

    g_main = new Fl_Tile(X, Y + g_top->h(), W, main_h);
    {
      tree_ = new dirtree(X, Y + g_top->h(), W/4, main_h);
      tree_->callback(CALLBACK(tree_callback()));
      tree_->selection_color(FL_WHITE);

      table_ = new filetable_sub(X + W/4, Y + g_top->h(), W - W/4, main_h, this);

      //tree_->selection_color(table_->selection_color());
    }
    g_main->end();

    // OK/Cancel buttons
    const int bot_y = g_main->y() + g_main->h();

    g_bot = new Fl_Group(X, bot_y, W, g_bot_h);
    {
      b_ok = new Fl_Return_Button(X + W - 90, bot_y + spacing, 90, g_bot_h - spacing, "OK");
      b_ok->callback(CALLBACK(double_click_callback()));
      b_ok->deactivate();

      b_cancel = new Fl_Button(X + W - 180 - spacing, bot_y + spacing, 90, g_bot_h - spacing, "Cancel");
      b_cancel->callback(CALLBACK(cancel()));

      g_bot_dummy = new Fl_Box(b_cancel->x() - 1, bot_y + spacing, 1, 1);
    }
    g_bot->end();
    g_bot->resizable(g_bot_dummy);


    // create "places" menu entries

    b_places->add("\\/", 0, places_cb, const_cast<char *>("/"));

    xdg xdg;
    const int rv = xdg.get(true, true);

    // $HOME
    if (rv != -1) {
      //data_[0] = { this, xdg.home() };
      data_[0] = xdg.home();
      b_places->add("Home", 0, places_cb, const_cast<char *>(data_[0].c_str()));
    }

    if (rv > 0) {
      // XDG_DESKTOP_DIR
      if (xdg.dir(xdg::DESKTOP) && xdg.basename(xdg::DESKTOP)) {
        data_[1] = xdg.dir(xdg::DESKTOP);
        b_places->add(xdg.basename(xdg::DESKTOP), 0, places_cb, const_cast<char *>(data_[1].c_str()));
      }

      // add remaining XDG directories
      std::vector<int> xdg_vec_;
      xdg_vec_.reserve(xdg::LAST - 1);

      // xdg::DESKTOP == 0, so start with 1
      for (int i = 1; i < xdg::LAST; ++i) {
        if (xdg.dir(i) && xdg.basename(i)) {
          xdg_vec_.push_back(i);
        }
      }

      if (xdg_vec_.size() > 0) {
        auto lambda = [xdg] (const int a, const int b) {
          return strcasecmp(xdg.basename(a), xdg.basename(b)) < 0;
        };
        std::sort(xdg_vec_.begin(), xdg_vec_.end(), lambda);

        int i = 2;

        for (const int n : xdg_vec_) {
          data_[i] = xdg.dir(n);
          b_places->add(xdg.basename(n), 0, places_cb, const_cast<char *>(data_[i].c_str()));
          i++;
        }
      }
    }

    //add_partitions();
    end();
    resizable(g_main);
  }

  // d'tor
  virtual ~fileselection()
  {
    kill_pid();
    stop_thread();
    if (tree_) delete tree_;
    if (table_) delete table_;
  }

  void cancel() {
    window()->hide();
  }

  // set directory without loading it; use refresh() to load it
  void set_dir(const char *dirname) {
    load_dir_.clear();
    if (!empty(dirname)) load_dir_ = dirname;
  }

  void set_dir(const std::string &dirname) {
    set_dir(dirname.c_str());
  }

  // calls load_dir() on table_ and tree_
  bool load_dir(const char *dirname)
  {
    std::string s;

    if (table_->open_directory()) {
      s = table_->open_directory();
    }

    if (!table_->load_dir(dirname)) {
      if (dirname[0] == '/') {
        move_up_tree(dirname);
      } else {
        add_partitions();
        return false;
      }
    }

    add_to_history(s.c_str(), vprev_);

    return load_open_directory();
  }

  bool load_dir(const std::string &dirname) {
    return load_dir(dirname.c_str());
  }

  // move directory up (table_ and tree_)
  bool dir_up()
  {
    std::string s;

    if (table_->open_directory()) {
      s = table_->open_directory();
    }

    if (!table_->dir_up()) move_up_tree();
    add_to_history(s.c_str(), vprev_);

    return load_open_directory();
  }

  // go back/forth in history
  void history(long num, bool next=false)
  {
    std::vector<path_t> *v1, *v2;
    Fl_Button *b1, *b2;
    Fl_Menu_Button *blist1, *blist2;

    v1 = next ? &vnext_ : &vprev_;
    if (v1->size() == 0) return;

    if (next) {
      v2 = &vprev_;
      b1 = b_next;
      b2 = b_prev;
      blist1 = b_list_next;
      blist2 = b_list_prev;
    } else {
      v2 = &vnext_;
      b1 = b_prev;
      b2 = b_next;
      blist1 = b_list_prev;
      blist2 = b_list_next;
    }

    add_to_history(table_->open_directory(), *v2);

    for (long i=0; i < num; ++i) {
      v2->insert(v2->begin(), v1->at(i));
    }

    path_t p = v1->at(num);
    std::string curr = p.path;

    v1->erase(v1->begin(), v1->begin() + num + 1);

    if (v1->size() == 0) {
      b1->deactivate();
      blist1->deactivate();
    }

    b2->activate();
    blist2->activate();
    update_history();

    if (table_->load_dir(curr.c_str())) {
      load_open_directory(true);
    }
  }

  // reload directory
  bool refresh()
  {
    std::string s;

    if (table_->open_directory()) {
      s = table_->open_directory();
    }

    if (!table_->refresh()) {
      move_up_tree();
      add_to_history(s.c_str(), vprev_);
    }

    return load_open_directory();
  }

  // alternative to refresh()
  bool load_dir() { return refresh(); }

  // returns selection
  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }

  void add_filter(const char *str) {table_->add_filter(str);}
  void add_filter_list(const char *list, const char *delim) {table_->add_filter_list(list, delim);}

  void load_default_icons() {tree_->load_default_icons(); table_->load_default_icons();}

  void labelsize(int i) {tree_->labelsize(i); table_->labelsize(i);}
  int labelsize() const {return table_->labelsize();}

  void color(Fl_Color c) {tree_->color(c); tree_->selection_color(c); table_->color(c);}
  Fl_Color color() const {return table_->color();}

  void selection_color(Fl_Color c) {/*tree_->selection_color(c);*/ table_->selection_color(c);}
  Fl_Color selection_color() const {return table_->selection_color();}

  void show_hidden(bool b) {tree_->show_hidden(b); table_->show_hidden(b); b_hidden->value(b);}
  bool show_hidden() const {return table_->show_hidden();}

  void double_click_timeout(double d) {table_->double_click_timeout(d);}
  double double_click_timeout() const {return table_->double_click_timeout();}

  void blend_w(int i) {table_->blend_w(i);}
  int blend_w() const {return table_->blend_w();}

  void autowidth_padding(int i) {table_->autowidth_padding(i);}
  int autowidth_padding() const {return table_->autowidth_padding();}

  void autowidth_max(int i) {table_->autowidth_max(i);}
  int autowidth_max() const {return table_->autowidth_max();}

  void label_header(filetable_::ECol idx, const char *l) {table_->label_header(idx, l);}
  const char *label_header(filetable_::ECol idx) const {return table_->label_header(idx);}

  void filesize_label(filetable_::EStrSize idx, const char *l) {table_->filesize_label(idx, l);}
  const char *filesize_label(filetable_::EStrSize idx) const {return table_->filesize_label(idx);}

  void sort_mode(uint u) {tree_->sort_mode(u); table_->sort_mode(u);}
  uint sort_mode() const {return table_->sort_mode();}

  void use_iec(bool b) { table_->use_iec(b); }
  bool use_iec() const { return table_->use_iec(); }
};

} // namespace fltk

#endif  // fltk_fileselection_hpp
