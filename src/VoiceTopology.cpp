#include "VoiceTopology.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace {

struct Edge {
    int from;
    int to;
    std::string label;
};

std::string safeLabel(const std::string &label) {
    std::string result;
    for (unsigned char ch : label) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
            result.push_back(static_cast<char>(ch));
        }
    }
    return result.empty() ? "link" : result;
}

} // namespace

bool repl_voice_graph_to_pikchr(const char *graphText,
                                std::string &pikchrSource,
                                int &rootVoice,
                                std::string &error) {
    pikchrSource.clear();
    error.clear();
    rootVoice = -1;
    if (!graphText || !*graphText) {
        error = "empty voice graph";
        return false;
    }

    int version = 0;
    bool ended = false;
    std::set<int> nodes;
    std::vector<Edge> rawEdges;
    std::istringstream input(graphText);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string record;
        fields >> record;
        if (record == "skred-voice-graph") {
            fields >> version;
        } else if (record == "root") {
            fields >> rootVoice;
        } else if (record == "node") {
            int voice = -1;
            if (fields >> voice) nodes.insert(voice);
        } else if (record == "edge") {
            int from = -1, to = -1, type = -1;
            std::string label;
            if (!(fields >> from >> to >> type >> label)) {
                error = "invalid edge record";
                return false;
            }
            (void)type;
            nodes.insert(from);
            nodes.insert(to);
            rawEdges.push_back({from, to, safeLabel(label)});
        } else if (record == "end") {
            ended = true;
        }
    }
    if (version != 1 || rootVoice < 0 || !ended) {
        error = "unsupported voice graph protocol";
        return false;
    }
    nodes.insert(rootVoice);

    std::map<std::pair<int, int>, std::string> grouped;
    for (const Edge &edge : rawEdges) {
        std::string &label = grouped[{edge.from, edge.to}];
        if (label.empty()) label = edge.label;
        else if (label.find(edge.label) == std::string::npos)
            label += "/" + edge.label;
    }

    std::map<int, std::vector<int>> outgoing;
    for (const auto &entry : grouped)
        outgoing[entry.first.first].push_back(entry.first.second);
    std::map<int, int> level;
    std::queue<int> pending;
    level[rootVoice] = 0;
    pending.push(rootVoice);
    while (!pending.empty()) {
        const int from = pending.front();
        pending.pop();
        for (int to : outgoing[from]) {
            if (!level.count(to)) {
                level[to] = level[from] + 1;
                pending.push(to);
            }
        }
    }
    int fallbackLevel = 0;
    for (const auto &entry : level) fallbackLevel = std::max(fallbackLevel, entry.second);
    for (int voice : nodes)
        if (!level.count(voice)) level[voice] = fallbackLevel + 1;

    std::map<int, std::vector<int>> columns;
    for (int voice : nodes) columns[level[voice]].push_back(voice);
    std::map<int, std::pair<double, double>> position;
    for (auto &column : columns) {
        std::sort(column.second.begin(), column.second.end());
        const double middle = (column.second.size() - 1) * 0.5;
        for (size_t row = 0; row < column.second.size(); ++row) {
            position[column.second[row]] = {
                column.first * 1.65,
                (middle - static_cast<double>(row)) * 0.9
            };
        }
    }

    std::ostringstream out;
    out << "boxwid = 0.82\nboxht = 0.46\nboxrad = 0.06\n"
           "linewid = 0.012\ncharht = 0.14\ncharwid = 0.075\n";
    for (int voice : nodes) {
        const auto at = position[voice];
        out << "V" << voice << ": box \"v" << voice << "\" at ("
            << at.first << "," << at.second << ") "
            << (voice == rootVoice ?
                "fill 0x29445a color 0xffc870 thickness 150%\n" :
                "fill 0x142a36 color 0x8fe9ff\n");
    }
    for (const auto &entry : grouped) {
        const int from = entry.first.first;
        const int to = entry.first.second;
        const std::string &label = entry.second;
        if (from == to) {
            out << "spline -> from V" << from
                << ".n up 35% then right 55% then down 35% to V"
                << to << ".e \"" << label << "\" above color 0xb4f0ff\n";
            continue;
        }
        const auto fromAt = position[from];
        const auto toAt = position[to];
        const char *fromSide;
        const char *toSide;
        if (std::fabs(toAt.first - fromAt.first) >=
            std::fabs(toAt.second - fromAt.second)) {
            fromSide = toAt.first >= fromAt.first ? ".e" : ".w";
            toSide = toAt.first >= fromAt.first ? ".w" : ".e";
        } else {
            fromSide = toAt.second >= fromAt.second ? ".n" : ".s";
            toSide = toAt.second >= fromAt.second ? ".s" : ".n";
        }
        out << "arrow from V" << from << fromSide << " to V" << to
            << toSide << " \"" << label << "\" above color 0xb4f0ff\n";
    }
    pikchrSource = out.str();
    return true;
}
