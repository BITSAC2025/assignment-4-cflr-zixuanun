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
    // 0. 定义一个辅助 Lambda 用于添加边，避免重复代码
    // 作用：检查边是否存在，若不存在则添加到图和工作列表
    auto addCFLItem = [&](unsigned u, unsigned v, EdgeLabel label) {
        if (!graph->hasEdge(u, v, label)) {
            graph->addEdge(u, v, label);
            workList.push(CFLREdge(u, v, label));
        }
    };

    // 1. 初始化：处理 ε 产生式 (V ::= ε)
    std::unordered_set<unsigned> allNodes;
    for (auto& node : graph->getSuccessorMap()) allNodes.insert(node.first);
    for (auto& node : graph->getPredecessorMap()) allNodes.insert(node.first);
    
    for (unsigned v : allNodes) {
        addCFLItem(v, v, VF);    // VF ::= ε
        addCFLItem(v, v, VFBar); // VFBar ::= ε
        addCFLItem(v, v, VA);    // VA ::= ε
    }

    // 2. 初始化工作列表：将图中已有的初始边加入 Worklist
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

    // 3. 主算法循环
    while (!workList.empty()) {
        CFLREdge edge = workList.pop();
        unsigned v_i = edge.src;
        unsigned v_j = edge.dst;
        EdgeLabel X = edge.label;

        // === 处理一元产生式 (A ::= X) ===
        if (X == Copy)    addCFLItem(v_i, v_j, VF);    // VF ::= Copy
        if (X == CopyBar) addCFLItem(v_i, v_j, VFBar); // VFBar ::= CopyBar

        // === 处理二元产生式: 顺向匹配 (Forward Checks) ===
        // 场景: 当前边是 X (v_i -> v_j)，寻找后继边 Y (v_j -> v_k) 以匹配 A ::= X Y
        if (graph->getSuccessorMap().count(v_j)) {
            auto& succs_vj = graph->getSuccessorMap()[v_j];
            for (auto& Y_item : succs_vj) {
                EdgeLabel Y = Y_item.first;
                for (unsigned v_k : Y_item.second) {
                    // 语法规则匹配 (X 是产生式左部)
                    if (X == VFBar && Y == AddrBar) addCFLItem(v_i, v_k, PT);    // PT ::= VFBar AddrBar
                    if (X == VF && Y == VF)         addCFLItem(v_i, v_k, VF);    // VF ::= VF VF
                    if (X == SV && Y == Load)       addCFLItem(v_i, v_k, VF);    // VF ::= SV Load
                    if (X == PV && Y == Load)       addCFLItem(v_i, v_k, VF);    // VF ::= PV Load
                    if (X == Store && Y == VP)      addCFLItem(v_i, v_k, VF);    // VF ::= Store VP
                    if (X == VFBar && Y == VFBar)   addCFLItem(v_i, v_k, VFBar); // VFBar ::= VFBar VFBar
                    if (X == LoadBar && Y == SVBar) addCFLItem(v_i, v_k, VFBar); // VFBar ::= LoadBar SVBar
                    if (X == LoadBar && Y == VP)    addCFLItem(v_i, v_k, VFBar); // VFBar ::= LoadBar VP
                    if (X == PV && Y == StoreBar)   addCFLItem(v_i, v_k, VFBar); // VFBar ::= PV StoreBar
                    if (X == LV && Y == Load)       addCFLItem(v_i, v_k, VA);    // VA ::= LV Load
                    if (X == VFBar && Y == VA)      addCFLItem(v_i, v_k, VA);    // VA ::= VFBar VA
                    if (X == VA && Y == VF)         addCFLItem(v_i, v_k, VA);    // VA ::= VA VF
                    
                    // 你之前遗漏的规则 (X 为左部)
                    if (X == Addr && Y == VF)       addCFLItem(v_i, v_k, PTBar); // PTBar ::= Addr VF
                    if (X == Store && Y == VA)      addCFLItem(v_i, v_k, SV);    // SV ::= Store VA
                    if (X == VA && Y == StoreBar)   addCFLItem(v_i, v_k, SVBar); // SVBar ::= VA StoreBar
                    if (X == PTBar && Y == VA)      addCFLItem(v_i, v_k, PV);    // PV ::= PTBar VA
                    if (X == VA && Y == PT)         addCFLItem(v_i, v_k, VP);    // VP ::= VA PT
                    if (X == LoadBar && Y == VA)    addCFLItem(v_i, v_k, LV);    // LV ::= LoadBar VA
                }
            }
        }

        // === 处理二元产生式: 逆向匹配 (Backward Checks) ===
        // 场景: 当前边是 X (v_i -> v_j)，寻找前驱边 Y (v_k -> v_i) 以匹配 A ::= Y X
        if (graph->getPredecessorMap().count(v_i)) {
            auto& preds_vi = graph->getPredecessorMap()[v_i];
            for (auto& Y_item : preds_vi) {
                EdgeLabel Y = Y_item.first;
                for (unsigned v_k : Y_item.second) {
                    // 语法规则匹配 (X 是产生式右部，注意 Y 在左)
                    if (Y == Addr && X == VF)       addCFLItem(v_k, v_j, PTBar); // PTBar ::= Addr VF
                    if (Y == Store && X == VA)      addCFLItem(v_k, v_j, SV);    // SV ::= Store VA
                    if (Y == VA && X == StoreBar)   addCFLItem(v_k, v_j, SVBar); // SVBar ::= VA StoreBar
                    if (Y == PTBar && X == VA)      addCFLItem(v_k, v_j, PV);    // PV ::= PTBar VA
                    if (Y == VA && X == PT)         addCFLItem(v_k, v_j, VP);    // VP ::= VA PT
                    if (Y == LoadBar && X == VA)    addCFLItem(v_k, v_j, LV);    // LV ::= LoadBar VA
                    
                    // 关键：你之前遗漏的规则 (X 为右部)
                    if (Y == VFBar && X == AddrBar) addCFLItem(v_k, v_j, PT);    // PT ::= VFBar AddrBar
                    if (Y == VF && X == VF)         addCFLItem(v_k, v_j, VF);    // VF ::= VF VF
                    if (Y == SV && X == Load)       addCFLItem(v_k, v_j, VF);    // VF ::= SV Load
                    if (Y == PV && X == Load)       addCFLItem(v_k, v_j, VF);    // VF ::= PV Load
                    if (Y == Store && X == VP)      addCFLItem(v_k, v_j, VF);    // VF ::= Store VP
                    if (Y == VFBar && X == VFBar)   addCFLItem(v_k, v_j, VFBar); // VFBar ::= VFBar VFBar
                    if (Y == LoadBar && X == SVBar) addCFLItem(v_k, v_j, VFBar); // VFBar ::= LoadBar SVBar
                    if (Y == LoadBar && X == VP)    addCFLItem(v_k, v_j, VFBar); // VFBar ::= LoadBar VP
                    if (Y == PV && X == StoreBar)   addCFLItem(v_k, v_j, VFBar); // VFBar ::= PV StoreBar
                    if (Y == LV && X == Load)       addCFLItem(v_k, v_j, VA);    // VA ::= LV Load
                    if (Y == VFBar && X == VA)      addCFLItem(v_k, v_j, VA);    // VA ::= VFBar VA
                    if (Y == VA && X == VF)         addCFLItem(v_k, v_j, VA);    // VA ::= VA VF
                }
            }
        }
    }
}
// void CFLR::solve()
// {
//     // TODO: complete this function. The implementations of graph and worklist are provided.
//     //  You need to:
//     //  1. implement the grammar production rules into code;
//     //  2. implement the dynamic-programming CFL-reachability algorithm.
//     //  You may need to add your new methods to 'CFLRGraph' and 'CFLR'.
//     // 按照标准CFL算法：先处理ε产生式
//     std::unordered_set<unsigned> allNodes;
//     for (auto& node : graph->getSuccessorMap()) {
//         allNodes.insert(node.first);
//     }
//     for (auto& node : graph->getPredecessorMap()) {
//         allNodes.insert(node.first);
//     }
    
//     // 添加ε边
//     for (unsigned v : allNodes) {
//         // VF ::= ε
//         if (!graph->hasEdge(v, v, VF)) {
//             graph->addEdge(v, v, VF);
//             workList.push(CFLREdge(v, v, VF));
//         }
//         // VFBar ::= ε  
//         if (!graph->hasEdge(v, v, VFBar)) {
//             graph->addEdge(v, v, VFBar);
//             workList.push(CFLREdge(v, v, VFBar));
//         }
//         // VA ::= ε
//         if (!graph->hasEdge(v, v, VA)) {
//             graph->addEdge(v, v, VA);
//             workList.push(CFLREdge(v, v, VA));
//         }
//     }

//     // 初始化工作列表
//     auto& succMap = graph->getSuccessorMap();
//     for (auto& srcItem : succMap) {
//         unsigned src = srcItem.first;
//         for (auto& labelItem : srcItem.second) {
//             EdgeLabel label = labelItem.first;
//             for (unsigned dst : labelItem.second) {
//                 workList.push(CFLREdge(src, dst, label));
//             }
//         }
//     }

//     // 主算法循环
//     while (!workList.empty()) {
//         CFLREdge edge = workList.pop();
//         unsigned v_i = edge.src;
//         unsigned v_j = edge.dst;
//         EdgeLabel X = edge.label;

//         // 处理单符号产生式 A ::= X
//         if (X == Copy) {
//             if (!graph->hasEdge(v_i, v_j, VF)) {
//                 graph->addEdge(v_i, v_j, VF);
//                 workList.push(CFLREdge(v_i, v_j, VF));
//             }
//         }
//         if (X == CopyBar) {
//             if (!graph->hasEdge(v_i, v_j, VFBar)) {
//                 graph->addEdge(v_i, v_j, VFBar);
//                 workList.push(CFLREdge(v_i, v_j, VFBar));
//             }
//         }

//         // 处理顺序双符号产生式 A ::= X Y
//         if (graph->getSuccessorMap().count(v_j)) {
//             auto& succs_vj = graph->getSuccessorMap()[v_j];
//             for (auto& Y_item : succs_vj) {
//                 EdgeLabel Y = Y_item.first;
//                 for (unsigned v_k : Y_item.second) {
//                     // PT ::= VFBar AddrBar
//                     if (X == VFBar && Y == AddrBar) {
//                         if (!graph->hasEdge(v_i, v_k, PT)) {
//                             graph->addEdge(v_i, v_k, PT);
//                             workList.push(CFLREdge(v_i, v_k, PT));
//                         }
//                     }
//                     // VF ::= VF VF
//                     if (X == VF && Y == VF) {
//                         if (!graph->hasEdge(v_i, v_k, VF)) {
//                             graph->addEdge(v_i, v_k, VF);
//                             workList.push(CFLREdge(v_i, v_k, VF));
//                         }
//                     }
//                     // VF ::= SV Load
//                     if (X == SV && Y == Load) {
//                         if (!graph->hasEdge(v_i, v_k, VF)) {
//                             graph->addEdge(v_i, v_k, VF);
//                             workList.push(CFLREdge(v_i, v_k, VF));
//                         }
//                     }
//                     // VF ::= PV Load
//                     if (X == PV && Y == Load) {
//                         if (!graph->hasEdge(v_i, v_k, VF)) {
//                             graph->addEdge(v_i, v_k, VF);
//                             workList.push(CFLREdge(v_i, v_k, VF));
//                         }
//                     }
//                     // VF ::= Store VP
//                     if (X == Store && Y == VP) {
//                         if (!graph->hasEdge(v_i, v_k, VF)) {
//                             graph->addEdge(v_i, v_k, VF);
//                             workList.push(CFLREdge(v_i, v_k, VF));
//                         }
//                     }
//                     // VFBar ::= VFBar VFBar
//                     if (X == VFBar && Y == VFBar) {
//                         if (!graph->hasEdge(v_i, v_k, VFBar)) {
//                             graph->addEdge(v_i, v_k, VFBar);
//                             workList.push(CFLREdge(v_i, v_k, VFBar));
//                         }
//                     }
//                     // VFBar ::= LoadBar SVBar
//                     if (X == LoadBar && Y == SVBar) {
//                         if (!graph->hasEdge(v_i, v_k, VFBar)) {
//                             graph->addEdge(v_i, v_k, VFBar);
//                             workList.push(CFLREdge(v_i, v_k, VFBar));
//                         }
//                     }
//                     // VFBar ::= LoadBar VP
//                     if (X == LoadBar && Y == VP) {
//                         if (!graph->hasEdge(v_i, v_k, VFBar)) {
//                             graph->addEdge(v_i, v_k, VFBar);
//                             workList.push(CFLREdge(v_i, v_k, VFBar));
//                         }
//                     }
//                     // VFBar ::= PV StoreBar
//                     if (X == PV && Y == StoreBar) {
//                         if (!graph->hasEdge(v_i, v_k, VFBar)) {
//                             graph->addEdge(v_i, v_k, VFBar);
//                             workList.push(CFLREdge(v_i, v_k, VFBar));
//                         }
//                     }
//                     // VA ::= LV Load
//                     if (X == LV && Y == Load) {
//                         if (!graph->hasEdge(v_i, v_k, VA)) {
//                             graph->addEdge(v_i, v_k, VA);
//                             workList.push(CFLREdge(v_i, v_k, VA));
//                         }
//                     }
//                     // VA ::= VFBar VA
//                     if (X == VFBar && Y == VA) {
//                         if (!graph->hasEdge(v_i, v_k, VA)) {
//                             graph->addEdge(v_i, v_k, VA);
//                             workList.push(CFLREdge(v_i, v_k, VA));
//                         }
//                     }
//                     // VA ::= VA VF
//                     if (X == VA && Y == VF) {
//                         if (!graph->hasEdge(v_i, v_k, VA)) {
//                             graph->addEdge(v_i, v_k, VA);
//                             workList.push(CFLREdge(v_i, v_k, VA));
//                         }
//                     }
//                     if (X == Addr && Y == VF)       addEdge(v_i, v_k, PTBar); // PTBar ::= Addr VF (补全)
//                     if (X == VA && Y == StoreBar)   addEdge(v_i, v_k, SVBar); // SVBar ::= VA StoreBar (补全)
//                     if (X == PTBar && Y == VA)      addEdge(v_i, v_k, PV);    // PV ::= PTBar VA (补全)
//                 }
//             }
//         }

//         // 处理逆序双符号产生式 A ::= Y X
//         if (graph->getPredecessorMap().count(v_i)) {
//             auto& preds_vi = graph->getPredecessorMap()[v_i];
//             for (auto& Y_item : preds_vi) {
//                 EdgeLabel Y = Y_item.first;
//                 for (unsigned v_k : Y_item.second) {
//                     // PTBar ::= Addr VF
//                     if (Y == Addr && X == VF) {
//                         if (!graph->hasEdge(v_k, v_j, PTBar)) {
//                             graph->addEdge(v_k, v_j, PTBar);
//                             workList.push(CFLREdge(v_k, v_j, PTBar));
//                         }
//                     }
//                     // SV ::= Store VA
//                     if (Y == Store && X == VA) {
//                         if (!graph->hasEdge(v_k, v_j, SV)) {
//                             graph->addEdge(v_k, v_j, SV);
//                             workList.push(CFLREdge(v_k, v_j, SV));
//                         }
//                     }
//                     // SVBar ::= VA StoreBar
//                     if (Y == VA && X == StoreBar) {
//                         if (!graph->hasEdge(v_k, v_j, SVBar)) {
//                             graph->addEdge(v_k, v_j, SVBar);
//                             workList.push(CFLREdge(v_k, v_j, SVBar));
//                         }
//                     }
//                     // PV ::= PTBar VA
//                     if (Y == PTBar && X == VA) {
//                         if (!graph->hasEdge(v_k, v_j, PV)) {
//                             graph->addEdge(v_k, v_j, PV);
//                             workList.push(CFLREdge(v_k, v_j, PV));
//                         }
//                     }
//                     // VP ::= VA PT
//                     if (Y == VA && X == PT) {
//                         if (!graph->hasEdge(v_k, v_j, VP)) {
//                             graph->addEdge(v_k, v_j, VP);
//                             workList.push(CFLREdge(v_k, v_j, VP));
//                         }
//                     }
//                     // LV ::= LoadBar VA
//                     if (Y == LoadBar && X == VA) {
//                         if (!graph->hasEdge(v_k, v_j, LV)) {
//                             graph->addEdge(v_k, v_j, LV);
//                             workList.push(CFLREdge(v_k, v_j, LV));
//                         }
//                     }
//                     if (Y == VFBar && X == AddrBar) addEdge(v_k, v_j, PT);    // PT ::= VFBar AddrBar (补全)
//                     if (Y == VF && X == VF)         addEdge(v_k, v_j, VF);    // VF ::= VF VF (关键遗漏!)
//                     if (Y == SV && X == Load)       addEdge(v_k, v_j, VF);    // VF ::= SV Load (补全)
//                     if (Y == PV && X == Load)       addEdge(v_k, v_j, VF);    // VF ::= PV Load (补全)
//                     if (Y == Store && X == VP)      addEdge(v_k, v_j, VF);    // VF ::= Store VP (补全)
//                     if (Y == VFBar && X == VFBar)   addEdge(v_k, v_j, VFBar); // VFBar ::= VFBar VFBar (补全)
//                 }
//             }
//         }
//     }
// }

