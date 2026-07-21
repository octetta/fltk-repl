#pragma once
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <functional>
#include <string>
#include <vector>
#include "Theme.h"

// A single scrolling text buffer that behaves like a shell:
//  - all prior output + prompts are plain, selectable, copyable text
//  - the tail of the buffer (after the most recent prompt) is the
//    "live" input region; typing/pasting always lands there
//  - Enter submits the live region as a command line and invokes the
//    line handler callback
//  - Up/Down recall command history into the live region
//
// This is intentionally the only piece of the library that touches
// FLTK/C++ directly in a nontrivial way; everything else is a thin
// C API wrapper around this class.
class TerminalView : public Fl_Text_Editor {
public:
    TerminalView(int x, int y, int w, int h);
    ~TerminalView() override;

    // Append raw text using the "normal output" style.
    void appendOutput(const std::string &utf8);

    // Append the prompt string and mark the start of the live input
    // region at the current end of buffer.
    void showPrompt();

    void setPrompt(const std::string &p) { prompt_ = p; }
    const std::string &prompt() const { return prompt_; }

    void setLineHandler(std::function<void(const std::string &)> handler) {
        onLine_ = std::move(handler);
    }

    void clearAll();

    void setColors(const ReplColors &c);
    const ReplColors &colors() const { return colors_; }

    void setFont(Fl_Font font, int size);

    // History access (oldest first).
    int historyCount() const { return (int)history_.size(); }
    const std::string &historyAt(int i) const { return history_.at((size_t)i); }
    void clearHistory() { history_.clear(); history_pos_ = -1; }

    int handle(int event) override;

    // Public only so the internal C-linkage buffer-modify callback can
    // reach it; not part of the library's supported API.
    void onBufferModifiedTrampolineImpl(int pos, int nInserted, int nDeleted);

    void zoomIn(int delta = 1);
    void zoomOut(int delta = 1);
    void updateFontSize(int new_size);

private:
    Fl_Text_Buffer *buffer_;
    Fl_Text_Buffer *style_;
    std::string prompt_ = "> ";
    int input_start_ = 0;

    std::vector<std::string> history_;
    int history_pos_ = -1;          // -1 == not currently browsing history
    std::string saved_live_edit_;   // live text stashed while browsing history

    std::function<void(const std::string &)> onLine_;

    ReplColors colors_;
    Fl_Font font_ = FL_COURIER;
    int font_size_ = 14;

    void rebuildStyleTable();
    void appendStyled(const std::string &utf8, char styleChar);
    std::string liveText() const;
    void replaceLiveText(const std::string &s);
    void snapCursorToEndIfBeforeInput();
    void moveHistory(int direction); // -1 = up/older, +1 = down/newer
};
