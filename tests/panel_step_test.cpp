#include "repl/panel_dsl.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include <string>

static std::string g_last_cmd;

static void test_cmd_handler(const char *cmd, void *) {
    if (cmd) g_last_cmd = cmd;
}

int main() {
    panel_set_command_handler(test_cmd_handler, nullptr);

    const char *dsl =
        "window \"Step Sequencer Test\" 400 200\n"
        "grid 1 4\n"
        "  button s1 \"v0l0.5\" \"v0l1.0\"\n"
        "  button s2 \"v0l0.5\" \"v0l1.0\"\n"
        "  button s3 \"v0l0.5\" \"v0l1.0\"\n"
        "  button s4 \"v0l0.5\" \"v0l1.0\"\n"
        "endgrid\n";

    panel_win_t *pw = panel_load_string_params(dsl, nullptr, "Test");
    assert(pw != nullptr);

    // 1. Test 3-state velocity transitions: Off (0) -> Normal (1) -> Accent (2)
    int rc = panel_set_step_state(pw, "s1", 1, 1);
    assert(rc == 0);
    assert(g_last_cmd == "v0l0.5");

    rc = panel_set_step_state(pw, "s1", 2, 1);
    assert(rc == 0);
    assert(g_last_cmd == "v0l1.0");

    // 2. Test active step playhead highlight
    panel_set_step_highlight(pw, 2);

    panel_destroy(pw);

    // 3. Test loading full drum_machine_pro.pnl file (including musical sharp note names like F#2)
    panel_win_t *pw_pro = panel_load_file("drum_machine_pro.pnl");
    assert(pw_pro != nullptr);
    panel_destroy(pw_pro);

    std::cout << "panel_step_test passed cleanly!" << std::endl;
    return 0;
}
