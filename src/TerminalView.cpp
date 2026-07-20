#include "TerminalView.h"
#include <FL/fl_ask.H>
#include <cstring>

namespace {

// FLTK's buffer modify callback has C linkage requirements (plain
// function pointer), so this free function forwards to the widget's
// onBufferModified() method, which keeps the style buffer in sync
// byte-for-byte with the text buffer on every insert/delete -- both
// ones we make programmatically (appendStyled) and ones the user
// makes by typing, pasting, or backspacing.
void s_modify_cb(int pos, int nInserted, int nDeleted, int nRestyled,
                  const char *deletedText, void *cbArg) {
    (void)nRestyled;
    (void)deletedText;
    static_cast<TerminalView *>(cbArg)->onBufferModifiedTrampolineImpl(pos, nInserted, nDeleted);
}

} // namespace

void TerminalView::onBufferModifiedTrampolineImpl(int pos, int nInserted, int nDeleted) {
    if (nDeleted > 0) {
        style_->remove(pos, pos + nDeleted);
    }
    if (nInserted > 0) {
        char defaultStyle = (pos >= input_start_) ? 'C' : 'A';
        std::string s((size_t)nInserted, defaultStyle);
        style_->insert(pos, s.c_str());
    }
}

TerminalView::TerminalView(int x, int y, int w, int h)
    : Fl_Text_Editor(x, y, w, h) {
    buffer_ = new Fl_Text_Buffer();
    style_ = new Fl_Text_Buffer();
    buffer(buffer_);
    buffer_->add_modify_callback(s_modify_cb, this);

    colors_ = repl_theme_defaults(true);
    wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    cursor_style(Fl_Text_Display::NORMAL_CURSOR);
    linenumber_width(0);
    rebuildStyleTable();
}

TerminalView::~TerminalView() {
    buffer_->remove_modify_callback(s_modify_cb, this);
    buffer(nullptr);
    delete buffer_;
    delete style_;
}

void TerminalView::rebuildStyleTable() {
    static Fl_Text_Display::Style_Table_Entry table[3];
    table[0] = { repl_rgb_to_flcolor(colors_.fg), font_, font_size_, 0 };     // 'A' output
    table[1] = { repl_rgb_to_flcolor(colors_.prompt), font_, font_size_, 0 }; // 'B' prompt
    table[2] = { repl_rgb_to_flcolor(colors_.input), font_, font_size_, 0 };  // 'C' input echo

    highlight_data(style_, table, 3, 'A', nullptr, nullptr);

    color(repl_rgb_to_flcolor(colors_.bg));
    textfont(font_);
    textsize(font_size_);
    textcolor(repl_rgb_to_flcolor(colors_.fg));
    cursor_color(repl_rgb_to_flcolor(colors_.cursor));
    redraw();
}

void TerminalView::setColors(const ReplColors &c) {
    colors_ = c;
    rebuildStyleTable();
}

void TerminalView::setFont(Fl_Font font, int size) {
    font_ = font;
    font_size_ = size;
    rebuildStyleTable();
}

void TerminalView::appendStyled(const std::string &utf8, char styleChar) {
    if (utf8.empty()) return;
    int pos = buffer_->length();
    buffer_->insert(pos, utf8.c_str());
    int len = (int)utf8.size();
    style_->replace(pos, pos + len, std::string((size_t)len, styleChar).c_str());
}

void TerminalView::appendOutput(const std::string &utf8) {
    appendStyled(utf8, 'A');
    insert_position(buffer_->length());
    show_insert_position();
}

void TerminalView::showPrompt() {
    appendStyled(prompt_, 'B');
    input_start_ = buffer_->length();
    insert_position(input_start_);
    show_insert_position();
    redraw();
}

void TerminalView::clearAll() {
    buffer_->text("");
    style_->text("");
    input_start_ = 0;
}

std::string TerminalView::liveText() const {
    char *raw = buffer_->text_range(input_start_, buffer_->length());
    std::string s = raw ? raw : "";
    if (raw) free(raw);
    return s;
}

void TerminalView::replaceLiveText(const std::string &s) {
    buffer_->replace(input_start_, buffer_->length(), s.c_str());
    insert_position(buffer_->length());
    show_insert_position();
}

void TerminalView::snapCursorToEndIfBeforeInput() {
    int selStart = 0, selEnd = 0;
    if (buffer_->selection_position(&selStart, &selEnd)) {
        if (selStart < input_start_) {
            buffer_->unselect();
        }
    }
    if (insert_position() < input_start_) {
        insert_position(buffer_->length());
        show_insert_position();
    }
}

void TerminalView::moveHistory(int direction) {
    if (history_.empty()) return;

    if (history_pos_ == -1) {
        if (direction > 0) return; // nothing to go "forward" to
        saved_live_edit_ = liveText();
        history_pos_ = (int)history_.size() - 1;
        replaceLiveText(history_[(size_t)history_pos_]);
        return;
    }

    history_pos_ += direction;
    if (history_pos_ < 0) {
        history_pos_ = 0;
    }
    if (history_pos_ >= (int)history_.size()) {
        history_pos_ = -1;
        replaceLiveText(saved_live_edit_);
        return;
    }
    replaceLiveText(history_[(size_t)history_pos_]);
}

int TerminalView::handle(int event) {
    if (event == FL_KEYBOARD) {
        int key = Fl::event_key();

        if (key == FL_Enter || key == FL_KP_Enter) {
            std::string line = liveText();
            appendStyled("\n", 'A');
            if (!line.empty()) {
                history_.push_back(line);
            }
            history_pos_ = -1;
            saved_live_edit_.clear();
            if (onLine_) onLine_(line);
            return 1;
        }
        if (key == FL_Up) { moveHistory(-1); return 1; }
        if (key == FL_Down) { moveHistory(1); return 1; }
        if (key == FL_Home) {
            insert_position(input_start_);
            show_insert_position();
            return 1;
        }
        if (key == FL_BackSpace) {
            if (insert_position() <= input_start_) return 1;
        }
        if (key == FL_Left) {
            if (insert_position() <= input_start_) return 1;
        }
        if (key != FL_Right) {
            snapCursorToEndIfBeforeInput();
        }
    } else if (event == FL_PASTE) {
        snapCursorToEndIfBeforeInput();
    }
    return Fl_Text_Editor::handle(event);
}
