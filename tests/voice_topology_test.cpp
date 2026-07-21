#include "VoiceTopology.h"
extern "C" {
#include "pikchr.h"
}

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    const char graph[] =
        "skred-voice-graph 1\n"
        "root 8\n"
        "node 8\n"
        "edge 8 9 0 pitch\n"
        "node 9\n"
        "edge 8 9 1 gate\n"
        "edge 8 8 2 phase-mod\n"
        "edge 9 8 0 pitch\n"
        "end\n";
    std::string source;
    std::string error;
    int root = -1;
    if (!repl_voice_graph_to_pikchr(graph, source, root, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (root != 8 || source.find("V8") == std::string::npos ||
        source.find("V9") == std::string::npos ||
        source.find("pitch/gate") == std::string::npos) {
        std::cerr << "voice graph conversion lost topology data\n";
        return 1;
    }

    int width = 0;
    int height = 0;
    char *svg = pikchr(source.c_str(), "test", PIKCHR_DARK_MODE |
                       PIKCHR_PLAINTEXT_ERRORS, &width, &height);
    const std::string rendered = svg ? svg : "";
    std::free(svg);
    if (width <= 0 || height <= 0 || rendered.find("<svg") == std::string::npos ||
        rendered.find("<text") == std::string::npos) {
        std::cerr << "Pikchr rejected generated source:\n" << rendered << '\n';
        return 1;
    }
    return 0;
}
