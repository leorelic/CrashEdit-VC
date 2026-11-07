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



GoolScriptLexResult lexGoolScript(const std::string& source) {
    GoolScriptLexResult result;
    std::size_t i = 0U;
    std::size_t line = 1U;
    std::size_t column = 1U;
    const auto isIdentifierStart = [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
    };
    const auto isIdentifierPart = [&](unsigned char ch) {
        return isIdentifierStart(ch) || (ch >= '0' && ch <= '9');
    };
    const auto isOperator = [](char ch) {
        switch (ch) {
        case '+': case '-': case '*': case '/': case '%': case '=':
        case '!': case '<': case '>': case '&': case '|': case '^': case '~':
            return true;
        default:
            return false;
        }
    };
    const auto advance = [](char ch, std::size_t& currentLine, std::size_t& currentColumn) {
        if (ch == '\n') {
            ++currentLine;
            currentColumn = 1U;
        } else {
            ++currentColumn;
        }
    };

    while (i < source.size()) {
        const char ch = source[i];
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance(ch, line, column);
            ++i;
            continue;
        }
        if (ch == '\n') {
            result.tokens.push_back({GoolScriptTokenKind::Newline, "\n", line, column, i});
            advance(ch, line, column);
            ++i;
            continue;
        }
        if (ch == '#') {
            while (i < source.size() && source[i] != '\n') {
                advance(source[i], line, column);
                ++i;
            }
            continue;
        }

        const std::size_t start = i;
        const std::size_t tokenLine = line;
        const std::size_t tokenColumn = column;
        if (ch == '"' || ch == '\'') {
            const char quote = ch;
            advance(ch, line, column);
            ++i;
            bool closed = false;
            while (i < source.size() && source[i] != '\n') {
                const char current = source[i];
                advance(current, line, column);
                ++i;
                if (current == '\\' && i < source.size() && source[i] != '\n') {
                    advance(source[i], line, column);
                    ++i;
                } else if (current == quote) {
                    closed = true;
                    break;
                }
            }
            result.tokens.push_back({GoolScriptTokenKind::String,
                                     source.substr(start, i - start), tokenLine, tokenColumn, start});
            if (!closed) {
                result.diagnostics.push_back({GoolScriptDiagnosticSeverity::Error, tokenLine,
                    tokenColumn, "GS200", "Unterminated string literal; strings must end on the same line."});
            }
            continue;
        }
        if (isIdentifierStart(uch)) {
            do {
                advance(source[i], line, column);
                ++i;
            } while (i < source.size() && isIdentifierPart(static_cast<unsigned char>(source[i])));
            result.tokens.push_back({GoolScriptTokenKind::Identifier,
                                     source.substr(start, i - start), tokenLine, tokenColumn, start});
            continue;
        }
        if (uch >= '0' && uch <= '9') {
            advance(source[i], line, column);
            ++i;
            const bool hexPrefix = ch == '0' && i < source.size() && (source[i] == 'x' || source[i] == 'X');
            if (hexPrefix) {
                advance(source[i], line, column);
                ++i;
                const std::size_t digitsStart = i;
                while (i < source.size()) {
                    const unsigned char digit = static_cast<unsigned char>(source[i]);
                    if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') ||
                          (digit >= 'A' && digit <= 'F') || digit == '_')) break;
                    advance(source[i], line, column);
                    ++i;
                }
                if (i == digitsStart) {
                    result.diagnostics.push_back({GoolScriptDiagnosticSeverity::Error, tokenLine,
                        tokenColumn, "GS203", "Hexadecimal literal requires at least one digit."});
                }
            } else {
                while (i < source.size()) {
                    const unsigned char digit = static_cast<unsigned char>(source[i]);
                    if (!((digit >= '0' && digit <= '9') || digit == '_')) break;
                    advance(source[i], line, column);
                    ++i;
                }
            }
            result.tokens.push_back({GoolScriptTokenKind::Number,
                                     source.substr(start, i - start), tokenLine, tokenColumn, start});
            continue;
        }
        if (isOperator(ch)) {
            do {
                advance(source[i], line, column);
                ++i;
            } while (i < source.size() && isOperator(source[i]));
            result.tokens.push_back({GoolScriptTokenKind::Operator,
                                     source.substr(start, i - start), tokenLine, tokenColumn, start});
            continue;
        }

        advance(ch, line, column);
        ++i;
        result.tokens.push_back({GoolScriptTokenKind::Punctuation,
                                 source.substr(start, 1U), tokenLine, tokenColumn, start});
    }
    result.tokens.push_back({GoolScriptTokenKind::End, "", line, column, source.size()});
    return result;
}


GoolScriptIndentationResult analyzeGoolScript(const std::string& source,
                                               std::size_t indentationWidth) {
    GoolScriptIndentationResult result = analyzeGoolScriptIndentation(source, indentationWidth);
    const GoolScriptLexResult lexed = lexGoolScript(source);
    result.diagnostics.insert(result.diagnostics.end(), lexed.diagnostics.begin(), lexed.diagnostics.end());
    struct Delimiter {
        char value;
        std::size_t line;
        std::size_t column;
    };
    std::vector<Delimiter> delimiters;
    for (const auto& token : lexed.tokens) {
        if (token.kind != GoolScriptTokenKind::Punctuation || token.text.empty()) continue;
        const char ch = token.text.front();
        if (ch == '(' || ch == '[') {
            delimiters.push_back({ch, token.line, token.column});
        } else if (ch == ')' || ch == ']') {
            const char expected = ch == ')' ? '(' : '[';
            if (delimiters.empty() || delimiters.back().value != expected) {
                result.diagnostics.push_back({GoolScriptDiagnosticSeverity::Error, token.line,
                    token.column, "GS201", "Closing delimiter does not match an earlier opening delimiter."});
            } else {
                delimiters.pop_back();
            }
        }
    }
    for (const auto& delimiter : delimiters) {
        result.diagnostics.push_back({GoolScriptDiagnosticSeverity::Error, delimiter.line,
            delimiter.column, "GS202", "Opening delimiter is not closed."});
    }
    return result;
}

} // namespace CrashEdit::Crash
