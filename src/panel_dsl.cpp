#include "panel_dsl.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Item.H>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace {

/* ---------------------------------------------------------------------
 * Parse tree
 * ------------------------------------------------------------------- */

enum ItemType { IT_SLIDER, IT_FIELD, IT_TOGGLE, IT_BUTTON, IT_CHOICE, IT_LABEL };

struct ParsedItem {
    ItemType type;
    std::string name;          /* also used as label / button text */
    double min = 0, max = 1;
    std::string unit;          /* slider only, optional */
    std::string tmpl;          /* command template (button: release template) */
    std::string tmpl_press;    /* button only: press-time template, empty = none */
    std::vector<std::string> options; /* choice only */
    std::string text;          /* label only */
    int weight = 1;

    bool has_default = false;
    double default_num = 0;    /* slider / field */
    bool default_bool = false; /* toggle */
    std::string default_str;   /* choice */

    /* Slider only. */
    bool has_step = false;
    double step = 0;
    bool live = false;
};

struct ParsedRow {
    std::vector<ParsedItem> items;
};

struct ParsedGrid {
    int rows = 1, cols = 1;
    std::vector<ParsedItem> buttons; /* row-major, may be fewer than rows*cols */
};

enum BlockKind { BK_ROW, BK_GRID };

struct ParsedBlock {
    BlockKind kind = BK_ROW;
    ParsedRow row;
    ParsedGrid grid;
};

struct ParsedWindow {
    std::string title = "Panel";
    int width = 320;
    int height = 200;
    std::vector<ParsedBlock> blocks;
};

/* ---------------------------------------------------------------------
 * Tokenizer: whitespace-separated tokens, with "double-quoted strings"
 * treated as a single token (quotes stripped). Each token remembers
 * whether it came from a quoted string -- this is what lets a template
 * like "=10,%f" be told apart from a bare =default modifier: modifiers
 * are always written unquoted, so a quoted token is never eligible to
 * be treated as one, no matter what character it starts with.
 * ------------------------------------------------------------------- */

struct Token {
    std::string text;
    bool quoted;
};

std::vector<Token> tokenize(const std::string &line) {
    std::vector<Token> out;
    size_t i = 0, n = line.size();
    while (i < n) {
        while (i < n && isspace((unsigned char)line[i])) ++i;
        if (i >= n) break;
        if (line[i] == '"') {
            size_t j = i + 1;
            std::string tok;
            while (j < n && line[j] != '"') {
                if (line[j] == '\\' && j + 1 < n) { tok += line[j + 1]; j += 2; }
                else { tok += line[j]; ++j; }
            }
            out.push_back(Token{tok, true});
            i = (j < n) ? j + 1 : j;
        } else {
            size_t j = i;
            while (j < n && !isspace((unsigned char)line[j])) ++j;
            out.push_back(Token{line.substr(i, j - i), false});
            i = j;
        }
    }
    return out;
}

bool looks_numeric(const std::string &s) {
    if (s.empty()) return false;
    char *end = NULL;
    strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

void pop_modifiers(std::vector<Token> &toks, int &weight,
                    bool &has_default, std::string &default_tok,
                    std::vector<std::string> *flags) {
    weight = 1;
    has_default = false;
    default_tok.clear();
    if (flags) flags->clear();
    while (!toks.empty()) {
        const Token &last = toks.back();
        if (last.quoted) break;
        if (last.text.size() > 1 && last.text[0] == '@') {
            int w = atoi(last.text.c_str() + 1);
            weight = w > 0 ? w : 1;
            toks.pop_back();
            continue;
        }
        if (last.text.size() > 1 && last.text[0] == '=') {
            default_tok = last.text.substr(1);
            has_default = true;
            toks.pop_back();
            continue;
        }
        if (last.text.size() > 1 && last.text[0] == '~') {
            if (flags) flags->push_back(last.text.substr(1));
            toks.pop_back();
            continue;
        }
        break;
    }
}

/* ---------------------------------------------------------------------
 * Parser
 * ------------------------------------------------------------------- */

bool parse_dsl(const std::string &text, ParsedWindow &pw, std::string &err) {
    std::istringstream in(text);
    std::string raw;
    int lineno = 0;
    bool in_row = false;
    bool in_grid = false;
    ParsedRow current_row;
    ParsedGrid current_grid;

    auto emit_item = [&](const ParsedItem &item) {
        if (in_row) { current_row.items.push_back(item); return; }
        ParsedBlock blk;
        blk.kind = BK_ROW;
        blk.row.items.push_back(item);
        pw.blocks.push_back(blk);
    };

    while (std::getline(in, raw)) {
        ++lineno;
        size_t hash = raw.find('#');
        std::string line = (hash == std::string::npos) ? raw : raw.substr(0, hash);
        std::vector<Token> t = tokenize(line);
        if (t.empty()) continue;

        const std::string &kw = t[0].text;

        if (in_grid && kw != "button" && kw != "endgrid") {
            if (kw == "grid") { err = "line " + std::to_string(lineno) + ": nested grid not supported"; return false; }
            err = "line " + std::to_string(lineno) + ": grid may only contain button statements (got '" + kw + "')";
            return false;
        }

        if (kw == "window") {
            if (t.size() < 4) { err = "line " + std::to_string(lineno) + ": window needs \"title\" width height"; return false; }
            pw.title = t[1].text;
            pw.width = atoi(t[2].text.c_str());
            pw.height = atoi(t[3].text.c_str());

        } else if (kw == "row") {
            if (in_row) { err = "line " + std::to_string(lineno) + ": nested row not supported"; return false; }
            in_row = true;
            current_row = ParsedRow();

        } else if (kw == "endrow") {
            if (!in_row) { err = "line " + std::to_string(lineno) + ": endrow without row"; return false; }
            ParsedBlock blk;
            blk.kind = BK_ROW;
            blk.row = current_row;
            pw.blocks.push_back(blk);
            in_row = false;

        } else if (kw == "grid") {
            if (in_row) { err = "line " + std::to_string(lineno) + ": grid not allowed inside row"; return false; }
            if (t.size() < 3) { err = "line " + std::to_string(lineno) + ": grid needs <rows> <cols>"; return false; }
            int gr = atoi(t[1].text.c_str());
            int gc = atoi(t[2].text.c_str());
            if (gr <= 0 || gc <= 0) { err = "line " + std::to_string(lineno) + ": grid rows/cols must be positive"; return false; }
            in_grid = true;
            current_grid = ParsedGrid();
            current_grid.rows = gr;
            current_grid.cols = gc;

        } else if (kw == "endgrid") {
            if (!in_grid) { err = "line " + std::to_string(lineno) + ": endgrid without grid"; return false; }
            int capacity = current_grid.rows * current_grid.cols;
            if ((int)current_grid.buttons.size() > capacity) {
                err = "line " + std::to_string(lineno) + ": grid declares " +
                      std::to_string(current_grid.rows) + "x" + std::to_string(current_grid.cols) +
                      " = " + std::to_string(capacity) + " cells but " +
                      std::to_string(current_grid.buttons.size()) + " buttons were given";
                return false;
            }
            ParsedBlock blk;
            blk.kind = BK_GRID;
            blk.grid = current_grid;
            pw.blocks.push_back(blk);
            in_grid = false;

        } else if (kw == "slider") {
            if (t.size() < 5) { err = "line " + std::to_string(lineno) + ": slider needs name min max [step] [unit] \"template\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);

            if (rest.size() < 4) { err = "line " + std::to_string(lineno) + ": slider is missing its \"template\""; return false; }

            ParsedItem it;
            it.type = IT_SLIDER;
            it.name = rest[0].text;
            it.min = atof(rest[1].text.c_str());
            it.max = atof(rest[2].text.c_str());

            std::string tmpl_tok = rest.back().text;
            std::vector<Token> middle(rest.begin() + 3, rest.end() - 1);
            if (middle.size() > 2) { err = "line " + std::to_string(lineno) + ": too many tokens before slider's template"; return false; }
            for (size_t mi = 0; mi < middle.size(); ++mi) {
                const std::string &m = middle[mi].text;
                if (looks_numeric(m)) {
                    if (it.has_step) { err = "line " + std::to_string(lineno) + ": slider has more than one numeric (step) token"; return false; }
                    it.step = atof(m.c_str());
                    it.has_step = true;
                } else {
                    if (!it.unit.empty()) { err = "line " + std::to_string(lineno) + ": slider has more than one unit-like token"; return false; }
                    it.unit = m;
                }
            }
            it.tmpl = tmpl_tok;
            it.weight = weight;

            for (size_t fi = 0; fi < flags.size(); ++fi) {
                if (flags[fi] == "live") it.live = true;
                else { err = "line " + std::to_string(lineno) + ": unknown slider flag '~" + flags[fi] + "'"; return false; }
            }

            if (has_default) {
                double dv = atof(default_tok.c_str());
                if (dv < it.min || dv > it.max) {
                    err = "line " + std::to_string(lineno) + ": default " + default_tok + " is outside slider's range [" + rest[1].text + ", " + rest[2].text + "]";
                    return false;
                }
                it.default_num = dv;
                it.has_default = true;
            }
            emit_item(it);

        } else if (kw == "field") {
            if (t.size() < 5) { err = "line " + std::to_string(lineno) + ": field needs name min max \"template\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);
            if (!flags.empty()) { err = "line " + std::to_string(lineno) + ": field does not support flag '~" + flags[0] + "'"; return false; }
            if (rest.size() != 4) { err = "line " + std::to_string(lineno) + ": field needs exactly name min max \"template\""; return false; }

            ParsedItem it;
            it.type = IT_FIELD;
            it.name = rest[0].text;
            it.min = atof(rest[1].text.c_str());
            it.max = atof(rest[2].text.c_str());
            it.tmpl = rest[3].text;
            it.weight = weight;
            if (has_default) {
                double dv = atof(default_tok.c_str());
                if (dv < it.min || dv > it.max) {
                    err = "line " + std::to_string(lineno) + ": default " + default_tok + " is outside field's range [" + rest[1].text + ", " + rest[2].text + "]";
                    return false;
                }
                it.default_num = dv;
                it.has_default = true;
            }
            emit_item(it);

        } else if (kw == "toggle") {
            if (t.size() < 3) { err = "line " + std::to_string(lineno) + ": toggle needs name \"template\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);
            if (!flags.empty()) { err = "line " + std::to_string(lineno) + ": toggle does not support flag '~" + flags[0] + "'"; return false; }
            if (rest.size() != 2) { err = "line " + std::to_string(lineno) + ": toggle needs exactly name \"template\""; return false; }

            ParsedItem it;
            it.type = IT_TOGGLE;
            it.name = rest[0].text;
            it.tmpl = rest[1].text;
            it.weight = weight;
            if (has_default) {
                std::string d = default_tok;
                for (size_t ci = 0; ci < d.size(); ++ci) d[ci] = (char)tolower((unsigned char)d[ci]);
                if (d == "1" || d == "on" || d == "true") it.default_bool = true;
                else if (d == "0" || d == "off" || d == "false") it.default_bool = false;
                else { err = "line " + std::to_string(lineno) + ": toggle default must be 0/1/on/off, got '" + default_tok + "'"; return false; }
                it.has_default = true;
            }
            emit_item(it);

        } else if (kw == "button") {
            if (t.size() < 3) { err = "line " + std::to_string(lineno) + ": button needs name \"template\" or name \"press\" \"release\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);
            if (!flags.empty()) { err = "line " + std::to_string(lineno) + ": button does not support flag '~" + flags[0] + "'"; return false; }
            if (has_default) { err = "line " + std::to_string(lineno) + ": button does not take a default value"; return false; }

            ParsedItem it;
            it.type = IT_BUTTON;
            it.name = rest[0].text;
            if (rest.size() == 2) {
                it.tmpl = rest[1].text;
            } else if (rest.size() == 3) {
                it.tmpl_press = rest[1].text;
                it.tmpl = rest[2].text;
            } else {
                err = "line " + std::to_string(lineno) + ": button needs name \"template\" or name \"press\" \"release\"";
                return false;
            }
            it.weight = weight;

            if (in_grid) current_grid.buttons.push_back(it);
            else emit_item(it);

        } else if (kw == "choice") {
            if (t.size() < 4) { err = "line " + std::to_string(lineno) + ": choice needs name \"options\" \"template\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);
            if (!flags.empty()) { err = "line " + std::to_string(lineno) + ": choice does not support flag '~" + flags[0] + "'"; return false; }
            if (rest.size() != 3) { err = "line " + std::to_string(lineno) + ": choice needs exactly name \"options\" \"template\""; return false; }

            ParsedItem it;
            it.type = IT_CHOICE;
            it.name = rest[0].text;
            {
                std::istringstream oss(rest[1].text);
                std::string opt;
                while (oss >> opt) it.options.push_back(opt);
            }
            it.tmpl = rest[2].text;
            it.weight = weight;
            if (it.options.empty()) { err = "line " + std::to_string(lineno) + ": choice has no options"; return false; }
            if (has_default) {
                bool found = false;
                for (size_t oi = 0; oi < it.options.size(); ++oi)
                    if (it.options[oi] == default_tok) { found = true; break; }
                if (!found) { err = "line " + std::to_string(lineno) + ": choice default '" + default_tok + "' is not one of the listed options"; return false; }
                it.default_str = default_tok;
                it.has_default = true;
            }
            emit_item(it);

        } else if (kw == "label") {
            if (t.size() < 2) { err = "line " + std::to_string(lineno) + ": label needs \"text\""; return false; }
            std::vector<Token> rest(t.begin() + 1, t.end());
            int weight; bool has_default; std::string default_tok; std::vector<std::string> flags;
            pop_modifiers(rest, weight, has_default, default_tok, &flags);
            if (!flags.empty()) { err = "line " + std::to_string(lineno) + ": label does not support flag '~" + flags[0] + "'"; return false; }
            if (has_default) { err = "line " + std::to_string(lineno) + ": label does not take a default value"; return false; }

            ParsedItem it;
            it.type = IT_LABEL;
            it.text = rest[0].text;
            it.weight = weight;
            emit_item(it);

        } else {
            err = "line " + std::to_string(lineno) + ": unknown statement '" + kw + "'";
            return false;
        }
    }

    if (in_row) { err = "unterminated row (missing endrow)"; return false; }
    if (in_grid) { err = "unterminated grid (missing endgrid)"; return false; }
    return true;
}

/* ---------------------------------------------------------------------
 * Template substitution
 * ------------------------------------------------------------------- */

std::string format_template_int(const std::string &tmpl, long v) {
    size_t p = tmpl.find("%d");
    if (p == std::string::npos) return tmpl;
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", v);
    return tmpl.substr(0, p) + buf + tmpl.substr(p + 2);
}

struct FloatSpec {
    bool found = false;
    size_t pos = 0, len = 0;
    int precision = 3;
};

FloatSpec find_float_spec(const std::string &tmpl) {
    FloatSpec spec;
    size_t p = tmpl.find('%');
    while (p != std::string::npos) {
        size_t i = p + 1;
        int precision = -1;
        if (i < tmpl.size() && tmpl[i] == '.') {
            size_t j = i + 1;
            while (j < tmpl.size() && isdigit((unsigned char)tmpl[j])) ++j;
            if (j > i + 1) {
                precision = atoi(tmpl.substr(i + 1, j - i - 1).c_str());
                i = j;
            }
        }
        if (i < tmpl.size() && tmpl[i] == 'f') {
            spec.found = true;
            spec.pos = p;
            spec.len = i - p + 1;
            spec.precision = (precision >= 0) ? precision : 3;
            if (spec.precision > 17) spec.precision = 17;
            return spec;
        }
        p = tmpl.find('%', p + 1);
    }
    return spec;
}

std::string format_template_float(const std::string &tmpl, double v) {
    FloatSpec spec = find_float_spec(tmpl);
    if (!spec.found) return format_template_int(tmpl, (long)v);
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", spec.precision, v);
    return tmpl.substr(0, spec.pos) + buf + tmpl.substr(spec.pos + spec.len);
}

std::string format_template_str(const std::string &tmpl, const std::string &v) {
    size_t p = tmpl.find("%s");
    if (p == std::string::npos) return tmpl;
    return tmpl.substr(0, p) + v + tmpl.substr(p + 2);
}

std::string format_display_value(const std::string &tmpl, double v) {
    FloatSpec spec = find_float_spec(tmpl);
    char buf[64];
    if (spec.found) {
        snprintf(buf, sizeof buf, "%.*f", spec.precision, v);
    } else if (tmpl.find("%d") != std::string::npos) {
        snprintf(buf, sizeof buf, "%ld", (long)v);
    } else {
        snprintf(buf, sizeof buf, "%.3f", v);
    }
    return buf;
}

/* ---------------------------------------------------------------------
 * Global command dispatch
 * ------------------------------------------------------------------- */

panel_command_fn g_cmd_fn = NULL;
void *g_cmd_user_data = NULL;

void dispatch(const std::string &line) {
    if (g_cmd_fn) g_cmd_fn(line.c_str(), g_cmd_user_data);
}

/* ---------------------------------------------------------------------
 * Per-widget bindings and behavior.
 * ------------------------------------------------------------------- */

struct ItemBinding {
    std::string name;        /* used to match widgets across a reload */
    std::string tmpl;
    std::string tmpl_press;
    std::vector<std::string> options;
};

const int LIVE_THROTTLE_MS = 30;

long now_ms() {
    using namespace std::chrono;
    return (long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/* Slider that:
 *  - dispatches on release always, and additionally (throttled) during
 *    drag when constructed with live=true;
 *  - shows its current value next to the unit in its label, e.g.
 *    "cutoff (2000 Hz)", kept in sync on every drag/release;
 *  - when a step is set, clicking the track to either side of the thumb
 *    nudges the value by one step and dispatches immediately, rather
 *    than jumping the thumb straight to the click position. Clicking
 *    the thumb itself still starts a normal drag. Dragging already
 *    snaps to step increments via FLTK's own Fl_Valuator::round(),
 *    since step() is set on the widget at construction time.
 * Driven from handle() instead of FLTK's when()/callback() throughout,
 * so drag/release/track-click can all be told apart cleanly. */
class LiveSlider : public Fl_Slider {
public:
    LiveSlider(int x, int y, int w, int h)
        : Fl_Slider(x, y, w, h, ""), binding_(NULL), live_(false), last_ms_(0), label_box_(NULL) {
        when(FL_WHEN_NEVER);
    }

    void set_binding(ItemBinding *b, bool live) { binding_ = b; live_ = live; }
    const std::string &name() const { return name_; }

    void set_display(Fl_Box *label_box, const std::string &name,
                      const std::string &unit, const std::string &display_tmpl) {
        label_box_ = label_box;
        name_ = name;
        unit_ = unit;
        display_tmpl_ = display_tmpl;
        update_display();
    }

    /* Sets the value and refreshes the label, without dispatching a
     * command -- used to restore a widget's prior value across a
     * panel_reload_file(), where the engine's state hasn't changed and
     * so nothing needs to be re-sent. */
    void restore_value(double v) {
        if (v < minimum()) v = minimum();
        if (v > maximum()) v = maximum();
        value(v);
        update_display();
    }

protected:
    /* When a step is set, a plain click/release with no real mouse
     * movement in between pages the value by exactly one step toward
     * the click side, like a scrollbar's trough click -- regardless of
     * how close to the thumb the click landed, since it turned out to
     * be too fragile to guess the rendered thumb's pixel width and
     * classify by position alone. If the mouse genuinely moves past a
     * small threshold before release, that's a real drag instead: we
     * hand off to Fl_Slider's normal (already step-snapped, via its own
     * round()) dragging from that point, anchored at the original
     * press position so it doesn't jump first. */
    int handle(int event) override {
        if (event == FL_PUSH && step() != 0.0) {
            paging_ = true;
            dragging_ = false;
            press_x_ = Fl::event_x();
            press_value_ = value();
            return 1;
        }

        if (paging_) {
            const int DRAG_THRESHOLD_PX = 3;
            if (event == FL_DRAG) {
                if (!dragging_ && std::abs(Fl::event_x() - press_x_) > DRAG_THRESHOLD_PX) {
                    dragging_ = true;
                    Fl_Slider::handle(FL_PUSH); /* anchor Fl_Slider's own tracking here */
                }
                if (dragging_) {
                    int r = Fl_Slider::handle(FL_DRAG);
                    update_display();
                    if (live_ && binding_) {
                        long t = now_ms();
                        if (t - last_ms_ >= LIVE_THROTTLE_MS) {
                            last_ms_ = t;
                            dispatch(format_template_float(binding_->tmpl, value()));
                        }
                    }
                    return r;
                }
                return 1; /* movement still within the click threshold; ignore */
            }
            if (event == FL_RELEASE) {
                paging_ = false;
                if (dragging_) {
                    int r = Fl_Slider::handle(FL_RELEASE);
                    update_display();
                    if (binding_) dispatch(format_template_float(binding_->tmpl, value()));
                    return r;
                }
                /* No real movement: a plain click, page by one step. */
                double range = maximum() - minimum();
                double frac = (range != 0) ? (press_value_ - minimum()) / range : 0.0;
                if (frac < 0) frac = 0;
                if (frac > 1) frac = 1;
                int thumb_px = x() + (int)(frac * w());
                double nv = press_value_;
                if (press_x_ < thumb_px) nv -= step();
                else if (press_x_ > thumb_px) nv += step();
                if (nv < minimum()) nv = minimum();
                if (nv > maximum()) nv = maximum();
                value(nv);
                update_display();
                if (binding_) dispatch(format_template_float(binding_->tmpl, nv));
                return 1;
            }
            return 1; /* swallow anything else mid-gesture */
        }

        int r = Fl_Slider::handle(event);
        if (event == FL_DRAG || event == FL_RELEASE) update_display();
        if (!binding_) return r;
        if (event == FL_RELEASE) {
            dispatch(format_template_float(binding_->tmpl, value()));
        } else if (live_ && event == FL_DRAG) {
            long t = now_ms();
            if (t - last_ms_ >= LIVE_THROTTLE_MS) {
                last_ms_ = t;
                dispatch(format_template_float(binding_->tmpl, value()));
            }
        }
        return r;
    }

private:
    void update_display() {
        if (!label_box_) return;
        std::string valstr = format_display_value(display_tmpl_, value());
        std::string full = name_ + " (" + valstr + (unit_.empty() ? "" : " " + unit_) + ")";
        label_box_->copy_label(full.c_str());
        label_box_->redraw();
    }

    ItemBinding *binding_;
    bool live_;
    long last_ms_;
    Fl_Box *label_box_;
    std::string name_, unit_, display_tmpl_;
    bool paging_ = false;
    bool dragging_ = false;
    int press_x_ = 0;
    double press_value_ = 0;
};

class ModalButton : public Fl_Button {
public:
    ModalButton(int x, int y, int w, int h) : Fl_Button(x, y, w, h, ""), binding_(NULL) {}
    void set_binding(ItemBinding *b) { binding_ = b; }

protected:
    int handle(int event) override {
        int r = Fl_Button::handle(event);
        if (!binding_) return r;
        if (event == FL_PUSH && !binding_->tmpl_press.empty()) {
            dispatch(binding_->tmpl_press);
        } else if (event == FL_RELEASE) {
            if (!binding_->tmpl.empty()) dispatch(binding_->tmpl);
        }
        return r;
    }

private:
    ItemBinding *binding_;
};

void field_cb(Fl_Widget *w, void *ud) {
    ItemBinding *b = (ItemBinding *)ud;
    long v = atol(((Fl_Int_Input *)w)->value());
    dispatch(format_template_int(b->tmpl, v));
}

void toggle_cb(Fl_Widget *w, void *ud) {
    ItemBinding *b = (ItemBinding *)ud;
    long v = ((Fl_Check_Button *)w)->value() ? 1 : 0;
    dispatch(format_template_int(b->tmpl, v));
}

void choice_cb(Fl_Widget *w, void *ud) {
    ItemBinding *b = (ItemBinding *)ud;
    Fl_Choice *c = (Fl_Choice *)w;
    int idx = c->value();
    if (idx < 0 || (size_t)idx >= b->options.size()) return;
    dispatch(format_template_str(b->tmpl, b->options[idx]));
}

/* ---------------------------------------------------------------------
 * Layout constants
 * ------------------------------------------------------------------- */

const int MARGIN = 10;
const int GAP_Y = 6;
const int GAP_X = 8;
const int H_CONTROL = 44;
const int H_TOGGLE = 26;
const int H_BUTTON = 28;
const int H_LABEL = 22;
const int H_GRID_BUTTON = 32;

int item_height(const ParsedItem &it) {
    switch (it.type) {
        case IT_SLIDER: case IT_FIELD: case IT_CHOICE: return H_CONTROL;
        case IT_TOGGLE: return H_TOGGLE;
        case IT_BUTTON: return H_BUTTON;
        case IT_LABEL: return H_LABEL;
    }
    return H_CONTROL;
}

int row_height(const ParsedRow &row) {
    int h = 0;
    for (size_t i = 0; i < row.items.size(); ++i) h = std::max(h, item_height(row.items[i]));
    return h;
}

std::vector<std::pair<int, int> > compute_cols(int content_w, const std::vector<int> &weights) {
    std::vector<std::pair<int, int> > out;
    int n = (int)weights.size();
    int weight_sum = 0;
    for (int i = 0; i < n; ++i) weight_sum += weights[i];
    int gaps_w = GAP_X * (n - 1 > 0 ? n - 1 : 0);
    int avail_w = content_w - gaps_w;

    int x = MARGIN;
    for (int i = 0; i < n; ++i) {
        int iw = (weight_sum > 0) ? (avail_w * weights[i] / weight_sum) : avail_w;
        if (i + 1 == n) iw = content_w - (x - MARGIN);
        out.push_back(std::make_pair(x, iw));
        x += iw + GAP_X;
    }
    return out;
}

/* ---------------------------------------------------------------------
 * Resize-time layout bookkeeping. A single ordered vector of LayoutBlock
 * mirrors ParsedWindow::blocks, so reflow can walk it in build order and
 * recompute a running y-cursor from scratch every time -- this is what
 * lets a grown button grid push everything below it down correctly, and
 * avoids compounding rounding error across repeated resizes (every
 * reflow recomputes positions from fixed base sizes, never from
 * "current" widget geometry).
 * ------------------------------------------------------------------- */

struct LayoutItem {
    Fl_Widget *label = NULL;
    Fl_Widget *control = NULL;
    int weight = 1;
};

struct LayoutRowBlock {
    std::vector<LayoutItem> items;
    int height = 0; /* fixed; rows don't grow vertically */
};

struct LayoutGridCell {
    Fl_Widget *btn;
    int row, col;
};

struct LayoutGridBlock {
    int rows = 0, cols = 0;
    int base_cell_h = 0; /* height as originally built; grows on resize, never shrinks below this */
    std::vector<LayoutGridCell> cells;
};

struct LayoutBlock {
    BlockKind kind = BK_ROW;
    LayoutRowBlock row;
    LayoutGridBlock grid;
};

} // namespace

/* ---------------------------------------------------------------------
 * panel_win: opaque handle
 * ------------------------------------------------------------------- */

struct panel_win {
    Fl_Double_Window *win;
    Fl_Group *content;
    std::vector<ItemBinding *> bindings;
    std::vector<LayoutBlock> blocks; /* rebuilt every build_widgets() call */
};

namespace {

void clear_bindings(panel_win_t *pw) {
    for (size_t i = 0; i < pw->bindings.size(); ++i) delete pw->bindings[i];
    pw->bindings.clear();
}

/* Reflows the whole panel for a new window size W x H: horizontal space
 * is redistributed across every row/grid column (as before); vertical
 * space beyond what was originally built is handed to button grids,
 * growing their cell height (and therefore total grid height) so grids
 * actually fill a taller window, with every row/grid below a grown grid
 * shifted down to match. Rows themselves never grow taller -- only
 * their x/width track the window, same as before. The window never
 * shrinks built sizes below what was constructed; making the window
 * shorter than its content just lets the OS window clip it, rather than
 * trying to compress controls to fit. */
void reflow_panel(panel_win_t *pw, int W, int H) {
    int content_w = W - 2 * MARGIN;
    if (content_w < 1) content_w = 1;

    int base_content_h = 0;
    int total_grid_rows = 0;
    for (size_t bi = 0; bi < pw->blocks.size(); ++bi) {
        const LayoutBlock &blk = pw->blocks[bi];
        int bh = (blk.kind == BK_ROW)
            ? blk.row.height
            : (blk.grid.rows * blk.grid.base_cell_h + (blk.grid.rows - 1) * GAP_Y);
        base_content_h += bh + GAP_Y;
        if (blk.kind == BK_GRID) total_grid_rows += blk.grid.rows;
    }

    int avail_h = H - 2 * MARGIN;
    int extra = avail_h - base_content_h;
    if (extra < 0) extra = 0;
    int extra_per_row = (total_grid_rows > 0) ? (extra / total_grid_rows) : 0;

    int y = MARGIN;
    for (size_t bi = 0; bi < pw->blocks.size(); ++bi) {
        LayoutBlock &blk = pw->blocks[bi];

        if (blk.kind == BK_GRID) {
            LayoutGridBlock &g = blk.grid;
            int cell_h = g.base_cell_h + extra_per_row;
            std::vector<int> weights(g.cols, 1);
            std::vector<std::pair<int, int> > cols = compute_cols(content_w, weights);

            for (size_t ci = 0; ci < g.cells.size(); ++ci) {
                LayoutGridCell &cell = g.cells[ci];
                int cx = cols[cell.col].first;
                int cw = cols[cell.col].second;
                int cy = y + cell.row * (cell_h + GAP_Y);
                cell.btn->resize(cx, cy, cw, cell_h);
            }

            int grid_h = g.rows * cell_h + (g.rows - 1) * GAP_Y;
            y += grid_h + GAP_Y;
            continue;
        }

        LayoutRowBlock &row = blk.row;
        int row_h = row.height;
        std::vector<int> weights;
        for (size_t i = 0; i < row.items.size(); ++i) weights.push_back(row.items[i].weight);
        std::vector<std::pair<int, int> > cols = compute_cols(content_w, weights);

        for (size_t i = 0; i < row.items.size(); ++i) {
            LayoutItem &it = row.items[i];
            int x = cols[i].first;
            int iw = cols[i].second;
            bool has_sublabel = (it.label != NULL);
            int control_dy = has_sublabel ? 16 : 0;
            int control_h = has_sublabel ? (row_h - 16) : row_h;
            if (it.label) it.label->resize(x, y, iw, 14);
            if (it.control) it.control->resize(x, y + control_dy, iw, control_h);
        }

        y += row_h + GAP_Y;
    }
}

class PanelWindow : public Fl_Double_Window {
public:
    PanelWindow(int W, int H, const char *title)
        : Fl_Double_Window(W, H, title), owner_(NULL) {}

    void set_owner(panel_win_t *owner) { owner_ = owner; }

    void resize(int X, int Y, int W, int H) override {
        Fl_Double_Window::resize(X, Y, W, H);
        if (!owner_) return;
        if (owner_->content) owner_->content->Fl_Widget::resize(0, 0, W, H);
        reflow_panel(owner_, W, H);
    }

private:
    panel_win_t *owner_;
};

int build_widgets(panel_win_t *pw, const ParsedWindow &pwin) {
    pw->content->clear();
    clear_bindings(pw);
    pw->blocks.clear();

    int content_w = pwin.width - 2 * MARGIN;
    int y = MARGIN;

    for (size_t bi = 0; bi < pwin.blocks.size(); ++bi) {
        const ParsedBlock &block = pwin.blocks[bi];

        if (block.kind == BK_GRID) {
            const ParsedGrid &g = block.grid;
            std::vector<int> weights(g.cols, 1);
            std::vector<std::pair<int, int> > cols = compute_cols(content_w, weights);

            LayoutBlock lb;
            lb.kind = BK_GRID;
            lb.grid.rows = g.rows;
            lb.grid.cols = g.cols;
            lb.grid.base_cell_h = H_GRID_BUTTON;

            for (int r = 0; r < g.rows; ++r) {
                int cell_y = y + r * (H_GRID_BUTTON + GAP_Y);
                for (int c = 0; c < g.cols; ++c) {
                    int idx = r * g.cols + c;
                    if (idx >= (int)g.buttons.size()) continue;
                    const ParsedItem &it = g.buttons[idx];

                    ItemBinding *bind = new ItemBinding();
                    bind->name = it.name;
                    bind->tmpl = it.tmpl;
                    bind->tmpl_press = it.tmpl_press;
                    pw->bindings.push_back(bind);

                    ModalButton *b = new ModalButton(cols[c].first, cell_y, cols[c].second, H_GRID_BUTTON);
                    b->copy_label(it.name.c_str());
                    b->align(FL_ALIGN_CENTER);
                    b->set_binding(bind);
                    b->user_data(bind);
                    pw->content->add(b);

                    LayoutGridCell cell;
                    cell.btn = b; cell.row = r; cell.col = c;
                    lb.grid.cells.push_back(cell);
                }
            }

            pw->blocks.push_back(lb);
            int grid_h = g.rows * H_GRID_BUTTON + (g.rows - 1) * GAP_Y;
            y += grid_h + GAP_Y;
            continue;
        }

        const ParsedRow &row = block.row;
        int rh = row_height(row);

        std::vector<int> weights;
        for (size_t i = 0; i < row.items.size(); ++i) weights.push_back(row.items[i].weight);
        std::vector<std::pair<int, int> > cols = compute_cols(content_w, weights);

        LayoutBlock lb;
        lb.kind = BK_ROW;
        lb.row.height = rh;

        for (size_t i = 0; i < row.items.size(); ++i) {
            const ParsedItem &it = row.items[i];
            int x = cols[i].first;
            int iw = cols[i].second;

            ItemBinding *bind = NULL;
            if (it.type != IT_LABEL) {
                bind = new ItemBinding();
                bind->name = it.name;
                bind->tmpl = it.tmpl;
                bind->tmpl_press = it.tmpl_press;
                bind->options = it.options;
                pw->bindings.push_back(bind);
            }

            LayoutItem litem;
            litem.weight = it.weight;

            switch (it.type) {
                case IT_SLIDER: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    LiveSlider *s = new LiveSlider(x, y + 16, iw, rh - 16);
                    s->type(FL_HOR_NICE_SLIDER);
                    s->bounds(it.min, it.max);
                    if (it.has_step) s->step(it.step);
                    s->value(it.has_default ? it.default_num : (it.min + it.max) / 2.0);
                    s->set_binding(bind, it.live);
                    s->user_data(bind);
                    s->set_display(lbl, it.name, it.unit, it.tmpl);
                    pw->content->add(lbl);
                    pw->content->add(s);
                    litem.label = lbl;
                    litem.control = s;
                    break;
                }
                case IT_FIELD: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->copy_label(it.name.c_str());
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    Fl_Int_Input *f = new Fl_Int_Input(x, y + 16, iw, rh - 16, "");
                    char defbuf[32];
                    snprintf(defbuf, sizeof defbuf, "%d", it.has_default ? (int)it.default_num : (int)it.min);
                    f->value(defbuf);
                    f->callback(field_cb, bind);
                    f->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
                    pw->content->add(lbl);
                    pw->content->add(f);
                    litem.label = lbl;
                    litem.control = f;
                    break;
                }
                case IT_TOGGLE: {
                    Fl_Check_Button *c = new Fl_Check_Button(x, y, iw, rh, "");
                    c->copy_label(it.name.c_str());
                    c->value(it.has_default && it.default_bool ? 1 : 0);
                    c->callback(toggle_cb, bind);
                    pw->content->add(c);
                    litem.control = c;
                    break;
                }
                case IT_BUTTON: {
                    ModalButton *b = new ModalButton(x, y, iw, rh);
                    b->copy_label(it.name.c_str());
                    b->align(FL_ALIGN_CENTER);
                    b->set_binding(bind);
                    b->user_data(bind);
                    pw->content->add(b);
                    litem.control = b;
                    break;
                }
                case IT_CHOICE: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->copy_label(it.name.c_str());
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    Fl_Choice *ch = new Fl_Choice(x, y + 16, iw, rh - 16, "");
                    int default_idx = 0;
                    for (size_t oi = 0; oi < it.options.size(); ++oi) {
                        ch->add(it.options[oi].c_str());
                        if (it.has_default && it.options[oi] == it.default_str) default_idx = (int)oi;
                    }
                    ch->value(default_idx);
                    ch->callback(choice_cb, bind);
                    pw->content->add(lbl);
                    pw->content->add(ch);
                    litem.label = lbl;
                    litem.control = ch;
                    break;
                }
                case IT_LABEL: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, rh, "");
                    lbl->copy_label(it.text.c_str());
                    lbl->labelfont(FL_HELVETICA_BOLD);
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    pw->content->add(lbl);
                    litem.control = lbl;
                    break;
                }
            }

            lb.row.items.push_back(litem);
        }

        pw->blocks.push_back(lb);
        y += rh + GAP_Y;
    }

    return y + MARGIN;
}

/* ---------------------------------------------------------------------
 * Value snapshot/restore across a reload: walks the live widget tree
 * (not the parse tree), keyed by each control's DSL `name` (recovered
 * via user_data(), which every interactive widget is tagged with at
 * build time). Widgets are matched purely by name -- if a reload
 * changes a control's type (e.g. was a toggle, now a slider) under the
 * same name, the old value is simply not restored for it, and it falls
 * back to its fresh DSL default; this is deliberately conservative
 * rather than trying to coerce values across type changes.
 * ------------------------------------------------------------------- */

struct ReloadSnapshot {
    std::map<std::string, double> slider_vals;
    std::map<std::string, std::string> field_vals;
    std::map<std::string, int> toggle_vals;
    std::map<std::string, std::string> choice_vals;
};

void snapshot_values(Fl_Group *g, ReloadSnapshot &snap) {
    for (int i = 0; i < g->children(); ++i) {
        Fl_Widget *w = g->child(i);
        if (LiveSlider *s = dynamic_cast<LiveSlider *>(w)) {
            if (!s->name().empty()) snap.slider_vals[s->name()] = s->value();
        } else if (Fl_Int_Input *f = dynamic_cast<Fl_Int_Input *>(w)) {
            ItemBinding *b = (ItemBinding *)f->user_data();
            if (b && !b->name.empty()) snap.field_vals[b->name] = f->value() ? f->value() : "";
        } else if (Fl_Check_Button *c = dynamic_cast<Fl_Check_Button *>(w)) {
            ItemBinding *b = (ItemBinding *)c->user_data();
            if (b && !b->name.empty()) snap.toggle_vals[b->name] = c->value();
        } else if (Fl_Choice *ch = dynamic_cast<Fl_Choice *>(w)) {
            ItemBinding *b = (ItemBinding *)ch->user_data();
            if (b && !b->name.empty() && ch->value() >= 0 && ch->text())
                snap.choice_vals[b->name] = ch->text();
        }
        if (Fl_Group *sub = dynamic_cast<Fl_Group *>(w)) snapshot_values(sub, snap);
    }
}

void restore_values(Fl_Group *g, const ReloadSnapshot &snap) {
    for (int i = 0; i < g->children(); ++i) {
        Fl_Widget *w = g->child(i);
        if (LiveSlider *s = dynamic_cast<LiveSlider *>(w)) {
            std::map<std::string, double>::const_iterator it = snap.slider_vals.find(s->name());
            if (it != snap.slider_vals.end()) s->restore_value(it->second);
        } else if (Fl_Int_Input *f = dynamic_cast<Fl_Int_Input *>(w)) {
            ItemBinding *b = (ItemBinding *)f->user_data();
            if (b) {
                std::map<std::string, std::string>::const_iterator it = snap.field_vals.find(b->name);
                if (it != snap.field_vals.end()) f->value(it->second.c_str());
            }
        } else if (Fl_Check_Button *c = dynamic_cast<Fl_Check_Button *>(w)) {
            ItemBinding *b = (ItemBinding *)c->user_data();
            if (b) {
                std::map<std::string, int>::const_iterator it = snap.toggle_vals.find(b->name);
                if (it != snap.toggle_vals.end()) c->value(it->second);
            }
        } else if (Fl_Choice *ch = dynamic_cast<Fl_Choice *>(w)) {
            ItemBinding *b = (ItemBinding *)ch->user_data();
            if (b) {
                std::map<std::string, std::string>::const_iterator it = snap.choice_vals.find(b->name);
                if (it != snap.choice_vals.end()) {
                    for (int mi = 0; mi < ch->size(); ++mi) {
                        const Fl_Menu_Item *item = ch->menu() + mi;
                        if (item->label() && it->second == item->label()) { ch->value(mi); break; }
                    }
                }
            }
        }
        if (Fl_Group *sub = dynamic_cast<Fl_Group *>(w)) restore_values(sub, snap);
    }
}

} // namespace

/* ---------------------------------------------------------------------
 * Public C API
 * ------------------------------------------------------------------- */

extern "C" {

void panel_set_command_handler(panel_command_fn fn, void *user_data) {
    g_cmd_fn = fn;
    g_cmd_user_data = user_data;
}

static panel_win_t *build_from_parsed(const ParsedWindow &pwin) {
    panel_win_t *pw = new panel_win();
    PanelWindow *pwn = new PanelWindow(pwin.width, pwin.height, pwin.title.c_str());
    pw->win = pwn;
    pw->content = new Fl_Group(0, 0, pwin.width, pwin.height);
    pw->win->add(pw->content);
    pwn->set_owner(pw);

    int used_h = build_widgets(pw, pwin);
    if (used_h > pwin.height) {
        pw->win->size(pwin.width, used_h);
        pw->content->size(pwin.width, used_h);
    }

    pw->content->end();
    pw->win->end();
    pw->win->callback([](Fl_Widget *w, void *) { w->hide(); });
    pw->win->size_range(2 * MARGIN + 80, 80, 0, 0);
    return pw;
}

panel_win_t *panel_load_string(const char *dsl_text, const char *fallback_title) {
    if (!dsl_text) return NULL;
    ParsedWindow pwin;
    if (fallback_title) pwin.title = fallback_title;
    std::string err;
    if (!parse_dsl(dsl_text, pwin, err)) {
        fprintf(stderr, "panel_dsl: %s\n", err.c_str());
        return NULL;
    }
    return build_from_parsed(pwin);
}

panel_win_t *panel_load_file(const char *path) {
    if (!path) return NULL;
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "panel_dsl: cannot open '%s'\n", path);
        return NULL;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return panel_load_string(ss.str().c_str(), path);
}

int panel_reload_file(panel_win_t *pw, const char *path) {
    if (!pw || !path) return -1;
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "panel_dsl: cannot open '%s'\n", path);
        return -1;
    }
    std::ostringstream ss;
    ss << in.rdbuf();

    ParsedWindow pwin;
    pwin.title = path;
    std::string err;
    if (!parse_dsl(ss.str(), pwin, err)) {
        fprintf(stderr, "panel_dsl: %s\n", err.c_str());
        return -1;
    }

    ReloadSnapshot snap;
    snapshot_values(pw->content, snap);

    pw->win->label(pwin.title.c_str());
    int used_h = build_widgets(pw, pwin);

    restore_values(pw->content, snap);

    int final_h = used_h > pwin.height ? used_h : pwin.height;
    pw->win->size(pwin.width, final_h);
    pw->content->size(pwin.width, final_h);
    pw->win->redraw();
    return 0;
}

void panel_show(panel_win_t *pw) { if (pw) pw->win->show(); }
void panel_hide(panel_win_t *pw) { if (pw) pw->win->hide(); }

void panel_destroy(panel_win_t *pw) {
    if (!pw) return;
    clear_bindings(pw);
    delete pw->win;
    delete pw;
}

} // extern "C"

/* ---------------------------------------------------------------------
 * Named panel registry (optional convenience layer)
 *
 * The core API above is already fully multi-instance: every
 * panel_load_file()/panel_load_string() call returns an independent
 * panel_win_t with its own window and widget state, and there is no
 * hidden global limiting you to one panel. This registry is purely a
 * convenience for a caller (like a Skred fallback handler) that wants
 * to manage several named panels -- "envelope", "pads", "mixer" -- via
 * simple commands like `panel load pads pads.pnl` / `panel reload pads`
 * without hand-rolling its own name -> panel_win_t map.
 * ------------------------------------------------------------------- */

namespace {
struct RegistryEntry {
    panel_win_t *pw;
    std::string path;
};
std::map<std::string, RegistryEntry> g_registry;
}

extern "C" {

panel_win_t *panel_registry_load(const char *name, const char *path) {
    if (!name || !path) return NULL;
    std::map<std::string, RegistryEntry>::iterator it = g_registry.find(name);
    if (it != g_registry.end()) {
        panel_destroy(it->second.pw);
        g_registry.erase(it);
    }
    panel_win_t *pw = panel_load_file(path);
    if (!pw) return NULL;
    RegistryEntry entry;
    entry.pw = pw;
    entry.path = path;
    g_registry[name] = entry;
    return pw;
}

panel_win_t *panel_registry_get(const char *name) {
    if (!name) return NULL;
    std::map<std::string, RegistryEntry>::iterator it = g_registry.find(name);
    return (it != g_registry.end()) ? it->second.pw : NULL;
}

int panel_registry_reload(const char *name) {
    if (!name) return -1;
    std::map<std::string, RegistryEntry>::iterator it = g_registry.find(name);
    if (it == g_registry.end()) return -1;
    return panel_reload_file(it->second.pw, it->second.path.c_str());
}

void panel_registry_show(const char *name) {
    panel_win_t *pw = panel_registry_get(name);
    if (pw) panel_show(pw);
}

void panel_registry_hide(const char *name) {
    panel_win_t *pw = panel_registry_get(name);
    if (pw) panel_hide(pw);
}

void panel_registry_destroy(const char *name) {
    if (!name) return;
    std::map<std::string, RegistryEntry>::iterator it = g_registry.find(name);
    if (it == g_registry.end()) return;
    panel_destroy(it->second.pw);
    g_registry.erase(it);
}

} // extern "C"
