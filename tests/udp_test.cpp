#include "repl/repl_api.h"
#include "repl/udp_bridge.h"

#include <iostream>
#include <cassert>
#include <cstring>

int main() {
    repl_ctx *ctx = repl_create("UDP Test", 400, 300);
    assert(ctx != nullptr);

    // Initial state: disconnected
    assert(repl_udp_is_connected(ctx) == 0);

    // Connect to local loopback port
    int rc = repl_udp_connect(ctx, "127.0.0.1", 60440);
    assert(rc == 1);
    assert(repl_udp_is_connected(ctx) == 1);

    // Change color and mode
    repl_udp_set_color(ctx, 214); // Amber
    repl_udp_set_mode(ctx, "forward");

    // Send UDP packet
    int bytes = repl_udp_send(ctx, "test_msg", 8);
    assert(bytes == 8);

    // Disconnect
    repl_udp_disconnect(ctx);
    assert(repl_udp_is_connected(ctx) == 0);

    repl_destroy(ctx);
    std::cout << "udp_test passed cleanly!" << std::endl;
    return 0;
}
