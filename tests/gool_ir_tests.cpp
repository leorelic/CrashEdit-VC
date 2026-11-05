#ifdef NDEBUG
#undef NDEBUG
#endif

#include "crash/formats/gool/gool_ir.hpp"
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
        0x1234, 7, true, true, NsfGoolStateHookStatus::External});
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
    assert(ir.stateHooks.size() == 1 && ir.stateHooks[0].rawValue == 0x1234);
    assert(ir.stateHooks[0].external && ir.stateHooks[0].targetFound);
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

    std::cout << "GOOL IR preservation and control-flow checks passed\\n";
}
