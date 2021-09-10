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

#include "svg_data.h"

#ifndef FLTK_FMT_INT
#define FLTK_FMT_INT   "%ld"
#endif
#ifndef FLTK_FMT_FLOAT
#define FLTK_FMT_FLOAT "%.1Lf"
#endif


namespace fltk
{

class filetable_ : public Fl_Table_Row
{
public:
  // columns
  enum ECol {
    COL_NAME = 0,  // Name
    COL_SIZE,      // Size
    COL_LAST_MOD,  // Last modified
    COL_MAX
  };

  // size column strings
  enum EStrSize {
    STR_SIZE_ELEMENTS = 0,  // n elements
    STR_SIZE_BYTES,         // n bytes
    STR_SIZE_KBYTES,        // n kiB
    STR_SIZE_MBYTES,        // n MiB
    STR_SIZE_GBYTES,        // n GiB
    STR_SIZE_TBYTES,        // n TiB
    STR_SIZE_MAX
  };

  // set file sort behavior
  enum {
    SORT_NUMERIC            = 0x0001,  // numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
    SORT_IGNORE_CASE        = 0x0002,  // case-insensitive sort
    SORT_IGNORE_LEADING_DOT = 0x0004,  // ignore leading dots in filenames ("a, b, .c" instead of ".c, a, b")
    SORT_DIRECTORY_AS_FILE  = 0x0100   // don't list directories and files separated
  };

protected:
  enum EIcn {
    ICN_DIR = 0,  // directory
    ICN_FILE,     // regular file
    ICN_LINK,     // symbolic link (overlay)
    ICN_LOCK,     // "no access" overlay icon
    ICN_BLK,      // block device
    ICN_CHR,      // character device
    ICN_PIPE,     // FIFO/pipe
    ICN_SOCK,     // socket
    ICN_LAST
  };

  // single row of columns
  typedef struct {
    char *cols[COL_MAX] = {0};
    Fl_SVG_Image *svg = NULL;
    char type = 0;
    bool isdir() const { return (type == 'D'); }
    long bytes = 0;
    long last_mod = 0;
    bool is_link = false;
    bool selected = false;
  } Row_t;

private:
  // Sort class to handle sorting column using std::sort
  class sort {
    int _col, _reverse;
    uint _mode;

  public:
    sort(int col, int reverse, uint mode) {
      _col = col;
      _reverse = reverse;
      _mode = mode;
    }

    bool operator() (Row_t a, Row_t b) {
      if (_col >= COL_MAX) return false;

      const char *ap = a.cols[_col];
      const char *bp = b.cols[_col];

      if (_col == COL_SIZE) {
        // on size column always list directories and files separated
        if (a.isdir() != b.isdir()) {
          return (a.isdir() && !b.isdir());
        }
        return _reverse ? a.bytes<b.bytes : b.bytes<a.bytes;
      }

      if (!(_mode & SORT_DIRECTORY_AS_FILE) && a.isdir() != b.isdir()) {
        return (a.isdir() && !b.isdir());
      }

      if (_col == COL_LAST_MOD) {
        return _reverse ? a.last_mod<b.last_mod : b.last_mod<a.last_mod;
      }

      // ignore leading dots in filenames ("bin, .config, data, .local"
      // instead of ".config, .local, bin, data")
      if (_mode & SORT_IGNORE_LEADING_DOT) {
        if (*ap == '.') ap++;
        if (*bp == '.') bp++;
      }

      if ((_mode & SORT_NUMERIC) && isdigit(*ap) && isdigit(*bp)) {
        // Numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
        long av = atol(ap);
        long bv = atol(bp);

        // if numbers are the same, continue to alphabetic sort
        if (av != bv) {
          return _reverse ? bv < av : av < bv;
        }
      }

      // Alphabetic sort
      if (_mode & SORT_IGNORE_CASE) {
        return _reverse ? strcasecmp(ap, bp) > 0 : strcasecmp(ap, bp) < 0;
      }

      return _reverse ? strcmp(ap, bp) > 0 : strcmp(ap, bp) < 0;
    }
  };

  // default sort modus
  uint sort_mode_ = SORT_NUMERIC|SORT_IGNORE_CASE|SORT_IGNORE_LEADING_DOT;

  // sort in reverse order or not
  int sort_reverse_ = 0;

  // number of last column that was sorted
  int last_row_sorted_ = 0;

  // double click timeout in seconds
  double dc_timeout_ = 0.8;

  // autowidth
  int autowidth_padding_ = 20;
  int autowidth_max_ = 0;

  // show hidden files or not
  bool show_hidden_ = false;

  // save the known number of directory entries to this value on a
  // double click before loading so we can use it to reserve space
  // in the rowdata_ vector, as an attempt to reduce unneeded reallocation
  ulong reserve_entries_ = 0;

  // list of filename extensions to filter in
  std::vector<std::string> filter_list_;

  // icons used for a filename "blend-over" effect;
  // 2 colors for selected and unselected row
  Fl_RGB_Image *icon_blend_[2] = {0};

  // width of the filename blend-over area in pixels;
  // 0 will disable this effect
  int blend_w_ = 8;

  // height of blend-over area; keep this initially at 0
  // so we won't call make_overlay_image() before we even
  // drew any table rows
  int blend_h_ = 0;

  // labels of the header entries
  const char *label_header_[COL_MAX] = {
    "Name", "Size", "Last modified"
  };

  // labels used for the file sizes
  std::string filesize_label_[STR_SIZE_MAX] =
  {
    FLTK_FMT_INT   " elements ",
    FLTK_FMT_INT   " bytes ",
    FLTK_FMT_FLOAT " KiB ",
    FLTK_FMT_FLOAT " MiB ",
    FLTK_FMT_FLOAT " GiB ",
    FLTK_FMT_FLOAT " TiB "
  };

  // this string is used to be strdup()ed instead of being
  // created with printf_alloc() each time
  std::string str_unknown_elements_ = "?? elements ";


  // extra width for icons
  inline int col_name_extra_w() const {
    return labelsize() + 10;
  }

  // callback for table events
  void event_callback()
  {
    //int row = callback_row();  // unused
    int col = callback_col();

    if (callback_context() == CONTEXT_COL_HEADER) {
      if (Fl::event() == FL_RELEASE && Fl::event_button() == 1) {
        if (last_row_sorted_ == col) {
          // Click same column? Toggle sort
          sort_reverse_ ^= 1;
        } else {
          // Click diff column? Up sort
          sort_reverse_ = 0;
        }
        sort_column(col);
        last_row_sorted_ = col;
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

  void blend_h(int i) {
    if (i == blend_h_) return;
    blend_h_ = i;
    update_overlays();
  }

  int blend_h() const { return blend_h_; }

protected:
  std::string open_directory_;
  std::string selection_;
  std::vector<Row_t> rowdata_;
  int last_row_clicked_ = -1;
  Fl_SVG_Image *svg_link_ = NULL;
  Fl_SVG_Image *svg_noaccess_ = NULL;

  // check if const char * is empty
  inline bool empty(const char *val) {
    return (!val || *val == 0) ? true : false;
  }

  // create an overlay image, transitioning from the left being full
  // transparent to the right being full opaque
  Fl_RGB_Image *make_overlay_image(uchar r, uchar g, uchar b, int W, int H)
  {
    if (W < 1 || H < 1) return NULL;

    uchar line[W*4];
    uchar *data = new uchar[W*H*4];
    const double div = static_cast<double>(sizeof(line)) / 255.0;

    // create a 1 pixel height line
    for (size_t i=0; i < sizeof(line); i+=4) {
      line[i] = r;
      line[i+1] = g;
      line[i+2] = b;
      line[i+3] = static_cast<uchar>(static_cast<double>(i)/div);
    }

    // copy the line multiple times
    for (int i=0; i < H; ++i) {
      memcpy(data + i*sizeof(line), line, sizeof(line));
    }

    Fl_RGB_Image *rgba = new Fl_RGB_Image(data, W, H, 4);
    rgba->alloc_array = 1;

    return rgba;
  }

  // wrapper taking an Fl_Color argument
  Fl_RGB_Image *make_overlay_image(Fl_Color c, int W, int H)
  {
    if (W < 1 || H < 1) return NULL;
    uchar r=0, g=0, b=0;
    Fl::get_color(c, r, g, b);
    return make_overlay_image(r, g, b, W, H);
  }

  void update_overlays()
  {
    if (icon_blend_[0]) delete icon_blend_[0];
    if (icon_blend_[1]) delete icon_blend_[1];
    icon_blend_[0] = make_overlay_image(color(), blend_w(), blend_h());
    icon_blend_[1] = make_overlay_image(selection_color(), blend_w(), blend_h());
  }

  // format localization string using "{}" as replacement for a variable:
  // replace first occurance of "{}" in "str" with "format";
  // i.e. str="{} bytes" and format="%ld" returns "%ld bytes "
  std::string format_localization(const char *str, const char *format, bool add_space=true)
  {
    if (empty(str)) return "";
    if (empty(format)) return str;

    std::string s;
    const char *p = str;

    while (*p) {
      if (*p == '{' && *(p+1) == '}') {
        p += 2;
        s.append(format);
        s.append(p);
        break;
      }
      s.push_back(*p);
      p++;
    }

    if (add_space) s.push_back(' ');

    return s;
  }

  // return a generic SVG icon by idx/enum
  static const char *default_icon_data(EIcn idx)
  {
#ifdef SVG_DATA_H
    switch (idx) {
      case ICN_DIR:
        return FOLDER_GENERIC_SVG_DATA;
      case ICN_FILE:
        return FILE_GENERIC_SVG_DATA;
      case ICN_LINK:
        return OVERLAY_LINK_SVG_DATA;
      case ICN_LOCK:
        return OVERLAY_PADLOCK_SVG_DATA;
      case ICN_BLK:
      case ICN_CHR:
        return FILE_DEVICE_SVG_DATA;
      case ICN_SOCK:
      case ICN_PIPE:
        return FILE_PIPE_SVG_DATA;
      default:
        break;
    }
#endif

    return NULL;
  }

  // Handle drawing all cells in table
  void draw_cell(TableContext context, int R=0, int C=0, int X=0, int Y=0, int W=0, int H=0)
  {
    if (C >= COL_MAX) {
      return;
    }

    switch (context) {
/*
      case CONTEXT_ENDPAGE:
        // create a gray stripe on the height of the column headers
        // issue: will overlay scroll bars
        fl_push_clip(tox, wiy, tow, col_header_height());
          fl_draw_box(FL_THIN_UP_BOX, tox, wiy, tow, col_header_height(), FL_BACKGROUND_COLOR);
        fl_pop_clip();

        // redraw column headers to become visible again
        for (int i=0; i < COL_MAX; ++i) {
          int hX, hY, hW, hH;
          find_cell(CONTEXT_COL_HEADER, 0, i, hX, hY, hW, hH);
          draw_cell(CONTEXT_COL_HEADER, 0, i, hX, hY, hW, hH);
        }
        break;
*/
      case CONTEXT_COL_HEADER:
        fl_push_clip(X, Y, W, H); {
          fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_BACKGROUND_COLOR);
          fl_font(labelfont(), labelsize());
          fl_color(FL_BLACK);
          fl_draw(label_header_[C], X + 2, Y, W, H, FL_ALIGN_LEFT, NULL, 0);  // +2=pad left
          if (C == last_row_sorted_) {
            draw_sort_arrow(X, Y, W, H);
          }
        }
        fl_pop_clip();
        break;

      case CONTEXT_CELL:
        fl_push_clip(X, Y, W, H); {
          Fl_Color bgcol = color();
          Fl_Align al = (C == COL_SIZE) ? FL_ALIGN_RIGHT : FL_ALIGN_LEFT;
          Fl_RGB_Image *blend = icon_blend_[0];
          int fw = 0;

          if (row_selected(R)) {
            blend = icon_blend_[1];
            bgcol = selection_color();
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

            if (rowdata_.at(R).is_link && svg_link_) {
              svg_link_->draw(X + 2, Y + 2);
            }

            if (rowdata_.at(R).bytes == -1 && svg_noaccess_) {
              svg_noaccess_->draw(X + 2, Y + 2);
            }

            X += col_name_extra_w();
            fw += col_name_extra_w();
          }

          // Text
          fl_font(labelfont(), labelsize());
          fl_color(fl_contrast(labelcolor(), bgcol));
          fl_draw(rowdata_.at(R).cols[C], X, Y + 2, W, H - 2, al, NULL, 0);

          // blend over long text at the end of name column
          if (C == COL_NAME && blend_w() > 0) {
            if (blend_h() != H) blend_h(H);

            if (blend && fw + blend_h() > W) {
              blend->draw(X + W - col_name_extra_w() - blend_w(), Y);
            }
          }
        }
        fl_pop_clip();
        break;

      default:
        break;
    }
  }

  // Sort a column up or down
  void sort_column(int col)
  {
    // save current selection state in rowdata_
    for (int i = 0; i < rows(); ++i) {
      rowdata_.at(i).selected = row_selected(i) ? true : false;
    }

    // sort data while preserving order between equal elements
    std::stable_sort(rowdata_.begin(), rowdata_.end(), sort(col, sort_reverse_, sort_mode()));

    // update table row selection from rowdata_
    for (int i = 0; i < rows(); ++i) {
      select_row(i, rowdata_.at(i).selected);
    }

    redraw();
  }

  void draw_sort_arrow(int X,int Y,int W,int H)
  {
    const int xlft = X + (W - 6) - 8;
    const int xctr = X + (W - 6) - 4;
    const int xrit = X + (W - 6) - 0;
    const int ytop = Y + (H / 2) - 4;
    const int ybot = Y + (H / 2) + 4;

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
    reserve_entries_ = 0;

    switch (callback_context()) {
      case CONTEXT_CELL:
        if (event == FL_RELEASE) {
          if (dc_timeout_ == 0) {   // double click was disabled
            reserve_entries_ = rowdata_.at(last_row_clicked_).bytes;
            double_click_callback();
          } else if (last_row_clicked_ == callback_row() && within_double_click_timelimit_) {  // double click
            Fl::remove_timeout(reset_timelimit_cb);
            within_double_click_timelimit_ = false;
            reserve_entries_ = rowdata_.at(last_row_clicked_).bytes;
            double_click_callback();
          } else {
            Fl::remove_timeout(reset_timelimit_cb);
            within_double_click_timelimit_ = true;
            Fl::add_timeout(dc_timeout_, reset_timelimit_cb);
          }
          last_row_clicked_ = callback_row();
        }
        break;

      case CONTEXT_TABLE:
        // "outside" area -> clear selection
        select_all_rows(0);
        // fall through to default case

      default:
        // not clicked on a cell -> remove timout
        Fl::remove_timeout(reset_timelimit_cb);
        within_double_click_timelimit_ = false;
        break;
    }

    //redraw();

    return ret;
  }

  // set a callback to handle double-clicks on entries
  virtual void double_click_callback()
  {
    if (last_clicked_item_isdir()) {
      load_dir(last_clicked_item().c_str());
    } else {
      selection_ = last_clicked_item();
      window()->hide();
    }
  }

  // return pointer to icon set for row entry
  virtual Fl_SVG_Image *icon(Row_t) const = 0;

  // similar to printf() but returns an allocated string
  static char *printf_alloc(const char *fmt, ...)
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
    const long s_kiB = 1024;
    const long s_MiB = 1024 * s_kiB;
    const long s_GiB = 1024 * s_MiB;
    const long s_TiB = 1024 * s_GiB;
    long double ld = bytes;
    EStrSize idx = STR_SIZE_BYTES;

    switch (bytes) {
      // GiB
      case s_GiB ... s_TiB-1:
        idx = STR_SIZE_GBYTES;
        ld /= s_GiB;
        break;

      // MiB
      case s_MiB ... s_GiB-1:
        idx = STR_SIZE_MBYTES;
        ld /= s_MiB;
        break;

      // kiB
      case s_kiB ... s_MiB-1:
        idx = STR_SIZE_KBYTES;
        ld /= s_kiB;
        break;

      // bytes
      case 0 ... s_kiB-1:
        return printf_alloc(filesize_label_[STR_SIZE_BYTES].c_str(), bytes);
        break;

      default:
        // TiB
        if (bytes >= s_TiB) {
          idx = STR_SIZE_TBYTES;
          ld /= s_TiB;
        } else {
          return printf_alloc(filesize_label_[STR_SIZE_BYTES].c_str(), bytes);
        }
        break;
    }

    return printf_alloc(filesize_label_[idx].c_str(), ld);
  }

  char *count_dir_entries(long &count, const char *directory)
  {
    DIR *d;
    struct dirent *dir;

    if (empty(directory)) {
      count = -1;
      return NULL;
    }

    { std::string temp;
      temp.reserve(open_directory_.size() + strlen(directory) + 1);
      temp = open_directory_ + "/" + directory;

      if ((d = opendir(temp.c_str())) == NULL) {
        count = -1;
        return strdup(str_unknown_elements_.c_str());
      }
    }  // end temp

    count = errno = 0;

    while ((dir = readdir(d)) != NULL)
    {
      // handle hidden files
      if (dir->d_name[0] == '.' && !show_hidden()) {
        continue;
      }

      // no "." and ".." entries
      if (show_hidden() && (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)) {
        continue;
      }

      count++;
    }

    int errsv = errno;
    closedir(d);

    if (errsv != 0 || count < 0) {
      count = -1;
      return strdup(str_unknown_elements_.c_str());
    }

    return printf_alloc(filesize_label_[STR_SIZE_ELEMENTS].c_str(), count);
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

    for (const std::string &ext : filter_list_) {
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
      col_width(c, w + autowidth_padding());

      // rows
      for (size_t r = 0; r < rowdata_.size(); ++r) {
        w = h = 0;
        fl_measure(rowdata_.at(r).cols[c], w, h, 0);
        w += autowidth_padding() + extra;

        if (autowidth_max() > col_resize_min() && w >= autowidth_max()) {
          // set to maximum autowidth and exit for-loop
          col_width(c, autowidth_max());
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
  // c'tor
  filetable_(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Table_Row(X, Y, W, H, L)
  {
    color(FL_WHITE);
    selection_color(FL_SELECTION_COLOR);

    col_header(1);
    col_resize(1);
    col_resize_min(col_name_extra_w() + blend_w());
    autowidth_max(W - col_name_extra_w());
    type(SELECT_SINGLE);

    when(FL_WHEN_RELEASE);
    callback(static_cast<void(*)(Fl_Widget *)>(
      [](Fl_Widget *o) { reinterpret_cast<filetable_ *>(o)->event_callback(); }
    ));

    end();
  }

  // d'tor
  virtual ~filetable_()
  {
    clear();
    if (icon_blend_[0]) delete icon_blend_[0];
    if (icon_blend_[1]) delete icon_blend_[1];
  }

  void clear()
  {
    last_row_clicked_ = -1;

    Fl_Table_Row::clear();

    // clear rowdata_ and free entries
    for (const auto e : rowdata_) {
      for (int i = 0; i < COL_MAX; ++i) {
        if (e.cols[i]) free(e.cols[i]);
      }
    }

    rowdata_.clear();
  }

  // resolve '.' and '..' but don't follow symlinks;
  static std::string simplify_directory_path(std::string path)
  {
    std::vector<std::string> vec;

    if (path.empty()) return "";

    // strip trailing '/'
    while (path.back() == '/') path.pop_back();

    // path was only slashes
    if (path.empty()) return "/";

    // return current directory path
    if (path == ".") {
      char *p = get_current_dir_name();
      if (!p) return "";
      path = p;
      free(p);
      return path;
    }

    // prepend current directory
    if (path.front() != '/') {
      char *p = get_current_dir_name();
      if (!p) return "";
      path.insert(0, 1, '/');
      path.insert(0, p);
      free(p);
    }

    // insert trailing '/' (needed for the next 2 steps)
    path.push_back('/');

    // check if the path even needs to be simplified
    if (path.find("//") == std::string::npos &&
        path.find("/./") == std::string::npos &&
        path.find("/../") == std::string::npos)
    {
      path.pop_back();  // remove trailing '/'
      return path;
    }

    // reserve some space
    vec.reserve(16);

    // a trailing '/' in the input string is needed
    for (auto it = path.cbegin(); it != path.cend(); ++it) {
      if (*it == '/') continue;

      std::string dir;

      while (*it != '/') {
        dir.push_back(*it);
        it++;
      }

      if (dir == "..") {
        // remove last entry or ignore
        if (vec.size() > 0) vec.pop_back();
      } else if (dir == ".") {
        // ignore '.' entry
        continue;
      } else if (dir.size() > 0) {
        vec.emplace_back(dir);
      }
    }

    // path was resolved to "/"
    if (vec.size() == 0) return "/";

    path = "";

    for (const auto elem : vec) {
      path.push_back('/');
      path.append(elem);
    }

    return path;
  }

  virtual bool load_dir() {
    return load_dir(".");
  }

  bool load_dir(const char *dirname)
  {
    DIR *d;
    struct dirent *dir;
    int fd = -1;

    // calling load_dir(NULL) acts as a "refresh" using
    // the current open_directory_
    if (empty(dirname) && open_directory_.empty()) {
      return false;
    }

    { std::string new_dir;

      if (empty(dirname)) {
        new_dir = open_directory_;
      } else {
        new_dir = simplify_directory_path(dirname);

        if (new_dir.empty()) {
          // fall back to using realpath() if needed
          char *p = realpath(dirname, NULL);
          new_dir = p ? p : dirname;
          free(p);
        }
      }

      if ((d = opendir(new_dir.c_str())) == NULL) {
        return false;
      }

      if ((fd = ::open(new_dir.c_str(), O_CLOEXEC | O_DIRECTORY, O_RDONLY)) == -1) {
        return false;
      }

      open_directory_ = new_dir;
    }  // new_dir end

    // clear current table
    clear();

    // reserve some space based on the known number of directory entries
    if (reserve_entries_ > 0) {
      if (reserve_entries_ > 2048) {
        reserve_entries_ = 2048;  // cap this for safety
      }
      rowdata_.reserve(reserve_entries_);
    }

    while ((dir = readdir(d)) != NULL) {
      struct stat st, lst;
      Row_t row;
      int rv_stat = -1;
      int rv_lstat = -1;

      // handle hidden files
      if (dir->d_name[0] == '.' && !show_hidden()) {
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
            row.is_link = true;
          }
        } else if (fstatat(fd, dir->d_name, &lst, AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW) == 0
                   && S_ISLNK(lst.st_mode))
        {
          row.is_link = true;
        }

        // dircheck and size
        if (S_ISDIR(st.st_mode)) {
          row.type = 'D';
          row.cols[COL_SIZE] = count_dir_entries(row.bytes, dir->d_name);
        } else {
          // check for file extensions
          if (!filter_show_entry(dir->d_name)) continue;

          row.cols[COL_SIZE] = human_readable_filesize(st.st_size);
          row.bytes = st.st_size;

          switch (st.st_mode & S_IFMT) {
            case S_IFBLK:
              // block device
              row.type = 'B';
              break;
            case S_IFCHR:
              // character device
              row.type = 'C';
              break;
            case S_IFIFO:
              // FIFO/pipe
              row.type = 'F';
              break;
            case S_IFSOCK:
              // socket
              row.type = 'S';
              break;
            default:
              // regular file or dead link
              row.type = 'R';
              break;
          }
        }

        // last modified
        row.cols[COL_LAST_MOD] = printf_alloc(" %s", ctime(&st.st_mtime));
        row.last_mod = st.st_mtime;
      }

      // name
      row.cols[COL_NAME] = strdup(dir->d_name);

      rowdata_.emplace_back(row);
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
    if (open_directory_.empty()) return load_dir("..");
    if (open_directory_ == "/") return false;
    std::string dir(open_directory_ + "/..");
    return load_dir(dir.c_str());
  }

  const char *selection() const {
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

  double double_click_timeout() const { return dc_timeout_; }

  // add file extension to filter
  void add_filter(const char *str)
  {
    if (empty(str)) return;

    if (str[0] == '.') {
      filter_list_.emplace_back(str);
    } else {
      filter_list_.emplace_back(std::string(".") + str);
    }
  }

  // add file extensions to filter
  void add_filter_list(const char *list, const char *delim)
  {
    if (empty(list) || empty(delim)) {
      return;
    }

    char *copy = strdup(list);

    for (char *str = copy; ; str = NULL) {
      char *tok = strtok(str, delim);
      if (!tok) break;
      add_filter(tok);
    }
    free(copy);
  }

  std::string last_clicked_item()
  {
    char * const name = rowdata_.at(last_row_clicked_).cols[COL_NAME];

    if (open_directory_.empty()) {
      return simplify_directory_path(name);
    }

    std::string s;
    s.reserve(open_directory_.size() + strlen(name) + 1);
    s = open_directory_;
    if (s.back() != '/') s.push_back('/');
    s.append(rowdata_.at(last_row_clicked_).cols[COL_NAME]);

    return simplify_directory_path(s);
  }

  bool last_clicked_item_isdir() const {
    return rowdata_.at(last_row_clicked_).isdir();
  }

  const char *open_directory() const {
    return open_directory_.empty() ? NULL : open_directory_.c_str();
  }

  bool open_directory_is_root() const {
    return open_directory_.compare("/") == 0;
  }

  virtual void load_default_icons() = 0;

  // set
  void label_header(ECol idx, const char *l) { label_header_[idx] = l; }

  void filesize_label(EStrSize idx, const char *l)
  {
    switch (idx) {
      case STR_SIZE_ELEMENTS:
        str_unknown_elements_ = format_localization(l, "??");
      case STR_SIZE_BYTES:
        filesize_label_[idx] = format_localization(l, FLTK_FMT_INT);
        break;
      default:
        filesize_label_[idx] = format_localization(l, FLTK_FMT_FLOAT);
        break;
    }
  }

  void blend_w(int i)
  {
    if (i == blend_w_) return;
    blend_w_ = i;
    col_resize_min(col_name_extra_w() + blend_w());
    update_overlays();
  }

  void autowidth_padding(int i) { autowidth_padding_ = (i < 0) ? 0 : i; }
  void autowidth_max(int i) { autowidth_max_ = i; }
  void show_hidden(bool b) { show_hidden_ = b; }
  void sort_mode(uint u) { sort_mode_ = u; }

  // get
  const char *label_header(ECol idx) const { return label_header_[idx]; }

  const char *filesize_label(EStrSize idx) const {
    return filesize_label_[idx].empty() ? NULL : filesize_label_[idx].c_str();
  }

  int blend_w() const { return blend_w_; }
  int autowidth_padding() const { return autowidth_padding_; }
  int autowidth_max() const { return autowidth_max_; }
  bool show_hidden() const { return show_hidden_; }
  uint sort_mode() const { return sort_mode_; }
};

bool filetable_::within_double_click_timelimit_ = false;

} // namespace fltk

#endif  // filetable__hpp
