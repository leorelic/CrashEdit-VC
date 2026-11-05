#ifdef NDEBUG
#undef NDEBUG
#endif

#include "crash/formats/gool/gool_ir.hpp"
#include "crash/formats/gool/gool_decompiler.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

using namespace CrashEdit::Crash;

int main() {
    NsfGool source;
    source.version = NsfGoolVersion::Version2;
    source.id = 17;
    source.classId = 23;
    source.format = 2;
    source.heapBase = 0x120;
    source.eventCount = 4;
    source.entryCount = 1;
    source.dataPool = {0x0200007B, -7};
    source.stateMap = {0, 1};

    NsfGoolInstruction known;
    known.byteOffset = 0x40;
    known.word = 0x00123456;
    known.opcode = 0;
    known.operandBits = 0x123456;
    known.mnemonic = "ADD";
    source.instructions.push_back(known);

    NsfGoolInstruction native;
    native.byteOffset = 0x44;
    native.word = 0x012A4021;
    native.isMips = true;
    native.mipsKnown = true;
    native.mnemonic = "addu";
    native.arguments = "$t0, $t1, $t2";
    source.instructions.push_back(native);

    NsfGoolInstruction unknown;
    unknown.byteOffset = 0x48;
    unknown.word = 0xFA123456;
    unknown.opcode = 250;
    unknown.operandBits = 0x123456;
    unknown.mnemonic = "UNK";
    source.instructions.push_back(unknown);

    source.stateDescriptors.push_back({1, 2, 3, 4, 5, 6});
    source.stateReferences.push_back({0, 2, 3, 1, 0x0200007B, true, NsfGoolStateReferenceStatus::External});
    source.stateHookReferences.push_back({0, 1, 2, NsfGoolStateHookKind::Event,
        0x5234, 0x1234, true, true, NsfGoolStateHookStatus::External});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Transition,
        0x0007, 7, false, true, NsfGoolStateHookStatus::Local});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0x3FFF, 0x3FFF, false, false, NsfGoolStateHookStatus::Absent});
    source.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Event,
        0x4009, 9, true, true, NsfGoolStateHookStatus::OffsetOutOfRange});
    source.frameGroupBytes = 12;
    source.frameGroupsDecoded = true;
    source.frameGroups.push_back({3, 8, 0x0200007B, 0x60, 0x18, 2, 9, true});

    const auto ir = buildGoolIR(source);
    assert(ir.targetVersion == static_cast<std::uint8_t>(NsfGoolVersion::Version2));
    assert(ir.id == 17 && ir.classId == 23 && ir.format == 2);
    assert(ir.heapBase == 0x120 && ir.eventCount == 4 && ir.entryCount == 1);
    assert(ir.dataPool == source.dataPool && ir.stateMap == source.stateMap);
    assert(ir.instructions.size() == 3);

    const auto& knownIR = ir.instructions[0];
    assert(knownIR.sourceByteOffset == 0x40 && knownIR.rawWord == known.word);
    assert(knownIR.rawOperandBits == known.operandBits && knownIR.opcode == 0);
    assert(!knownIR.nativeMips && knownIR.opcodeKnown && !knownIR.operands.empty());

    const auto& nativeIR = ir.instructions[1];
    assert(nativeIR.sourceByteOffset == 0x44 && nativeIR.rawWord == native.word);
    assert(nativeIR.nativeMips && nativeIR.nativeInstructionDecoded);
    assert(nativeIR.mnemonic == "addu" && nativeIR.nativeArguments == "$t0, $t1, $t2");
    assert(nativeIR.operands.empty());

    const auto& unknownIR = ir.instructions[2];
    assert(unknownIR.sourceByteOffset == 0x48 && unknownIR.rawWord == unknown.word);
    assert(!unknownIR.nativeMips && !unknownIR.opcodeKnown);
    assert(unknownIR.rawOperandBits == unknown.operandBits);

    assert(ir.stateDescriptors.size() == 1);
    assert(ir.stateDescriptors[0].stateFlags == 1 && ir.stateDescriptors[0].blockFlags == 2);
    assert(ir.stateDescriptors[0].goolIndex == 3 && ir.stateDescriptors[0].eventHook == 4);
    assert(ir.stateDescriptors[0].transitionHook == 5 && ir.stateDescriptors[0].codeHook == 6);
    assert(ir.stateReferences.size() == 1 && ir.stateReferences[0].eid == 0x0200007B);
    assert(ir.stateReferences[0].targetFound && ir.stateReferences[0].targetChunkIndex == 2);
    assert(ir.stateHooks.size() == 4 && ir.stateHooks[0].rawValue == 0x5234);
    assert(ir.stateHooks[0].external && ir.stateHooks[0].targetFound);
    assert(ir.hookDispatches.size() == 4);
    assert(ir.hookDispatches[0].kind == GoolIRHookKind::Event);
    assert(ir.hookDispatches[0].resolution == GoolIRHookResolution::External);
    assert(ir.hookDispatches[0].instructionOffset == 0x1234);
    assert(ir.hookDispatches[0].targetChunkIndex == 1 && ir.hookDispatches[0].targetEntryIndex == 2);
    assert(ir.hookDispatches[1].kind == GoolIRHookKind::Transition);
    assert(ir.hookDispatches[1].resolution == GoolIRHookResolution::Local);
    assert(ir.hookDispatches[2].kind == GoolIRHookKind::Code);
    assert(ir.hookDispatches[2].resolution == GoolIRHookResolution::Absent);
    assert(ir.hookDispatches[3].resolution == GoolIRHookResolution::OffsetOutOfRange);
    assert(ir.routineEntries.empty() && ir.routines.empty());
    assert(ir.frameGroupBytes == 12 && ir.frameGroupsDecoded && ir.frameGroups.size() == 1);
    assert(ir.frameGroups[0].sourceByteOffset == 0x60 && ir.frameGroups[0].interpolated);

    NsfGool flowSource;
    flowSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction branch;
    branch.opcode = 50; // BRA with zero displacement targets the next instruction.
    branch.operandBits = 0;
    flowSource.instructions.push_back(branch);
    NsfGoolInstruction sequential;
    sequential.opcode = 0;
    flowSource.instructions.push_back(sequential);
    NsfGoolInstruction ret;
    ret.opcode = 49;
    flowSource.instructions.push_back(ret);

    const auto flowIR = buildGoolIR(flowSource);
    assert(flowIR.basicBlocks.size() == 2);
    assert(flowIR.instructionToBlock.size() == 3);
    assert(flowIR.instructionToBlock[0] == 0 && flowIR.instructionToBlock[1] == 1);
    assert(flowIR.instructionToBlock[2] == 1);
    assert(!flowIR.flowEdges.empty());
    assert(flowIR.flowEdges[0].kind == GoolIRFlowEdgeKind::Branch);
    assert(flowIR.flowEdges[0].targetValid && flowIR.flowEdges[0].targetInstruction == 1);
    bool hasDynamicReturn = false;
    for (const auto& edge : flowIR.flowEdges)
        if (edge.kind == GoolIRFlowEdgeKind::Return && !edge.targetValid)
            hasDynamicReturn = true;
    assert(hasDynamicReturn);

    // Calls, returns, and state transitions have explicit source-mapped IR records.
    NsfGool semanticSource;
    semanticSource.version = NsfGoolVersion::Version2;
    semanticSource.dataPool = {static_cast<std::int32_t>(0x0200007BU)};
    semanticSource.stateDescriptors.push_back({0, 0, 0, 0, 0, 0});
    semanticSource.stateReferences.push_back({0, 0, 0, 0, 0x0200007B, true,
        NsfGoolStateReferenceStatus::Local});
    NsfGoolInstruction localCall;
    localCall.opcode = 59;
    localCall.byteOffset = 0x80;
    localCall.operandBits = 2U; // I = instruction 2; E = local; V = 0
    semanticSource.instructions.push_back(localCall);
    NsfGoolInstruction go;
    go.opcode = 53;
    go.byteOffset = 0x84;
    go.operandBits = 0U; // S = descriptor 0; V = 0
    semanticSource.instructions.push_back(go);
    NsfGoolInstruction retSemantic;
    retSemantic.opcode = 49;
    retSemantic.byteOffset = 0x88;
    semanticSource.instructions.push_back(retSemantic);
    const auto semanticIR = buildGoolIR(semanticSource);
    assert(semanticIR.controlOperations.size() == 3);
    assert(semanticIR.controlOperations[0].kind == GoolIRControlOperationKind::Call);
    assert(semanticIR.controlOperations[0].sourceByteOffset == 0x80);
    assert(semanticIR.controlOperations[0].targetInstructionValid);
    assert(semanticIR.controlOperations[0].targetInstruction == 2);
    assert(semanticIR.routineEntries.size() == 1);
    assert(semanticIR.routineEntries[0].instruction == 2);
    assert(semanticIR.routineEntries[0].fromLocalCall);
    assert(semanticIR.routines.size() == 1);
    assert(semanticIR.routines[0].entryInstruction == 2);
    assert((semanticIR.routines[0].instructions == std::vector<std::size_t>{2}));
    assert(!semanticIR.controlOperations[0].external);
    assert(semanticIR.controlOperations[1].kind == GoolIRControlOperationKind::StateTransition);
    assert(semanticIR.controlOperations[1].targetDescriptorValid);
    assert(semanticIR.controlOperations[1].targetGoolEidValid);
    assert(semanticIR.controlOperations[1].targetGoolEid == 0x0200007B);
    assert(semanticIR.controlOperations[1].hasTargetReferenceStatus);
    assert(semanticIR.controlOperations[2].kind == GoolIRControlOperationKind::Return);
    assert(!semanticIR.controlOperations[2].targetInstructionValid);

    NsfGool mipsSource;
    mipsSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction jalr;
    jalr.isMips = true;
    jalr.word = 0x02A0F809U; // jalr $ra, $s5
    jalr.byteOffset = 0x44;
    jalr.mnemonic = "jalr";
    jalr.arguments = "$ra,$s5";
    jalr.mipsKnown = true;
    mipsSource.instructions.push_back(jalr);
    const auto mipsIR = buildGoolIR(mipsSource);
    assert(mipsIR.instructions.size() == 1);
    const auto& jalrIR = mipsIR.instructions[0];
    assert(jalrIR.nativeMips && jalrIR.nativeFieldsDecoded);
    assert(jalrIR.nativeFields.format == GoolIRMipsFormat::Register);
    assert(jalrIR.nativeFields.primaryOpcode == 0);
    assert(jalrIR.nativeFields.rs == 21);
    assert(jalrIR.nativeFields.rd == 31);
    assert(jalrIR.nativeFields.function == 9);
    assert(jalrIR.rawWord == 0x02A0F809U);
    assert(jalrIR.sourceByteOffset == 0x44);

    // The first source-like decompiler keeps branches, calls, transitions,
    // returns, and unsupported words traceable to their original instruction.
    NsfGool decompileSource;
    decompileSource.version = NsfGoolVersion::Version2;
    decompileSource.id = 0x0200007B;
    decompileSource.dataPool = {static_cast<std::int32_t>(0x0200007B)};
    decompileSource.stateDescriptors.push_back({0, 0, 0, 0, 0, 0});
    decompileSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        4, 4, false, true, NsfGoolStateHookStatus::Local});

    NsfGoolInstruction conditionalBranch;
    conditionalBranch.byteOffset = 0x100;
    conditionalBranch.opcode = 51; // BNEZ subtype, relative target = instruction 2.
    conditionalBranch.operandBits = (29U << 14U) | 1U;
    decompileSource.instructions.push_back(conditionalBranch);

    NsfGoolInstruction call;
    call.byteOffset = 0x104;
    call.opcode = 59;
    call.operandBits = 3U | (1U << 20U); // Local instruction 3, one argument.
    decompileSource.instructions.push_back(call);

    NsfGoolInstruction transition;
    transition.byteOffset = 0x108;
    transition.opcode = 53; // GO state descriptor 0.
    transition.operandBits = 0U;
    decompileSource.instructions.push_back(transition);

    NsfGoolInstruction returnInstruction;
    returnInstruction.byteOffset = 0x10C;
    returnInstruction.opcode = 49;
    decompileSource.instructions.push_back(returnInstruction);

    NsfGoolInstruction unknownInstruction;
    unknownInstruction.byteOffset = 0x110;
    unknownInstruction.word = 0xFA123456U;
    unknownInstruction.opcode = 250;
    unknownInstruction.operandBits = 0x123456U;
    decompileSource.instructions.push_back(unknownInstruction);

    NsfGoolInstruction unknownMips;
    unknownMips.byteOffset = 0x114;
    unknownMips.word = 0xFFFFFFFFU;
    unknownMips.isMips = true;
    decompileSource.instructions.push_back(unknownMips);

    const auto decompileIR = buildGoolIR(decompileSource);
    const auto decompiled = decompileGoolIR(decompileIR);
    assert(decompiled.text.find("if (subtype != 0) goto L00002") != std::string::npos);
    assert(decompiled.text.find("call L00003(args=1)") != std::string::npos);
    assert(decompiled.text.find("transition state_descriptor[0]") != std::string::npos);
    assert(decompiled.text.find("GOOL 0x0200007B") != std::string::npos);
    assert(decompiled.text.find("return") != std::string::npos);
    assert(decompiled.text.find("L00004:") != std::string::npos);
    assert(decompiled.text.find(".gool_word 0xFA123456") != std::string::npos);
    assert(decompiled.text.find(".mips_word 0xFFFFFFFF") != std::string::npos);
    assert(decompiled.text.find("@0x0100") != std::string::npos);
    assert(decompiled.diagnostics.size() == 2);

    GoolIRDecompileOptions selectedRange;
    selectedRange.startInstruction = 2;
    selectedRange.endInstruction = 4;
    selectedRange.includeHeader = false;
    const auto selectedDecompilation = decompileGoolIR(decompileIR, selectedRange);
    assert(selectedDecompilation.text.find("L00002:") == 0);
    assert(selectedDecompilation.text.find("transition state_descriptor[0]") != std::string::npos);
    assert(selectedDecompilation.text.find("return") != std::string::npos);
    assert(selectedDecompilation.text.find(".gool_word") == std::string::npos);

    // A forward conditional branch whose fallthrough arm ends in BRA to a
    // later join can be represented as a structured if block without changing
    // the branch condition or dropping source mapping.
    NsfGool structuredSource;
    structuredSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction skipBody;
    skipBody.opcode = 52; // BEQZ branches to instruction 3; fallthrough means field != 0.
    skipBody.operandBits = 2U | (29U << 14U); // displacement 2, field subtype.
    skipBody.byteOffset = 0x200;
    structuredSource.instructions.push_back(skipBody);
    NsfGoolInstruction bodyInstruction;
    bodyInstruction.opcode = 0;
    bodyInstruction.byteOffset = 0x204;
    structuredSource.instructions.push_back(bodyInstruction);
    NsfGoolInstruction joinBranch;
    joinBranch.opcode = 50; // BRA from instruction 2 to instruction 4.
    joinBranch.operandBits = 1U;
    joinBranch.byteOffset = 0x208;
    structuredSource.instructions.push_back(joinBranch);
    NsfGoolInstruction alternateInstruction;
    alternateInstruction.opcode = 47;
    alternateInstruction.byteOffset = 0x20C;
    structuredSource.instructions.push_back(alternateInstruction);
    NsfGoolInstruction joinInstruction;
    joinInstruction.opcode = 47;
    joinInstruction.byteOffset = 0x210;
    structuredSource.instructions.push_back(joinInstruction);

    const auto structuredIR = buildGoolIR(structuredSource);
    const auto structuredText = decompileGoolIR(structuredIR);
    assert(structuredText.text.find("if (subtype != 0) {") != std::string::npos);
    assert(structuredText.text.find("    }\n") != std::string::npos);
    assert(structuredText.text.find("goto L00004") == std::string::npos);
    assert(structuredText.text.find("@0x0204") != std::string::npos);
    assert(structuredText.text.find("@0x0208") == std::string::npos);

    // Two adjacent forward-if clauses with a shared join become else-if.
    GoolIRProgram chainProgram;
    auto appendChainInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                      std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        chainProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand chainField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->subtype"};
    const GoolIROperand chainValue2{'R', 12, 2, 2,
        GoolIROperandKind::Value, "2"};
    const auto chainDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendChainInstruction(4, 0x400, {chainField, chainValue2});
    appendChainInstruction(52, 0x404, {chainDisplacement(2)});
    appendChainInstruction(47, 0x408);
    appendChainInstruction(50, 0x40C, {chainDisplacement(5)});
    appendChainInstruction(4, 0x410, {chainField,
        GoolIROperand{'R', 12, 10, 10, GoolIROperandKind::Value, "10"}});
    appendChainInstruction(52, 0x414, {chainDisplacement(2)});
    appendChainInstruction(47, 0x418);
    appendChainInstruction(50, 0x41C, {chainDisplacement(1)});
    appendChainInstruction(47, 0x420);
    appendChainInstruction(47, 0x424);
    const auto chainOutput = decompileGoolIR(chainProgram);
    assert(chainOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(chainOutput.text.find("} else if (self->subtype == 10) {") != std::string::npos);

    // A unique fallback target becomes a plain else arm.
    GoolIRProgram elseProgram;
    auto appendElseInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                     std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        elseProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand elseField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->subtype"};
    const GoolIROperand elseValue{'R', 12, 2, 2,
        GoolIROperandKind::Value, "2"};
    const auto elseDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendElseInstruction(4, 0x500, {elseField, elseValue});
    appendElseInstruction(52, 0x504, {elseDisplacement(3)});
    appendElseInstruction(47, 0x508);
    appendElseInstruction(50, 0x50C, {elseDisplacement(2)});
    appendElseInstruction(47, 0x510);
    appendElseInstruction(47, 0x514);
    appendElseInstruction(47, 0x518); // common join
    const auto elseOutput = decompileGoolIR(elseProgram);
    assert(elseOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(elseOutput.text.find("} else {") != std::string::npos);
    assert(elseOutput.text.find("L00004:") == std::string::npos);
    assert(elseOutput.text.find("@0x0514") != std::string::npos);

    // A pre-test loop with one backward BRA is reconstructed as while.
    GoolIRProgram loopProgram;
    auto appendLoopInstruction = [&](std::uint8_t opcode, std::uint32_t offset,
                                     std::vector<GoolIROperand> operands = {}) {
        GoolIRInstruction item;
        item.opcode = opcode;
        item.opcodeKnown = true;
        item.sourceByteOffset = offset;
        item.mnemonic = opcode == 4U ? "EQL" : opcode == 52U ? "BEQZ" :
                        opcode == 50U ? "BRA" : "NOP";
        item.operands = std::move(operands);
        loopProgram.instructions.push_back(std::move(item));
    };
    const GoolIROperand loopField{'L', 12, 0, 0,
        GoolIROperandKind::ProcessField, "self->counter"};
    const GoolIROperand loopLimit{'R', 12, 3, 3,
        GoolIROperandKind::Value, "3"};
    const auto loopDisplacement = [](std::int32_t value) {
        return GoolIROperand{'I', 12, static_cast<std::uint32_t>(value), value,
            GoolIROperandKind::Signed, std::to_string(value)};
    };
    appendLoopInstruction(4, 0x600, {loopField, loopLimit});
    appendLoopInstruction(52, 0x604, {loopDisplacement(3)});
    appendLoopInstruction(47, 0x608);
    appendLoopInstruction(47, 0x60C);
    appendLoopInstruction(50, 0x610, {loopDisplacement(-4)});
    appendLoopInstruction(47, 0x614);
    const auto loopOutput = decompileGoolIR(loopProgram);
    assert(loopOutput.text.find("while (self->counter == 3) {") != std::string::npos);
    assert(loopOutput.text.find("goto L") == std::string::npos);
    assert(loopOutput.text.find("@0x0614") != std::string::npos);

    // A comparison immediately feeding BEQZ should become the source
    // expression, not expose the temporary stack result as "sp != 0".
    GoolIRProgram comparisonProgram;
    GoolIRInstruction comparison;
    comparison.opcode = 4;
    comparison.opcodeKnown = true;
    comparison.mnemonic = "EQL";
    comparison.sourceByteOffset = 0x300;
    comparison.operands.push_back({'L', 12, 0, 0, GoolIROperandKind::ProcessField, "self->subtype"});
    comparison.operands.push_back({'R', 12, 2, 2, GoolIROperandKind::Value, "2"});
    comparisonProgram.instructions.push_back(comparison);

    GoolIRInstruction conditional;
    conditional.opcode = 52;
    conditional.opcodeKnown = true;
    conditional.mnemonic = "BEQZ";
    conditional.sourceByteOffset = 0x304;
    conditional.operands.push_back({'I', 12, 2, 2, GoolIROperandKind::Signed, "2"});
    conditional.operands.push_back({'V', 4, 0, 0, GoolIROperandKind::Value, "0"});
    conditional.operands.push_back({'R', 12, 0, 0, GoolIROperandKind::ProcessField, "sp"});
    comparisonProgram.instructions.push_back(conditional);

    GoolIRInstruction comparisonBody;
    comparisonBody.opcode = 47;
    comparisonBody.opcodeKnown = true;
    comparisonBody.mnemonic = "NOP";
    comparisonBody.sourceByteOffset = 0x308;
    comparisonProgram.instructions.push_back(comparisonBody);

    GoolIRInstruction comparisonJoinBranch;
    comparisonJoinBranch.opcode = 50;
    comparisonJoinBranch.opcodeKnown = true;
    comparisonJoinBranch.mnemonic = "BRA";
    comparisonJoinBranch.sourceByteOffset = 0x30C;
    comparisonJoinBranch.operands.push_back({'I', 12, 1, 1, GoolIROperandKind::Signed, "1"});
    comparisonProgram.instructions.push_back(comparisonJoinBranch);

    GoolIRInstruction comparisonElse;
    comparisonElse.opcode = 47;
    comparisonElse.opcodeKnown = true;
    comparisonElse.mnemonic = "NOP";
    comparisonElse.sourceByteOffset = 0x310;
    comparisonProgram.instructions.push_back(comparisonElse);

    GoolIRInstruction comparisonJoin;
    comparisonJoin.opcode = 47;
    comparisonJoin.opcodeKnown = true;
    comparisonJoin.mnemonic = "NOP";
    comparisonJoin.sourceByteOffset = 0x314;
    comparisonProgram.instructions.push_back(comparisonJoin);

    const auto comparisonOutput = decompileGoolIR(comparisonProgram);
    assert(comparisonOutput.text.find("if (self->subtype == 2) {") != std::string::npos);
    assert(comparisonOutput.text.find("if (sp != 0)") == std::string::npos);


    // Routine discovery is shared across GOOL objects: local Code hooks
    // establish entry points, while RET prevents traversal into the next routine.
    NsfGool routineSource;
    routineSource.version = NsfGoolVersion::Version2;
    for (std::size_t i = 0; i < 6; ++i) {
        NsfGoolInstruction instruction;
        instruction.opcode = (i == 2U || i == 4U) ? 49U : 47U;
        instruction.byteOffset = i * 4U;
        routineSource.instructions.push_back(instruction);
    }
    routineSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    routineSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        3, 3, false, true, NsfGoolStateHookStatus::Local});
    const auto routineIR = buildGoolIR(routineSource);
    assert(routineIR.routineEntries.size() == 2);
    assert(routineIR.routines.size() == 2);
    assert(routineIR.routines[0].entryInstruction == 0);
    assert((routineIR.routines[0].instructions == std::vector<std::size_t>{0, 1, 2}));
    assert(routineIR.routines[1].entryInstruction == 3);
    assert((routineIR.routines[1].instructions == std::vector<std::size_t>{3, 4}));
    assert(!routineIR.routines[0].hasSharedInstructions);
    assert(!routineIR.routines[1].hasSharedInstructions);
    assert((routineIR.unassignedInstructions == std::vector<std::size_t>{5}));

    const auto firstRoutineText = decompileGoolIRRoutine(boundaryIR, 0);
    assert(firstRoutineText.text.find("Routine 0 entry: L00000") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0000") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0004") != std::string::npos);
    assert(firstRoutineText.text.find("@0x0008") == std::string::npos);
    assert(firstRoutineText.text.find("entry-point boundary") != std::string::npos);
    const auto invalidRoutineText = decompileGoolIRRoutine(invalidBranchIR, 0);
    assert(invalidRoutineText.text.find("invalid branch target") != std::string::npos);
    assert(!decompileGoolIRRoutine(boundaryIR, 99).diagnostics.empty());

    // Reaching another known entry point is reported as a boundary rather than
    // silently treating it as shared code or absorbing it into the first routine.
    NsfGool boundarySource;
    boundarySource.version = NsfGoolVersion::Version2;
    for (std::size_t i = 0; i < 4; ++i) {
        NsfGoolInstruction instruction;
        instruction.opcode = 47U;
        instruction.byteOffset = i * 4U;
        boundarySource.instructions.push_back(instruction);
    }
    boundarySource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    boundarySource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        2, 2, false, true, NsfGoolStateHookStatus::Local});
    const auto boundaryIR = buildGoolIR(boundarySource);
    assert(boundaryIR.routines.size() == 2);
    assert((boundaryIR.routines[0].instructions == std::vector<std::size_t>{0, 1}));
    assert((boundaryIR.routines[0].entryPointBoundaries == std::vector<std::size_t>{2}));
    assert((boundaryIR.routines[1].instructions == std::vector<std::size_t>{2, 3}));
    assert(!boundaryIR.routines[0].hasSharedInstructions);
    assert(!boundaryIR.routines[1].hasSharedInstructions);

    assert(boundaryIR.routines[0].exits.size() == 1);
    assert(boundaryIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::EntryPointBoundary);
    assert(boundaryIR.routines[0].exits[0].sourceInstruction == 1);
    assert(boundaryIR.routines[0].exits[0].targetInstruction == 2);

    // RET is an explicit routine exit, while a final ordinary instruction is
    // reported as an incomplete end-of-program path rather than an invented RET.
    NsfGool exitSource;
    exitSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction exitReturn;
    exitReturn.opcode = 49U;
    exitSource.instructions.push_back(exitReturn);
    NsfGoolInstruction exitTail;
    exitTail.opcode = 47U;
    exitSource.instructions.push_back(exitTail);
    exitSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    exitSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        1, 1, false, true, NsfGoolStateHookStatus::Local});
    const auto exitIR = buildGoolIR(exitSource);
    assert(exitIR.routines.size() == 2);
    assert(exitIR.routines[0].exits.size() == 1);
    assert(exitIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::Return);
    assert(exitIR.routines[0].exits[0].sourceInstruction == 0);
    assert(exitIR.routines[1].exits.size() == 1);
    assert(exitIR.routines[1].exits[0].kind == GoolIRRoutineExitKind::EndOfProgram);
    assert(!exitIR.routines[1].exits[0].targetValid);
    assert(!exitIR.routines[1].diagnostics.empty());

    // Invalid branch destinations remain explicit routine exits. The
    // analysis must not silently drop an out-of-range edge or invent a return.
    NsfGool invalidBranchSource;
    invalidBranchSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction invalidBranch;
    invalidBranch.opcode = 50U; // BRA to instruction 6, outside this one-word program.
    invalidBranch.operandBits = 5U;
    invalidBranchSource.instructions.push_back(invalidBranch);
    invalidBranchSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto invalidBranchIR = buildGoolIR(invalidBranchSource);
    assert(invalidBranchIR.routines.size() == 1);
    assert(invalidBranchIR.routines[0].instructions == std::vector<std::size_t>{0});
    assert(invalidBranchIR.routines[0].exits.size() == 1);
    assert(invalidBranchIR.routines[0].exits[0].kind ==
        GoolIRRoutineExitKind::InvalidBranchTarget);
    assert(invalidBranchIR.routines[0].exits[0].sourceInstruction == 0);
    assert(invalidBranchIR.routines[0].exits[0].flowKind == GoolIRFlowEdgeKind::Branch);
    assert(!invalidBranchIR.routines[0].exits[0].targetValid);
    assert(!invalidBranchIR.routines[0].exits[0].diagnostic.empty());

    // Local calls are represented as call relationships, not traversed as
    // ordinary control flow. The destination remains its own discovered routine.
    NsfGool callSource;
    callSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction routineLocalCall;
    routineLocalCall.opcode = 59U;
    routineLocalCall.operandBits = 1U; // local instruction target 1
    callSource.instructions.push_back(routineLocalCall);
    NsfGoolInstruction callReturn;
    callReturn.opcode = 49U;
    callSource.instructions.push_back(callReturn);
    callSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto callIR = buildGoolIR(callSource);
    assert(callIR.routines.size() == 2);
    assert(callIR.routines[0].calls.size() == 1);
    assert(callIR.routines[0].calls[0].sourceInstruction == 0);
    assert(callIR.routines[0].calls[0].targetInstructionValid);
    assert(callIR.routines[0].calls[0].targetInstruction == 1);
    assert(callIR.routines[0].calls[0].targetRoutineFound);
    assert(callIR.routines[0].calls[0].targetRoutineIndex == 1);
    assert(callIR.routines[1].entryInstruction == 1);
    assert(callIR.routines[1].exits.size() == 1);
    assert(callIR.routines[1].exits[0].kind == GoolIRRoutineExitKind::Return);

    // Two independent entries can converge on ordinary code that is not itself
    // a known entry point. Preserve the exact shared instruction, not just a flag.
    NsfGool sharedSource;
    sharedSource.version = NsfGoolVersion::Version2;
    NsfGoolInstruction firstConditional;
    firstConditional.opcode = 51U; // BNEZ: branch from 0 to 3, or fall through to 1.
    firstConditional.operandBits = 2U | (29U << 14U);
    sharedSource.instructions.push_back(firstConditional);
    NsfGoolInstruction firstJump;
    firstJump.opcode = 50U; // BRA from 1 to the shared instruction at 3.
    firstJump.operandBits = 1U;
    sharedSource.instructions.push_back(firstJump);
    NsfGoolInstruction secondEntry;
    secondEntry.opcode = 47U;
    sharedSource.instructions.push_back(secondEntry);
    NsfGoolInstruction sharedReturn;
    sharedReturn.opcode = 49U;
    sharedSource.instructions.push_back(sharedReturn);
    sharedSource.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    sharedSource.stateHookReferences.push_back({1, 0, 0, NsfGoolStateHookKind::Code,
        2, 2, false, true, NsfGoolStateHookStatus::Local});
    const auto sharedIR = buildGoolIR(sharedSource);
    assert(sharedIR.routines.size() == 2);
    assert((sharedIR.routines[0].instructions == std::vector<std::size_t>{0, 1, 3}));
    assert((sharedIR.routines[1].instructions == std::vector<std::size_t>{2, 3}));
    assert(sharedIR.routines[0].hasSharedInstructions);
    assert(sharedIR.routines[1].hasSharedInstructions);
    assert((sharedIR.routines[0].sharedInstructions == std::vector<std::size_t>{3}));
    assert((sharedIR.routines[1].sharedInstructions == std::vector<std::size_t>{3}));

    // Real Crash 2 sample: DispC's Code hook at instruction 0. These twenty
    // words are copied from the extracted entry; this is a focused IR/decompiler
    // regression, not a replacement for parsing the complete NSF entry.
    NsfGool dispC;
    dispC.version = NsfGoolVersion::Version2;
    dispC.id = static_cast<std::int32_t>(0x4E938CCDU);
    dispC.format = 1;
    dispC.entryCount = 2;
    dispC.dataPool.resize(21, 0);
    dispC.dataPool[0] = static_cast<std::int32_t>(0x4E938CCDU);
    dispC.dataPool[2] = -256;
    dispC.dataPool[3] = 6;
    dispC.dataPool[4] = 0x333;
    dispC.dataPool[20] = 0x40000;
    dispC.stateDescriptors.resize(4);
    dispC.stateDescriptors[3].goolIndex = 0;
    dispC.stateReferences.push_back({3, 0, 0, 0, 0x4E938CCD, true,
        NsfGoolStateReferenceStatus::Local});
    const std::uint32_t dispCWords[] = {
        0x2086480DU, 0x11014E1BU, 0x07002C38U, 0x08003E1FU,
        0x11E1FC38U, 0x11004E0EU, 0x11802E0FU, 0x11802E10U,
        0x11882E08U, 0x118BEE18U, 0x11800E0AU, 0x11800E4AU,
        0x11841E4BU, 0x11800E4CU, 0x11800E19U, 0x38C41D00U,
        0x1C305E4BU, 0x1C400E4BU, 0x35494003U, 0x31894000U
    };
    const char* dispCMnemonics[] = {
        "GVAW", "SETF", "ANDL", "ORL", "SETF", "SETF", "SETF",
        "SETF", "SETF", "SETF", "SETF", "SETF", "SETF", "SETF",
        "SETF", "ANIM", "MISC", "MISC", "GO", "RET"
    };
    for (std::size_t i = 0; i < 20; ++i) {
        NsfGoolInstruction instruction;
        instruction.byteOffset = i * 4U;
        instruction.word = dispCWords[i];
        instruction.opcode = static_cast<std::uint8_t>(dispCWords[i] >> 24U);
        instruction.operandBits = dispCWords[i] & 0x00FFFFFFU;
        instruction.mnemonic = dispCMnemonics[i];
        dispC.instructions.push_back(std::move(instruction));
    }
    // DispC descriptor 0's resolved local Code hook begins at instruction 0.
    dispC.stateHookReferences.push_back({0, 0, 0, NsfGoolStateHookKind::Code,
        0, 0, false, true, NsfGoolStateHookStatus::Local});
    const auto dispCIR = buildGoolIR(dispC);
    assert(dispCIR.instructions.size() == 20);
    assert(dispCIR.routineEntries.size() == 1);
    assert(dispCIR.routineEntries[0].instruction == 0);
    assert(dispCIR.routineEntries[0].fromCodeHook);
    assert(dispCIR.routines.size() == 1);
    assert(dispCIR.routines[0].entryInstruction == 0);
    assert(dispCIR.routines[0].instructions.size() == 20);
    assert(dispCIR.routines[0].instructions.front() == 0);
    assert(dispCIR.routines[0].instructions.back() == 19);
    assert(!dispCIR.routines[0].hasSharedInstructions);
    assert(dispCIR.routines[0].entryPointBoundaries.empty());
    assert(dispCIR.unassignedInstructions.empty());
    assert(dispCIR.instructions.front().rawWord == 0x2086480DU);
    assert(dispCIR.instructions[18].rawWord == 0x35494003U);
    assert(dispCIR.instructions[19].rawWord == 0x31894000U);
    assert(dispCIR.controlOperations.size() == 2);
    assert(dispCIR.controlOperations[0].kind == GoolIRControlOperationKind::StateTransition);
    assert(dispCIR.controlOperations[0].sourceInstruction == 18);
    assert(dispCIR.controlOperations[0].targetDescriptorValid);
    assert(dispCIR.controlOperations[0].targetDescriptorIndex == 3);
    assert(dispCIR.controlOperations[0].targetGoolEidValid);
    assert(dispCIR.controlOperations[0].targetGoolEid == 0x4E938CCD);
    assert(dispCIR.controlOperations[1].kind == GoolIRControlOperationKind::Return);
    assert(dispCIR.routines[0].exits.size() == 1);
    assert(dispCIR.routines[0].exits[0].kind == GoolIRRoutineExitKind::Return);
    assert(dispCIR.routines[0].exits[0].sourceInstruction == 19);
    const auto goBlock = dispCIR.instructionToBlock[18];
    const auto returnBlock = dispCIR.instructionToBlock[19];
    assert(goBlock != returnBlock);
    bool hasGoTransitionEdge = false;
    bool hasGoFallthroughEdge = false;
    for (const auto& edge : dispCIR.flowEdges) {
        if (edge.sourceBlock != goBlock) continue;
        if (edge.kind == GoolIRFlowEdgeKind::StateTransition && !edge.targetValid)
            hasGoTransitionEdge = true;
        if (edge.kind == GoolIRFlowEdgeKind::Fallthrough &&
            edge.targetValid && edge.targetInstruction == 19)
            hasGoFallthroughEdge = true;
    }
    assert(hasGoTransitionEdge && hasGoFallthroughEdge);
    const auto dispCText = decompileGoolIR(dispCIR);
    assert(dispCText.text.find("transition state_descriptor[3]") != std::string::npos);
    assert(dispCText.text.find("GOOL 0x4E938CCD") != std::string::npos);
    assert(dispCText.text.find("return") != std::string::npos);
    assert(dispCText.text.find("@0x0048") != std::string::npos);
    assert(dispCText.text.find("@0x004C") != std::string::npos);

    std::cout << "GOOL IR preservation and control-flow checks passed\\n";
}
