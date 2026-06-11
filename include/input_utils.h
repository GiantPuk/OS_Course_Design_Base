#pragma once

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

namespace osbasic {

inline bool readIntegerToken(int& value) {
    std::string token;
    char ch = '\0';

    while (std::cin.get(ch)) {
        const auto byte = static_cast<unsigned char>(ch);

        if (byte == 0 || byte == 0xEF || byte == 0xBB || byte == 0xBF ||
            byte == 0xFF || byte == 0xFE) {
            continue;
        }

        if (std::isspace(byte)) {
            if (token.empty()) continue;
            break;
        }

        token.push_back(ch);
    }

    if (token.empty()) {
        return false;
    }

    char* end = nullptr;
    const long parsed = std::strtol(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0') {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

inline void exitOnInputEnd() {
    std::cout << "\n输入结束，程序退出。\n";
    std::exit(0);
}

}  // namespace osbasic
