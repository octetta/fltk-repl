#include "repl/foreign_bridge.h"

#include <FL/Fl.H>
#include <cstring>
#include <thread>
#include <mutex>

namespace {

foreign_bridge_dispatch_fn g_handler = NULL;
void *g_user_data = NULL;
std::thread::id g_main_thread_id;
bool g_lock_engaged = false;
std::mutex g_lock_mutex;

struct Request {
    char line[256];
};

void run_on_fltk_thread(void *data) {
    Request *req = (Request *)data;
    if (g_handler) g_handler(req->line, g_user_data);
    delete req;
}

} // namespace

extern "C" {

void foreign_bridge_init(void) {
    /* Just remember which thread counts as "the FLTK thread". Deliberately
     * does NOT call Fl::lock() here -- that switches FLTK into
     * multithreaded mode globally and can change event-loop behavior even
     * when nothing else is actually threaded, which is more disruption
     * than the common case (a foreign call firing from the same thread
     * that's already running the REPL) needs. Fl::lock() is only engaged
     * lazily, the first time foreign_bridge_dispatch() is actually called
     * from a thread other than this one. */
    g_main_thread_id = std::this_thread::get_id();
}

void foreign_bridge_set_handler(foreign_bridge_dispatch_fn fn, void *user_data) {
    g_handler = fn;
    g_user_data = user_data;
}

void foreign_bridge_dispatch(const char *line) {
    if (!line || !g_handler) return;

    if (std::this_thread::get_id() == g_main_thread_id) {
        /* Already on the FLTK thread (e.g. /ff9 fired from a line typed
         * live at the prompt) -- call the handler directly and
         * synchronously. No Fl::lock(), no Fl::awake(), no behavior
         * change from before this bridge existed. */
        g_handler(line, g_user_data);
        return;
    }

    /* Genuinely a different thread (e.g. Skred's control-dispatcher
     * thread, per api.h's Threading Notes). Engage Fl::lock() the first
     * time this actually happens -- not before -- then marshal onto the
     * FLTK thread via Fl::awake(). */
    {
        std::lock_guard<std::mutex> lk(g_lock_mutex);
        if (!g_lock_engaged) {
            Fl::lock();
            g_lock_engaged = true;
        }
    }

    Request *req = new Request();
    strncpy(req->line, line, sizeof(req->line) - 1);
    req->line[sizeof(req->line) - 1] = '\0';
    Fl::awake(run_on_fltk_thread, req);
}

} // extern "C"
