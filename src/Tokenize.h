#pragma once
#include <string>
#include <vector>

// Splits a command line into tokens, honoring single/double quotes and
// backslash escapes, similar to a simple shell. Not a full shell
// grammar -- no globbing, no variable expansion, just quoting.
std::vector<std::string> repl_tokenize(const std::string &line);
