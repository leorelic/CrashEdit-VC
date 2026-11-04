#include "crash/formats/gool/gool_control_flow.hpp"
#include "crash/formats/gool/gool_opcodes.hpp"
#include <algorithm>
#include <set>

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
            if(opcode!=50U&&i+1U<count) leaders.insert(i+1U);
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
}
