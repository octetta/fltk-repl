#include "repl/foreign_bridge.h"

#include <FL/Fl.H>
#include <mutex>
#include <string>
#include <thread>

namespace {

foreign_bridge_dispatch_fn g_handler = nullptr;
void *g_user_data = nullptr;
std::thread::id g_main_thread_id;
std::mutex g_state_mutex;

struct Request {
    std::string line;
};

void run_on_fltk_thread(void *data) {
    Request *req = static_cast<Request *>(data);
    if (!req) return;

    foreign_bridge_dispatch_fn handler;
    void *user_data;
    {
        std::lock_guard<std::mutex> lk(g_state_mutex);
        handler = g_handler;
        user_data = g_user_data;
    }
    if (handler) handler(req->line.c_str(), user_data);
    delete req;
}

} // namespace

extern "C" {

void foreign_bridge_init(void) {
    /* FLTK requires the main thread to initialize its lock before workers
     * call Fl::awake(). It must never be initialized lazily by a worker. */
    g_main_thread_id = std::this_thread::get_id();
    Fl::lock();
}

void foreign_bridge_set_handler(foreign_bridge_dispatch_fn fn, void *user_data) {
    std::lock_guard<std::mutex> lk(g_state_mutex);
    g_handler = fn;
    g_user_data = user_data;
}

void foreign_bridge_shutdown(void) {
    std::lock_guard<std::mutex> lk(g_state_mutex);
    g_handler = nullptr;
    g_user_data = nullptr;
}

void foreign_bridge_dispatch(const char *line) {
    if (!line) return;

    foreign_bridge_dispatch_fn handler;
    void *user_data;
    {
        std::lock_guard<std::mutex> lk(g_state_mutex);
        handler = g_handler;
        user_data = g_user_data;
    }
    if (!handler) return;

    if (std::this_thread::get_id() == g_main_thread_id) {
        handler(line, user_data);
        return;
    }

    Request *req = new Request();
    req->line = line;
    Fl::awake(run_on_fltk_thread, req);
}

} // extern "C"
