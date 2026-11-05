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
    bool preserveStructuredBranchOffsets = false;
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

// Decompile only the instructions assigned to one discovered routine. This
// initial view preserves instruction order and reports known exits; it does
// not yet claim structured control flow across non-contiguous blocks.
GoolIRDecompilation decompileGoolIRRoutine(
    const GoolIRProgram& program, std::size_t routineIndex);

}
