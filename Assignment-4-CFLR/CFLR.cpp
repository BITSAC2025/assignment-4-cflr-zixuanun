/**
 * CFLR.cpp
 * @author kisslune 
 */

#include "A4Header.h"

using namespace SVF;
using namespace llvm;
using namespace std;

int main(int argc, char **argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
                                     "[options] <input-bitcode...>");

    LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVFIRBuilder builder;
    auto pag = builder.build();
    pag->dump("pag");

    CFLR solver;
    solver.buildGraph(pag);
    // TODO: complete this method
    solver.solve();
    solver.dumpResult();

    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}


void CFLR::solve()
{
    // TODO: complete this function. The implementations of graph and worklist are provided.
    //  You need to:
    //  1. implement the grammar production rules into code;
    //  2. implement the dynamic-programming CFL-reachability algorithm.
    //  You may need to add your new methods to 'CFLRGraph' and 'CFLR'.
    // 按照标准CFL算法：先处理ε产生式
    std::unordered_set<unsigned> allNodes;
    for (auto& node : graph->getSuccessorMap()) {
        allNodes.insert(node.first);
    }
    for (auto& node : graph->getPredecessorMap()) {
        allNodes.insert(node.first);
    }
    
    // 添加ε边
    for (unsigned v : allNodes) {
        // VF ::= ε
        if (!graph->hasEdge(v, v, VF)) {
            graph->addEdge(v, v, VF);
            workList.push(CFLREdge(v, v, VF));
        }
        // VFBar ::= ε  
        if (!graph->hasEdge(v, v, VFBar)) {
            graph->addEdge(v, v, VFBar);
            workList.push(CFLREdge(v, v, VFBar));
        }
        // VA ::= ε
        if (!graph->hasEdge(v, v, VA)) {
            graph->addEdge(v, v, VA);
            workList.push(CFLREdge(v, v, VA));
        }
    }

    // 初始化工作列表
    auto& succMap = graph->getSuccessorMap();
    for (auto& srcItem : succMap) {
        unsigned src = srcItem.first;
        for (auto& labelItem : srcItem.second) {
            EdgeLabel label = labelItem.first;
            for (unsigned dst : labelItem.second) {
                workList.push(CFLREdge(src, dst, label));
            }
        }
    }

    // 主算法循环
    while (!workList.empty()) {
        CFLREdge edge = workList.pop();
        unsigned v_i = edge.src;
        unsigned v_j = edge.dst;
        EdgeLabel X = edge.label;

        // 处理单符号产生式 A ::= X
        if (X == Copy) {
            if (!graph->hasEdge(v_i, v_j, VF)) {
                graph->addEdge(v_i, v_j, VF);
                workList.push(CFLREdge(v_i, v_j, VF));
            }
        }
        if (X == CopyBar) {
            if (!graph->hasEdge(v_i, v_j, VFBar)) {
                graph->addEdge(v_i, v_j, VFBar);
                workList.push(CFLREdge(v_i, v_j, VFBar));
            }
        }

        // 处理顺序双符号产生式 A ::= X Y
        if (graph->getSuccessorMap().count(v_j)) {
            auto& succs_vj = graph->getSuccessorMap()[v_j];
            for (auto& Y_item : succs_vj) {
                EdgeLabel Y = Y_item.first;
                for (unsigned v_k : Y_item.second) {
                    // PT ::= VFBar AddrBar
                    if (X == VFBar && Y == AddrBar) {
                        if (!graph->hasEdge(v_i, v_k, PT)) {
                            graph->addEdge(v_i, v_k, PT);
                            workList.push(CFLREdge(v_i, v_k, PT));
                        }
                    }
                    // VF ::= VF VF
                    if (X == VF && Y == VF) {
                        if (!graph->hasEdge(v_i, v_k, VF)) {
                            graph->addEdge(v_i, v_k, VF);
                            workList.push(CFLREdge(v_i, v_k, VF));
                        }
                    }
                    // VF ::= SV Load
                    if (X == SV && Y == Load) {
                        if (!graph->hasEdge(v_i, v_k, VF)) {
                            graph->addEdge(v_i, v_k, VF);
                            workList.push(CFLREdge(v_i, v_k, VF));
                        }
                    }
                    // VF ::= PV Load
                    if (X == PV && Y == Load) {
                        if (!graph->hasEdge(v_i, v_k, VF)) {
                            graph->addEdge(v_i, v_k, VF);
                            workList.push(CFLREdge(v_i, v_k, VF));
                        }
                    }
                    // VF ::= Store VP
                    if (X == Store && Y == VP) {
                        if (!graph->hasEdge(v_i, v_k, VF)) {
                            graph->addEdge(v_i, v_k, VF);
                            workList.push(CFLREdge(v_i, v_k, VF));
                        }
                    }
                    // VFBar ::= VFBar VFBar
                    if (X == VFBar && Y == VFBar) {
                        if (!graph->hasEdge(v_i, v_k, VFBar)) {
                            graph->addEdge(v_i, v_k, VFBar);
                            workList.push(CFLREdge(v_i, v_k, VFBar));
                        }
                    }
                    // VFBar ::= LoadBar SVBar
                    if (X == LoadBar && Y == SVBar) {
                        if (!graph->hasEdge(v_i, v_k, VFBar)) {
                            graph->addEdge(v_i, v_k, VFBar);
                            workList.push(CFLREdge(v_i, v_k, VFBar));
                        }
                    }
                    // VFBar ::= LoadBar VP
                    if (X == LoadBar && Y == VP) {
                        if (!graph->hasEdge(v_i, v_k, VFBar)) {
                            graph->addEdge(v_i, v_k, VFBar);
                            workList.push(CFLREdge(v_i, v_k, VFBar));
                        }
                    }
                    // VFBar ::= PV StoreBar
                    if (X == PV && Y == StoreBar) {
                        if (!graph->hasEdge(v_i, v_k, VFBar)) {
                            graph->addEdge(v_i, v_k, VFBar);
                            workList.push(CFLREdge(v_i, v_k, VFBar));
                        }
                    }
                    // VA ::= LV Load
                    if (X == LV && Y == Load) {
                        if (!graph->hasEdge(v_i, v_k, VA)) {
                            graph->addEdge(v_i, v_k, VA);
                            workList.push(CFLREdge(v_i, v_k, VA));
                        }
                    }
                    // VA ::= VFBar VA
                    if (X == VFBar && Y == VA) {
                        if (!graph->hasEdge(v_i, v_k, VA)) {
                            graph->addEdge(v_i, v_k, VA);
                            workList.push(CFLREdge(v_i, v_k, VA));
                        }
                    }
                    // VA ::= VA VF
                    if (X == VA && Y == VF) {
                        if (!graph->hasEdge(v_i, v_k, VA)) {
                            graph->addEdge(v_i, v_k, VA);
                            workList.push(CFLREdge(v_i, v_k, VA));
                        }
                    }
                }
            }
        }

        // 处理逆序双符号产生式 A ::= Y X
        if (graph->getPredecessorMap().count(v_i)) {
            auto& preds_vi = graph->getPredecessorMap()[v_i];
            for (auto& Y_item : preds_vi) {
                EdgeLabel Y = Y_item.first;
                for (unsigned v_k : Y_item.second) {
                    // PTBar ::= Addr VF
                    if (Y == Addr && X == VF) {
                        if (!graph->hasEdge(v_k, v_j, PTBar)) {
                            graph->addEdge(v_k, v_j, PTBar);
                            workList.push(CFLREdge(v_k, v_j, PTBar));
                        }
                    }
                    // SV ::= Store VA
                    if (Y == Store && X == VA) {
                        if (!graph->hasEdge(v_k, v_j, SV)) {
                            graph->addEdge(v_k, v_j, SV);
                            workList.push(CFLREdge(v_k, v_j, SV));
                        }
                    }
                    // SVBar ::= VA StoreBar
                    if (Y == VA && X == StoreBar) {
                        if (!graph->hasEdge(v_k, v_j, SVBar)) {
                            graph->addEdge(v_k, v_j, SVBar);
                            workList.push(CFLREdge(v_k, v_j, SVBar));
                        }
                    }
                    // PV ::= PTBar VA
                    if (Y == PTBar && X == VA) {
                        if (!graph->hasEdge(v_k, v_j, PV)) {
                            graph->addEdge(v_k, v_j, PV);
                            workList.push(CFLREdge(v_k, v_j, PV));
                        }
                    }
                    // VP ::= VA PT
                    if (Y == VA && X == PT) {
                        if (!graph->hasEdge(v_k, v_j, VP)) {
                            graph->addEdge(v_k, v_j, VP);
                            workList.push(CFLREdge(v_k, v_j, VP));
                        }
                    }
                    // LV ::= LoadBar VA
                    if (Y == LoadBar && X == VA) {
                        if (!graph->hasEdge(v_k, v_j, LV)) {
                            graph->addEdge(v_k, v_j, LV);
                            workList.push(CFLREdge(v_k, v_j, LV));
                        }
                    }
                }
            }
        }
    }
}

