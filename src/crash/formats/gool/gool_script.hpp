#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace CrashEdit::Crash {

enum class GoolScriptDiagnosticSeverity {
    Error
};

struct GoolScriptDiagnostic {
    GoolScriptDiagnosticSeverity severity = GoolScriptDiagnosticSeverity::Error;
    std::size_t line = 1;
    std::size_t column = 1;
    std::string code;
    std::string message;
};

enum class GoolScriptTokenKind {
    Identifier,
    Number,
    String,
    Operator,
    Punctuation,
    Newline,
    End
};

struct GoolScriptToken {
    GoolScriptTokenKind kind = GoolScriptTokenKind::End;
    std::string text;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t offset = 0;
};

struct GoolScriptLexResult {
    std::vector<GoolScriptToken> tokens;
    std::vector<GoolScriptDiagnostic> diagnostics;

    bool valid() const { return diagnostics.empty(); }
};

struct GoolScriptIndentationResult {
    std::size_t lineCount = 0;
    std::vector<GoolScriptDiagnostic> diagnostics;

    bool valid() const { return diagnostics.empty(); }
};

// Preliminary structural checker for the indentation-based GOOL Script draft.
// A trailing ':' introduces a block; semantic parsing is intentionally separate.
GoolScriptIndentationResult analyzeGoolScriptIndentation(
    const std::string& source,
    std::size_t indentationWidth = 4);

// Tokenize source without assigning language-level meaning to identifiers or operators.
// Newline and End tokens are retained for a future indentation-aware parser.
GoolScriptLexResult lexGoolScript(const std::string& source);

// Run indentation and conservative lexical validation for the current draft language.
// This does not claim to parse or validate GOOL semantics.
GoolScriptIndentationResult analyzeGoolScript(const std::string& source,
                                               std::size_t indentationWidth = 4);

} // namespace CrashEdit::Crash
