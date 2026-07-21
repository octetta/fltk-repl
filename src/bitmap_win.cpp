#include "bitmap_win.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

/* A widget that owns its pixel buffer and draws it scaled-to-fit with
 * letterboxing, preserving aspect ratio, instead of Fl_Box's default
 * center-at-natural-size behavior. */
class BitmapView : public Fl_Widget {
public:
    BitmapView(int x, int y, int w, int h)
        : Fl_Widget(x, y, w, h), scaled_img_(NULL),
          src_w_(0), src_h_(0), scaled_w_(0), scaled_h_(0) {}

    ~BitmapView() override { delete scaled_img_; }

    void set_rgb(const uint8_t *rgb, int w, int h) {
        if (w <= 0 || h <= 0 || !rgb) return;
        src_.assign(rgb, rgb + (size_t)w * h * 3);
        src_w_ = w;
        src_h_ = h;
        invalidate_cache();
        redraw();
    }

    void set_gray(const uint8_t *gray, int w, int h) {
        if (w <= 0 || h <= 0 || !gray) return;
        src_.resize((size_t)w * h * 3);
        for (int i = 0; i < w * h; ++i) {
            uint8_t v = gray[i];
            src_[i * 3 + 0] = v;
            src_[i * 3 + 1] = v;
            src_[i * 3 + 2] = v;
        }
        src_w_ = w;
        src_h_ = h;
        invalidate_cache();
        redraw();
    }

    void clear() {
        src_.clear();
        src_w_ = src_h_ = 0;
        invalidate_cache();
        redraw();
    }

protected:
    void resize(int X, int Y, int W, int H) override {
        Fl_Widget::resize(X, Y, W, H);
        invalidate_cache(); /* dest size changed, cached scaled image is stale */
    }

    void draw() override {
        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());
        if (src_w_ <= 0 || src_h_ <= 0) return;

        /* Letterboxed destination rect, preserving aspect ratio. */
        double sx = (double)w() / src_w_;
        double sy = (double)h() / src_h_;
        double s = sx < sy ? sx : sy;
        int dw = (int)(src_w_ * s);
        int dh = (int)(src_h_ * s);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        int dx = x() + (w() - dw) / 2;
        int dy = y() + (h() - dh) / 2;

        ensure_scaled(dw, dh);
        if (scaled_img_) scaled_img_->draw(dx, dy);
    }

private:
    void invalidate_cache() {
        delete scaled_img_;
        scaled_img_ = NULL;
        scaled_w_ = scaled_h_ = 0;
    }

    /* Nearest-neighbor resample src_ into a scaled_ buffer of size dw x dh,
     * cached until the destination size changes again. Avoids depending on
     * Fl_Image::scale(), which only exists from FLTK 1.4 onward -- this
     * works the same on 1.3 and 1.4+. Swap for a proper filter later if
     * quality matters more than simplicity (e.g. for zoomed-in waveforms). */
    void ensure_scaled(int dw, int dh) {
        if (scaled_img_ && dw == scaled_w_ && dh == scaled_h_) return;

        scaled_.resize((size_t)dw * dh * 3);
        for (int yy = 0; yy < dh; ++yy) {
            int sy = yy * src_h_ / dh;
            if (sy >= src_h_) sy = src_h_ - 1;
            for (int xx = 0; xx < dw; ++xx) {
                int sx2 = xx * src_w_ / dw;
                if (sx2 >= src_w_) sx2 = src_w_ - 1;
                const uint8_t *sp = &src_[((size_t)sy * src_w_ + sx2) * 3];
                uint8_t *dp = &scaled_[((size_t)yy * dw + xx) * 3];
                dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
            }
        }

        delete scaled_img_;
        /* Fl_RGB_Image does not take ownership of the buffer, so scaled_
         * must outlive scaled_img_ -- it does, as a member with the same
         * lifetime, only reassigned here right before the image is rebuilt. */
        scaled_img_ = new Fl_RGB_Image(scaled_.data(), dw, dh, 3);
        scaled_w_ = dw;
        scaled_h_ = dh;
    }

    std::vector<uint8_t> src_;    /* original pixels, RGB */
    std::vector<uint8_t> scaled_; /* cached resampled pixels, RGB */
    Fl_RGB_Image *scaled_img_;
    int src_w_, src_h_;
    int scaled_w_, scaled_h_;
};

/* Close callback: hide instead of destroying, so bitmap_win_show() can
 * re-show the same window/state later without recreating it. */
void close_cb(Fl_Widget *w, void *) {
    w->hide();
}

} // namespace

struct bitmap_win {
    std::string name;
    Fl_Double_Window *win;
    BitmapView *view;
};

/* --- registry -------------------------------------------------------
 * Deliberately a flat vector + linear search: the number of named bitmap
 * windows is expected to stay small (single digits), so this is simpler
 * and just as fast in practice as a hash map. Swap the storage here if
 * that assumption ever stops holding -- no caller changes needed either
 * way, since bitmap_win_get() is the only entry point.
 */
static std::vector<bitmap_win_t *> g_registry;

static bitmap_win_t *find_or_create(const char *name) {
    std::string key = name ? name : "bitmap";

    /* For now, collapse every name to a single shared window. Flip this to
     * a real per-name lookup by uncommenting the loop below and removing
     * the early-return-first-entry shortcut. */
    if (!g_registry.empty()) return g_registry.front();

    /*
    for (auto *bw : g_registry) {
        if (bw->name == key) return bw;
    }
    */

    const int default_w = 512, default_h = 384;
    bitmap_win_t *bw = new bitmap_win();
    bw->name = key;
    bw->win = new Fl_Double_Window(default_w, default_h, key.c_str());
    bw->view = new BitmapView(0, 0, default_w, default_h);
    bw->win->resizable(bw->view);
    bw->win->end();
    bw->win->callback(close_cb);

    g_registry.push_back(bw);
    return bw;
}

extern "C" {

bitmap_win_t *bitmap_win_get(const char *name) {
    return find_or_create(name);
}

void bitmap_win_show(bitmap_win_t *bw) {
    if (!bw) return;
    bw->win->show();
}

void bitmap_win_hide(bitmap_win_t *bw) {
    if (!bw) return;
    bw->win->hide();
}

int bitmap_win_visible(const bitmap_win_t *bw) {
    return bw && bw->win->visible() ? 1 : 0;
}

void bitmap_win_set_rgb(bitmap_win_t *bw, const uint8_t *rgb, int w, int h) {
    if (!bw) return;
    bw->view->set_rgb(rgb, w, h);
}

void bitmap_win_set_gray(bitmap_win_t *bw, const uint8_t *gray, int w, int h) {
    if (!bw) return;
    bw->view->set_gray(gray, w, h);
}

void bitmap_win_clear(bitmap_win_t *bw) {
    if (!bw) return;
    bw->view->clear();
}

void bitmap_win_set_title(bitmap_win_t *bw, const char *title) {
    if (!bw || !title) return;
    bw->name = title;
    bw->win->label(bw->name.c_str());
}

} // extern "C"
