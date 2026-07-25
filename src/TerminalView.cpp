#include "TerminalView.h"
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <sstream>

#include <fstream>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
static uint32_t xterm256_to_rgb(int idx) {
    if (idx < 0) idx = 0;
    if (idx > 255) idx = 255;
    if (idx < 8) {
        static const uint32_t std_cols[8] = {
            0x000000, 0xCD0000, 0x00CD00, 0xCDCD00,
            0x0000EE, 0xCD00CD, 0x00CDCD, 0xE5E5E5
        };
        return std_cols[idx];
    }
    if (idx < 16) {
        static const uint32_t bright_cols[8] = {
            0x7F7F7F, 0xFF0000, 0x00FF00, 0xFFFF00,
            0x5C5CFF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
        };
        return bright_cols[idx - 8];
    }
    if (idx < 232) {
        int n = idx - 16;
        int r = n / 36;
        int g = (n / 6) % 6;
        int b = n % 6;
        static const unsigned char steps[6] = {0, 95, 135, 175, 215, 255};
        return ((uint32_t)steps[r] << 16) | ((uint32_t)steps[g] << 8) | (uint32_t)steps[b];
    }
    unsigned char gray = (unsigned char)(8 + (idx - 232) * 10);
    return ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | (uint32_t)gray;
}
}

namespace {

static std::string get_default_history_file() {
    const char *home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) return ".skrepl_history";
    std::string dir = std::string(home) + "/.config/skrepl";
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0755);
#endif
    return dir + "/history";
}

bool isUrlTerminator(unsigned char ch) {
    return std::isspace(ch) || ch == '<' || ch == '>' || ch == '"';
}

bool isTrailingUrlPunctuation(char ch) {
    return ch == '.' || ch == ',' || ch == ';' || ch == ':' ||
           ch == '!' || ch == '?' || ch == ')' || ch == ']' || ch == '}';
}

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
    loadHistory();
}

TerminalView::~TerminalView() {
    saveHistory();
    buffer_->remove_modify_callback(s_modify_cb, this);
    buffer(nullptr);
    delete buffer_;
    delete style_;
}

void TerminalView::loadHistory(const std::string &filename) {
    std::string path = filename.empty() ? get_default_history_file() : filename;
    std::ifstream in(path.c_str());
    if (!in.is_open()) return;
    history_.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) history_.push_back(line);
    }
    history_pos_ = -1;
}

void TerminalView::saveHistory(const std::string &filename) {
    std::string path = filename.empty() ? get_default_history_file() : filename;
    std::ofstream out(path.c_str());
    if (!out.is_open()) return;
    for (size_t i = 0; i < history_.size(); ++i) {
        out << history_[i] << "\n";
    }
}

void TerminalView::clearHistory() {
    history_.clear();
    history_pos_ = -1;
    std::string path = get_default_history_file();
    std::remove(path.c_str());
}

void TerminalView::rebuildStyleTable() {
    // Calculate luminance of background color to pick high-contrast output shade
    unsigned char r = (colors_.bg >> 16) & 0xFF;
    unsigned char g = (colors_.bg >> 8) & 0xFF;
    unsigned char b = colors_.bg & 0xFF;
    bool isDarkBg = ((0.299 * r + 0.587 * g + 0.114 * b) < 128);

    // Style 'A': Muted/slate contrast for output so live input ('C') stands out cleanly
    uint32_t outputColor = isDarkBg ? 0xA0AAB0 : 0x4A5568;

    static Fl_Text_Display::Style_Table_Entry table[260];
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

    // Style 'D': clickable URL in terminal output.
    table[3].color = repl_rgb_to_flcolor(isDarkBg ? 0x75C8FF : 0x0068B5);
    table[3].font  = font_;
    table[3].size  = font_size_;
    table[3].attr  = Fl_Text_Display::ATTR_UNDERLINE;

    // Styles 'E'..'E'+255 (indices 4..259): 256 ANSI colors
    for (int i = 0; i < 256; ++i) {
        table[4 + i].color = repl_rgb_to_flcolor(xterm256_to_rgb(i));
        table[4 + i].font  = font_;
        table[4 + i].size  = font_size_;
        table[4 + i].attr  = 0;
    }

    highlight_data(style_, table, 260, 'A', nullptr, nullptr);

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

void TerminalView::updateFontSize(int new_size) {
    if (new_size < 6 || new_size > 72) return; // Guard against extreme font sizes
    font_size_ = new_size;
    
    // 1. Rebuild style table entries with new font size
    rebuildStyleTable();

    // 2. Force FLTK to recalculate line heights and character wrapping metrics
    wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

    // 3. Force full redraw of current buffer bounds
    if (buffer_) {
        redisplay_range(0, buffer_->length());
    }
    redraw();
}

void TerminalView::zoomIn(int delta) {
    updateFontSize(font_size_ + delta);
}

void TerminalView::zoomOut(int delta) {
    updateFontSize(font_size_ - delta);
}

void TerminalView::appendStyled(const std::string &utf8, char styleChar) {
    if (utf8.empty()) return;
    int pos = buffer_->length();
    buffer_->insert(pos, utf8.c_str());
    int len = (int)utf8.size();
    style_->replace(pos, pos + len, std::string((size_t)len, styleChar).c_str());
}

void TerminalView::appendOutput(const std::string &utf8) {
    if (utf8.empty()) return;

    if (utf8.find('\033') == std::string::npos && utf8.find('\x1b') == std::string::npos) {
        const int start = buffer_->length();
        appendStyled(utf8, 'A');
        styleOutputUrls(start, buffer_->length());
        insert_position(buffer_->length());
        show_insert_position();
        return;
    }

    std::string clean_text;
    clean_text.reserve(utf8.size());
    std::string style_bytes;
    style_bytes.reserve(utf8.size());

    char current_style = 'A';
    size_t i = 0;
    size_t len = utf8.size();

    while (i < len) {
        if ((utf8[i] == '\033' || utf8[i] == '\x1b') && (i + 1 < len && utf8[i + 1] == '[')) {
            i += 2; // skip \033[
            std::string params_str;
            while (i < len && utf8[i] != 'm' && !std::isalpha((unsigned char)utf8[i])) {
                params_str += utf8[i];
                i++;
            }
            char command = (i < len) ? utf8[i] : '\0';
            if (i < len) i++; // skip command char 'm'

            if (command == 'm') {
                std::vector<int> params;
                std::stringstream ss(params_str);
                std::string token;
                while (std::getline(ss, token, ';')) {
                    if (!token.empty()) params.push_back(std::atoi(token.c_str()));
                    else params.push_back(0);
                }
                if (params.empty()) params.push_back(0);

                for (size_t p = 0; p < params.size(); ++p) {
                    int code = params[p];
                    if (code == 0) {
                        current_style = 'A';
                    } else if (code >= 30 && code <= 37) {
                        current_style = (char)('E' + (code - 30));
                    } else if (code >= 90 && code <= 97) {
                        current_style = (char)('E' + 8 + (code - 90));
                    } else if (code == 38 && p + 2 < params.size() && params[p + 1] == 5) {
                        int color_idx = params[p + 2];
                        if (color_idx >= 0 && color_idx <= 255) {
                            current_style = (char)('E' + color_idx);
                        }
                        p += 2;
                    }
                }
            }
        } else {
            clean_text += utf8[i];
            style_bytes += current_style;
            i++;
        }
    }

    if (!clean_text.empty()) {
        const int start = buffer_->length();
        int pos = buffer_->length();
        buffer_->insert(pos, clean_text.c_str());
        style_->replace(pos, pos + (int)clean_text.size(), style_bytes.c_str());
        styleOutputUrls(start, buffer_->length());
        insert_position(buffer_->length());
        show_insert_position();
    }
}

void TerminalView::styleOutputUrls(int start, int end) {
    if (start >= end) return;
    start = buffer_->line_start(start);
    end = buffer_->line_end(std::min(end, buffer_->length()));
    char *raw = buffer_->text_range(start, end);
    if (!raw) return;
    const std::string text(raw);
    free(raw);

    size_t cursor = 0;
    while (cursor < text.size()) {
        const size_t http = text.find("http://", cursor);
        const size_t https = text.find("https://", cursor);
        size_t begin = std::min(http, https);
        if (http == std::string::npos) begin = https;
        if (https == std::string::npos) begin = http;
        if (begin == std::string::npos) break;
        size_t finish = begin;
        while (finish < text.size() &&
               !isUrlTerminator(static_cast<unsigned char>(text[finish]))) {
            ++finish;
        }
        while (finish > begin && isTrailingUrlPunctuation(text[finish - 1]))
            --finish;
        if (finish > begin)
            style_->replace(start + static_cast<int>(begin),
                            start + static_cast<int>(finish),
                            std::string(finish - begin, 'D').c_str());
        cursor = std::max(finish, begin + 1);
    }
}

std::string TerminalView::urlAtPosition(int position) const {
    if (position < 0 || position >= input_start_) return {};
    const int lineStart = buffer_->line_start(position);
    const int lineEnd = buffer_->line_end(position);
    char *raw = buffer_->text_range(lineStart, lineEnd);
    if (!raw) return {};
    const std::string line(raw);
    free(raw);
    const size_t offset = static_cast<size_t>(position - lineStart);

    size_t cursor = 0;
    while (cursor < line.size()) {
        const size_t http = line.find("http://", cursor);
        const size_t https = line.find("https://", cursor);
        size_t begin = std::min(http, https);
        if (http == std::string::npos) begin = https;
        if (https == std::string::npos) begin = http;
        if (begin == std::string::npos) break;
        size_t finish = begin;
        while (finish < line.size() &&
               !isUrlTerminator(static_cast<unsigned char>(line[finish]))) {
            ++finish;
        }
        while (finish > begin && isTrailingUrlPunctuation(line[finish - 1]))
            --finish;
        if (offset >= begin && offset < finish)
            return line.substr(begin, finish - begin);
        cursor = std::max(finish, begin + 1);
    }
    return {};
}

std::string TerminalView::urlAtEvent() const {
    return urlAtPosition(xy_to_position(Fl::event_x(), Fl::event_y()));
}

void TerminalView::updateUrlCursor() {
    if (window())
        window()->cursor(urlAtEvent().empty() ? FL_CURSOR_DEFAULT : FL_CURSOR_HAND);
}

bool TerminalView::openUrl(const std::string &url) {
    char message[256] = {0};
    if (fl_open_uri(url.c_str(), message, sizeof(message)) == 0) {
        fl_alert("Could not open URL:\n%s", message[0] ? message : url.c_str());
        return false;
    }
    return true;
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

    if (event == FL_MOVE) {
        updateUrlCursor();
    } else if (event == FL_LEAVE) {
        if (window()) window()->cursor(FL_CURSOR_DEFAULT);
    } else if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
        pressed_url_ = urlAtEvent();
        press_x_ = Fl::event_x();
        press_y_ = Fl::event_y();
        url_dragged_ = false;
    } else if (event == FL_DRAG && !pressed_url_.empty()) {
        if (std::abs(Fl::event_x() - press_x_) > 3 ||
            std::abs(Fl::event_y() - press_y_) > 3) {
            url_dragged_ = true;
        }
    } else if (event == FL_RELEASE && Fl::event_button() == FL_LEFT_MOUSE &&
               !pressed_url_.empty()) {
        const std::string releasedUrl = urlAtEvent();
        const std::string pressedUrl = pressed_url_;
        const bool activate = !url_dragged_ && releasedUrl == pressedUrl;
        pressed_url_.clear();
        url_dragged_ = false;
        const int handled = Fl_Text_Editor::handle(event);
        if (activate) {
            buffer_->unselect();
            redraw();
            openUrl(pressedUrl);
            return 1;
        }
        return handled;
    }

    if (event == FL_KEYBOARD) {
        int key = Fl::event_key();
        int state = Fl::event_state();
        bool mod = (state & (FL_CTRL | FL_COMMAND)) != 0;

        // Dynamic Zoom Controls: Ctrl/Cmd + '=', '+', '-', or '0'
        if (mod) {
            if (key == '=' || key == '+') {
                zoomIn(1);
                return 1;
            } else if (key == '-') {
                zoomOut(1);
                return 1;
            } else if (key == '0') {
                updateFontSize(14); // Reset to standard 14pt
                return 1;
            }
        }

        // Handle Ctrl+C / Cmd+C: Copy text, clear selection, and snap cursor back to prompt
        if (mod && (key == 'c' || key == 'C')) {
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
        if (key == FL_Home || (mod && (key == 'a' || key == 'A'))) {
            insert_position(input_start_);
            show_insert_position();
            return 1;
        }

        // Ctrl+E or End: Jump to end of prompt input
        if (key == FL_End || (mod && (key == 'e' || key == 'E'))) {
            insert_position(buffer_->length());
            show_insert_position();
            return 1;
        }

        // Unix line discard (Ctrl+U): Delete from cursor back to prompt start
        if (mod && (key == 'u' || key == 'U')) {
            int cur = insert_position();
            if (cur > input_start_) {
                buffer_->remove(input_start_, cur);
                insert_position(input_start_);
                show_insert_position();
            }
            return 1;
        }

        // Kill forward (Ctrl+K): Delete from cursor to end of input line
        if (mod && (key == 'k' || key == 'K')) {
            int cur = insert_position();
            int end = buffer_->length();
            if (cur >= input_start_ && cur < end) {
                buffer_->remove(cur, end);
            }
            return 1;
        }

        // Backward kill word (Ctrl+W): Delete word before cursor
        if (mod && (key == 'w' || key == 'W')) {
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
                saveHistory();
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
