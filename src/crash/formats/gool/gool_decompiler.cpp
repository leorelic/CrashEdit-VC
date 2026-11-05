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

    if (options.includeHeader) {
        result.text += "// Best-effort GOOL decompilation; unresolved semantics remain explicit.\n";
        result.text += "// Instruction range: [" + std::to_string(start) + ", " +
            std::to_string(boundedEnd) + ")\n";
        result.text += "gool " + hexValue(static_cast<std::uint32_t>(program.id), 8) + " {\n";
    }

    for (std::size_t index = start; index < boundedEnd; ++index) {
        const auto& instruction = program.instructions[index];
        if (labels.find(index) != labels.end())
            result.text += labelName(index) + ":\n";

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
        result.text += line.str();
    }

    if (options.includeHeader) result.text += "}\n";
    return result;
}

}
