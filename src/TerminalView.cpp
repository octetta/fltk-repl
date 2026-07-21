#include "TerminalView.h"
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Item.H>
#include <cstring>
#include <cstdint>
#include <cctype>

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
    // Calculate luminance of background color to pick high-contrast output shade
    unsigned char r = (colors_.bg >> 16) & 0xFF;
    unsigned char g = (colors_.bg >> 8) & 0xFF;
    unsigned char b = colors_.bg & 0xFF;
    bool isDarkBg = ((0.299 * r + 0.587 * g + 0.114 * b) < 128);

    // Style 'A': Muted/slate contrast for output so live input ('C') stands out cleanly
    // Dark mode: Softened grayish-blue (0xA0AAB0)
    // Light mode: Dark slate gray (0x4A5568)
    uint32_t outputColor = isDarkBg ? 0xA0AAB0 : 0x4A5568;

    static Fl_Text_Display::Style_Table_Entry table[3];
    table[0].color = repl_rgb_to_flcolor(outputColor);
    table[0].font  = font_;
    table[0].size  = font_size_;
    table[0].attr  = 0;

    table[1].color = repl_rgb_to_flcolor(colors_.prompt);
    table[1].font  = font_;
    table[1].size  = font_size_;
    table[1].attr  = 0;

    table[2].color = repl_rgb_to_flcolor(colors_.input);
    table[2].font  = font_;
    table[2].size  = font_size_;
    table[2].attr  = 0;

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
    // Only snap cursor and unselect if we are NOT making/holding a text selection
    int selStart = 0, selEnd = 0;
    if (buffer_->selection_position(&selStart, &selEnd)) {
        return; // Preserve active highlight selection!
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
    // Right-click context menu (cross-platform: Linux, macOS, Windows)
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
        bool hasSelection = buffer_->selected();

        Fl_Menu_Item popup_menu[] = {
            { "Copy",       0, nullptr, nullptr, hasSelection ? 0 : FL_MENU_INACTIVE },
            { "Paste",      0, nullptr, nullptr, 0 },
            { "Clear Line", 0, nullptr, nullptr, 0 },
            { 0 } // Sentinel
        };

        const Fl_Menu_Item* m = popup_menu->popup(Fl::event_x(), Fl::event_y(), nullptr, nullptr, nullptr);

        if (m) {
            std::string label = m->label() ? m->label() : "";
            if (label == "Copy" && hasSelection) {
                char* text = buffer_->selection_text();
                if (text) {
                    Fl::copy(text, (int)strlen(text), 1); // Clipboard
                    Fl::copy(text, (int)strlen(text), 0); // Primary Selection
                    free(text);
                }
                buffer_->unselect();
                insert_position(buffer_->length());
                show_insert_position();
                redraw();
            } else if (label == "Paste") {
                snapCursorToEndIfBeforeInput();
                Fl::paste(*this, 1);
            } else if (label == "Clear Line") {
                replaceLiveText("");
            }
        }
        return 1;
    }

    if (event == FL_KEYBOARD) {
        int key = Fl::event_key();
        int state = Fl::event_state();

        // Handle Ctrl+C / Cmd+C: Copy text, clear selection, and snap cursor back to prompt
        if ((state & (FL_CTRL | FL_COMMAND)) && (key == 'c' || key == 'C')) {
            if (buffer_->selected()) {
                char* text = buffer_->selection_text();
                if (text) {
                    Fl::copy(text, (int)strlen(text), 1); // System Clipboard
                    Fl::copy(text, (int)strlen(text), 0); // Primary Selection
                    free(text);
                }
                buffer_->unselect();
            }
            insert_position(buffer_->length());
            show_insert_position();
            redraw();
            return 1;
        }

        // Ctrl+A or Home: Jump to start of prompt input
        if (key == FL_Home || ((state & (FL_CTRL | FL_COMMAND)) && (key == 'a' || key == 'A'))) {
            insert_position(input_start_);
            show_insert_position();
            return 1;
        }

        // Ctrl+E or End: Jump to end of prompt input
        if (key == FL_End || ((state & (FL_CTRL | FL_COMMAND)) && (key == 'e' || key == 'E'))) {
            insert_position(buffer_->length());
            show_insert_position();
            return 1;
        }

        // Unix line discard (Ctrl+U): Delete from cursor back to prompt start
        if ((state & (FL_CTRL | FL_COMMAND)) && (key == 'u' || key == 'U')) {
            int cur = insert_position();
            if (cur > input_start_) {
                buffer_->remove(input_start_, cur);
                insert_position(input_start_);
                show_insert_position();
            }
            return 1;
        }

        // Kill forward (Ctrl+K): Delete from cursor to end of input line
        if ((state & (FL_CTRL | FL_COMMAND)) && (key == 'k' || key == 'K')) {
            int cur = insert_position();
            int end = buffer_->length();
            if (cur >= input_start_ && cur < end) {
                buffer_->remove(cur, end);
            }
            return 1;
        }

        // Backward kill word (Ctrl+W): Delete word before cursor
        if ((state & (FL_CTRL | FL_COMMAND)) && (key == 'w' || key == 'W')) {
            int cur = insert_position();
            if (cur > input_start_) {
                int pos = cur;
                // Skip trailing whitespace before cursor
                while (pos > input_start_) {
                    char c = buffer_->byte_at(pos - 1);
                    if (!std::isspace((unsigned char)c)) break;
                    pos--;
                }
                // Skip word characters
                while (pos > input_start_) {
                    char c = buffer_->byte_at(pos - 1);
                    if (std::isspace((unsigned char)c)) break;
                    pos--;
                }
                buffer_->remove(pos, cur);
                insert_position(pos);
                show_insert_position();
            }
            return 1;
        }

        // Escape key: Clear the entire live line completely
        if (key == FL_Escape) {
            replaceLiveText("");
            return 1;
        }

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
