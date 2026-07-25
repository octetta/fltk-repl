/*
 * examples/pipe_demo/main.c - CLI Subprocess Wrapper Demo for fltk-repl
 *
 * Demonstrates how fltk-repl can serve as a desktop GUI frontend for any
 * line-buffered command-line application (such as sqlite3, bc, python -i,
 * gforth, or custom CLI utilities).
 *
 * Lines typed into the GUI REPL are sent to the child process's stdin.
 * Output produced by the child process is read over a pipe and rendered
 * into the fltk-repl scrollback view with full editing, clipboard, ANSI
 * 256 colors, and history support.
 *
 * Usage:
 *   ./build/repl_pipe_demo [command]
 *
 * Examples:
 *   ./build/repl_pipe_demo "bc -l"
 *   ./build/repl_pipe_demo "sqlite3"
 *   ./build/repl_pipe_demo "python3 -i"
 */

#include <repl/repl_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef struct {
    repl_ctx *ctx;
    int stdin_fd;
    int stdout_fd;
    pid_t pid;
} pipe_wrapper_t;

static pipe_wrapper_t g_wrapper;

/* Called when the user hits Enter in the GUI REPL */
static void on_line_entered(const char *line, void *userdata) {
    pipe_wrapper_t *w = (pipe_wrapper_t *)userdata;
    if (!w || w->stdin_fd < 0) return;

    // Send the line + newline to the child process stdin
    size_t len = strlen(line);
    ssize_t ret = write(w->stdin_fd, line, len);
    (void)ret;
    ret = write(w->stdin_fd, "\n", 1);
    (void)ret;
}

/* Callback registered with repl_add_fd() when child process stdout has data ready */
static void on_child_stdout_ready(int fd, void *userdata) {
    pipe_wrapper_t *w = (pipe_wrapper_t *)userdata;
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        repl_print(w->ctx, buf);
    } else if (n == 0) {
        // Child closed stdout (process exited)
        repl_remove_fd(fd);
        repl_println(w->ctx, "\n\x1b[38;5;208m[Child process exited]\x1b[0m");
        close(fd);
        w->stdout_fd = -1;
    }
}

static int spawn_child_process(pipe_wrapper_t *w, const char *cmdline) {
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child process: connect pipes to stdin/stdout/stderr
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // Execute shell command
        execl("/bin/sh", "sh", "-c", cmdline, (char *)NULL);
        perror("execl");
        _exit(127);
    }

    // Parent process
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    w->pid = pid;
    w->stdin_fd = stdin_pipe[1];
    w->stdout_fd = stdout_pipe[0];

    // Set non-blocking on stdout pipe
    int flags = fcntl(w->stdout_fd, F_GETFL, 0);
    fcntl(w->stdout_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}
#endif

int main(int argc, char **argv) {
    const char *cmd = (argc >= 2) ? argv[1] : "bc -l";

    repl_ctx *ctx = repl_create("fltk-repl Subprocess Wrapper", 920, 600);
    repl_register_default_commands(ctx);

    repl_printf(ctx, "\x1b[38;5;51mfltk-repl Subprocess Wrapper\x1b[0m\n");
    repl_printf(ctx, "Wrapping CLI command: \x1b[38;5;226m%s\x1b[0m\n\n", cmd);

#ifndef _WIN32
    g_wrapper.ctx = ctx;
    if (spawn_child_process(&g_wrapper, cmd) == 0) {
        repl_set_fallback_handler(ctx, on_line_entered, &g_wrapper);
        repl_set_prompt(ctx, "> ");

        // Register child stdout file descriptor with FLTK event loop
        repl_add_fd(g_wrapper.stdout_fd, on_child_stdout_ready, &g_wrapper);
    } else {
        repl_printf(ctx, "Failed to spawn command: %s\n", cmd);
    }
#else
    repl_println(ctx, "Subprocess pipe wrapper demo is implemented for POSIX systems.");
#endif

    int rc = repl_run(ctx);

#ifndef _WIN32
    if (g_wrapper.stdout_fd >= 0) {
        repl_remove_fd(g_wrapper.stdout_fd);
        close(g_wrapper.stdout_fd);
    }
    if (g_wrapper.stdin_fd >= 0) {
        close(g_wrapper.stdin_fd);
    }
    if (g_wrapper.pid > 0) {
        kill(g_wrapper.pid, SIGTERM);
        waitpid(g_wrapper.pid, NULL, 0);
    }
#endif

    repl_destroy(ctx);
    return rc;
}
