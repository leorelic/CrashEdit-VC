#pragma once
#include "crash/formats/nsf/nsf_archive.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace CrashEdit::Crash {
enum class NsfGoolFlowEdgeKind : std::uint8_t {
    Fallthrough, Branch, BranchTrue, BranchFalse, Call, ExternalCall, StateTransition, StateTransitionTrue, StateTransitionFalse, Return
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
enum class NsfGoolOperandAccess : std::uint8_t { Read, Write, Unknown };
struct NsfGoolOperandEffect {
    char operandName='?';
    NsfGoolOperandAccess access=NsfGoolOperandAccess::Read;
    std::string reference;
    bool trackable = false;
};
struct NsfGoolDataFlowDefinition { std::size_t instructionIndex=0; std::string location; };
// True when at least one incoming path may still carry an initial or otherwise
// unmodeled value for this location. Reaching definitions alone cannot express
// that possibility at joins or after unknown/native side effects.
struct NsfGoolDataFlowUse { std::size_t instructionIndex=0; std::string location; std::vector<std::size_t> reachingDefinitions; bool mayHaveUnmodeledValue=false; };
struct NsfGoolDataFlowResult { std::vector<NsfGoolDataFlowDefinition> definitions; std::vector<NsfGoolDataFlowUse> uses; };
struct NsfGoolInstructionAnalysis {
    bool operandsKnown=false;
    std::vector<NsfGoolOperandEffect> operandEffects;
    std::string executionEffect;
};
NsfGoolInstructionAnalysis analyzeGoolInstruction(const NsfGool& gool, std::size_t instructionIndex);
NsfGoolDataFlowResult analyzeGoolDataFlow(const NsfGool& gool);
struct NsfGoolControlFlowGraph {
    std::vector<NsfGoolBasicBlock> blocks;
    std::vector<NsfGoolFlowEdge> edges;
    std::vector<std::string> diagnostics;
};
NsfGoolControlFlowGraph analyzeGoolControlFlow(const NsfGool& gool);
}
