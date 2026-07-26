/*
 * udp_bridge.h - Public C API for fltk-repl UDP network communication.
 *
 * Allows fltk-repl to attach to external programs or remote instances via UDP,
 * send Skode/text commands, and render incoming responses directly in the GUI
 * terminal with a distinct ANSI color.
 */

#ifndef UDP_BRIDGE_H
#define UDP_BRIDGE_H

#include <stddef.h>
#include "repl_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Attach UDP client to remote target host and port. Returns 1 on success, 0 on failure. */
int repl_udp_connect(repl_ctx *ctx, const char *host, int port);

/* Disconnect and close the UDP client socket. */
void repl_udp_disconnect(repl_ctx *ctx);

/* Send raw text or binary data packet to the attached UDP target. Returns bytes sent or -1 on error. */
int repl_udp_send(repl_ctx *ctx, const char *data, size_t len);

/* Set the ANSI 256-color code used to render incoming UDP responses (default 51 = bright cyan). */
void repl_udp_set_color(repl_ctx *ctx, int ansi_color_code);

/* Set UDP line mode: "forward" (auto-send unhandled input), "log" (receive only), "off". */
void repl_udp_set_mode(repl_ctx *ctx, const char *mode);

/* Returns 1 if attached to a UDP target, 0 otherwise. */
int repl_udp_is_connected(repl_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UDP_BRIDGE_H */
