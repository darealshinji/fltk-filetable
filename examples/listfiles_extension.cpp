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

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <cassert>

#include "fltk_filetable_extension.hpp"

#include "Folder_generic.svg.h"
#include "File_Generic.svg.h"
#include "Overlay_link.svg.h"
#include "File_Text.svg.h"
#include "File_Image_3.svg.h"
#include "File_Video.svg.h"
#include "File_Audio_2.svg.h"
#include "File_Archive.svg.h"


static void up_cb(Fl_Widget *, void *v)
{
  assert(v != NULL);
  reinterpret_cast<fltk::filetable_extension *>(v)->dir_up();
}

static void refresh_cb(Fl_Widget *, void *v)
{
  assert(v != NULL);
  reinterpret_cast<fltk::filetable_extension *>(v)->refresh();
}

static void hidden_cb(Fl_Widget *, void *v)
{
  assert(v != NULL);
  fltk::filetable_extension *t = reinterpret_cast<fltk::filetable_extension *>(v);
  t->show_hidden(t->show_hidden() ? false : true);
  t->refresh();
}


#define MARGIN 20

int main()
{
  const char *imgs = "jpg/jpeg/png/gif/bmp/tif/tiff/svg/svgz/xpm/xbm/ico/icns",
    *arch = "zip/7z/rar/gz/bz2/xz/tar/tgz/txz/tbz/ar/cpio/rpm/deb/apk/z",
    *vid = "avi/mkv/mp4/wmv/ogv/ogm/mpg/mpeg/ts/mov/vob",
    *aud = "MP3/OGG/.aac/m4a/Wav/mka/fLaC",
    *txt = "txt/md/sh/py/pl/rb/bat/yml/c/h/cpp/cxx/hpp/hxx/cs/pas/lfm/lpi/lpr/"
      "fl/mak/ac/m4/cmake/sln/vcxproj/xml/htm/html/css/js/json/php/ass/srt";

  const char *delim = "/";

  Fl_Double_Window win(900, 500, "Test");

    fltk::filetable_extension table(MARGIN, MARGIN, win.w()-MARGIN*2, win.h()-40-MARGIN*2);
    table.autowidth_max(table.w()/2);
    table.double_click_timeout(0.5);
    table.labelfont(FL_HELVETICA);
    table.labelsize(16);
    //table.add_filter(".cpp");
    //table.add_filter_list(".zip|.tar|rar||", "|");

    table.set_icon(NULL, folder_generic_svg, fltk::filetable_extension::ICN_DIR);
    table.set_icon(NULL, file_generic_svg, fltk::filetable_extension::ICN_FILE);
    table.set_icon(NULL, overlay_link_svg, fltk::filetable_extension::ICN_LINK);
    table.set_icon(NULL, file_text_svg, txt, delim);
    table.set_icon(NULL, file_image_3_svg, imgs, delim);
    table.set_icon(NULL, file_video_svg, vid, delim);
    table.set_icon(NULL, file_audio_2_svg, aud, delim);
    table.set_icon(NULL, file_archive_svg, arch, delim);

    table.load_dir();

    Fl_Button up(MARGIN, table.h() + MARGIN*2-10, 40, 36, "Up");
    up.callback(up_cb, &table);

    Fl_Button refresh(up.x()+up.w()+MARGIN/2, up.y(), 64, 36, "Refresh");
    refresh.callback(refresh_cb, &table);

    Fl_Button hidden(refresh.x()+refresh.w()+MARGIN/2, up.y(), 110, 36, "Toggle hidden");
    hidden.callback(hidden_cb, &table);

  win.end();
  win.resizable(table);

  win.show();

  int rv = Fl::run();

  if (table.selection()) {
    printf("File selected: %s\n", table.selection());
  } else {
    printf("No file selected.\n");
  }

  return rv;
}


