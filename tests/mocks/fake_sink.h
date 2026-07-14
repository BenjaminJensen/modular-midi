#pragma once

#include <string>
#include <string_view>
#include <vector>

class FakeSink {
public:
    std::vector<std::string> lines;

    void write(std::string_view text) {
        lines.emplace_back(text);
    }
};
