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

#ifndef fltk_filetable__hpp
#define fltk_filetable__hpp

#include <FL/Fl.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_SVG_Image.H>
#include <vector>

#include "fltk_dirtree.hpp"


// single row of columns
class fltk_filetable_Row
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

  fltk_filetable_Row();
  virtual ~fltk_filetable_Row() {}

  bool selected() const { return _sel; }
  void setselected() { _sel = true; }
  void setunselected() { _sel = false; }

  char type() const { return _type; }
  void type(char c) { _type = c; }

  bool isdir() { return (_type == 'D'); }

  bool islnk() const { return _lnk; }
  void setlnk() { _lnk = true; }
};

class fltk_filetable_ : public Fl_Table_Row
{
private:
  friend bool fltk_dirtree::load_directory(const char *path);

public:
  typedef struct {
    char *str;
    size_t size;
  } ext_t;

  // columns
  enum {
    COL_NAME = fltk_filetable_Row::COL_NAME,
    COL_SIZE = fltk_filetable_Row::COL_SIZE,
    COL_LAST_MOD = fltk_filetable_Row::COL_LAST_MOD,
    COL_MAX = fltk_filetable_Row::COL_MAX
  };

private:
  int sort_reverse_;
  int sort_last_row_;
  double dc_timeout_;
  int autowidth_padding_;
  int autowidth_max_;
  bool show_hidden_;
  std::vector<ext_t> filter_list_;

  Fl_SVG_Image *icon_square_[2];
  const char *label_header_[COL_MAX];

  static void event_callback(Fl_Widget*, void*);
  void event_callback2();  // callback for table events

  static bool within_double_click_timelimit_;
  static void reset_timelimit_cb(void *);
  Fl_Callback *double_click_callback_;

protected:
  char *open_directory_;
  char *selection_;
  std::vector<fltk_filetable_Row> rowdata_;
  int last_row_clicked_;
  Fl_SVG_Image *svg_link_;
  Fl_SVG_Image *svg_overlay_;

  void draw_cell(TableContext context, int R=0, int C=0, int X=0, int Y=0, int W=0, int H=0);
  void sort_column(int col, int reverse=0);  // sort table by a column
  void draw_sort_arrow(int X,int Y,int W,int H);
  int handle(int event);

  // set a callback to handle doubleklicks on entries
  virtual void double_click_callback() {}

  // set icon for row entry; here you can i.e. set icons dependant on
  // the magic bytes or simply the file extension; set NULL for no icon
  virtual Fl_SVG_Image *icon(fltk_filetable_Row) { return NULL; }

  // these functions always change the buffer
  void human_readable_filesize(char *buf, size_t bufsize, long bytes);
  long count_dir_entries(char *buf, size_t bufsize, const char *path);

  // returns allocated string (must be free()d later) or NULL on error
  static char *simplify_directory_path(const char *in);

  // returns true if the current filename is accepted by the
  // filename filter (always returns true if no filter was set)
  virtual bool filter_show_entry(const char *filename);

  // automatically set column widths to data
  void autowidth();

public:
  fltk_filetable_(int X, int Y, int W, int H, const char *L=NULL);
  ~fltk_filetable_();

  void clear();

  bool load_dir(const char *dirname);
  virtual bool load_dir();
  virtual bool refresh();
  virtual bool dir_up();
  char * const selection();

  // Resize parent window to size of table
  //void resize_window();

  void double_click_timeout(double d);
  void add_filter(const char *ext);
  void add_filter_list(const char *list, const char *delim);

  // set
  void label_header(int idx, const char *l);
  void autowidth_padding(int i) { autowidth_padding_ = (i < 0) ? 0 : i; }
  void autowidth_max(int i) { autowidth_max_ = i; }
  void show_hidden(bool b) { show_hidden_ = b; }

  // get
  const char *label_header(int idx);
  int autowidth_padding() const { return autowidth_padding_; }
  int autowidth_max() const { return autowidth_max_; }
  bool show_hidden() const { return show_hidden_; }
};

#endif  // fltk_filetable__hpp
