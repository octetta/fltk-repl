#pragma once

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <functional>
#include <string>

#include "Theme.h"

class EditorTextEditor : public Fl_Text_Editor {
public:
    EditorTextEditor(int x, int y, int w, int h, const char *label = nullptr);
    int handle(int event) override;
    void setEvalCallback(std::function<void()> cb) { onEval_ = std::move(cb); }
private:
    std::function<void()> onEval_;
};

class EditorWindow : public Fl_Double_Window {
public:
    EditorWindow(int w, int h, const char *title = "fltk-repl Editor");
    ~EditorWindow() override;

    bool openFile(const std::string &filepath);
    bool saveFile(const std::string &filepath = "");

    void setEvalHandler(std::function<void(const std::string &code)> handler) {
        onEvalHandler_ = std::move(handler);
    }

    void setColors(const ReplColors &c);
    void evalSelectionOrBuffer();

    const std::string &filepath() const { return filepath_; }

private:
    void updateTitleAndStatus();
    static void onModifyCallback(int pos, int nInserted, int nDeleted,
                                 int nRestyled, const char *deletedText, void *cbArg);

    Fl_Text_Buffer *buffer_ = nullptr;
    EditorTextEditor *editor_ = nullptr;
    Fl_Button *btnOpen_ = nullptr;
    Fl_Button *btnSave_ = nullptr;
    Fl_Button *btnRun_ = nullptr;
    Fl_Box *statusBox_ = nullptr;

    std::string filepath_;
    std::string titleBuf_;
    bool isModified_ = false;
    ReplColors colors_;
    std::function<void(const std::string &code)> onEvalHandler_;
};

EditorWindow *editor_win_get_or_create();
void editor_win_close();
