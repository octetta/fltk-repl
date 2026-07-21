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
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

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
    std::string tmpl;          /* command template */
    std::vector<std::string> options; /* choice only */
    std::string text;          /* label only */
    int weight = 1;
};

struct ParsedRow {
    std::vector<ParsedItem> items;
};

struct ParsedWindow {
    std::string title = "Panel";
    int width = 320;
    int height = 200;
    std::vector<ParsedRow> rows;
};

/* ---------------------------------------------------------------------
 * Tokenizer: whitespace-separated tokens, with "double-quoted strings"
 * treated as a single token (quotes stripped).
 * ------------------------------------------------------------------- */

std::vector<std::string> tokenize(const std::string &line) {
    std::vector<std::string> out;
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
            out.push_back(tok);
            i = (j < n) ? j + 1 : j;
        } else {
            size_t j = i;
            while (j < n && !isspace((unsigned char)line[j])) ++j;
            out.push_back(line.substr(i, j - i));
            i = j;
        }
    }
    return out;
}

int parse_weight(std::vector<std::string> &toks) {
    if (!toks.empty() && toks.back().size() > 1 && toks.back()[0] == '@') {
        int w = atoi(toks.back().c_str() + 1);
        toks.pop_back();
        return w > 0 ? w : 1;
    }
    return 1;
}

/* ---------------------------------------------------------------------
 * Parser
 * ------------------------------------------------------------------- */

bool parse_dsl(const std::string &text, ParsedWindow &pw, std::string &err) {
    std::istringstream in(text);
    std::string raw;
    int lineno = 0;
    bool in_row = false;
    ParsedRow current_row;

    auto emit_item = [&](const ParsedItem &item) {
        if (in_row) current_row.items.push_back(item);
        else { ParsedRow r; r.items.push_back(item); pw.rows.push_back(r); }
    };

    while (std::getline(in, raw)) {
        ++lineno;
        size_t hash = raw.find('#');
        std::string line = (hash == std::string::npos) ? raw : raw.substr(0, hash);
        std::vector<std::string> t = tokenize(line);
        if (t.empty()) continue;

        const std::string &kw = t[0];

        if (kw == "window") {
            if (t.size() < 4) { err = "line " + std::to_string(lineno) + ": window needs \"title\" width height"; return false; }
            pw.title = t[1];
            pw.width = atoi(t[2].c_str());
            pw.height = atoi(t[3].c_str());
        } else if (kw == "row") {
            if (in_row) { err = "line " + std::to_string(lineno) + ": nested row not supported"; return false; }
            in_row = true;
            current_row = ParsedRow();
        } else if (kw == "endrow") {
            if (!in_row) { err = "line " + std::to_string(lineno) + ": endrow without row"; return false; }
            pw.rows.push_back(current_row);
            in_row = false;
        } else if (kw == "slider" || kw == "field") {
            /* name min max [unit] "template" [@weight] */
            if (t.size() < 5) { err = "line " + std::to_string(lineno) + ": " + kw + " needs name min max [unit] \"template\""; return false; }
            std::vector<std::string> rest(t.begin() + 1, t.end());
            int weight = parse_weight(rest);
            ParsedItem it;
            it.type = (kw == "slider") ? IT_SLIDER : IT_FIELD;
            it.name = rest[0];
            it.min = atof(rest[1].c_str());
            it.max = atof(rest[2].c_str());
            if (rest.size() == 5) { it.unit = rest[3]; it.tmpl = rest[4]; }
            else if (rest.size() == 4) { it.tmpl = rest[3]; }
            else { err = "line " + std::to_string(lineno) + ": too many tokens for " + kw; return false; }
            it.weight = weight;
            emit_item(it);
        } else if (kw == "toggle" || kw == "button") {
            /* name "template" [@weight] */
            if (t.size() < 3) { err = "line " + std::to_string(lineno) + ": " + kw + " needs name \"template\""; return false; }
            std::vector<std::string> rest(t.begin() + 1, t.end());
            int weight = parse_weight(rest);
            if (rest.size() != 2) { err = "line " + std::to_string(lineno) + ": " + kw + " needs exactly name \"template\""; return false; }
            ParsedItem it;
            it.type = (kw == "toggle") ? IT_TOGGLE : IT_BUTTON;
            it.name = rest[0];
            it.tmpl = rest[1];
            it.weight = weight;
            emit_item(it);
        } else if (kw == "choice") {
            /* name "opt opt opt" "template" [@weight] */
            if (t.size() < 4) { err = "line " + std::to_string(lineno) + ": choice needs name \"options\" \"template\""; return false; }
            std::vector<std::string> rest(t.begin() + 1, t.end());
            int weight = parse_weight(rest);
            if (rest.size() != 3) { err = "line " + std::to_string(lineno) + ": choice needs exactly name \"options\" \"template\""; return false; }
            ParsedItem it;
            it.type = IT_CHOICE;
            it.name = rest[0];
            {
                std::istringstream oss(rest[1]);
                std::string opt;
                while (oss >> opt) it.options.push_back(opt);
            }
            it.tmpl = rest[2];
            it.weight = weight;
            if (it.options.empty()) { err = "line " + std::to_string(lineno) + ": choice has no options"; return false; }
            emit_item(it);
        } else if (kw == "label") {
            if (t.size() < 2) { err = "line " + std::to_string(lineno) + ": label needs \"text\""; return false; }
            std::vector<std::string> rest(t.begin() + 1, t.end());
            int weight = parse_weight(rest);
            ParsedItem it;
            it.type = IT_LABEL;
            it.text = rest[0];
            it.weight = weight;
            emit_item(it);
        } else {
            err = "line " + std::to_string(lineno) + ": unknown statement '" + kw + "'";
            return false;
        }
    }

    if (in_row) { err = "unterminated row (missing endrow)"; return false; }
    return true;
}

/* ---------------------------------------------------------------------
 * Template substitution: replaces the first %d, %f, or %s occurrence.
 * ------------------------------------------------------------------- */

std::string format_template_int(const std::string &tmpl, long v) {
    size_t p = tmpl.find("%d");
    if (p == std::string::npos) return tmpl;
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", v);
    return tmpl.substr(0, p) + buf + tmpl.substr(p + 2);
}

std::string format_template_float(const std::string &tmpl, double v) {
    size_t p = tmpl.find("%f");
    if (p == std::string::npos) return format_template_int(tmpl, (long)v);
    char buf[64];
    snprintf(buf, sizeof buf, "%.3f", v);
    return tmpl.substr(0, p) + buf + tmpl.substr(p + 2);
}

std::string format_template_str(const std::string &tmpl, const std::string &v) {
    size_t p = tmpl.find("%s");
    if (p == std::string::npos) return tmpl;
    return tmpl.substr(0, p) + v + tmpl.substr(p + 2);
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
 * Per-widget callbacks: each Fl_Widget's user_data() points at a
 * heap-allocated ItemBinding that carries what's needed to build the
 * command line. Bindings are owned by the panel and freed on rebuild.
 * ------------------------------------------------------------------- */

struct ItemBinding {
    std::string tmpl;
    std::vector<std::string> options; /* choice only */
};

void slider_cb(Fl_Widget *w, void *ud) {
    ItemBinding *b = (ItemBinding *)ud;
    double v = ((Fl_Slider *)w)->value();
    dispatch(format_template_float(b->tmpl, v));
}

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

void button_cb(Fl_Widget *, void *ud) {
    ItemBinding *b = (ItemBinding *)ud;
    dispatch(b->tmpl); /* fired as-is, no substitution */
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
const int H_CONTROL = 44;  /* slider/field/choice: label line + control */
const int H_TOGGLE = 26;
const int H_BUTTON = 28;
const int H_LABEL = 22;

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

} // namespace

/* ---------------------------------------------------------------------
 * panel_win: opaque handle
 * ------------------------------------------------------------------- */

struct panel_win {
    Fl_Double_Window *win;
    Fl_Group *content;                       /* holds all built widgets */
    std::vector<ItemBinding *> bindings;      /* owned, freed on rebuild/destroy */
};

namespace {

void clear_bindings(panel_win_t *pw) {
    for (size_t i = 0; i < pw->bindings.size(); ++i) delete pw->bindings[i];
    pw->bindings.clear();
}

/* Builds widgets for `pwin` into pw->content, sized to pw->win's current
 * width, growing pw->win's height if the content doesn't fit. Returns the
 * total content height used. */
int build_widgets(panel_win_t *pw, const ParsedWindow &pwin) {
    pw->content->clear();
    clear_bindings(pw);

    int content_w = pwin.width - 2 * MARGIN;
    int y = MARGIN;

    for (size_t ri = 0; ri < pwin.rows.size(); ++ri) {
        const ParsedRow &row = pwin.rows[ri];
        int rh = row_height(row);

        int weight_sum = 0;
        for (size_t i = 0; i < row.items.size(); ++i) weight_sum += row.items[i].weight;
        int n = (int)row.items.size();
        int gaps_w = GAP_X * (n - 1 > 0 ? n - 1 : 0);
        int avail_w = content_w - gaps_w;

        int x = MARGIN;
        for (size_t i = 0; i < row.items.size(); ++i) {
            const ParsedItem &it = row.items[i];
            int iw = (weight_sum > 0) ? (avail_w * it.weight / weight_sum) : avail_w;
            /* last item absorbs rounding remainder */
            if (i + 1 == row.items.size()) {
                iw = content_w - (x - MARGIN) - 0;
            }

            ItemBinding *bind = NULL;
            if (it.type != IT_LABEL) {
                bind = new ItemBinding();
                bind->tmpl = it.tmpl;
                bind->options = it.options;
                pw->bindings.push_back(bind);
            }

            switch (it.type) {
                case IT_SLIDER: {
                    std::string label = it.name + (it.unit.empty() ? "" : (" (" + it.unit + ")"));
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->copy_label(label.c_str());
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    Fl_Slider *s = new Fl_Slider(x, y + 16, iw, rh - 16, "");
                    s->type(FL_HOR_NICE_SLIDER);
                    s->bounds(it.min, it.max);
                    s->value((it.min + it.max) / 2.0);
                    s->callback(slider_cb, bind);
                    s->when(FL_WHEN_RELEASE);
                    pw->content->add(lbl);
                    pw->content->add(s);
                    break;
                }
                case IT_FIELD: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->copy_label(it.name.c_str());
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    Fl_Int_Input *f = new Fl_Int_Input(x, y + 16, iw, rh - 16, "");
                    char defbuf[32];
                    snprintf(defbuf, sizeof defbuf, "%d", (int)it.min);
                    f->value(defbuf);
                    f->callback(field_cb, bind);
                    f->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
                    pw->content->add(lbl);
                    pw->content->add(f);
                    break;
                }
                case IT_TOGGLE: {
                    Fl_Check_Button *c = new Fl_Check_Button(x, y, iw, rh, "");
                    c->copy_label(it.name.c_str());
                    c->callback(toggle_cb, bind);
                    pw->content->add(c);
                    break;
                }
                case IT_BUTTON: {
                    Fl_Button *b = new Fl_Button(x, y, iw, rh, "");
                    b->copy_label(it.name.c_str());
                    b->callback(button_cb, bind);
                    pw->content->add(b);
                    break;
                }
                case IT_CHOICE: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, 14, "");
                    lbl->copy_label(it.name.c_str());
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    Fl_Choice *ch = new Fl_Choice(x, y + 16, iw, rh - 16, "");
                    for (size_t oi = 0; oi < it.options.size(); ++oi)
                        ch->add(it.options[oi].c_str());
                    ch->value(0);
                    ch->callback(choice_cb, bind);
                    pw->content->add(lbl);
                    pw->content->add(ch);
                    break;
                }
                case IT_LABEL: {
                    Fl_Box *lbl = new Fl_Box(x, y, iw, rh, "");
                    lbl->copy_label(it.text.c_str());
                    lbl->labelfont(FL_HELVETICA_BOLD);
                    lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
                    pw->content->add(lbl);
                    break;
                }
            }

            x += iw + GAP_X;
        }

        y += rh + GAP_Y;
    }

    return y + MARGIN;
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
    pw->win = new Fl_Double_Window(pwin.width, pwin.height, pwin.title.c_str());
    pw->content = new Fl_Group(0, 0, pwin.width, pwin.height);
    pw->win->add(pw->content);

    int used_h = build_widgets(pw, pwin);
    if (used_h > pwin.height) {
        pw->win->size(pwin.width, used_h);
        pw->content->size(pwin.width, used_h);
    }

    pw->content->end();
    pw->win->end();
    pw->win->callback([](Fl_Widget *w, void *) { w->hide(); });
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
        return -1; /* existing panel left untouched */
    }

    pw->win->label(pwin.title.c_str());
    int used_h = build_widgets(pw, pwin);
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
    delete pw->win; /* deletes contained widgets, incl. pw->content */
    delete pw;
}

} // extern "C"
