#include "crash/formats/gool/gool_script.hpp"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace CrashEdit::Crash {
namespace {

struct IndentationLevel {
    std::size_t columns = 0;
    std::size_t line = 0;
};

std::string_view trimLeft(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    return first == std::string_view::npos ? std::string_view{} : value.substr(first);
}

std::string_view trimRight(std::string_view value) {
    const auto last = value.find_last_not_of(" \t\r");
    return last == std::string_view::npos ? std::string_view{} : value.substr(0, last + 1U);
}

std::string_view withoutComment(std::string_view value) {
    char quote = '\0';
    bool escaped = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\' && quote != '\0') {
            escaped = true;
            continue;
        }
        if (quote != '\0') {
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            continue;
        }
        if (ch == '#') return value.substr(0, i);
    }
    return value;
}

void addError(GoolScriptIndentationResult& result, std::size_t line,
              std::size_t column, const char* code, const char* message) {
    result.diagnostics.push_back({GoolScriptDiagnosticSeverity::Error, line, column, code, message});
}

} // namespace

GoolScriptIndentationResult analyzeGoolScriptIndentation(
    const std::string& source, std::size_t indentationWidth) {
    GoolScriptIndentationResult result;
    indentationWidth = std::max<std::size_t>(1U, indentationWidth);

    std::vector<IndentationLevel> levels{{0U, 0U}};
    bool expectsIndentedBlock = false;
    std::size_t blockHeaderLine = 0;
    std::size_t lineNumber = 0;
    std::size_t start = 0;

    while (start <= source.size()) {
        const std::size_t newline = source.find('\n', start);
        const std::size_t end = newline == std::string::npos ? source.size() : newline;
        std::string_view line(source.data() + start, end - start);
        ++lineNumber;
        result.lineCount = lineNumber;

        std::size_t columns = 0;
        std::size_t rawIndentCharacters = 0;
        bool sawSpace = false;
        bool sawTab = false;
        while (rawIndentCharacters < line.size()) {
            const char ch = line[rawIndentCharacters];
            if (ch == ' ') {
                sawSpace = true;
                ++columns;
            } else if (ch == '\t') {
                sawTab = true;
                addError(result, lineNumber, rawIndentCharacters + 1U,
                         "GS100", "Tabs are not allowed for indentation; use spaces.");
                columns += indentationWidth - (columns % indentationWidth);
            } else {
                break;
            }
            ++rawIndentCharacters;
        }

        const std::string_view content = trimRight(trimLeft(line));
        const std::string_view code = trimRight(withoutComment(content));
        if (!code.empty()) {
            if (sawSpace && sawTab) {
                addError(result, lineNumber, 1U, "GS101",
                         "Do not mix tabs and spaces in indentation.");
            }
            if (columns % indentationWidth != 0U) {
                addError(result, lineNumber, 1U, "GS102",
                         "Indentation must use consistent multiples of the configured width.");
            }

            if (expectsIndentedBlock && columns <= levels.back().columns) {
                addError(result, lineNumber, rawIndentCharacters + 1U,
                         "GS103", "Expected an indented block after the preceding ':' line.");
            }
            expectsIndentedBlock = false;

            if (columns > levels.back().columns) {
                if (columns % indentationWidth == 0U && levels.back().columns < columns) {
                    // An increase is valid only when the preceding significant line opened a block.
                    const bool previousLineOpenedBlock =
                        blockHeaderLine != 0U && blockHeaderLine < lineNumber;
                    if (!previousLineOpenedBlock) {
                        addError(result, lineNumber, 1U, "GS104",
                                 "Unexpected indentation; no preceding block header opens this level.");
                    } else {
                        levels.push_back({columns, lineNumber});
                    }
                }
            } else if (columns < levels.back().columns) {
                while (levels.size() > 1U && columns < levels.back().columns)
                    levels.pop_back();
                if (columns != levels.back().columns) {
                    addError(result, lineNumber, rawIndentCharacters + 1U,
                             "GS105", "Dedentation does not match an earlier indentation level.");
                }
            }

            const bool opensBlock = !code.empty() && code.back() == ':';
            if (opensBlock) {
                expectsIndentedBlock = true;
                blockHeaderLine = lineNumber;
            } else {
                blockHeaderLine = 0U;
            }
        }

        if (newline == std::string::npos) break;
        start = newline + 1U;
    }

    if (expectsIndentedBlock) {
        addError(result, blockHeaderLine, 1U, "GS103",
                 "Expected an indented block after the final ':' line.");
    }
    return result;
}

} // namespace CrashEdit::Crash
