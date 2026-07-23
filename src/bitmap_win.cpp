#include "repl/bitmap_win.h"
#include "Spectrogram.h"
#include "VectorFont.h"
#include "Waveform.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include <algorithm>
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
        labels_.clear();
        invalidate_cache();
        redraw();
    }

    void set_rgb_labeled(const uint8_t *rgb, int w, int h,
                         std::vector<ReplVectorLabel> labels) {
        if (w <= 0 || h <= 0 || !rgb) return;
        src_.assign(rgb, rgb + (size_t)w * h * 3);
        src_w_ = w;
        src_h_ = h;
        labels_ = std::move(labels);
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
        labels_.clear();
        invalidate_cache();
        redraw();
    }

    void clear() {
        src_.clear();
        labels_.clear();
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
        if (!labels_.empty()) {
            fl_push_clip(dx, dy, dw, dh);
            for (const ReplVectorLabel &label : labels_) {
                repl_draw_vector_text_fltk(
                    label.text.c_str(),
                    dx + static_cast<float>(label.x * s),
                    dy + static_cast<float>(label.y * s),
                    static_cast<float>(label.cellHeight * s),
                    label.red, label.green, label.blue);
            }
            fl_pop_clip();
        }
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
    std::vector<ReplVectorLabel> labels_;
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
 * and just as fast in practice as a hash map.
 */
static std::vector<bitmap_win_t *> g_registry;

static bitmap_win_t *find_or_create(const char *name) {
    std::string key = name ? name : "bitmap";

    for (size_t i = 0; i < g_registry.size(); ++i) {
        if (g_registry[i]->name == key) return g_registry[i];
    }

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

int bitmap_win_set_spectrogram(bitmap_win_t *bw, const float *samples,
                               int frames, int channels, int channel,
                               int width, int height) {
    if (!bw) return -1;
    std::vector<uint8_t> rgb;
    if (!repl_render_spectrogram_rgb(samples, frames, channels, channel,
                                     width, height, rgb)) return -1;
    bw->view->set_rgb(rgb.data(), width, height);
    return 0;
}

int bitmap_win_set_spectrogram_labeled(bitmap_win_t *bw, const float *samples,
                                       int frames, int channels, int channel,
                                       int width, int height,
                                       const char *title) {
    return bitmap_win_set_spectrogram_labeled_ex(
        bw, samples, frames, channels, channel, width, height, title, 0.0f);
}

int bitmap_win_set_spectrogram_labeled_ex(bitmap_win_t *bw,
                                          const float *samples, int frames,
                                          int channels, int channel, int width,
                                          int height, const char *title,
                                          float sample_rate) {
    if (!bw) return -1;
    if (width < 240 || height < 120) {
        return bitmap_win_set_spectrogram(bw, samples, frames, channels,
                                          channel, width, height);
    }
    const int bandHeight = std::clamp(height / 7, 36, 56);
    const int plotHeight = height - bandHeight * 2;
    std::vector<uint8_t> plot;
    std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3, 0);
    ReplSpectralMetrics spectralMetrics;
    std::vector<ReplVectorLabel> labels;
    if (!repl_render_spectrogram_rgb(samples, frames, channels, channel,
                                     width, plotHeight, plot,
                                     &spectralMetrics)) {
        return -1;
    }
    for (int y = 0; y < plotHeight; ++y) {
        std::memcpy(&rgb[(static_cast<size_t>(y + bandHeight) * width) * 3],
                    &plot[(static_cast<size_t>(y) * width) * 3],
                    static_cast<size_t>(width) * 3);
    }
    repl_annotate_spectrogram_rgb(rgb, width, height, title, samples, frames,
                                  channels, channel, &spectralMetrics,
                                  sample_rate, &labels);
    bw->view->set_rgb_labeled(rgb.data(), width, height, std::move(labels));
    return 0;
}

int bitmap_win_set_waveform(bitmap_win_t *bw, const float *samples,
                            int frames, int channels, int channel,
                            int width, int height, const char *title,
                            int loop_start, int loop_end) {
    return bitmap_win_set_waveform_ex(bw, samples, frames, channels, channel,
                                      width, height, title, loop_start,
                                      loop_end, 0.0f);
}

int bitmap_win_set_waveform_ex(bitmap_win_t *bw, const float *samples,
                               int frames, int channels, int channel,
                               int width, int height, const char *title,
                               int loop_start, int loop_end,
                               float sample_rate) {
    if (!bw) return -1;
    std::vector<uint8_t> rgb;
    std::vector<ReplVectorLabel> labels;
    if (!repl_render_waveform_rgb(samples, frames, channels, channel,
                                  width, height, title, loop_start, loop_end,
                                  rgb, sample_rate, &labels)) return -1;
    bw->view->set_rgb_labeled(rgb.data(), width, height, std::move(labels));
    return 0;
}

void bitmap_win_clear(bitmap_win_t *bw) {
    if (!bw) return;
    bw->view->clear();
}

void bitmap_win_set_title(bitmap_win_t *bw, const char *title) {
    if (!bw || !title) return;
    /* Only changes what's shown in the window's title bar -- deliberately
     * does NOT touch bw->name, which is the registry lookup key set at
     * bitmap_win_get() time. Changing that here would silently break a
     * later bitmap_win_get() call using the original name. */
    bw->win->copy_label(title);
}

void bitmap_win_hide_all(void) {
    for (size_t i = 0; i < g_registry.size(); ++i) g_registry[i]->win->hide();
}

void bitmap_win_destroy_all(void) {
    for (size_t i = 0; i < g_registry.size(); ++i) {
        delete g_registry[i]->win; /* deletes the contained BitmapView too */
        delete g_registry[i];
    }
    g_registry.clear();
}

} // extern "C"
