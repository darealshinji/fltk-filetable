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

class filetable_ : public Fl_Table_Row
{
private:
  friend class dirtree;

public:
  // columns
  enum {
    COL_NAME = 0,
    COL_SIZE = 1,
    COL_LAST_MOD = 2,
    COL_MAX = 3
  };

protected:
  enum {
    ICN_DIR,   // directory
    ICN_FILE,  // regular file
    ICN_LINK,  // symbolic link (overlay)
    ICN_LOCK,  // "no access" overlay icon
    ICN_BLK,   // block device
    ICN_CHR,   // character device
    ICN_PIPE,  // FIFO/pipe
    ICN_SOCK,  // socket
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

  public:
    sort(int col, int reverse) {
      _col = col;
      _reverse = reverse;
    }

    bool operator() (Row_t a, Row_t b) {
      const char *ap = (_col < COL_MAX) ? a.cols[_col] : "";
      const char *bp = (_col < COL_MAX) ? b.cols[_col] : "";

      if (a.isdir() != b.isdir()) {
        return (a.isdir() && !b.isdir());
      }
      else if (_col == COL_SIZE) {
        long as = a.bytes;
        long bs = b.bytes;
        return _reverse ? as < bs : bs < as;
      }
      else if (_col == COL_LAST_MOD) {
        long am = a.last_mod;
        long bm = b.last_mod;
        return _reverse ? am < bm : bm < am;
      }

      // ignore leading dots in filenames ("bin, .config, data, .local"
      // instead of ".config, .local, bin, data")
      if (_col == COL_NAME) {
        if (*ap == '.') ap++;
        if (*bp == '.') bp++;
      }

      if (isdigit(*ap) && isdigit(*bp)) {
        // Numeric sort ("1, 2, 3, 100" instead of "1, 100, 2, 3")
        long av = atol(ap);
        long bv = atol(bp);

        // if numbers are the same, continue to alphabetic sort
        if (av != bv) {
          return _reverse ? bv < av : av < bv;
        }
      }

      // Alphabetic sort
      return _reverse ? strcasecmp(ap, bp) > 0 : strcasecmp(ap, bp) < 0;
    }
  };

  int sort_reverse_ = 0;
  int sort_last_row_ = 0;
  double dc_timeout_ = 0.8;
  int autowidth_padding_ = 20;
  int autowidth_max_ = 0;
  bool show_hidden_ = false;
  std::vector<std::string> filter_list_;

  Fl_SVG_Image *icon_blend_[2];
  const char *label_header_[COL_MAX];

  // extra width for icons
  int col_name_extra_w() const {
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

protected:
  std::string open_directory_;
  std::string selection_;
  std::vector<Row_t> rowdata_;
  int last_row_clicked_ = -1;
  Fl_SVG_Image *svg_link_ = NULL;
  Fl_SVG_Image *svg_noaccess_ = NULL;

  bool empty(const char *val) {
    return (!val || *val == 0) ? true : false;
  }

  // return a generic SVG icon by idx/enum
  static const char *default_icon_data(int idx)
  {
    // Icons were taken from https://git.haiku-os.org/haiku/tree/data/artwork/icons
    // exported to SVG, minimized with Scour and manually edited
    // (scour --no-renderer-workaround --strip-xml-prolog --remove-descriptive-elements --enable-comment-stripping
    //  --disable-embed-rasters --indent=none --strip-xml-space --enable-id-stripping --shorten-ids)

    const char *svg_data_folder_generic =
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
    "</svg>";

    const char *svg_data_file_generic =
    "<svg width='64' height='64'>"
      "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
      "<radialGradient id='b' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#c0d5ff' offset='1'/>"
        "<stop stop-color='#896eff' offset='.4886'/>"
      "</radialGradient>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#b)'/>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
      "<linearGradient id='a' x1='105.45' x2='119.92' y1='-23.42' y2='34.32' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#a5b1ff' offset='0'/>"
        "<stop stop-color='#eaf1ff' offset='.7386'/>"
        "<stop stop-color='#b3b8ff' offset='1'/>"
      "</linearGradient>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#a)'/>"
    "</svg>";

    const char *svg_data_file_device =
    "<svg width='64' height='64'>"
      "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
      "<radialGradient id='g' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#c0d5ff' offset='1'/>"
        "<stop stop-color='#896eff' offset='.4886'/>"
      "</radialGradient>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#g)'/>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
      "<linearGradient id='f' x1='105.45' x2='119.92' y1='-23.42' y2='34.32' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#a5b1ff' offset='0'/>"
        "<stop stop-color='#eaf1ff' offset='.7386'/>"
        "<stop stop-color='#b3b8ff' offset='1'/>"
      "</linearGradient>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#f)'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m38 60h6l-4-2 24-24v-5h-2l-3 2-4 3-11-4 1-2-23-11-18 19 34 17v7z' "
        "fill='#010000' fill-opacity='.4196'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m38 60h6l-4-2 24-24v-5h-2l-3 2-4 3-11-4 1-2-23-11-18 19 34 17v7z' "
        "fill='#010000' fill-opacity='.4196'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='M4 32L40 50L56 34L49.93 31.13L48 33L38 28L40.21 26.56L22 18L4 32z' "
        "fill='none' stroke='#010101' stroke-linecap='square' stroke-width='4'/>"
      "<linearGradient id='e' x1='41.93' x2='57.8' y1='7.25' y2='35.03' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#a3d444' offset='.0039'/>"
        "<stop stop-color='#70a804' offset='1'/>"
      "</linearGradient>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='M4 32L40 50L56 34L49.93 31.13L48 33L38 28L40.21 26.56L22 18L4 32z' fill='url(#e)'/>"
      "<linearGradient id='d' x1='48.17' x2='58.29' y1='5.64' y2='35.99' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#fff2ac' offset='.0039'/>"
        "<stop stop-color='#ffd805' offset='1'/>"
      "</linearGradient>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m23 20-14 12 14 7 3-2 15 8m-15-24-7 6 11 7-1 1 14 8m-14-20-11 10 4 2 10-10m4 "
        "2-3 3 2 4-3 4 3 2 5-6 8 4' fill='none' stroke='url(#d)' stroke-width='2'/>"
      "<linearGradient id='a' x1='35.74' x2='37.78' y1='21.92' y2='31.28' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#717171' offset='0'/>"
        "<stop stop-color='#010101' offset='1'/>"
      "</linearGradient>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m23 26 8 4-5 5-8-4 5-5z' fill='url(#a)'/>"
      "<path transform='matrix(.6924 0 0 .6924 10.295 6.0948)' d='m23 26 8 4-5 5-8-4 5-5z' fill='url(#a)'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m34 56 4 2v-2s-1.6-1.4-2-2l22-22c0-1 2-4 2-4v-2s-2 1-2 0l-24 22v8z' "
        "fill='none' stroke='#010101' stroke-linecap='square' stroke-width='4'/>"
      "<linearGradient id='c' x1='15.86' x2='45.6' y1='64.94' y2='17.74' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#0c0c0c' offset='0'/>"
        "<stop stop-color='#797979' offset='.3686'/>"
        "<stop stop-color='#c6c6c6' offset='.647'/>"
        "<stop stop-color='#a2a2a2' offset='.7882'/>"
        "<stop stop-color='#e8e8e8' offset='1'/>"
      "</linearGradient>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m34 56 24-24c0-1 3-5 3-5v-2s-3 2-3 1l-24 22v8z' fill='url(#c)'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m34 48 4 2v2s-2-1-2 1 2 3 2 3-1-1-1-2 3-4 3-4v-2l-6-2v2z' fill='#010101'/>"
      "<linearGradient id='b' x1='44.73' x2='50.41' y1='40.53' y2='50.57' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#d3d3d3' offset='0'/>"
        "<stop stop-color='#a9a9a9' offset='1'/>"
      "</linearGradient>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m34 48 4 2v2s-2-1-2 1 2 3 2 3v2l-4-2v-8z' fill='url(#b)'/>"
      "<path transform='matrix(.8902 0 0 .8902 -1.9706 -5.2808)' d='m45 44 3-3v-4l-3 3v4z' fill='#010101'/>"
    "</svg>";

    const char *svg_data_file_pipe =
    "<svg width='64' height='64'>"
      "<path d='m27 57 2 2 3.13-3.17 4.87 3.17 8.39-11.46 18.61-9.54-5-3 2 2-25 12-9 8zm31-23 4-3-4-2-3 3 3 2z' fill='#010101' fill-opacity='.5725'/>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='none' stroke='#010101' stroke-width='4'/>"
      "<radialGradient id='e' cx='0' cy='0' r='64' gradientTransform='matrix(.5714 0 0 .3333 26 35)' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#c0d5ff' offset='1'/>"
        "<stop stop-color='#896eff' offset='.4886'/>"
      "</radialGradient>"
      "<path d='m6.25 30s5.62 6 10.62 12.5 9.38 13.25 9.38 13.25l32.87-18.88-27.12-21.87-25.75 15z' fill='url(#e)'/>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='none' stroke='#010101' stroke-width='4'/>"
      "<linearGradient id='d' x1='105.45' x2='119.92' y1='-23.42' y2='34.32' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#a5b1ff' offset='0'/>"
        "<stop stop-color='#eaf1ff' offset='.7386'/>"
        "<stop stop-color='#b3b8ff' offset='1'/>"
      "</linearGradient>"
      "<path d='m2.62 30.87s8.33 5.39 16.38 11c8.44 5.88 16.75 10.88 16.75 10.88l22.37-26.38s-12.68-5.67-22.75-11c-5.19-2.75-9.37-6-9.37-6l-23.38 21.5z' "
        "fill='url(#d)'/>"
      "<path d='m36 44h9c7 0 12-6 8-8l-11-5' fill-opacity='.6901'/>"
      "<path transform='translate(0 7)' d='m33 30c3.91 0 7 1.31 7 3 0 1.67-3.09 3-7 3-3.93 0-7-1.33-7-3 0-1.69 3.07-3 7-3z' "
        "fill='none' stroke='#000' stroke-width='6'/>"
      "<path transform='translate(0 3)' d='m33 30c3.91 0 7 1.31 7 3 0 1.67-3.09 3-7 3-3.93 0-7-1.33-7-3 0-1.69 3.07-3 7-3z' "
        "fill='none' stroke='#000' stroke-width='6'/>"
      "<path transform='translate(0 -1)' d='m33 30c3.91 0 7 1.31 7 3 0 1.67-3.09 3-7 3-3.93 0-7-1.33-7-3 0-1.69 3.07-3 7-3z' "
        "fill='none' stroke='#000' stroke-width='6'/>"
      "<radialGradient id='a' cx='0' cy='0' r='64' gradientTransform='matrix(.3241 .1144 -.1144 .3241 26.292 -7.0357)' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#40e905' offset='.7568'/>"
        "<stop stop-color='#04b300' offset='1'/>"
      "</radialGradient>"
      "<path transform='matrix(.7272 0 0 1.625 9 23.25)' d='m33 6c6.15 0 11 1.74 11 4 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0-2.26 "
        "4.83-4 11-4zm-11 0h22v4h-22v-4z' fill='url(#a)'/>"
      "<path transform='translate(0 19)' d='m33 6c6.15 0 11 1.74 11 4 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0-2.26 4.83-4 11-4zm-11 "
        "0h22v4h-22v-4z' fill='none' stroke='#000' stroke-linecap='round' stroke-linejoin='round' stroke-width='4'/>"
      "<path transform='translate(0 15)' d='m33 6c6.15 0 11 1.74 11 4 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0-2.26 4.83-4 11-4z' "
        "fill='none' stroke='#000' stroke-width='4'/>"
      "<path transform='translate(0 19)' d='m33 6c6.15 0 11 1.74 11 4 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0-2.26 4.83-4 11-4zm-11 "
        "0h22v4h-22v-4z' fill='url(#a)'/>"
      "<linearGradient id='c' x1='38.34' x2='48.77' y1='-4.73' y2='5.26' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#9cff79' offset='0'/>"
        "<stop stop-color='#25e400' offset='1'/>"
      "</linearGradient>"
      "<path transform='translate(0 15)' d='m33 6c6.15 0 11 1.74 11 4 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0-2.26 4.83-4 11-4z' fill='url(#c)'/>"
      "<linearGradient id='b' x1='38.34' x2='48.77' y1='-4.73' y2='5.26' gradientUnits='userSpaceOnUse'>"
        "<stop stop-color='#b3ffe8' offset='0'/>"
        "<stop stop-color='#89ffb2' offset='1'/>"
      "</linearGradient>"
      "<path transform='translate(0 15)' d='m33 13c6 0 11-2 11-3 0 2.23-4.85 4-11 4-6.17 0-11-1.77-11-4 0 1 5 3 11 3z' fill='url(#b)'/>"
      "<path transform='matrix(2.6666,0,0,2.5,-55,4.5)' d='m33 7c1.67 0 3 .43 3 1 0 .55-1.33 1-3 1-1.69 0-3-.45-3-1 0-.57 1.31-1 3-1z' fill-opacity='.8823'/>"
    "</svg>";

    const char *svg_data_overlay_link =
    "<svg width='64' height='64'>"
      "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z' "
        "fill='none' stroke='#000' stroke-linecap='round' stroke-linejoin='round' stroke-width='3.6416'/>"
      "<path d='m17.081 15.693c-8.4347 0-15.26 3.1006-15.26 6.9364 0-7.6648 6.8324-13.873 15.26-13.873v-6.9364l11.098 10.405-11.098 10.405z' "
        "fill='#0ff' stroke-width='.69364'/>"
    "</svg>";

    const char *svg_data_overlay_lock =
    "<svg width='64' height='64'>"
      "<defs>"
        "<linearGradient id='b' x1='102.6' x2='102.74' y1='8.5' y2='47.07' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#face79' offset='0'/>"
          "<stop stop-color='#bc4105' offset='1'/>"
        "</linearGradient>"
        "<linearGradient id='a' x1='103.24' x2='103.39' y1='12.68' y2='55.34' gradientUnits='userSpaceOnUse'>"
          "<stop stop-color='#fff' offset='0'/>"
          "<stop stop-color='#8e8e8e' offset='1'/>"
        "</linearGradient>"
      "</defs>"
      "<g transform='matrix(1.0184 0 0 1.0184 -1.1056 -.81075)'>"
        "<path transform='matrix(1 0 0 .9473 0 3.2114)' d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='none' stroke='#000' stroke-width='4'/>"
        "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='#000' stroke-width='8'/>"
        "<path d='m44 48v-8s0-4 6-4 6 4 6 4v8z' fill='none' stroke='url(#b)' stroke-width='4'/>"
        "<path d='m40 56c-4-4 0-12 0-12h20s4 8 0 12-16 4-20 0z' fill='url(#a)'/>"
        "<path d='m50 49c-1 0-2 1-2 2s0 1 1 2c0 2 0 3 1 3s1-1 1-3c1-1 1-1 1-2s-1-2-2-2z'/>"
        "<path d='m40 44h20' fill='none' stroke='#000'/>"
      "</g>"
    "</svg>";

    switch (idx) {
      case ICN_DIR:
        return svg_data_folder_generic;
      case ICN_FILE:
        return svg_data_file_generic;
      case ICN_LINK:
        return svg_data_overlay_link;
      case ICN_LOCK:
        return svg_data_overlay_lock;
      case ICN_BLK:
      case ICN_CHR:
        return svg_data_file_device;
      case ICN_SOCK:
      case ICN_PIPE:
        return svg_data_file_pipe;
      default:
        break;
    }

    return NULL;
  }

  // Handle drawing all cells in table
  void draw_cell(TableContext context, int R=0, int C=0, int X=0, int Y=0, int W=0, int H=0)
  {
    const int icon_blend_W = 8;

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
          Fl_SVG_Image *svg = icon_blend_[0];
          int fw = 0;

          if (row_selected(R)) {
            svg = icon_blend_[1];
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
          fl_color(textcol);
          fl_draw(rowdata_.at(R).cols[C], X, Y + 2, W, H - 2, al, NULL, 0);

          // blend over long text at the end of name column
          if (C == COL_NAME && svg && fw > W) {
            if (svg->w() != icon_blend_W || svg->h() != H) {
              svg->resize(icon_blend_W, H);
              col_resize_min(col_name_extra_w() + svg->w());
            }
            svg->draw(X + W - (col_name_extra_w() + svg->w()), Y);
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
      rowdata_.at(i).selected = row_selected(i) ? true : false;
    }

    // sort data while preserving order between equal elements
    std::stable_sort(rowdata_.begin(), rowdata_.end(), sort(col, reverse));

    // update table row selection from rowdata_
    for (int i = 0; i < rows(); ++i) {
      select_row(i, rowdata_.at(i).selected);
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

  std::string last_clicked_item()
  {
    std::string s;

    if (open_directory_.empty()) {
      s = rowdata_[last_row_clicked_].cols[COL_NAME];
    } else {
      s = open_directory_;
      if (s.back() != '/') s.push_back('/');
      s += rowdata_[last_row_clicked_].cols[COL_NAME];
    }

    return s;
  }

  // set a callback to handle double-clicks on entries
  virtual void double_click_callback()
  {
    if (!rowdata_[last_row_clicked_].isdir()) {
      selection_ = last_clicked_item();
      window()->hide();
      return;
    }

    load_dir(last_clicked_item().c_str());
  }

  // return pointer to icon set for row entry;
  // setting the icons is done in sub-classes
  virtual Fl_SVG_Image *icon(Row_t) const { return NULL; }

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

    if (empty(directory)) {
      return NULL;
    }

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

    if (!path || *path == 0) {
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
      if (!cdn) return NULL;

      in.insert(0, 1, '/');
      in.insert(0, cdn);
      free(cdn);
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
  filetable_(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Table_Row(X, Y, W, H, L)
  {
    label_header_[COL_NAME] = STR_NAME;
    label_header_[COL_SIZE] = STR_SIZE;
    label_header_[COL_LAST_MOD] = STR_LAST_MOD;

    color(FL_WHITE, fl_rgb_color(0x41, 0x69, 0xE1));

    // icon_blend_[0]
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

    icon_blend_[0] = new Fl_SVG_Image(NULL, svg_data1);
    icon_blend_[0]->proportional = false;

    if (icon_blend_[0]->fail()) {
      delete icon_blend_[0];
      icon_blend_[0] = NULL;
    }

    // icon_blend_[1]
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

    icon_blend_[1] = new Fl_SVG_Image(NULL, svg_data2);
    icon_blend_[1]->proportional = false;

    if (icon_blend_[1]->fail()) {
      delete icon_blend_[1];
      icon_blend_[1] = NULL;
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
    if (icon_blend_[0]) delete icon_blend_[0];
    if (icon_blend_[1]) delete icon_blend_[1];
  }

  void clear()
  {
    Fl_Table_Row::clear();

    // clear rowdata_ entirely

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
    int fd = -1;

    // calling load_dir(NULL) acts as a "refresh" using
    // the current open_directory_
    if (empty(dirname) && open_directory_.empty()) {
      return false;
    }

    { std::string new_dir;  // used only within this scope

      if (dirname) {
        new_dir = simplify_directory_path(dirname);

        if (new_dir.empty()) {
          // fall back to using realpath() if needed
          char *rp = realpath(dirname, NULL);
          new_dir = rp ? rp : dirname;
          free(rp);
        }
      } else {
        new_dir = open_directory_;
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

    while ((dir = readdir(d)) != NULL) {
      struct stat st, lst;
      Row_t row;
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
          if (!filter_show_entry(dir->d_name)) {
            continue;
          }

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

      rowdata_.push_back(row);
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

  // add file extension to filter
  void add_filter(const char *str)
  {
    if (empty(str)) {
      return;
    }

    std::string ext;
    if (str[0] != '.') ext = ".";
    ext += str;

    filter_list_.push_back(ext);
  }

  // add file extensions to filter
  void add_filter_list(const char *list, const char *delim)
  {
    char *tok;

    if (empty(list) || empty(delim)) {
      return;
    }

    char *copy = strdup(list);

    for (char *str = copy; ; str = NULL) {
      if ((tok = strtok(str, delim)) == NULL) {
        break;
      }

      add_filter(tok);
    }

    free(copy);
  }

  const char *open_directory() const {
    return open_directory_.empty() ? NULL : open_directory_.c_str();
  }

  virtual void load_default_icons() {}

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
  const char *label_header(int idx) const {
    return (idx >= 0 && idx < COL_MAX) ? label_header_[idx] : NULL;
  }

  int autowidth_padding() const { return autowidth_padding_; }
  int autowidth_max() const { return autowidth_max_; }
  bool show_hidden() const { return show_hidden_; }
};

bool filetable_::within_double_click_timelimit_ = false;

} // namespace fltk

#endif  // filetable__hpp
