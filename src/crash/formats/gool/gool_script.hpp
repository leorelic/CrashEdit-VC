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

// Run indentation and conservative lexical validation for the current draft language.
// This does not claim to parse or validate GOOL semantics.
GoolScriptIndentationResult analyzeGoolScript(const std::string& source,
                                               std::size_t indentationWidth = 4);

} // namespace CrashEdit::Crash
