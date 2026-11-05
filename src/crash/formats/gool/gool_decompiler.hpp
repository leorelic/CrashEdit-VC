#pragma once

#include "crash/formats/gool/gool_ir.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace CrashEdit::Crash {

struct GoolIRDecompileOptions {
    std::size_t startInstruction = 0;
    std::size_t endInstruction = static_cast<std::size_t>(-1); // Exclusive.
    bool includeHeader = true;
};

struct GoolIRDecompilation {
    std::string text;
    std::vector<std::string> diagnostics;
};

// Produces a readable, source-mapped listing. This first pass deliberately
// keeps instruction-level operations where GOOL semantics are not established.
GoolIRDecompilation decompileGoolIR(
    const GoolIRProgram& program,
    const GoolIRDecompileOptions& options = {});

}
