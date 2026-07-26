#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <functional>
#include <string>
#include <vector>

#include "Theme.h"

class FontPreviewBox : public Fl_Box {
public:
    FontPreviewBox(int x, int y, int w, int h, const char *l = nullptr);
    void setPreviewFont(const std::string &name, int size);
    void setColors(const ReplColors &c);
    void draw() override;
private:
    std::string fontName_ = "Courier";
    int fontSize_ = 14;
    ReplColors colors_;
};

class FontPickerWindow : public Fl_Double_Window {
public:
    FontPickerWindow(int w = 580, int h = 460, const char *title = "Font Chooser");
    ~FontPickerWindow() override;

    void setInitialFont(const std::string &fontName, int fontSize);
    void setColors(const ReplColors &c);
    void resize(int x, int y, int w, int h) override;

    void setApplyHandler(std::function<void(const std::string &fontName, int fontSize)> handler) {
        onApply_ = std::move(handler);
    }

private:
    void layoutWidgets();
    void populateFonts();
    void updatePreview();

    Fl_Box *lblFontList_ = nullptr;
    Fl_Box *lblSize_ = nullptr;
    Fl_Box *lblPreview_ = nullptr;
    Fl_Hold_Browser *fontBrowser_ = nullptr;
    Fl_Spinner *sizeSpinner_ = nullptr;
    Fl_Check_Button *monoCheck_ = nullptr;
    FontPreviewBox *previewBox_ = nullptr;
    std::vector<Fl_Button*> presetBtns_;
    Fl_Button *btnApply_ = nullptr;
    Fl_Button *btnOK_ = nullptr;
    Fl_Button *btnCancel_ = nullptr;

    std::string currentFontName_ = "Courier";
    int currentFontSize_ = 14;
    std::string titleBuf_;
    ReplColors colors_;
    std::function<void(const std::string &, int)> onApply_;
};

FontPickerWindow *font_picker_get_or_create();
void font_picker_close();
