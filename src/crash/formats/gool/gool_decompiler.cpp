#include "crash/formats/gool/gool_decompiler.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string_view>

namespace CrashEdit::Crash {
namespace {

const GoolIROperand* findOperand(const GoolIRInstruction& instruction, char name) {
    for (const auto& operand : instruction.operands)
        if (operand.name == name) return &operand;
    return nullptr;
}

std::string hexValue(std::uint32_t value, unsigned width = 0) {
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setfill('0');
    if (width != 0) stream << std::setw(static_cast<int>(width));
    stream << value;
    return stream.str();
}

std::string labelName(std::size_t instruction) {
    std::ostringstream stream;
    stream << "L" << std::setfill('0') << std::setw(5) << instruction;
    return stream.str();
}

std::string operandText(const GoolIROperand& operand) {
    if (!operand.resolvedText.empty()) return operand.resolvedText;
    if (operand.kind == GoolIROperandKind::Signed ||
        operand.kind == GoolIROperandKind::Value)
        return std::to_string(operand.value);
    if (operand.kind == GoolIROperandKind::ProcessField)
        return goolObjectFieldName(operand.rawValue);
    return hexValue(operand.rawValue);
}

std::string fieldText(const GoolIRInstruction& instruction) {
    const auto* field = findOperand(instruction, 'R');
    if (!field) return "unknown_field";
    if (!field->resolvedText.empty()) return field->resolvedText;
    return goolObjectFieldName(field->rawValue);
}

const GoolIRControlOperation* findControlOperation(
    const GoolIRProgram& program, std::size_t instructionIndex) {
    for (const auto& operation : program.controlOperations)
        if (operation.sourceInstruction == instructionIndex) return &operation;
    return nullptr;
}

void addDiagnostic(GoolIRDecompilation& result, std::size_t instruction,
                   const std::string& message) {
    result.diagnostics.push_back("instruction " + std::to_string(instruction) + ": " + message);
}

std::string comparisonCondition(const GoolIRProgram& program, std::size_t branchIndex) {
    if (branchIndex == 0 || branchIndex >= program.instructions.size()) return {};
    const auto& comparison = program.instructions[branchIndex - 1];
    const auto& branch = program.instructions[branchIndex];
    if (comparison.nativeMips || !comparison.opcodeKnown ||
        comparison.opcode < 4U || comparison.opcode > 12U)
        return {};

    const auto* left = findOperand(comparison, 'L');
    const auto* right = findOperand(comparison, 'R');
    if (!left || !right) return {};

    static constexpr const char* normal[] = {
        "", "", "", "", "==", "", "", "", "", "<", "<=", ">", ">="
    };
    static constexpr const char* inverted[] = {
        "", "", "", "", "!=", "", "", "", "", ">=", ">", "<=", "<"
    };
    const bool branchOnZero = branch.opcode == 52U;
    const char* op = branchOnZero ? normal[comparison.opcode] : inverted[comparison.opcode];
    if (!op || *op == '\0') return {};
    return operandText(*left) + " " + op + " " + operandText(*right);
}


} // namespace

GoolIRDecompilation decompileGoolIR(
    const GoolIRProgram& program, const GoolIRDecompileOptions& options) {
    GoolIRDecompilation result;
    const std::size_t start = std::min(options.startInstruction, program.instructions.size());
    const std::size_t end = std::min(
        options.endInstruction == std::numeric_limits<std::size_t>::max()
            ? program.instructions.size() : options.endInstruction,
        program.instructions.size());
    const std::size_t boundedEnd = std::max(start, end);

    std::map<std::size_t, bool> labels;
    // Scan the whole entry for incoming edges too, so a selected range still
    // receives a label when a branch or call outside the range targets it.
    for (std::size_t index = 0; index < program.instructions.size(); ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.nativeMips || !instruction.opcodeKnown) continue;

        if (instruction.opcode >= 50U && instruction.opcode <= 52U) {
            const auto* displacement = findOperand(instruction, 'I');
            if (displacement) {
                const auto target = static_cast<std::int64_t>(index) + 1 + displacement->value;
                if (target >= 0 && static_cast<std::uint64_t>(target) < program.instructions.size())
                    labels[static_cast<std::size_t>(target)] = true;
            }
        }

        if (const auto* operation = findControlOperation(program, index)) {
            if (operation->kind == GoolIRControlOperationKind::Call &&
                !operation->external && operation->targetInstructionValid)
                labels[operation->targetInstruction] = true;
        }
    }
    for (const auto& hook : program.hookDispatches) {
        if (hook.resolution == GoolIRHookResolution::Local &&
            hook.instructionOffset < program.instructions.size())
            labels[hook.instructionOffset] = true;
    }

    // Recognize a conservative forward-if pattern: a conditional branch skips
    // to the next clause, while the fallthrough body ends in an unconditional
    // branch to a later join point. This is common in GOOL's if/else-if chains.
    // We only structure the body when the complete pattern is visible.
    struct StructuredIf { std::size_t bodyEnd; std::size_t branchTarget; std::size_t joinTarget; };
    std::map<std::size_t, StructuredIf> structuredIfs;
    std::map<std::size_t, bool> suppressedBranches;
    std::map<std::size_t, bool> elseIfClauses;
    std::map<std::size_t, bool> suppressedConditionProducers;
    std::map<std::size_t, bool> elseArms;
    std::map<std::size_t, bool> suppressedElseLabels;
    std::map<std::size_t, bool> closeElseAt;
    struct StructuredLoop { std::size_t backBranch; std::size_t exitTarget; };
    std::map<std::size_t, StructuredLoop> structuredLoops;
    std::map<std::size_t, bool> suppressedLoopBackBranches;
    std::map<std::size_t, bool> suppressedLoopLabels;
    std::map<std::size_t, bool> suppressedLoopConditions;
    for (std::size_t index = start; index < boundedEnd; ++index) {
        const auto& instruction = program.instructions[index];
        if (instruction.nativeMips || !instruction.opcodeKnown ||
            (instruction.opcode != 51U && instruction.opcode != 52U))
            continue;
        const auto* displacement = findOperand(instruction, 'I');
        const auto* popCount = findOperand(instruction, 'V');
        if (!displacement || (popCount && popCount->value != 0)) continue;
        const auto branchTargetSigned = static_cast<std::int64_t>(index) + 1 + displacement->value;
        if (branchTargetSigned <= static_cast<std::int64_t>(index + 1) ||
            branchTargetSigned >= static_cast<std::int64_t>(boundedEnd))
            continue;
        const auto branchTarget = static_cast<std::size_t>(branchTargetSigned);

        // The fallthrough body must terminate with BRA to a join beyond the
        // conditional destination. Do not infer structure across ambiguous flow.
        for (std::size_t bodyEnd = index + 1; bodyEnd < branchTarget; ++bodyEnd) {
            const auto& terminator = program.instructions[bodyEnd];
            if (terminator.nativeMips || !terminator.opcodeKnown || terminator.opcode != 50U)
                continue;
            const auto* joinDisplacement = findOperand(terminator, 'I');
            if (!joinDisplacement) continue;
            const auto joinSigned = static_cast<std::int64_t>(bodyEnd) + 1 + joinDisplacement->value;
            if (joinSigned <= branchTargetSigned ||
                joinSigned >= static_cast<std::int64_t>(boundedEnd))
                continue;

            bool hasInteriorEntry = false;
            for (const auto& [target, unused] : labels) {
                (void)unused;
                if (target >= index + 1 && target <= bodyEnd) {
                    hasInteriorEntry = true;
                    break;
                }
            }
            if (hasInteriorEntry) continue;
            structuredIfs[index] = {bodyEnd, branchTarget, static_cast<std::size_t>(joinSigned)};
            suppressedBranches[bodyEnd] = true;
            break;
        }
    }

    // Join adjacent clauses only when the preceding conditional skips to
    // this clause's comparison producer and both clauses share the same join.
    for (const auto& [branchIndex, clause] : structuredIfs) {
        if (branchIndex == 0 || comparisonCondition(program, branchIndex).empty())
            continue;
        const auto producerIndex = branchIndex - 1;
        for (const auto& [previousBranch, previousClause] : structuredIfs) {
            if (previousBranch >= branchIndex) break;
            if (previousClause.branchTarget == producerIndex &&
                previousClause.joinTarget == clause.joinTarget) {
                elseIfClauses[branchIndex] = true;
                suppressedConditionProducers[producerIndex] = true;
                break;
            }
        }
    }

    // A final conditional may skip to a plain fallback arm. Only structure
    // it when that target is unique and both paths reach the same join.
    for (const auto& [branchIndex, clause] : structuredIfs) {
        const auto target = clause.branchTarget;
        if (target >= clause.joinTarget || target >= boundedEnd)
            continue;
        bool startsAnotherClause = false;
        if (target + 1 < boundedEnd) {
            const auto& next = program.instructions[target + 1];
            startsAnotherClause = !next.nativeMips && next.opcodeKnown &&
                (next.opcode == 51U || next.opcode == 52U);
        }
        if (startsAnotherClause) continue;
        std::size_t incomingEdges = 0;
        for (std::size_t source = 0; source < program.instructions.size(); ++source) {
            const auto& candidate = program.instructions[source];
            if (candidate.nativeMips || !candidate.opcodeKnown ||
                candidate.opcode < 50U || candidate.opcode > 52U)
                continue;
            const auto* displacement = findOperand(candidate, 'I');
            if (!displacement) continue;
            const auto destination = static_cast<std::int64_t>(source) + 1 +
                displacement->value;
            if (destination == static_cast<std::int64_t>(target))
                ++incomingEdges;
        }
        if (incomingEdges != 1) continue;
        elseArms[branchIndex] = true;
        suppressedElseLabels[target] = true;
        closeElseAt[clause.joinTarget] = true;
    }

    // Recognize a simple pre-test loop: a conditional branch exits forward,
    // while the final instruction in its fallthrough body branches back to
    // the condition. Reject interior entries and additional control flow.
    for (std::size_t header = start + 1; header < boundedEnd; ++header) {
        const auto& conditionBranch = program.instructions[header];
        if (conditionBranch.nativeMips || !conditionBranch.opcodeKnown ||
            (conditionBranch.opcode != 51U && conditionBranch.opcode != 52U) ||
            comparisonCondition(program, header).empty())
            continue;
        const auto* exitDisplacement = findOperand(conditionBranch, 'I');
        const auto* popCount = findOperand(conditionBranch, 'V');
        if (!exitDisplacement || (popCount && popCount->value != 0))
            continue;
        const auto exitSigned = static_cast<std::int64_t>(header) + 1 +
            exitDisplacement->value;
        if (exitSigned <= static_cast<std::int64_t>(header + 1) ||
            exitSigned >= static_cast<std::int64_t>(boundedEnd))
            continue;
        const auto exitTarget = static_cast<std::size_t>(exitSigned);

        for (std::size_t back = header + 1; back < exitTarget; ++back) {
            const auto& candidate = program.instructions[back];
            if (candidate.nativeMips || !candidate.opcodeKnown || candidate.opcode != 50U)
                continue;
            const auto* backDisplacement = findOperand(candidate, 'I');
            if (!backDisplacement) continue;
            const auto backTarget = static_cast<std::int64_t>(back) + 1 +
                backDisplacement->value;
            if (backTarget != static_cast<std::int64_t>(header) || back + 1 != exitTarget)
                continue;

            bool unsafeBody = false;
            for (std::size_t body = header + 1; body < back; ++body) {
                const auto& bodyInstruction = program.instructions[body];
                if (bodyInstruction.nativeMips || !bodyInstruction.opcodeKnown ||
                    (bodyInstruction.opcode >= 50U && bodyInstruction.opcode <= 55U)) {
                    unsafeBody = true;
                    break;
                }
                if (labels.find(body) != labels.end()) {
                    unsafeBody = true;
                    break;
                }
            }
            if (unsafeBody) continue;

            structuredLoops[header] = {back, exitTarget};
            suppressedLoopBackBranches[back] = true;
            suppressedLoopConditions[header - 1] = true;
            suppressedLoopLabels[header] = true;
            std::size_t exitIncoming = 0;
            for (std::size_t source = 0; source < program.instructions.size(); ++source) {
                const auto& sourceInstruction = program.instructions[source];
                if (sourceInstruction.nativeMips || !sourceInstruction.opcodeKnown ||
                    sourceInstruction.opcode < 50U || sourceInstruction.opcode > 52U)
                    continue;
                const auto* displacement = findOperand(sourceInstruction, 'I');
                if (!displacement) continue;
                const auto destination = static_cast<std::int64_t>(source) + 1 +
                    displacement->value;
                if (destination == static_cast<std::int64_t>(exitTarget))
                    ++exitIncoming;
            }
            if (exitIncoming == 1)
                suppressedLoopLabels[exitTarget] = true;
            break;
        }
    }

    if (options.includeHeader) {
        result.text += "// Best-effort GOOL decompilation; unresolved semantics remain explicit.\n";
        result.text += "// Instruction range: [" + std::to_string(start) + ", " +
            std::to_string(boundedEnd) + ")\n";
        result.text += "gool " + hexValue(static_cast<std::uint32_t>(program.id), 8) + " {\n";
    }

    for (std::size_t index = start; index < boundedEnd; ++index) {
        const auto& instruction = program.instructions[index];
        if (suppressedBranches.find(index) != suppressedBranches.end()) {
            const auto owningClause = std::find_if(structuredIfs.begin(), structuredIfs.end(),
                [&](const auto& entry) { return entry.second.bodyEnd == index; });
            std::string closingLine;
            if (owningClause != structuredIfs.end() &&
                elseArms.find(owningClause->first) != elseArms.end()) {
                closingLine = "    } else {";
            } else {
                const bool closesForElseIf = std::any_of(elseIfClauses.begin(), elseIfClauses.end(),
                    [&](const auto& entry) {
                        const auto clause = structuredIfs.find(entry.first);
                        return clause != structuredIfs.end() && clause->second.bodyEnd == index;
                    });
                if (!closesForElseIf) closingLine = "    }";
            }
            if (!closingLine.empty()) {
                // Structured braces replace the original branch instruction,
                // but keep its byte address visible for source-level tracing.
                if (options.preserveStructuredBranchOffsets)
                    closingLine += " // @" + hexValue(
                        static_cast<std::uint32_t>(instruction.sourceByteOffset), 4);
                result.text += closingLine + "\n";
            }
            continue;
        }
        if (closeElseAt.find(index) != closeElseAt.end())
            result.text += "    }\n";
        if (suppressedLoopBackBranches.find(index) != suppressedLoopBackBranches.end()) {
            result.text += "    }\n";
            continue;
        }
        if (suppressedLoopLabels.find(index) == suppressedLoopLabels.end() &&
            suppressedConditionProducers.find(index) == suppressedConditionProducers.end() &&
            suppressedElseLabels.find(index) == suppressedElseLabels.end() &&
            labels.find(index) != labels.end())
            result.text += labelName(index) + ":\n";
        if (structuredLoops.find(index) != structuredLoops.end()) {
            result.text += "    while (" + comparisonCondition(program, index) + ") {\n";
            continue;
        }
        if (suppressedLoopConditions.find(index) != suppressedLoopConditions.end())
            continue;

        if (structuredIfs.find(index) != structuredIfs.end()) {
            const auto comparison = comparisonCondition(program, index);
            if (elseIfClauses.find(index) != elseIfClauses.end()) {
                result.text += "    } else if (" + comparison + ") {\n";
            } else if (!comparison.empty()) {
                result.text += "    if (" + comparison + ") {\n";
            } else {
                result.text += "    if (" + fieldText(instruction) +
                    (instruction.opcode == 51U ? " == 0) {\n" : " != 0) {\n");
            }
            continue;
        }
        if (suppressedConditionProducers.find(index) != suppressedConditionProducers.end())
            continue;

        std::ostringstream line;
        line << "    ";

        if (instruction.nativeMips) {
            if (instruction.nativeInstructionDecoded && !instruction.mnemonic.empty()) {
                line << "mips " << instruction.mnemonic;
                if (!instruction.nativeArguments.empty()) line << " " << instruction.nativeArguments;
            } else {
                line << ".mips_word " << hexValue(instruction.rawWord, 8);
                addDiagnostic(result, index, "native MIPS instruction is not decoded.");
            }
        } else if (!instruction.opcodeKnown) {
            line << ".gool_word " << hexValue(instruction.rawWord, 8)
                 << " // unknown opcode " << static_cast<unsigned>(instruction.opcode);
            addDiagnostic(result, index, "unknown GOOL opcode preserved as its original word.");
        } else if (instruction.opcode == 17U) {
            const auto* destination = findOperand(instruction, 'D');
            const auto* value = findOperand(instruction, 'S');
            if (destination && value) {
                line << operandText(*destination) << " = " << operandText(*value);
            } else {
                line << "SETF /* missing destination or source */";
                addDiagnostic(result, index, "SETF operands could not be resolved.");
            }
        } else if (instruction.opcode == 73U) {
            line << ".mips_mode";
        } else if (instruction.opcode == 49U) {
            line << "return";
            line << " // destination depends on runtime call context";
        } else if (instruction.opcode >= 50U && instruction.opcode <= 52U) {
            const auto* displacement = findOperand(instruction, 'I');
            const auto target = displacement
                ? static_cast<std::int64_t>(index) + 1 + displacement->value
                : -1;
            if (target >= 0 && static_cast<std::uint64_t>(target) < program.instructions.size()) {
                if (instruction.opcode == 50U) {
                    line << "goto " << labelName(static_cast<std::size_t>(target));
                } else {
                    line << "if (" << fieldText(instruction)
                         << (instruction.opcode == 51U ? " != 0" : " == 0")
                         << ") goto " << labelName(static_cast<std::size_t>(target));
                }
                if (static_cast<std::size_t>(target) < start || static_cast<std::size_t>(target) >= boundedEnd)
                    line << " // target is outside selected range";
                const auto* popCount = findOperand(instruction, 'V');
                if (popCount && popCount->value != 0)
                    line << " // pop " << popCount->value;
            } else {
                line << instruction.mnemonic << " /* invalid branch target */";
                addDiagnostic(result, index, "branch destination could not be resolved.");
            }
        } else if (instruction.opcode == 59U) {
            const auto* operation = findControlOperation(program, index);
            const auto* argumentCount = findOperand(instruction, 'V');
            const std::string arguments = argumentCount
                ? std::to_string(argumentCount->value) : "unknown";
            if (operation && operation->external) {
                const auto* target = findOperand(instruction, 'I');
                line << "call_external(" << (target ? operandText(*target) : "unknown_target")
                     << ", args=" << arguments << ")";
            } else if (operation && operation->targetInstructionValid) {
                line << "call " << labelName(operation->targetInstruction)
                     << "(args=" << arguments << ")";
                if (operation->targetInstruction < start || operation->targetInstruction >= boundedEnd)
                    line << " // target is outside selected range";
            } else {
                line << "call_unresolved(args=" << arguments << ")";
                addDiagnostic(result, index, operation && !operation->diagnostic.empty()
                    ? operation->diagnostic : "local call target could not be resolved.");
            }
        } else if (instruction.opcode >= 53U && instruction.opcode <= 55U) {
            const auto* operation = findControlOperation(program, index);
            if (instruction.opcode == 54U || instruction.opcode == 55U) {
                line << "if (" << fieldText(instruction)
                     << (instruction.opcode == 54U ? " != 0" : " == 0") << ") ";
            }
            if (operation && operation->targetDescriptorValid) {
                line << "transition state_descriptor[" << operation->targetDescriptorIndex << "]";
                const auto* argumentCount = findOperand(instruction, 'V');
                line << "(args=" << (argumentCount ? std::to_string(argumentCount->value) : "unknown") << ")";
                if (operation->targetGoolEidValid)
                    line << " // GOOL " << hexValue(operation->targetGoolEid, 8);
                else if (!operation->diagnostic.empty())
                    line << " // " << operation->diagnostic;
            } else {
                const auto* state = findOperand(instruction, 'S');
                line << "transition state_descriptor["
                     << (state ? std::to_string(state->rawValue) : "unknown") << "]";
                line << " // unresolved descriptor";
                addDiagnostic(result, index, operation && !operation->diagnostic.empty()
                    ? operation->diagnostic : "state descriptor could not be resolved.");
            }
        } else {
            line << (instruction.mnemonic.empty() ? "UNK" : instruction.mnemonic);
            if (!instruction.operands.empty()) {
                line << " ";
                for (std::size_t operandIndex = 0;
                     operandIndex < instruction.operands.size(); ++operandIndex) {
                    if (operandIndex != 0) line << ", ";
                    const auto& operand = instruction.operands[operandIndex];
                    line << operand.name << "=" << operandText(operand);
                }
            }
        }

        line << "  // @" << hexValue(static_cast<std::uint32_t>(instruction.sourceByteOffset), 4)
             << " raw=" << hexValue(instruction.rawWord, 8) << "\n";
        for (const auto& use : program.valueUses) {
            if (use.instructionIndex != index) continue;
            line << "    // value " << use.location << ": ";
            if (use.reachingDefinitionInstructions.empty()) {
                line << "no in-stream definition";
            } else {
                line << "defined at ";
                for (std::size_t definition = 0; definition < use.reachingDefinitionInstructions.size(); ++definition) {
                    if (definition != 0) line << ", ";
                    line << "instruction " << use.reachingDefinitionInstructions[definition];
                }
            }
            if (use.mayHaveUnmodeledValue)
                line << "; an initial or unmodeled value may also reach here";
            line << "\n";
        }
        result.text += line.str();
    }

    if (options.includeHeader) result.text += "}\n";
    return result;
}


GoolIRDecompilation decompileGoolIRRoutine(
    const GoolIRProgram& program, std::size_t routineIndex) {
    GoolIRDecompilation result;
    if (routineIndex >= program.routines.size()) {
        result.diagnostics.push_back("routine index is out of range.");
        return result;
    }

    const auto& routine = program.routines[routineIndex];
    result.text = "// Routine " + std::to_string(routineIndex) +
        " entry: " + labelName(routine.entryInstruction) + "\n";
    result.text += "// Instructions are listed in discovered routine order.\n";

    // Routine listings need label definitions for branches that remain inside
    // the selected routine. The range decompiler emits branch references, but
    // a one-instruction range cannot know which other routine instructions
    // should receive those labels.
    const auto belongsToRoutine = [&routine](std::size_t instructionIndex) {
        return std::find(routine.instructions.begin(), routine.instructions.end(),
            instructionIndex) != routine.instructions.end();
    };
    std::vector<std::size_t> routineBranchTargets;
    for (const auto instructionIndex : routine.instructions) {
        if (instructionIndex >= program.instructions.size()) continue;
        const auto& instruction = program.instructions[instructionIndex];
        if (instruction.nativeMips || !instruction.opcodeKnown ||
            instruction.opcode < 50U || instruction.opcode > 52U)
            continue;
        const auto* displacement = findOperand(instruction, 'I');
        if (!displacement) continue;
        const auto target = static_cast<std::int64_t>(instructionIndex) + 1 +
            displacement->value;
        if (target >= 0 && static_cast<std::uint64_t>(target) < program.instructions.size() &&
            belongsToRoutine(static_cast<std::size_t>(target)))
            routineBranchTargets.push_back(static_cast<std::size_t>(target));
    }
    std::sort(routineBranchTargets.begin(), routineBranchTargets.end());
    routineBranchTargets.erase(std::unique(routineBranchTargets.begin(),
        routineBranchTargets.end()), routineBranchTargets.end());

    // A single-instruction routine has no region to structure. Keep it on
    // the routine-aware listing path so outgoing branches retain their
    // explicit cross-routine destination annotation.
    bool contiguousRoutine = routine.instructions.size() > 1;
    for (std::size_t i = 0; i < routine.instructions.size(); ++i) {
        if (routine.instructions[i] >= program.instructions.size() ||
            (i != 0 && routine.instructions[i] != routine.instructions[i - 1] + 1U)) {
            contiguousRoutine = false;
            break;
        }
    }

    if (contiguousRoutine) {
        // When the routine occupies one uninterrupted instruction span, let
        // the range decompiler see its complete control-flow region. This
        // enables the existing conservative if/else/loop recognizers to
        // reason across instructions instead of formatting each word alone.
        GoolIRDecompileOptions options;
        options.startInstruction = routine.instructions.front();
        options.endInstruction = routine.instructions.back() + 1U;
        options.includeHeader = false;
        auto structured = decompileGoolIR(program, options);
        result.text += structured.text;
        result.diagnostics.insert(result.diagnostics.end(),
            structured.diagnostics.begin(), structured.diagnostics.end());

        for (const auto& call : routine.calls) {
            if (call.targetRoutineFound)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " reaches discovered routine " +
                    std::to_string(call.targetRoutineIndex) + "\n";
            else if (call.external)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " targets external GOOL\n";
            else if (!call.targetInstructionValid)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " has unresolved local target\n";
        }
    } else {
        // Compact a routine's own instruction sequence into a temporary IR
        // stream when every control-flow edge remains inside the routine.
        // This lets the existing structured recognizers see across gaps in
        // the original object without importing unrelated instructions.
        bool canCompact = !routine.instructions.empty();
        std::map<std::size_t, std::size_t> compactIndex;
        for (const auto original : routine.instructions) {
            if (original >= program.instructions.size() ||
                compactIndex.find(original) != compactIndex.end()) {
                canCompact = false;
                break;
            }
            compactIndex.emplace(original, compactIndex.size());
        }
        GoolIRProgram compactProgram;
        if (canCompact) {
            compactProgram = program;
            compactProgram.instructions.clear();
            compactProgram.instructions.reserve(routine.instructions.size());
            compactProgram.valueUses.clear();
            for (std::size_t compact = 0; compact < routine.instructions.size(); ++compact) {
                const auto original = routine.instructions[compact];
                compactProgram.instructions.push_back(program.instructions[original]);
                for (const auto& use : program.valueUses) {
                    if (use.instructionIndex != original) continue;
                    auto compactUse = use;
                    compactUse.instructionIndex = compact;
                    for (auto& definition : compactUse.reachingDefinitionInstructions) {
                        const auto mapped = compactIndex.find(definition);
                        if (mapped != compactIndex.end()) definition = mapped->second;
                    }
                    compactProgram.valueUses.push_back(std::move(compactUse));
                }
            }

            for (std::size_t compactSource = 0;
                 compactSource < compactProgram.instructions.size(); ++compactSource) {
                auto& instruction = compactProgram.instructions[compactSource];
                if (instruction.nativeMips || !instruction.opcodeKnown ||
                    instruction.opcode < 50U || instruction.opcode > 52U)
                    continue;
                auto* displacement = const_cast<GoolIROperand*>(findOperand(instruction, 'I'));
                if (!displacement) { canCompact = false; break; }
                const auto originalSource = routine.instructions[compactSource];
                const auto originalTarget = static_cast<std::int64_t>(originalSource) +
                    1 + displacement->value;
                if (originalTarget < 0 ||
                    static_cast<std::uint64_t>(originalTarget) >= program.instructions.size()) {
                    canCompact = false;
                    break;
                }
                const auto mappedTarget = compactIndex.find(static_cast<std::size_t>(originalTarget));
                if (mappedTarget == compactIndex.end()) { canCompact = false; break; }
                displacement->value = static_cast<std::int32_t>(mappedTarget->second) -
                    static_cast<std::int32_t>(compactSource) - 1;
            }
        }
        if (canCompact) {
            compactProgram.controlOperations.clear();
            for (auto operation : program.controlOperations) {
                const auto source = compactIndex.find(operation.sourceInstruction);
                if (source == compactIndex.end()) continue;
                operation.sourceInstruction = source->second;
                if (operation.kind == GoolIRControlOperationKind::Call &&
                    operation.targetInstructionValid) {
                    const auto target = compactIndex.find(operation.targetInstruction);
                    if (target == compactIndex.end()) {
                        // Calls into another discovered routine remain calls;
                        // the routine relationship is emitted separately below.
                        operation.targetInstructionValid = false;
                    } else {
                        operation.targetInstruction = target->second;
                    }
                }
                compactProgram.controlOperations.push_back(std::move(operation));
            }
        }
        if (canCompact) {
            compactProgram.hookDispatches.clear();
            for (auto hook : program.hookDispatches) {
                if (hook.resolution == GoolIRHookResolution::Local) {
                    const auto target = compactIndex.find(hook.instructionOffset);
                    // Other routines' hook entries are outside this routine's
                    // compact view and must not create spurious local labels.
                    if (target == compactIndex.end()) continue;
                    if (target->second > std::numeric_limits<std::uint16_t>::max()) {
                        canCompact = false;
                        break;
                    }
                    hook.instructionOffset = static_cast<std::uint16_t>(target->second);
                }
                compactProgram.hookDispatches.push_back(std::move(hook));
            }
        }
        if (canCompact) {
            // Keep the original stream's region boundaries visible in the
            // source even though the compact IR can now structure across them.
            for (std::size_t i = 1; i < routine.instructions.size(); ++i) {
                if (routine.instructions[i] != routine.instructions[i - 1] + 1U) {
                    const auto first = routine.instructions[i];
                    std::size_t end = i + 1;
                    while (end < routine.instructions.size() &&
                        routine.instructions[end] < program.instructions.size() &&
                        routine.instructions[end - 1] < program.instructions.size() &&
                        routine.instructions[end] == routine.instructions[end - 1] + 1U)
                        ++end;
                    const auto last = routine.instructions[end - 1];
                    result.text += "    // Discontiguous routine region [" +
                        std::to_string(first) + ", " + std::to_string(last + 1U) + ")\\n";
                    i = end - 1;
                }
            }

            GoolIRDecompileOptions options;
            options.startInstruction = 0;
            options.endInstruction = compactProgram.instructions.size();
            options.includeHeader = false;
            options.preserveStructuredBranchOffsets = true;
            auto structured = decompileGoolIR(compactProgram, options);

            // Translate temporary labels back to their original instruction
            // indices without allowing one replacement to affect another.
            for (const auto& [original, compact] : compactIndex) {
                const auto temporary = labelName(compact);
                const auto placeholder = "__GOOL_ROUTINE_LABEL_" +
                    std::to_string(compact) + "__";
                std::size_t position = 0;
                while ((position = structured.text.find(temporary, position)) !=
                    std::string::npos) {
                    structured.text.replace(position, temporary.size(), placeholder);
                    position += placeholder.size();
                }
            }
            for (const auto& [original, compact] : compactIndex) {
                const auto placeholder = "__GOOL_ROUTINE_LABEL_" +
                    std::to_string(compact) + "__";
                std::size_t position = 0;
                const auto replacement = labelName(original);
                while ((position = structured.text.find(placeholder, position)) !=
                    std::string::npos) {
                    structured.text.replace(position, placeholder.size(), replacement);
                    position += replacement.size();
                }
            }
            result.text += structured.text;
            result.diagnostics.insert(result.diagnostics.end(),
                structured.diagnostics.begin(), structured.diagnostics.end());
        } else {
        // Non-contiguous routines are split into maximal adjacent instruction
        // regions. Each region is decompiled as a unit, so local control-flow
        // patterns can still be recognized without pulling unrelated
        // instructions from the containing GOOL object into the routine.
        std::size_t regionBegin = 0;
        while (regionBegin < routine.instructions.size()) {
            const std::size_t first = routine.instructions[regionBegin];
            if (first >= program.instructions.size()) {
                result.diagnostics.push_back(
                    "routine contains an out-of-range instruction index " +
                    std::to_string(first) + ".");
                ++regionBegin;
                continue;
            }

            std::size_t regionEnd = regionBegin + 1;
            while (regionEnd < routine.instructions.size() &&
                routine.instructions[regionEnd] < program.instructions.size() &&
                routine.instructions[regionEnd] == routine.instructions[regionEnd - 1] + 1U) {
                ++regionEnd;
            }
            const std::size_t last = routine.instructions[regionEnd - 1];
            if (regionBegin != 0)
                result.text += "    // Discontiguous routine region [" +
                    std::to_string(first) + ", " + std::to_string(last + 1U) + ")\n";

            for (std::size_t i = regionBegin; i < regionEnd; ++i) {
                const auto instructionIndex = routine.instructions[i];
                if (std::binary_search(routineBranchTargets.begin(), routineBranchTargets.end(),
                        instructionIndex))
                    result.text += labelName(instructionIndex) + ":\n";
            }

            GoolIRDecompileOptions options;
            options.startInstruction = first;
            options.endInstruction = last + 1U;
            options.includeHeader = false;
            auto region = decompileGoolIR(program, options);

            // Branches into another region of this same routine retain a
            // label that will be defined at that region's start. Branches
            // leaving the routine name the absolute instruction instead.
            for (const auto instructionIndex : routine.instructions) {
                if (instructionIndex < first || instructionIndex > last) continue;
                const auto& sourceInstruction = program.instructions[instructionIndex];
                if (sourceInstruction.nativeMips || !sourceInstruction.opcodeKnown ||
                    sourceInstruction.opcode < 50U || sourceInstruction.opcode > 52U)
                    continue;
                const auto* displacement = findOperand(sourceInstruction, 'I');
                if (!displacement) continue;
                const auto target = static_cast<std::int64_t>(instructionIndex) + 1 +
                    displacement->value;
                if (target < 0 ||
                    static_cast<std::uint64_t>(target) >= program.instructions.size() ||
                    belongsToRoutine(static_cast<std::size_t>(target)))
                    continue;

                const auto targetIndex = static_cast<std::size_t>(target);
                const auto label = labelName(targetIndex);
                const auto absoluteTarget = "instruction " + std::to_string(targetIndex);
                std::size_t labelPosition = 0;
                while ((labelPosition = region.text.find(label, labelPosition)) !=
                    std::string::npos) {
                    region.text.replace(labelPosition, label.size(), absoluteTarget);
                    labelPosition += absoluteTarget.size();
                }

                std::ostringstream offset;
                offset << "@0x" << std::uppercase << std::hex << std::setfill('0')
                       << std::setw(4)
                       << static_cast<std::uint32_t>(sourceInstruction.sourceByteOffset);
                const auto lineEnd = region.text.find('\n',
                    region.text.find(offset.str()));
                if (lineEnd != std::string::npos)
                    region.text.insert(lineEnd + 1U, "    // branch leaves this routine\n");
            }

            result.text += region.text;
            result.diagnostics.insert(result.diagnostics.end(),
                region.diagnostics.begin(), region.diagnostics.end());
            regionBegin = regionEnd;
        }

        } // End region-by-region fallback when safe compaction is not possible.

        for (const auto& call : routine.calls) {
            if (!call.targetRoutineFound && !call.external && call.targetInstructionValid)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " reaches local instruction " +
                    std::to_string(call.targetInstruction) + "\n";
            else if (call.external)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " targets external GOOL\n";
            else if (!call.targetInstructionValid)
                result.text += "    // call at instruction " +
                    std::to_string(call.sourceInstruction) + " has unresolved local target\n";
        }
    }

    const auto exitName = [](GoolIRRoutineExitKind kind) -> const char* {
        switch (kind) {
        case GoolIRRoutineExitKind::Return: return "return";
        case GoolIRRoutineExitKind::EntryPointBoundary: return "entry-point boundary";
        case GoolIRRoutineExitKind::InvalidBranchTarget: return "invalid branch target";
        case GoolIRRoutineExitKind::EndOfProgram: return "end of program (incomplete)";
        }
        return "unknown exit";
    };
    for (const auto& exit : routine.exits) {
        result.text += "    // exit at instruction " +
            std::to_string(exit.sourceInstruction) + ": " + exitName(exit.kind);
        if (exit.targetValid)
            result.text += " -> " + labelName(exit.targetInstruction);
        if (!exit.diagnostic.empty())
            result.text += " (" + exit.diagnostic + ")";
        result.text += "\n";
        if (!exit.diagnostic.empty())
            result.diagnostics.push_back("routine exit at instruction " +
                std::to_string(exit.sourceInstruction) + ": " + exit.diagnostic);
    }
    for (const auto& diagnostic : routine.diagnostics)
        result.diagnostics.push_back("routine " + std::to_string(routineIndex) +
            ": " + diagnostic);
    return result;
}

}
