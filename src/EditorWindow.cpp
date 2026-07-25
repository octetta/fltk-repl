#include "EditorWindow.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>

#include "repl/repl_api.h"

static EditorWindow *g_editor_win = nullptr;

// ---------------------------------------------------------------------
// EditorTextEditor
// ---------------------------------------------------------------------
EditorTextEditor::EditorTextEditor(int x, int y, int w, int h, const char *label)
    : Fl_Text_Editor(x, y, w, h, label) {}

int EditorTextEditor::handle(int event) {
    if (event == FL_KEYBOARD) {
        int key = Fl::event_key();
        int state = Fl::event_state();
        if ((key == FL_Enter || key == 'r') && (state & (FL_CTRL | FL_COMMAND))) {
            if (onEval_) {
                onEval_();
                return 1;
            }
        }
    }
    return Fl_Text_Editor::handle(event);
}

// ---------------------------------------------------------------------
// EditorWindow
// ---------------------------------------------------------------------
EditorWindow::EditorWindow(int w, int h, const char *title)
    : Fl_Double_Window(w, h, title) {
    
    // Top bar: Open, Save, Run buttons
    btnOpen_ = new Fl_Button(8, 6, 75, 26, "Open...");
    btnSave_ = new Fl_Button(88, 6, 75, 26, "Save");
    btnRun_  = new Fl_Button(168, 6, 120, 26, "@> Run (Ctrl+Enter)");

    // Main text editor with line numbers
    buffer_ = new Fl_Text_Buffer();
    editor_ = new EditorTextEditor(8, 38, w - 16, h - 70);
    editor_->buffer(buffer_);
    editor_->linenumber_width(45);
    editor_->textfont(FL_COURIER);
    editor_->textsize(14);

    editor_->setEvalCallback([this]() {
        evalSelectionOrBuffer();
    });

    // Bottom status bar
    statusBox_ = new Fl_Box(8, h - 28, w - 16, 22, "Untitled");
    statusBox_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    statusBox_->labelsize(12);

    end();
    resizable(editor_);

    buffer_->add_modify_callback(onModifyCallback, this);

    // Button callbacks
    btnOpen_->callback([](Fl_Widget *, void *v) {
        EditorWindow *self = (EditorWindow *)v;
        char *path = repl_open_file_dialog(nullptr, "Open File", "All Files\t*");
        if (path) {
            self->openFile(path);
            repl_free_string(path);
        }
    }, this);

    btnSave_->callback([](Fl_Widget *, void *v) {
        EditorWindow *self = (EditorWindow *)v;
        if (self->filepath_.empty()) {
            char *path = repl_save_file_dialog(nullptr, "Save File", "All Files\t*");
            if (path) {
                self->saveFile(path);
                repl_free_string(path);
            }
        } else {
            self->saveFile();
        }
    }, this);

    btnRun_->callback([](Fl_Widget *, void *v) {
        EditorWindow *self = (EditorWindow *)v;
        self->evalSelectionOrBuffer();
    }, this);

    ReplColors dark_c;
    dark_c.bg = 0x181A1F;
    dark_c.fg = 0xABB2BF;
    dark_c.prompt = 0x61AFEF;
    dark_c.input = 0x98C379;
    setColors(dark_c);
}

EditorWindow::~EditorWindow() {
    if (g_editor_win == this) g_editor_win = nullptr;
    delete buffer_;
}

void EditorWindow::setColors(const ReplColors &c) {
    colors_ = c;
    color(repl_rgb_to_flcolor(c.bg));
    editor_->color(repl_rgb_to_flcolor(c.bg));
    editor_->textcolor(repl_rgb_to_flcolor(c.fg));
    editor_->cursor_color(repl_rgb_to_flcolor(c.input));
    statusBox_->labelcolor(repl_rgb_to_flcolor(c.fg));
    redraw();
}

void EditorWindow::onModifyCallback(int pos, int nInserted, int nDeleted,
                                    int, const char *, void *cbArg) {
    EditorWindow *self = (EditorWindow *)cbArg;
    if (nInserted > 0 || nDeleted > 0) {
        if (!self->isModified_) {
            self->isModified_ = true;
            self->updateTitleAndStatus();
        }
    }
}

void EditorWindow::updateTitleAndStatus() {
    std::string name = filepath_.empty() ? "Untitled" : filepath_;
    if (isModified_) name += " *";
    titleBuf_ = "fltk-repl Editor - " + name;
    copy_label(titleBuf_.c_str());
    statusBox_->copy_label(name.c_str());
}

bool EditorWindow::openFile(const std::string &filepath) {
    if (buffer_->loadfile(filepath.c_str()) == 0) {
        filepath_ = filepath;
        isModified_ = false;
        updateTitleAndStatus();
        return true;
    }
    return false;
}

bool EditorWindow::saveFile(const std::string &filepath) {
    std::string target = filepath.empty() ? filepath_ : filepath;
    if (target.empty()) return false;

    if (buffer_->savefile(target.c_str()) == 0) {
        filepath_ = target;
        isModified_ = false;
        updateTitleAndStatus();
        return true;
    }
    return false;
}

void EditorWindow::evalSelectionOrBuffer() {
    std::string code;
    if (buffer_->selected()) {
        char *sel = buffer_->selection_text();
        if (sel) {
            code = sel;
            free(sel);
        }
    } else {
        char *txt = buffer_->text();
        if (txt) {
            code = txt;
            free(txt);
        }
    }

    if (code.empty()) return;

    if (onEvalHandler_) {
        onEvalHandler_(code);
    }
}

EditorWindow *editor_win_get_or_create() {
    if (!g_editor_win) {
        g_editor_win = new EditorWindow(720, 520);
    }
    return g_editor_win;
}

void editor_win_close() {
    if (g_editor_win) {
        g_editor_win->hide();
    }
}
