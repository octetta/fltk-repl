#pragma once

#include <string>

bool repl_voice_graph_to_pikchr(const char *graphText,
                                std::string &pikchrSource,
                                int &rootVoice,
                                std::string &error);
