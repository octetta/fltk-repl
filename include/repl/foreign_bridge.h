#ifndef FOREIGN_BRIDGE_H
#define FOREIGN_BRIDGE_H

/*
 * foreign_bridge: lets Skred (via a /ff0..9 foreign-C-function callback)
 * safely trigger an fltk-repl GUI command line, whether that call arrives
 * on the FLTK main thread or on some other thread.
 *
 * Skred's foreign-function callbacks can run on the FLTK main thread (if
 * invoked immediately from a line typed at the REPL) or on Skred's own
 * control-dispatcher thread (if invoked from a pattern, repeat, deferred
 * macro, or /ceb response chain -- see api.h's Threading Notes). Calling
 * FLTK/bitmap_win/panel_dsl functions directly from the wrong thread is
 * unsafe.
 *
 * foreign_bridge_dispatch() handles this by checking which thread it was
 * called from: if it's already the FLTK thread, the handler runs immediately
 * and synchronously. Calls from other threads are copied and marshaled onto
 * the FLTK thread via Fl::awake().
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Matches repl's fallback-handler signature, so an existing dispatch
 * function (e.g. the one passed to repl_set_fallback_handler()) can be
 * used directly as the handler here too. */
typedef void (*foreign_bridge_dispatch_fn)(const char *line, void *user_data);

/* Call once, from the FLTK main thread, during setup -- records which
 * thread counts as "the FLTK thread" for foreign_bridge_dispatch()'s
 * same-thread-vs-other-thread check, and initializes FLTK threading. */
void foreign_bridge_init(void);

/* Sets which function receives dispatched lines, and what user_data it's
 * called with. Call this on the FLTK main thread during setup. */
void foreign_bridge_set_handler(foreign_bridge_dispatch_fn fn, void *user_data);

/* Stop delivering new lines. Queued requests are discarded when consumed. */
void foreign_bridge_shutdown(void);

/* Safe to call from any thread, including Skred's control-dispatcher
 * thread. If called from the same thread that called foreign_bridge_init(),
 * runs the handler immediately and synchronously. Otherwise, copies `line`
 * internally and queues it to run on the FLTK thread via Fl::awake(). If no
 * handler has been set yet, this is a no-op. */
void foreign_bridge_dispatch(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* FOREIGN_BRIDGE_H */
