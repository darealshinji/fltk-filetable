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

#ifndef filetable__hpp
#define filetable__hpp

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_SVG_Image.H>

#include <algorithm>
#include <string>
#include <vector>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// make it possible to set these?
#define STR_NAME       "Name"
#define STR_SIZE       "Size"
#define STR_LAST_MOD   "Last modified"
#define STR_BYTES      "bytes"
#define STR_KBYTES     "KiB"
#define STR_MBYTES     "MiB"
#define STR_GBYTES     "GiB"
#define STR_ELEMENTS   "elements"


namespace fltk
{

// forward declaration
class dirtree;

// single row of columns
class filetable_Row
{
private:
  char _type;
  bool _lnk, _sel;

public:
  // columns
  enum {
    COL_NAME = 0,
    COL_SIZE = 1,
    COL_LAST_MOD = 2,
    COL_MAX = 3
  };

  char *cols[COL_MAX];
  Fl_SVG_Image *svg;
  long bytes, last_mod;

  filetable_Row()
  {
    _type = 0;
    _lnk = false;
    _sel = false;
    bytes = 0;
    last_mod = 0;
    svg = NULL;

    for (int i = 0; i < COL_MAX; ++i) {
      cols[i] = NULL;
    }
  }

  virtual ~filetable_Row() {}

  bool selected() const { return _sel; }
  void setselected() { _sel = true; }
  void setunselected() { _sel = false; }

  char type() const { return _type; }
  void type(char c) { _type = c; }

  bool isdir() { return (_type == 'D'); }

  bool islnk() const { return _lnk; }
  void setlnk() { _lnk = true; }
};

// Sort class to handle sorting column using std::sort
class filetable_Sort {
  int _col, _reverse;

public:
  filetable_Sort(int col, int reverse) {
    _col = col;
    _reverse = reverse;
  }

  bool operator() (filetable_Row a, filetable_Row b) {
    const char *ap = (_col < filetable_Row::COL_MAX) ? a.cols[_col] : "";
    const char *bp = (_col < filetable_Row::COL_MAX) ? b.cols[_col] : "";

    if (a.isdir() != b.isdir()) {
      return (a.isdir() && !b.isdir());
    }
    else if (_col == filetable_Row::COL_SIZE) {
      long as = a.bytes;
      long bs = b.bytes;
      return _reverse ? as < bs : bs < as;
    }
    else if (_col == filetable_Row::COL_LAST_MOD) {
      long am = a.last_mod;
      long bm = b.last_mod;
      return _reverse ? am < bm : bm < am;
    }
    else if (isdigit(*ap) && isdigit(*bp)) {
      // Numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
      long av = atol(ap);
      long bv = atol(bp);

      // if numbers are the same, continue to alphabetic sort
      if (av != bv) {
        return _reverse ? bv < av : av < bv;
      }
    }

    // ignore leading dots in filenames ("bin, .config, data, .local"
    // instead of ".config, .local, bin, data")
    if (_col == filetable_Row::COL_NAME) {
      if (*ap == '.') { ap++; }
      if (*bp == '.') { bp++; }
    }

    // Alphabetic sort
    return _reverse ? strcasecmp(ap, bp) > 0 : strcasecmp(ap, bp) < 0;
  }
};

class filetable_ : public Fl_Table_Row
{
private:
  friend class dirtree;

public:
  // columns
  enum {
    COL_NAME = filetable_Row::COL_NAME,
    COL_SIZE = filetable_Row::COL_SIZE,
    COL_LAST_MOD = filetable_Row::COL_LAST_MOD,
    COL_MAX = filetable_Row::COL_MAX
  };

private:
  int sort_reverse_;
  int sort_last_row_;
  double dc_timeout_;
  int autowidth_padding_;
  int autowidth_max_;
  bool show_hidden_;
  std::vector<std::string> filter_list_;

  Fl_SVG_Image *icon_square_[2];
  const char *label_header_[COL_MAX];

  // extra width for icons
  int col_name_extra_w() {
    return labelsize() + 10;
  }

  // Callback whenever someone clicks on different parts of the table
  static void event_callback(Fl_Widget *o, void *) {
    dynamic_cast<filetable_ *>(o)->event_callback2();
  }

  // callback for table events
  void event_callback2()
  {
    //int row = callback_row();  // unused
    int col = callback_col();

    if (callback_context() == CONTEXT_COL_HEADER) {
      if (Fl::event() == FL_RELEASE && Fl::event_button() == 1) {
        if (sort_last_row_ == col) {
          // Click same column? Toggle sort
          sort_reverse_ ^= 1;
        } else {
          // Click diff column? Up sort
          sort_reverse_ = 0;
        }
        sort_column(col, sort_reverse_);
        sort_last_row_ = col;
      }
      last_row_clicked_ = -1;
      return;
    }

    //last_row_clicked_ = row;
  }

  static bool within_double_click_timelimit_;

  static void reset_timelimit_cb(void *) {
    within_double_click_timelimit_ = false;
  }

  // unused!!
  Fl_Callback *double_click_callback_;

protected:
  std::string open_directory_;
  std::string selection_;
  std::vector<filetable_Row> rowdata_;
  int last_row_clicked_;
  Fl_SVG_Image *svg_link_;
  Fl_SVG_Image *svg_overlay_;

  // Handle drawing all cells in table
  void draw_cell(TableContext context, int R=0, int C=0, int X=0, int Y=0, int W=0, int H=0)
  {
    if (C >= COL_MAX) {
      return;
    }

    switch (context) {
      case CONTEXT_COL_HEADER:
        fl_push_clip(X, Y, W, H); {
          fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND_COLOR);
          fl_font(labelfont(), labelsize());
          fl_color(FL_BLACK);
          fl_draw(label_header_[C], X + 2, Y, W, H, FL_ALIGN_LEFT, NULL, 0);  // +2=pad left
          if (C == sort_last_row_) {
            draw_sort_arrow(X, Y, W, H);
          }
        }
        fl_pop_clip();
        break;

      case CONTEXT_CELL:
        fl_push_clip(X, Y, W, H); {
          Fl_Color bgcol = FL_WHITE;
          Fl_Color textcol = FL_BLACK;
          Fl_Align al = (C == COL_SIZE) ? FL_ALIGN_RIGHT : FL_ALIGN_LEFT;
          Fl_SVG_Image *svg = icon_square_[0];
          int fw = 0;

          if (row_selected(R)) {
            svg = icon_square_[1];
            bgcol = selection_color();
            textcol = FL_WHITE;
          }

          if (C != COL_LAST_MOD) {
            int fh = 0;
            fl_measure(rowdata_.at(R).cols[C], fw, fh, 0);

            if (C == COL_SIZE && fw > W) {
              al = FL_ALIGN_LEFT;
            }
          }

          // Bg color
          fl_rectf(X, Y, W, H, bgcol);

          // Icon
          if (C == COL_NAME) {
            if (!rowdata_.at(R).svg) {
              rowdata_.at(R).svg = icon(rowdata_.at(R));
            }

            if (rowdata_.at(R).svg) {
              rowdata_.at(R).svg->draw(X + 2, Y + 2);
            }

            if (rowdata_.at(R).islnk() && svg_link_) {
              svg_link_->draw(X + 2, Y + 2);
            }

            X += col_name_extra_w();
            fw += col_name_extra_w();
          }

          // Text
          fl_font(labelfont(), labelsize());
          fl_color(textcol);
          fl_draw(rowdata_.at(R).cols[C], X, Y + 2, W, H - 2, al, NULL, 0);

          // blend over long text at the end of name column
          if (C == COL_NAME && svg && fw > W) {
            if (svg->h() != H && svg->w() != H) {
              svg->resize(H, H);
              col_resize_min(col_name_extra_w() + svg->w());
            }
            svg->draw(W + col_name_extra_w() - svg->w(), Y);
          }
        }
        fl_pop_clip();
        break;

      default:
        break;
    }
  }

  // Sort a column up or down
  void sort_column(int col, int reverse=0)
  {
    // save current selection state in rowdata_
    for (int i = 0; i < rows(); ++i) {
      if (row_selected(i)) {
        rowdata_.at(i).setselected();
      } else {
        rowdata_.at(i).setunselected();
      }
    }

    // sort data while preserving order between equal elements
    std::stable_sort(rowdata_.begin(), rowdata_.end(), filetable_Sort(col, reverse));

    // update table row selection from rowdata_
    for (int i = 0; i < rows(); ++i) {
      select_row(i, rowdata_.at(i).selected());
    }

    redraw();
  }

  void draw_sort_arrow(int X,int Y,int W,int H)
  {
    int xlft = X + (W - 6) - 8;
    int xctr = X + (W - 6) - 4;
    int xrit = X + (W - 6) - 0;
    int ytop = Y + (H / 2) - 4;
    int ybot = Y + (H / 2) + 4;

    if (sort_reverse_) {
      // Engraved down arrow
      fl_color(FL_WHITE);
      fl_line(xrit, ytop, xctr, ybot);
      fl_color(41); // dark gray
      fl_line(xlft, ytop, xrit, ytop);
      fl_line(xlft, ytop, xctr, ybot);
    } else {
      // Engraved up arrow
      fl_color(FL_WHITE);
      fl_line(xrit, ybot, xctr, ytop);
      fl_line(xrit, ybot, xlft, ybot);
      fl_color(41); // dark gray
      fl_line(xlft, ybot, xctr, ytop);
    }
  }

  int handle(int event)
  {
    int ret = Fl_Table_Row::handle(event);
    int row = callback_row();

    if (callback_context() == CONTEXT_CELL) {
      if (event == FL_RELEASE) {
        if (dc_timeout_ == 0) {   // double click disabled
          double_click_callback();
        } else if (last_row_clicked_ == row && within_double_click_timelimit_) {  // double click
          Fl::remove_timeout(reset_timelimit_cb);
          within_double_click_timelimit_ = false;
          double_click_callback();
        } else {
          Fl::remove_timeout(reset_timelimit_cb);
          within_double_click_timelimit_ = true;
          Fl::add_timeout(dc_timeout_, reset_timelimit_cb);
        }
        last_row_clicked_ = row;
      }
    } else {
      // not clicked on a cell -> remove timout
      Fl::remove_timeout(reset_timelimit_cb);
      within_double_click_timelimit_ = false;
    }

    //redraw();

    return ret;
  }

  // set a callback to handle double-clicks on entries
  void double_click_callback()
  {
    const char *name = rowdata_[last_row_clicked_].cols[COL_NAME];
    std::string dir;

    if (!open_directory_.empty()) {
      dir = open_directory_;

      if (dir.back() != '/') {
        dir.push_back('/');
      }
      dir += name;
      name = dir.c_str();
    }

    if (rowdata_[last_row_clicked_].isdir()) {
      if (!load_dir(name)) {
        // refresh current directory if we cannot access
        load_dir(NULL);
      }
    } else {
      selection_ = name;
      window()->hide();
    }
  }

  // set icon for row entry; here you can i.e. set icons dependant on
  // the magic bytes or simply the file extension; set NULL for no icon
  virtual Fl_SVG_Image *icon(filetable_Row) { return NULL; }

  // similar to printf() buf returns an allocated string
  char *printf_alloc(const char *fmt, ...)
  {
    char *buf;
    va_list args, args2;

    va_start(args, fmt);
    va_copy(args2, args);
    buf = reinterpret_cast<char *>(malloc(vsnprintf(nullptr, 0, fmt, args2) + 1));
    va_end(args2);
    vsprintf(buf, fmt, args);
    va_end(args);

    return buf;
  }

  // return values must be free()d later
  char *human_readable_filesize(long bytes)
  {
    const int KiBYTES = 1024;
    const int MiBYTES = 1024 * KiBYTES;
    const int GiBYTES = 1024 * MiBYTES;

    if (bytes > GiBYTES) {
      return printf_alloc("%.1Lf " STR_GBYTES " ", static_cast<long double>(bytes) / GiBYTES);
    } else if (bytes > MiBYTES) {
      return printf_alloc("%.1Lf " STR_MBYTES " ", static_cast<long double>(bytes) / MiBYTES);
    } else if (bytes > KiBYTES) {
      return printf_alloc("%.1Lf " STR_KBYTES " ", static_cast<long double>(bytes) / KiBYTES);
    }

    return printf_alloc("%ld " STR_BYTES " ", bytes);
  }

  char *count_dir_entries(long &count, const char *directory)
  {
    const char *unknown = "?? " STR_ELEMENTS " ";
    char *buf;

    std::string path = open_directory_;
    path.push_back('/');
    path += directory;

    DIR *dirp = opendir(path.c_str());

    if (!dirp) {
      count = -1;
      return strdup(unknown);
    }

    count = 0;
    errno = 0;

    while (readdir(dirp)) {
      count++;
    }

    count -= 2;  // don't count the '.' and '..' entries

    if (errno != 0 || count < 0) {
      buf = strdup(unknown);
      count = -1;
    } else {
      buf = printf_alloc("%ld " STR_ELEMENTS " ", count);
    }

    closedir(dirp);

    return buf;
  }

  // resolve '.' and '..' but don't follow symlinks;
  // returns allocated string (must be free()d later) or NULL on error
  static char *simplify_directory_path(const char *path)
  {
    std::vector<std::string> vec;
    std::string in, out;

    if (!path || path[0] == 0) {
      return NULL;
    }

    in = path;

    // strip trailing '/'
    while (in.back() == '/') {
      in.pop_back();
    }

    // path was only slashes
    if (in.empty()) {
      return strdup("/");
    }

    if (in == ".") {
      return get_current_dir_name();
    }

    // prepend current directory
    if (in[0] != '/') {
      char *cdn = get_current_dir_name();

      if (!cdn) {
        return NULL;
      }
      in.insert(0, 1, '/');
      in.insert(0, cdn);
    }

    // insert trailing '/' (it's needed)
    in.push_back('/');

    // check if the path even needs to be simplified
    if (in.find("//") == std::string::npos &&
        in.find("/./") == std::string::npos &&
        in.find("/../") == std::string::npos)
    {
      in.pop_back();  // remove trailing '/'
      return strdup(in.c_str());
    }

    for (size_t i = 0; i < in.size(); ++i) {
      std::string dir;

      while (in[i] == '/') {
        i++;
      }

      while (in[i] != '/') {
        dir.push_back(in[i]);
        i++;
      }

      if (dir == "..") {
        // remove last entry or ignore
        if (vec.size() > 0) vec.pop_back();
      } else if (dir == ".") {
        // ignore '.' entry
        continue;
      } else if (dir.size() > 0) {
        vec.push_back(dir);
      }
    }

    // path was resolved to "/"
    if (vec.size() == 0) {
      return strdup("/");
    }

    for (const auto elem : vec) {
      out.push_back('/');
      out += elem;
    }

    return strdup(out.c_str());
  }

  // returns true if the current filename is accepted by the
  // filename filter (always returns true if no filter was set)
  virtual bool filter_show_entry(const char *filename)
  {
    size_t len;

    if (filter_list_.empty()) {
      return true;
    }

    if (!filename || (len = strlen(filename)) == 0) {
      return false;
    }

    for (std::string & ext : filter_list_) {
      if (ext.size() >= len) {
        continue;
      }

      if (strcasecmp(filename + (len - ext.size()), ext.c_str()) == 0) {
        return true;
      }
    }

    return false;
  }

  // automatically set column widths to data
  void autowidth()
  {
    int w, h;

    fl_font(labelfont(), labelsize());

    for (int c = 0; c < COL_MAX; ++c) {
      // consider extra space for icons
      int extra = (c == COL_NAME) ? col_name_extra_w() : 0;

      // header
      w = h = 0;
      fl_measure(label_header_[c], w, h, 0);
      col_width(c, w + autowidth_padding_);

      // rows
      for (size_t r = 0; r < rowdata_.size(); ++r) {
        w = h = 0;
        fl_measure(rowdata_.at(r).cols[c], w, h, 0);
        w += autowidth_padding_ + extra;

        if (autowidth_max_ > col_resize_min() && w >= autowidth_max_) {
          // set to maximum autowidth and exit for-loop
          col_width(c, autowidth_max_);
          break;
        }

        if (w > col_width(c)) {
          col_width(c, w);
        }
      }

      // set to minimum size if needed
      if (col_width(c) < col_resize_min()) {
        col_width(c, col_resize_min());
      }
    }

    table_resized();
    redraw();
  }

public:
  filetable_(int X, int Y, int W, int H, const char *L=NULL) : Fl_Table_Row(X, Y, W, H, L)
  {
    label_header_[COL_NAME] = STR_NAME;
    label_header_[COL_SIZE] = STR_SIZE;
    label_header_[COL_LAST_MOD] = STR_LAST_MOD;

    sort_reverse_ = 0;
    sort_last_row_ = 0;
    last_row_clicked_ = -1;
    dc_timeout_ = 0.8;
    autowidth_padding_ = 20;
    autowidth_max_ = 0;
    show_hidden_ = false;
    svg_link_ = NULL;

    // icon_square_[0]

    const char *svg_data1 = \
      "<svg width='16' height='16'>" \
       "<defs>" \
        "<linearGradient id='a' x1='16' x2='0' y1='8' y2='8'>" \
         "<stop stop-color='#fff' offset='0'/>" \
         "<stop stop-color='#fff' stop-opacity='0' offset='1'/>" \
        "</linearGradient>" \
       "</defs>" \
       "<rect x='0' y='0' width='16' height='16' fill='url(#a)'/>" \
      "</svg>";

    color(FL_WHITE);
    icon_square_[0] = new Fl_SVG_Image(NULL, svg_data1);

    if (icon_square_[0]->fail()) {
      delete icon_square_[0];
      icon_square_[0] = NULL;
    } else {
      icon_square_[0]->proportional = false;
    }

    // icon_square_[1]

    const char *svg_data2 = \
      "<svg width='16' height='16'>" \
       "<defs>" \
        "<linearGradient id='a' x1='16' x2='0' y1='8' y2='8'>" \
         "<stop stop-color='#4169E1' offset='0'/>" \
         "<stop stop-color='#4169E1' stop-opacity='0' offset='1'/>" \
        "</linearGradient>" \
       "</defs>" \
       "<rect x='0' y='0' width='16' height='16' fill='url(#a)'/>" \
      "</svg>";

    selection_color(fl_rgb_color(0x41, 0x69, 0xE1));
    icon_square_[1] = new Fl_SVG_Image(NULL, svg_data2);

    if (icon_square_[1]->fail()) {
      delete icon_square_[1];
      icon_square_[1] = NULL;
    } else {
      icon_square_[1]->proportional = false;
    }

    col_header(1);
    col_resize(1);
    col_resize_min(col_name_extra_w() + labelsize());

    when(FL_WHEN_RELEASE);

    end();
    callback(event_callback, NULL);
  }

  ~filetable_()
  {
    clear();

    if (icon_square_[0]) {
      delete icon_square_[0];
    }

    if (icon_square_[1]) {
      delete icon_square_[1];
    }
  }

  void clear()
  {
    Fl_Table_Row::clear();

    while (rowdata_.size() > 0) {
      for (int i = 0; i < COL_MAX; ++i) {
        // free() each rowdata_.cols entry
        if (rowdata_.back().cols[i]) {
          free(rowdata_.back().cols[i]);
        }
      }
      rowdata_.pop_back();
    }

    rowdata_.clear();
  }

  virtual bool load_dir() {
    return load_dir(".");
  }

  bool load_dir(const char *dirname)
  {
    DIR *d;
    struct dirent *dir;
    int r = 0;

    clear();

    if (!dirname && open_directory_.empty()) {
      return false;
    }

    if (dirname) {
      open_directory_ = simplify_directory_path(dirname);

      if (open_directory_.empty()) {
        // fall back to using realpath() if needed
        char *rp = realpath(dirname, NULL);
        open_directory_ = rp ? rp : dirname;
      }
    }

    // calling load_dir(NULL) acts as a "refresh" using
    // the current open_directory_
    if ((d = opendir(open_directory_.c_str())) == NULL) {
      return false;
    }

    int fd = ::open(open_directory_.c_str(), O_CLOEXEC | O_DIRECTORY, O_RDONLY);

    if (fd == -1) {
      return false;
    }

    while ((dir = readdir(d)) != NULL) {
      struct stat st, lst;
      filetable_Row row;
      int rv_stat = -1;
      int rv_lstat = -1;

      // handle hidden files
      if (!show_hidden() && dir->d_name[0] == '.') {
        continue;
      }

      // no "." and ".." entries
      if (show_hidden() && (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)) {
        continue;
      }

      if ((rv_stat = fstatat(fd, dir->d_name, &st, AT_NO_AUTOMOUNT)) == -1) {
        // don't follow link in case of dead link
        rv_lstat = fstatat(fd, dir->d_name, &st, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
      }

      if (rv_stat == 0 || rv_lstat == 0) {
        // is link?
        if (rv_lstat == 0) {
          if (S_ISLNK(st.st_mode)) {
            row.setlnk();
          }
        } else if (fstatat(fd, dir->d_name, &lst, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW) == 0
                   && S_ISLNK(lst.st_mode))
        {
          row.setlnk();
        }

        // dircheck and size
        if (S_ISDIR(st.st_mode)) {
          row.type('D');
          row.cols[COL_SIZE] = count_dir_entries(row.bytes, dir->d_name);
        } else {
          // check for file extensions
          if (!filter_show_entry(dir->d_name)) {
            continue;
          }

          row.cols[COL_SIZE] = human_readable_filesize(st.st_size);
          row.bytes = st.st_size;

          switch (st.st_mode & S_IFMT) {
            case S_IFBLK:
              // block device
              row.type('B');
              break;
            case S_IFCHR:
              // character device
              row.type('C');
              break;
            case S_IFIFO:
              // FIFO/pipe
              row.type('F');
              break;
            case S_IFSOCK:
              // socket
              row.type('S');
              break;
            default:
              // regular file or dead link
              row.type('R');
              break;
          }
        }

        // last modified
        row.cols[COL_LAST_MOD] = printf_alloc(" %s", ctime(&st.st_mtime));
        row.last_mod = st.st_mtime;
      }

      // name
      row.cols[COL_NAME] = strdup(dir->d_name);

      rowdata_.push_back(row);
      r++;
    }

    closedir(d);

    cols(COL_MAX);
    rows(rowdata_.size());
    autowidth();
    sort_column(0);  // initial sort

    return true;
  }

  virtual bool refresh() {
    return load_dir(NULL);
  }

  virtual bool dir_up()
  {
    if (open_directory_.empty()) {
      return load_dir("..");
    }

    if (open_directory_ == "/") {
      return false;
    }

    std::string dir = open_directory_;
    dir += "/..";

    return load_dir(dir.c_str());
  }

  const char *selection() {
    return selection_.empty() ? NULL : selection_.c_str();
  }

  // Resize parent window to size of table
  /*
  void resize_window()
  {
    // Determine exact outer width of table with all columns visible
    int width = 2;  // width of table borders

    for (int t=0; t<cols(); ++t) {
      width += col_width(t);  // total width of all columns
    }
    width += vscrollbar->w();  // include width of scrollbar
    width += MARGIN*2;

    if (width < 200 || width > Fl::w()) {
      return;
    }

    window()->resize(window()->x(), window()->y(), width, window()->h());  // resize window to fit
  }
  */

  void double_click_timeout(double d)
  {
    const double timeout_min = 0.1;
    const double timeout_max = 2.0;

    if (d < timeout_min) {
      dc_timeout_ = 0;
    } else if (d > timeout_max) {
      dc_timeout_ = timeout_max;
    } else {
      dc_timeout_ = d;
    }
  }

  // add file extension to filter
  void add_filter(const char *str)
  {
    if (!str || str[0] == 0) {
      return;
    }

    std::string ext;

    if (str[0] != '.') {
      ext = ".";
    }
    ext += str;

    filter_list_.push_back(ext);
  }

  // add file extensions to filter
  void add_filter_list(const char *list, const char *delim)
  {
    char *copy, *str, *tok;
    int i;

    if (!list || list[0] == 0 || !delim || delim[0] == 0) {
      return;
    }

    copy = strdup(list);

    for (i = 1, str = copy; ; ++i, str = NULL) {
      if ((tok = strtok(str, delim)) == NULL) {
        break;
      }

      add_filter(tok);
    }

    free(copy);
  }

  // set
  void label_header(int idx, const char *l) {
    if (idx >= 0 && idx < COL_MAX) {
      label_header_[idx] = l;
    }
  }

  void autowidth_padding(int i) { autowidth_padding_ = (i < 0) ? 0 : i; }
  void autowidth_max(int i) { autowidth_max_ = i; }
  void show_hidden(bool b) { show_hidden_ = b; }

  // get
  const char *label_header(int idx) {
    return (idx >= 0 && idx < COL_MAX) ? label_header_[idx] : NULL;
  }

  int autowidth_padding() const { return autowidth_padding_; }
  int autowidth_max() const { return autowidth_max_; }
  bool show_hidden() const { return show_hidden_; }
};

bool filetable_::within_double_click_timelimit_ = false;

} // namespace fltk

#endif  // filetable__hpp
