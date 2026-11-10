/**
 * @file test_grammar_no_utf8string.cpp
 * @brief No libe3 grammar uses UTF8String.
 *
 * asn1c from v1.5.1 on writes the length of a UTF8String with a SIZE constraint as
 * a 4 or 6 bit field, where asn1tools and the asn1c libe3 used before write a one
 * octet length. A peer cannot tell which it gets, so libe3 carries text in an
 * OCTET STRING. This test reads every grammar under messages/ and examples/ and
 * fails on a UTF8String outside a comment.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string strip_comments(const std::string& text) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        if (text.compare(i, 2, "--") == 0) {
            const size_t close = text.find("--", i + 2);
            const size_t eol = text.find('\n', i + 2);
            if (close != std::string::npos && (eol == std::string::npos || close < eol)) {
                i = close + 2;
            } else {
                i = eol == std::string::npos ? text.size() : eol;
            }
        } else if (text.compare(i, 2, "/*") == 0) {
            const size_t close = text.find("*/", i + 2);
            i = close == std::string::npos ? text.size() : close + 2;
        } else {
            out += text[i++];
        }
    }
    return out;
}

std::vector<fs::path> grammars() {
    std::vector<fs::path> found;
    for (const char* dir : {"messages", "examples"}) {
        const fs::path root = fs::path(LIBE3_GRAMMAR_ROOT) / dir;
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            const auto ext = entry.path().extension();
            if (entry.is_regular_file() && (ext == ".asn" || ext == ".asn1")) found.push_back(entry.path());
        }
    }
    return found;
}

}  // namespace

TEST(GrammarNoUtf8String_grammarsAreFound) {
    ASSERT_GE(grammars().size(), static_cast<size_t>(3));
}

TEST(GrammarNoUtf8String_noGrammarUsesUtf8String) {
    std::string found;
    for (const auto& path : grammars()) {
        std::ifstream in(path);
        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string text = strip_comments(buffer.str());
        if (text.find("UTF8String") != std::string::npos) found += "\n  " + path.string();
    }
    ASSERT_STREQ(found, "");
}

int main() {
    return RUN_ALL_TESTS();
}
