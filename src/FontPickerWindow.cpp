#include "FontPickerWindow.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>

static FontPickerWindow *g_font_picker = nullptr;

static bool ci_contains(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    for (auto &c : h) c = (char)tolower((unsigned char)c);
    for (auto &c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

static Fl_Font find_font_by_name(const char *name) {
    if (!name || !*name) return (Fl_Font)-1;
    int n = Fl::set_fonts("*");
    for (int i = 0; i < n; ++i) {
        int attr = 0;
        const char *fname = Fl::get_font_name((Fl_Font)i, &attr);
        if (fname && ci_contains(fname, name)) {
            return (Fl_Font)i;
        }
    }
    return (Fl_Font)-1;
}

static bool font_looks_monospace(Fl_Font f, int size) {
    fl_font(f, size);
    double wi = fl_width("i");
    double wM = fl_width("M");
    double wl = fl_width("l");
    return wi > 0.0 && std::fabs(wi - wM) < 0.01 && std::fabs(wi - wl) < 0.01;
}

// ---------------------------------------------------------------------
// FontPreviewBox
// ---------------------------------------------------------------------
FontPreviewBox::FontPreviewBox(int x, int y, int w, int h, const char *l)
    : Fl_Box(x, y, w, h, l) {
    box(FL_THIN_DOWN_BOX);
}

void FontPreviewBox::setPreviewFont(const std::string &name, int size) {
    fontName_ = name;
    fontSize_ = size;
    redraw();
}

void FontPreviewBox::setColors(const ReplColors &c) {
    colors_ = c;
    color(repl_rgb_to_flcolor(c.bg));
    labelcolor(repl_rgb_to_flcolor(c.fg));
    redraw();
}

void FontPreviewBox::draw() {
    Fl_Box::draw();

    int bx = x() + 8;
    int by = y() + 8;
    int bw = w() - 16;
    int bh = h() - 16;

    fl_push_clip(bx, by, bw, bh);

    Fl_Font f = find_font_by_name(fontName_.c_str());
    if (f < 0) f = FL_COURIER;

    fl_font(f, fontSize_);
    fl_color(repl_rgb_to_flcolor(colors_.fg));

    int line_h = fl_height() + 2;
    if (line_h < 1) line_h = 1;
    int curr_y = by + fl_height() - fl_descent();

    int max_lines = bh / line_h;
    if (max_lines < 1) max_lines = 1;

    if (max_lines >= 4) {
        std::string line1 = fontName_ + " " + std::to_string(fontSize_) + "pt";
        std::string line2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz";
        std::string line3 = "0123456789 () [] {} + - * / = == != <>";
        std::string line4 = "Braille: ⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿⠏⠏⣿";

        fl_draw(line1.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line2.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line3.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line4.c_str(), bx, curr_y);
    } else if (max_lines == 3) {
        std::string line1 = fontName_ + " " + std::to_string(fontSize_) + "pt";
        std::string line2 = "AaBbCc 0123456789 () [] {} +=-";
        std::string line3 = "Braille: ⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿⠏⠏⣿";

        fl_draw(line1.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line2.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line3.c_str(), bx, curr_y);
    } else if (max_lines == 2) {
        std::string line1 = fontName_ + " " + std::to_string(fontSize_) + "pt";
        std::string line2 = "AaBbCc 01234 ⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿⠏⠏⣿";

        fl_draw(line1.c_str(), bx, curr_y); curr_y += line_h;
        fl_draw(line2.c_str(), bx, curr_y);
    } else {
        std::string line1 = std::to_string(fontSize_) + "pt ⢀⣴⣾⣿⣿⣷⣦⡀ ⣾⣿";
        fl_draw(line1.c_str(), bx, curr_y);
    }

    fl_pop_clip();
}

// ---------------------------------------------------------------------
// FontPickerWindow
// ---------------------------------------------------------------------
FontPickerWindow::FontPickerWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title) {

    titleBuf_ = title ? title : "Font Chooser";
    copy_label(titleBuf_.c_str());

    // Section labels
    lblFontList_ = new Fl_Box(0, 0, 10, 10, "Installed Fonts:");
    lblFontList_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lblFontList_->labelfont(FL_HELVETICA_BOLD);

    lblSize_ = new Fl_Box(0, 0, 10, 10, "Size (pt):");
    lblSize_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lblSize_->labelfont(FL_HELVETICA_BOLD);

    lblPreview_ = new Fl_Box(0, 0, 10, 10, "Sample Preview:");
    lblPreview_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lblPreview_->labelfont(FL_HELVETICA_BOLD);

    // Font list browser
    fontBrowser_ = new Fl_Hold_Browser(0, 0, 10, 10);

    // Monospace checkbox
    monoCheck_ = new Fl_Check_Button(0, 0, 10, 10, "Monospace fonts only");
    monoCheck_->value(1);

    // Size spinner
    sizeSpinner_ = new Fl_Spinner(0, 0, 10, 10);
    sizeSpinner_->range(8, 72);
    sizeSpinner_->step(1);
    sizeSpinner_->value(14);

    // Size preset pills
    int presets[] = {10, 11, 12, 13, 14, 16, 18, 24};
    for (int p : presets) {
        std::string lbl = std::to_string(p) + "pt";
        Fl_Button *btn = new Fl_Button(0, 0, 10, 10, strdup(lbl.c_str()));
        btn->callback([](Fl_Widget *w, void *v) {
            FontPickerWindow *self = (FontPickerWindow *)v;
            int sz = atoi(w->label());
            if (sz > 0) {
                self->sizeSpinner_->value(sz);
                self->updatePreview();
            }
        }, this);
        presetBtns_.push_back(btn);
    }

    // Live preview box
    previewBox_ = new FontPreviewBox(0, 0, 10, 10);

    // Action buttons
    btnCancel_ = new Fl_Button(0, 0, 10, 10, "Cancel");
    btnApply_  = new Fl_Button(0, 0, 10, 10, "Apply");
    btnOK_     = new Fl_Button(0, 0, 10, 10, "OK");

    end();
    resizable(this);

    layoutWidgets();
    populateFonts();

    // Callbacks
    fontBrowser_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        self->updatePreview();
    }, this);

    monoCheck_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        self->populateFonts();
    }, this);

    sizeSpinner_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        self->updatePreview();
    }, this);

    btnApply_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        if (self->onApply_) {
            self->onApply_(self->currentFontName_, self->currentFontSize_);
        }
    }, this);

    btnOK_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        if (self->onApply_) {
            self->onApply_(self->currentFontName_, self->currentFontSize_);
        }
        self->hide();
    }, this);

    btnCancel_->callback([](Fl_Widget *, void *v) {
        FontPickerWindow *self = (FontPickerWindow *)v;
        self->hide();
    }, this);

    ReplColors dark_c;
    dark_c.bg = 0x181A1F;
    dark_c.fg = 0xABB2BF;
    dark_c.prompt = 0x61AFEF;
    dark_c.input = 0x98C379;
    setColors(dark_c);
}

FontPickerWindow::~FontPickerWindow() {
    if (g_font_picker == this) g_font_picker = nullptr;
}

void FontPickerWindow::resize(int x, int y, int w, int h) {
    Fl_Double_Window::resize(x, y, w, h);
    layoutWidgets();
}

void FontPickerWindow::layoutWidgets() {
    int W = w();
    int H = h();
    int pad = 14;

    int right_col_w = 150;
    int left_col_w = W - pad * 3 - right_col_w;
    if (left_col_w < 200) left_col_w = 200;

    int bottom_bar_h = 42;
    int preview_h = 100;
    int top_y = 32;

    int mid_h = H - top_y - preview_h - bottom_bar_h - pad * 3;
    if (mid_h < 130) mid_h = 130;

    // Left Column: Installed Fonts List
    lblFontList_->resize(pad, 10, left_col_w, 20);
    fontBrowser_->resize(pad, top_y, left_col_w, mid_h - 30);
    monoCheck_->resize(pad, top_y + mid_h - 26, left_col_w, 24);

    // Right Column: Size & Presets
    int right_x = pad * 2 + left_col_w;
    lblSize_->resize(right_x, 10, right_col_w, 20);
    sizeSpinner_->resize(right_x, top_y, right_col_w, 30);

    // Quick size buttons grid
    int btn_w = (right_col_w - 6) / 2;
    int btn_y = top_y + 42;
    for (size_t i = 0; i < presetBtns_.size(); ++i) {
        int r = (int)i / 2;
        int c = (int)i % 2;
        presetBtns_[i]->resize(right_x + c * (btn_w + 6), btn_y + r * 32, btn_w, 28);
    }

    // Preview Box
    int preview_y = top_y + mid_h + pad + 20;
    lblPreview_->resize(pad, preview_y - 20, W - pad * 2, 18);
    previewBox_->resize(pad, preview_y, W - pad * 2, preview_h);

    // Action Buttons at Bottom
    int btn_y_bot = H - pad - 32;
    btnOK_->resize(W - pad - 84, btn_y_bot, 84, 32);
    btnApply_->resize(W - pad - 84 - 8 - 84, btn_y_bot, 84, 32);
    btnCancel_->resize(W - pad - 84 - 8 - 84 - 8 - 84, btn_y_bot, 84, 32);
}

void FontPickerWindow::setColors(const ReplColors &c) {
    colors_ = c;
    Fl_Color bg = repl_rgb_to_flcolor(c.bg);
    Fl_Color fg = repl_rgb_to_flcolor(c.fg);

    color(bg);
    lblFontList_->labelcolor(fg);
    lblSize_->labelcolor(fg);
    lblPreview_->labelcolor(fg);

    fontBrowser_->color(bg);
    fontBrowser_->textcolor(fg);
    previewBox_->setColors(c);
    monoCheck_->labelcolor(fg);
    redraw();
}

void FontPickerWindow::setInitialFont(const std::string &fontName, int fontSize) {
    currentFontName_ = fontName;
    currentFontSize_ = fontSize;
    sizeSpinner_->value(fontSize);

    for (int i = 1; i <= fontBrowser_->size(); ++i) {
        const char *text = fontBrowser_->text(i);
        if (text && ci_contains(text, fontName)) {
            fontBrowser_->select(i);
            break;
        }
    }
    updatePreview();
}

void FontPickerWindow::populateFonts() {
    fontBrowser_->clear();
    int count = Fl::set_fonts("*");
    bool monoOnly = (monoCheck_->value() != 0);

    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) {
        int attr = 0;
        const char *fname = Fl::get_font_name((Fl_Font)i, &attr);
        if (!fname || !*fname) continue;
        if (monoOnly && !font_looks_monospace((Fl_Font)i, 14)) continue;

        if (std::find(names.begin(), names.end(), std::string(fname)) == names.end()) {
            names.push_back(fname);
        }
    }

    std::sort(names.begin(), names.end());

    int selected_idx = 1;
    for (size_t i = 0; i < names.size(); ++i) {
        fontBrowser_->add(names[i].c_str());
        if (ci_contains(names[i], currentFontName_)) {
            selected_idx = (int)i + 1;
        }
    }

    if (fontBrowser_->size() > 0) {
        fontBrowser_->select(selected_idx);
    }
    updatePreview();
}

void FontPickerWindow::updatePreview() {
    int sel = fontBrowser_->value();
    if (sel > 0 && sel <= fontBrowser_->size()) {
        const char *text = fontBrowser_->text(sel);
        if (text) currentFontName_ = text;
    }
    currentFontSize_ = (int)sizeSpinner_->value();
    previewBox_->setPreviewFont(currentFontName_, currentFontSize_);
}

FontPickerWindow *font_picker_get_or_create() {
    if (!g_font_picker) {
        g_font_picker = new FontPickerWindow(580, 460);
    }
    return g_font_picker;
}

void font_picker_close() {
    if (g_font_picker) {
        g_font_picker->hide();
    }
}
