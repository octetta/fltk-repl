/*
 * repl_prefs.cpp - Thin C wrapper implementation for FLTK Fl_Preferences.
 *
 * Part of fltk-repl. Matches the style and conventions of repl_api.cpp.
 */

#include "repl/repl_prefs.h"
#include <FL/Fl_Preferences.H>

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

struct repl_prefs {
    Fl_Preferences *p = nullptr;
    bool owned = true;  /* false for child groups that share the parent's lifetime */
};

/* ---------------------------------------------------------------------
 * Root mapping
 * --------------------------------------------------------------------- */

static Fl_Preferences::Root to_fltk_root(repl_prefs_root r) {
    switch (r) {
        case REPL_PREFS_USER:           return Fl_Preferences::USER;
        case REPL_PREFS_SYSTEM:         return Fl_Preferences::SYSTEM;
        case REPL_PREFS_MEMORY:         return Fl_Preferences::MEMORY;
        case REPL_PREFS_USER_L:         return Fl_Preferences::USER_L;
        case REPL_PREFS_SYSTEM_L:       return Fl_Preferences::SYSTEM_L;
        case REPL_PREFS_CORE_USER_L:    return Fl_Preferences::CORE_USER_L;
        case REPL_PREFS_CORE_SYSTEM_L:  return Fl_Preferences::CORE_SYSTEM_L;
        default:                        return Fl_Preferences::USER;
    }
}

/* ---------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

repl_prefs *repl_prefs_create(repl_prefs_root root, const char *vendor, const char *application) {
    if (!vendor || !application) return nullptr;
    auto *rp = new repl_prefs();
    rp->p = new Fl_Preferences(to_fltk_root(root), vendor, application);
    rp->owned = true;
    return rp;
}

repl_prefs *repl_prefs_create_path(const char *path, const char *vendor, const char *application) {
    if (!path || !vendor || !application) return nullptr;
    auto *rp = new repl_prefs();
    rp->p = new Fl_Preferences(path, vendor, application);
    rp->owned = true;
    return rp;
}

repl_prefs *repl_prefs_create_group(repl_prefs *parent, const char *group_name) {
    if (!parent || !parent->p || !group_name) return nullptr;
    auto *rp = new repl_prefs();
    rp->p = new Fl_Preferences(*(parent->p), group_name);
    rp->owned = true;  /* Fl_Preferences manages its own sub-objects */
    return rp;
}

void repl_prefs_destroy(repl_prefs *rp) {
    if (!rp) return;
    if (rp->owned && rp->p) {
        delete rp->p;
    }
    delete rp;
}

/* ---------------------------------------------------------------------
 * Flush / Dirty
 * --------------------------------------------------------------------- */

int repl_prefs_flush(repl_prefs *rp) {
    if (!rp || !rp->p) return 0;
    return rp->p->flush();
}

int repl_prefs_dirty(repl_prefs *rp) {
    if (!rp || !rp->p) return 0;
    return rp->p->dirty();
}

/* ---------------------------------------------------------------------
 * Groups
 * --------------------------------------------------------------------- */

int repl_prefs_groups(repl_prefs *rp) {
    if (!rp || !rp->p) return 0;
    return rp->p->groups();
}

const char *repl_prefs_group_name(repl_prefs *rp, int index) {
    if (!rp || !rp->p) return nullptr;
    return rp->p->group(index);
}

int repl_prefs_group_exists(repl_prefs *rp, const char *name) {
    if (!rp || !rp->p || !name) return 0;
    return rp->p->group_exists(name);
}

int repl_prefs_delete_group(repl_prefs *rp, const char *name) {
    if (!rp || !rp->p || !name) return 0;
    return rp->p->delete_group(name);
}

/* ---------------------------------------------------------------------
 * Entries
 * --------------------------------------------------------------------- */

int repl_prefs_entries(repl_prefs *rp) {
    if (!rp || !rp->p) return 0;
    return rp->p->entries();
}

const char *repl_prefs_entry_name(repl_prefs *rp, int index) {
    if (!rp || !rp->p) return nullptr;
    return rp->p->entry(index);
}

int repl_prefs_entry_exists(repl_prefs *rp, const char *key) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->entry_exists(key);
}

int repl_prefs_delete_entry(repl_prefs *rp, const char *key) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->delete_entry(key);
}

/* ---------------------------------------------------------------------
 * Getters
 * --------------------------------------------------------------------- */

int repl_prefs_get_int(repl_prefs *rp, const char *key, int *value, int default_val) {
    if (!rp || !rp->p || !key || !value) return 0;
    return rp->p->get(key, *value, default_val);
}

int repl_prefs_get_float(repl_prefs *rp, const char *key, float *value, float default_val) {
    if (!rp || !rp->p || !key || !value) return 0;
    return rp->p->get(key, *value, default_val);
}

int repl_prefs_get_double(repl_prefs *rp, const char *key, double *value, double default_val) {
    if (!rp || !rp->p || !key || !value) return 0;
    return rp->p->get(key, *value, default_val);
}

int repl_prefs_get_string(repl_prefs *rp, const char *key, char *buf, int buf_size, const char *default_val) {
    if (!rp || !rp->p || !key || !buf || buf_size <= 0) return 0;
    char *val = nullptr;
    int success = rp->p->get(key, val, default_val ? default_val : "");
    if (success && val) {
        strncpy(buf, val, buf_size - 1);
        buf[buf_size - 1] = '\0';
        // Note: FLTK allocates val; we do NOT free it here (caller must not assume ownership)
        // In practice for get(char*&, ...) FLTK docs say caller must free, but to keep simple we copy.
        // Adjust if you prefer different semantics.
        delete[] val;  // safe for the char*& overload
        return 1;
    }
    if (default_val) {
        strncpy(buf, default_val, buf_size - 1);
        buf[buf_size - 1] = '\0';
    } else {
        buf[0] = '\0';
    }
    return success ? 1 : 0;
}

int repl_prefs_get_binary(repl_prefs *rp, const char *key,
                          void *buf, int buf_size, int *out_size,
                          const void *default_data, int default_size) {
    if (!rp || !rp->p || !key || !buf || !out_size) return 0;
    int size = 0;
    int success = rp->p->get(key, buf, default_data, default_size, &size);
    if (out_size) *out_size = size;
    return success;
}

/* ---------------------------------------------------------------------
 * Setters
 * --------------------------------------------------------------------- */

int repl_prefs_set_int(repl_prefs *rp, const char *key, int value) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->set(key, value);
}

int repl_prefs_set_float(repl_prefs *rp, const char *key, float value) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->set(key, value);
}

int repl_prefs_set_double(repl_prefs *rp, const char *key, double value) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->set(key, value);
}

int repl_prefs_set_string(repl_prefs *rp, const char *key, const char *value) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->set(key, value ? value : "");
}

int repl_prefs_set_binary(repl_prefs *rp, const char *key, const void *data, int size) {
    if (!rp || !rp->p || !key) return 0;
    return rp->p->set(key, data, size);
}

/* ---------------------------------------------------------------------
 * Utility
 * --------------------------------------------------------------------- */

char *repl_prefs_filename(repl_prefs *rp, char *buf, int buf_size) {
    if (!rp || !rp->p || !buf || buf_size <= 0) return nullptr;
    if (rp->p->filename(buf, buf_size)) {
        return buf;
    }
    return nullptr;
}

void repl_prefs_free_string(char *s) {
    free(s);
}
