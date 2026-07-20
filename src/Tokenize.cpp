#include "Tokenize.h"

std::vector<std::string> repl_tokenize(const std::string &line) {
    std::vector<std::string> out;
    std::string cur;
    bool inToken = false;
    char quote = 0; // 0, '\'' or '"'

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (quote) {
            if (c == '\\' && quote == '"' && i + 1 < line.size()) {
                char n = line[i + 1];
                if (n == '"' || n == '\\' || n == '$') {
                    cur += n;
                    ++i;
                    continue;
                }
            }
            if (c == quote) {
                quote = 0;
            } else {
                cur += c;
            }
            continue;
        }

        if (c == ' ' || c == '\t') {
            if (inToken) {
                out.push_back(cur);
                cur.clear();
                inToken = false;
            }
            continue;
        }

        if (c == '\'' || c == '"') {
            quote = c;
            inToken = true;
            continue;
        }

        if (c == '\\' && i + 1 < line.size()) {
            cur += line[i + 1];
            ++i;
            inToken = true;
            continue;
        }

        cur += c;
        inToken = true;
    }

    if (inToken || !cur.empty()) {
        out.push_back(cur);
    }
    return out;
}
