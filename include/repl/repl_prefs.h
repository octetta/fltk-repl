/*
 * repl_prefs.h - Thin pure-C wrapper for FLTK Fl_Preferences.
 *
 * Part of the fltk-repl library. Matches the style and conventions of repl_api.h:
 * opaque handle, simple C types, no C++ exposure.
 *
 * Typical usage:
 *
 *     repl_prefs *prefs = repl_prefs_create(REPL_PREFS_USER, "MyVendor", "MyApp");
 *     int volume = 80;
 *     repl_prefs_get_int(prefs, "audio/volume", &volume, 80);
 *     repl_prefs_set_int(prefs, "audio/volume", volume + 5);
 *     repl_prefs_flush(prefs);
 *     repl_prefs_destroy(prefs);
 */

#ifndef REPL_PREFS_H
#define REPL_PREFS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to a preferences group/database. */
typedef struct repl_prefs repl_prefs;

/* Root scope (mirrors Fl_Preferences::Root) */
typedef enum {
    REPL_PREFS_UNKNOWN = -1,

    REPL_PREFS_SYSTEM = 0,
    REPL_PREFS_USER,
    REPL_PREFS_MEMORY,

    /* Locale-aware variants (recommended for FLTK 1.4+; UTF-8 + C locale) */
    REPL_PREFS_SYSTEM_L,
    REPL_PREFS_USER_L,

    REPL_PREFS_CORE_SYSTEM_L,
    REPL_PREFS_CORE_USER_L,

    /* Add more as needed from FLTK */
} repl_prefs_root;

/* ---------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

/* Create top-level preferences (most common). */
repl_prefs *repl_prefs_create(repl_prefs_root root,
                              const char *vendor,
                              const char *application);

/* Create at arbitrary path (older/deprecated style). */
repl_prefs *repl_prefs_create_path(const char *path,
                                   const char *vendor,
                                   const char *application);

/* Create/open a child group. */
repl_prefs *repl_prefs_create_group(repl_prefs *parent, const char *group_name);

/* Destroy and free resources. */
void repl_prefs_destroy(repl_prefs *prefs);

/* ---------------------------------------------------------------------
 * Persistence
 * --------------------------------------------------------------------- */

/* Write changes to disk if dirty. Returns -1 on error, 0 if written, and 1 if
 * there was nothing to write. */
int repl_prefs_flush(repl_prefs *prefs);

/* Returns non-zero if there are unsaved changes. */
int repl_prefs_dirty(repl_prefs *prefs);

/* ---------------------------------------------------------------------
 * Groups (hierarchical sections)
 * --------------------------------------------------------------------- */

/* Number of child groups. */
int repl_prefs_groups(repl_prefs *prefs);

/* Name of the Nth group (pointer valid until next structural change). */
const char *repl_prefs_group_name(repl_prefs *prefs, int index);

/* Check if a group exists. */
int repl_prefs_group_exists(repl_prefs *prefs, const char *name);

/* Delete a group. */
int repl_prefs_delete_group(repl_prefs *prefs, const char *name);

/* ---------------------------------------------------------------------
 * Entries (key/value pairs)
 * --------------------------------------------------------------------- */

/* Number of entries in this group. */
int repl_prefs_entries(repl_prefs *prefs);

/* Name of the Nth entry. */
const char *repl_prefs_entry_name(repl_prefs *prefs, int index);

/* Check if an entry exists. */
int repl_prefs_entry_exists(repl_prefs *prefs, const char *key);

/* Delete a single entry. */
int repl_prefs_delete_entry(repl_prefs *prefs, const char *key);

/* ---------------------------------------------------------------------
 * Getters (return non-zero on success)
 * --------------------------------------------------------------------- */

int repl_prefs_get_int(repl_prefs *prefs, const char *key, int *value, int default_val);
int repl_prefs_get_float(repl_prefs *prefs, const char *key, float *value, float default_val);
int repl_prefs_get_double(repl_prefs *prefs, const char *key, double *value, double default_val);

/* String: copies into caller-provided buffer. */
int repl_prefs_get_string(repl_prefs *prefs,
                          const char *key,
                          char *buf,
                          int buf_size,
                          const char *default_val);

/* Binary data (stored as hex). out_size receives actual size read. */
int repl_prefs_get_binary(repl_prefs *prefs,
                          const char *key,
                          void *buf,
                          int buf_size,
                          int *out_size,
                          const void *default_data,
                          int default_size);

/* ---------------------------------------------------------------------
 * Setters (return non-zero on success)
 * --------------------------------------------------------------------- */

int repl_prefs_set_int(repl_prefs *prefs, const char *key, int value);
int repl_prefs_set_float(repl_prefs *prefs, const char *key, float value);
int repl_prefs_set_double(repl_prefs *prefs, const char *key, double value);
int repl_prefs_set_string(repl_prefs *prefs, const char *key, const char *value);
int repl_prefs_set_binary(repl_prefs *prefs, const char *key, const void *data, int size);

/* ---------------------------------------------------------------------
 * Utility
 * --------------------------------------------------------------------- */

/* Fill buf with the full path to the preferences file. Returns buf on success. */
char *repl_prefs_filename(repl_prefs *prefs, char *buf, int buf_size);

/* Free any strings returned by the API (currently only used for future-proofing). */
void repl_prefs_free_string(char *s);

#ifdef __cplusplus
}
#endif

#endif /* REPL_PREFS_H */
