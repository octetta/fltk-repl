#include "repl/panel_dsl.h"
#include <iostream>
#include <cassert>
#include <cstring>

static std::string g_last_cmd;

static void test_cmd_handler(const char *cmd, void *) {
    if (cmd) g_last_cmd = cmd;
}

int main() {
    const char *dsl =
        "window \"Pitch Bend Test\" 300 200\n"
        "slider pitch -12 12 1 \"pitch_bend %f\" =0 ~spring ~live\n";

    panel_win_t *pw = panel_load_string_params(dsl, nullptr, "Test");
    assert(pw != nullptr);

    // Verify initial value is 0
    char valbuf[64] = {0};
    int rc = panel_get_value(pw, "pitch", valbuf, sizeof(valbuf));
    assert(rc == 0);
    assert(atof(valbuf) == 0.0);

    // Set value to +7 (simulate drag)
    rc = panel_set_value_num(pw, "pitch", 7.0, 1);
    assert(rc == 0);
    assert(g_last_cmd.find("7.000") != std::string::npos);

    panel_destroy(pw);
    std::cout << "panel_spring_test passed cleanly!" << std::endl;
    return 0;
}
