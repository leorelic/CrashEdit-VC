#pragma once
#include "crash/formats/nsf/nsf_archive.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace CrashEdit::Crash {
enum class NsfGoolFlowEdgeKind : std::uint8_t {
    Fallthrough, Branch, BranchTrue, BranchFalse, Call, ExternalCall, Return
};
struct NsfGoolBasicBlock {
    std::size_t startInstruction=0;
    std::size_t endInstruction=0; // exclusive
};
struct NsfGoolFlowEdge {
    std::size_t sourceBlock=0;
    std::size_t targetInstruction=0;
    NsfGoolFlowEdgeKind kind=NsfGoolFlowEdgeKind::Fallthrough;
    bool targetValid=false;
    std::string diagnostic;
};
struct NsfGoolControlFlowGraph {
    std::vector<NsfGoolBasicBlock> blocks;
    std::vector<NsfGoolFlowEdge> edges;
    std::vector<std::string> diagnostics;
};
NsfGoolControlFlowGraph analyzeGoolControlFlow(const NsfGool& gool);
}
