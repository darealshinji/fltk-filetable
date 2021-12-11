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

#ifndef mountbutton__hpp
#define mountbutton__hpp

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>


namespace fltk
{

class mountbutton : public Fl_Group
{
private:

  class bt : public Fl_Box
  {
  private:
    void *data_ = NULL;

  public:
    bt(int X, int Y, int W, int H, const char *L=NULL)
    : Fl_Box(X, Y, W, H, L) {}

    virtual ~bt() {}

    int handle(int e) {
      if (e == FL_PUSH) do_callback();
      return 0;
    }

    void do_callback() {
      // use the parent widget
      Fl_Box::do_callback(parent(), data_);
    }

    void callback(Fl_Callback *cb, void *data=NULL) {
      data_ = data;
      Fl_Box::callback(cb, data);
    }

    Fl_Callback *callback() { return Fl_Box::callback(); }
  };

  bt *b_, *ov_;
  Fl_Box *bg_;
  Fl_Callback *cb_ = NULL, *ov_cb_ = NULL;
  void *cb_data_ = NULL, *ov_cb_data_ = NULL;

public:
  mountbutton(int X, int Y, int W, int H, const char *L=NULL)
  : Fl_Group(X,Y,W,H, NULL)
  {
    // background
    bg_ = new Fl_Box(X, Y, W, H, NULL);

    // button
    b_ = new bt(X, Y, W-24, H, L);
    //b_->box(FL_BORDER_BOX);
    b_->align(FL_ALIGN_INSIDE|FL_ALIGN_LEFT);

    // overlay
    ov_ = new bt(X + (W-24), Y, 24, H);

    resizable(b_);
    end();
  }

  virtual ~mountbutton() {}

  int color() { return bg_->color(); }
  void color(int c) { bg_->color(c); }

  Fl_Callback *callback() { return b_->callback(); }
  void callback(Fl_Callback *cb, void *data=NULL)
  {
    cb_ = cb;
    cb_data_ = data;
    b_->callback(cb, data);

    if (!overlay()) {
      ov_->callback(cb, data);
    }
  }

  Fl_Callback *callback_overlay() { return ov_->callback(); }
  void callback_overlay(Fl_Callback *cb, void *data=NULL) {
    ov_cb_ = cb;
    ov_cb_data_ = data;
    ov_->callback(cb, data);
  }

  bool overlay() { return ov_->label(); }
  void overlay(bool b)
  {
    if (b) {
      ov_->label("@-18|>");
      ov_->callback(ov_cb_, ov_cb_data_);
    } else {
      ov_->label(NULL);
      ov_->callback(cb_, cb_data_);
    }
  }

  Fl_Boxtype box() { return bg_->box(); }
  void box(Fl_Boxtype b) { bg_->box(b); }

  int labelsize() { return b_->labelsize(); }
  void labelsize(int i) {
    b_->labelsize(i);
    ov_->labelsize(i);
  }
};

} // namespace fltk

#endif  // mountbutton__hpp


/*
static void button_cb(Fl_Widget *o, void *) {
  printf("button_cb\n");
  static_cast<fltk::mountbutton *>(o)->overlay(true);
}

static void ov_cb(Fl_Widget *o, void *) {
  printf("ov_cb\n");
  // if (!partition_mounted()) return;
  static_cast<fltk::mountbutton *>(o)->overlay(false);
}


int main()
{
  Fl_Window win(400, 300, "Test");

  fltk::mountbutton b(10, 10, 90, 30, "Button");
  b.callback(button_cb);
  b.callback_overlay(ov_cb);

  win.end();
  win.resizable(b);
  win.show();

  return Fl::run();
}
*/
