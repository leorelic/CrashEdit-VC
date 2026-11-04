#include "crash/formats/gool/gool_control_flow.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace CrashEdit::Crash {
namespace {
const NsfGoolOperand* findOperand(const std::vector<NsfGoolOperand>& operands,char name) {
    const auto it=std::find_if(operands.begin(),operands.end(),[name](const auto& operand){return operand.name==name;});
    return it==operands.end()?nullptr:&*it;
}
}
NsfGoolControlFlowGraph analyzeGoolControlFlow(const NsfGool& gool) {
    NsfGoolControlFlowGraph graph;
    const std::size_t count=gool.instructions.size();
    if(count==0) return graph;

    std::set<std::size_t> leaders{0};
    struct PendingEdge { std::size_t source; std::size_t target; NsfGoolFlowEdgeKind kind; bool valid; std::string diagnostic; };
    std::vector<PendingEdge> pending;
    auto addTarget=[&](std::size_t source,std::int64_t target,NsfGoolFlowEdgeKind kind) {
        const bool valid=target>=0&&static_cast<std::uint64_t>(target)<count;
        if(valid) leaders.insert(static_cast<std::size_t>(target));
        pending.push_back({source,valid?static_cast<std::size_t>(target):0U,kind,valid,
            valid?std::string{}:"Instruction target "+std::to_string(target)+" is outside this GOOL program."});
    };
    for(std::size_t i=0;i<count;++i) {
        const auto& instruction=gool.instructions[i];
        if(instruction.isMips) continue;
        const auto opcode=instruction.opcode;
        if(opcode>=50U&&opcode<=52U) {
            const auto operands=decodeGoolOperands(gool.version,opcode,instruction.operandBits);
            const auto* displacement=findOperand(operands,'I');
            const std::int64_t target=static_cast<std::int64_t>(i)+1+(displacement?displacement->value:0);
            const auto kind=opcode==50U?NsfGoolFlowEdgeKind::Branch:
                opcode==51U?NsfGoolFlowEdgeKind::BranchTrue:NsfGoolFlowEdgeKind::BranchFalse;
            addTarget(i,target,kind);
            if(i+1U<count) leaders.insert(i+1U);
        } else if(opcode>=53U&&opcode<=55U) {
            const auto kind=opcode==53U?NsfGoolFlowEdgeKind::StateTransition:
                opcode==54U?NsfGoolFlowEdgeKind::StateTransitionTrue:NsfGoolFlowEdgeKind::StateTransitionFalse;
            // State transitions are intentionally represented as dynamic edges.
            // Their destinations are interpreted through state descriptors and hooks;
            // this is expected behavior, not an analysis diagnostic.
            pending.push_back({i,0U,kind,false,{}});
            if(i+1U<count) leaders.insert(i+1U);
        } else if(opcode==59U) {
            const auto operands=decodeGoolOperands(gool.version,opcode,instruction.operandBits);
            const auto* external=findOperand(operands,'E');
            const auto* target=findOperand(operands,'I');
            if(external&&external->rawValue!=0U) {
                pending.push_back({i,0U,NsfGoolFlowEdgeKind::ExternalCall,false,"External subroutine target is not part of this GOOL program."});
            } else {
                addTarget(i,target?target->value: -1,NsfGoolFlowEdgeKind::Call);
            }
            if(i+1U<count) leaders.insert(i+1U);
        } else if(opcode==49U) {
            // RET ends the current block; the return destination is dynamic.
            if(i+1U<count) leaders.insert(i+1U);
        }
    }

    std::vector<std::size_t> starts(leaders.begin(),leaders.end());
    std::vector<std::size_t> instructionToBlock(count,0U);
    for(std::size_t b=0;b<starts.size();++b) {
        const std::size_t end=b+1U<starts.size()?starts[b+1U]:count;
        graph.blocks.push_back({starts[b],end});
        for(std::size_t i=starts[b];i<end;++i) instructionToBlock[i]=b;
    }
    auto addEdge=[&](std::size_t sourceInstruction,std::size_t targetInstruction,NsfGoolFlowEdgeKind kind,bool valid,const std::string& diagnostic) {
        const auto sourceBlock=instructionToBlock[sourceInstruction];
        graph.edges.push_back({sourceBlock,targetInstruction,kind,valid,diagnostic});
        if(!diagnostic.empty()) graph.diagnostics.push_back(diagnostic);
    };
    for(const auto& edge:pending) addEdge(edge.source,edge.target,edge.kind,edge.valid,edge.diagnostic);

    for(std::size_t b=0;b<graph.blocks.size();++b) {
        const auto& block=graph.blocks[b];
        const std::size_t last=block.endInstruction-1U;
        const auto& instruction=gool.instructions[last];
        if(instruction.isMips) {
            if(last+1U<count) addEdge(last,last+1U,NsfGoolFlowEdgeKind::Fallthrough,true,{});
            continue;
        }
        const auto opcode=instruction.opcode;
        if(opcode==49U) {
            graph.edges.push_back({b,0U,NsfGoolFlowEdgeKind::Return,false,"Return destination is dynamic."});
        } else if(opcode==50U) {
            // Unconditional relative branch: no fallthrough edge.
        } else if(opcode==51U||opcode==52U) {
            if(last+1U<count) addEdge(last,last+1U,NsfGoolFlowEdgeKind::Fallthrough,true,{});
        } else if(opcode==53U) {
            // Unconditional GO transfers control to another state.
        } else if(opcode==54U||opcode==55U) {
            if(last+1U<count) addEdge(last,last+1U,NsfGoolFlowEdgeKind::Fallthrough,true,{});
        } else if(opcode==59U) {
            if(last+1U<count) addEdge(last,last+1U,NsfGoolFlowEdgeKind::Fallthrough,true,{});
        } else if(last+1U<count) {
            addEdge(last,last+1U,NsfGoolFlowEdgeKind::Fallthrough,true,{});
        }
    }
    return graph;
}

NsfGoolInstructionAnalysis analyzeGoolInstruction(const NsfGool& gool, std::size_t instructionIndex) {
    NsfGoolInstructionAnalysis analysis;
    if(instructionIndex>=gool.instructions.size()) {
        analysis.executionEffect="Instruction index is outside this GOOL program.";
        return analysis;
    }
    const auto& instruction=gool.instructions[instructionIndex];
    if(instruction.isMips) {
        analysis.executionEffect="Native MIPS instruction; register and memory effects are not yet analyzed.";
        return analysis;
    }
    const auto& opcodeInfo=goolOpcodeInfo(gool.version,instruction.opcode);
    if(!opcodeInfo.known || goolOpcodeFormat(gool.version,instruction.opcode).empty()) {
        analysis.executionEffect="Unknown or unsupported GOOL opcode; effects are not inferred.";
        return analysis;
    }

    auto operands=decodeGoolOperands(gool.version,instruction.opcode,instruction.operandBits);
    resolveGoolOperands(operands,gool);
    analysis.operandsKnown=true;
    for(const auto& operand:operands) {
        NsfGoolOperandAccess access;
        switch(operand.kind) {
        case NsfGoolOperandKind::Reference:
            access=NsfGoolOperandAccess::Read;
            break;
        case NsfGoolOperandKind::DestinationReference:
            access=NsfGoolOperandAccess::Write;
            break;
        case NsfGoolOperandKind::ProcessField:
            // Conditional branch/state opcodes test this field. Other process-field
            // encodings need opcode-specific research before their access is assigned.
            access=(instruction.opcode==51U||instruction.opcode==52U||
                    instruction.opcode==54U||instruction.opcode==55U)
                ?NsfGoolOperandAccess::Read:NsfGoolOperandAccess::Unknown;
            break;
        default:
            continue; // Literal values and signed offsets are not storage accesses.
        }
        std::string reference=operand.resolvedText;
        if(reference.empty()&&operand.kind==NsfGoolOperandKind::ProcessField)
            reference=goolObjectFieldName(operand.rawValue);
        if(reference.empty()) reference=std::string(1,operand.name);
        const bool trackable=(operand.kind==NsfGoolOperandKind::Reference || operand.kind==NsfGoolOperandKind::DestinationReference) && (operand.rawValue&0x800U)!=0U;
        analysis.operandEffects.push_back({operand.name,access,std::move(reference),trackable});
    }

    const auto opcode=instruction.opcode;
    if(opcode==49U) analysis.executionEffect="Returns to the caller.";
    else if(opcode>=50U&&opcode<=52U)
        analysis.executionEffect=formatGoolControlFlow(opcode,operands,gool,instructionIndex);
    else if(opcode>=53U&&opcode<=55U)
        analysis.executionEffect=formatGoolControlFlow(opcode,operands,gool,instructionIndex);
    else if(opcode==59U)
        analysis.executionEffect=formatGoolControlFlow(opcode,operands,gool,instructionIndex);
    else
        analysis.executionEffect="Sequential control flow; no transfer is encoded by this instruction.";
    return analysis;
}


NsfGoolDataFlowResult analyzeGoolDataFlow(const NsfGool& gool) {
    NsfGoolDataFlowResult result;
    const std::size_t count=gool.instructions.size();
    if(count==0U) return result;
    std::vector<NsfGoolInstructionAnalysis> analyses;
    analyses.reserve(count);
    for(std::size_t i=0;i<count;++i) analyses.push_back(analyzeGoolInstruction(gool,i));

    std::vector<std::vector<std::size_t>> successors(count),predecessors(count);
    const auto graph=analyzeGoolControlFlow(gool);
    for(const auto& block:graph.blocks)
        for(std::size_t i=block.startInstruction;i+1U<block.endInstruction&&i+1U<count;++i)
            successors[i].push_back(i+1U);
    for(const auto& edge:graph.edges) {
        if(!edge.targetValid || edge.kind==NsfGoolFlowEdgeKind::Call) continue;
        if(edge.sourceBlock>=graph.blocks.size() || edge.targetInstruction>=count) continue;
        const auto source=graph.blocks[edge.sourceBlock].endInstruction-1U;
        if(source<count) successors[source].push_back(edge.targetInstruction);
    }
    for(std::size_t i=0;i<count;++i) {
        auto& next=successors[i];
        std::sort(next.begin(),next.end());
        next.erase(std::unique(next.begin(),next.end()),next.end());
        for(const auto target:next) predecessors[target].push_back(i);
    }

    std::vector<std::vector<std::size_t>> definitionsAt(count);
    for(std::size_t i=0;i<count;++i)
        for(const auto& effect:analyses[i].operandEffects)
            if(effect.trackable && effect.access==NsfGoolOperandAccess::Write) {
                definitionsAt[i].push_back(result.definitions.size());
                result.definitions.push_back({i,effect.reference});
            }

    using DefinitionSet=std::set<std::size_t>;
    std::vector<DefinitionSet> incoming(count),outgoing(count);
    auto transfer=[&](std::size_t instructionIndex,DefinitionSet state) {
        const auto& analysis=analyses[instructionIndex];
        if(!analysis.operandsKnown) { state.clear(); return state; }
        for(const auto id:definitionsAt[instructionIndex]) {
            const auto& location=result.definitions[id].location;
            for(auto it=state.begin();it!=state.end();) {
                if(result.definitions[*it].location==location) it=state.erase(it);
                else ++it;
            }
            state.insert(id);
        }
        return state;
    };
    bool changed=true;
    while(changed) {
        changed=false;
        for(std::size_t i=0;i<count;++i) {
            DefinitionSet nextIn;
            for(const auto predecessor:predecessors[i])
                nextIn.insert(outgoing[predecessor].begin(),outgoing[predecessor].end());
            auto nextOut=transfer(i,nextIn);
            if(nextIn!=incoming[i] || nextOut!=outgoing[i]) {
                incoming[i]=std::move(nextIn);
                outgoing[i]=std::move(nextOut);
                changed=true;
            }
        }
    }
    for(std::size_t i=0;i<count;++i)
        for(const auto& effect:analyses[i].operandEffects) {
            if(!effect.trackable || effect.access!=NsfGoolOperandAccess::Read) continue;
            NsfGoolDataFlowUse use;
            use.instructionIndex=i;
            use.location=effect.reference;
            for(const auto id:incoming[i])
                if(result.definitions[id].location==effect.reference) use.reachingDefinitions.push_back(id);
            result.uses.push_back(std::move(use));
        }
    return result;
}

}
