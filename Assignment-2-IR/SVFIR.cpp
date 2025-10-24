/**
 * SVFIR.cpp
 * @author kisslune
 */

#include "Graphs/SVFG.h"
#include "SVF-LLVM/SVFIRBuilder.h"

using namespace SVF;
using namespace llvm;
using namespace std;

int main(int argc, char** argv)
{
    int arg_num = 0;
    int extraArgc = 4;
    char** arg_value = new char*[argc + extraArgc];
    for (; arg_num < argc; ++arg_num) {
        arg_value[arg_num] = argv[arg_num];
    }
    std::vector<std::string> moduleNameVec;

    int orgArgNum = arg_num;
    arg_value[arg_num++] = (char*)"-model-arrays=true";
    arg_value[arg_num++] = (char*)"-pre-field-sensitive=false";
    arg_value[arg_num++] = (char*)"-model-consts=true";
    arg_value[arg_num++] = (char*)"-stat=false";
    assert(arg_num == (orgArgNum + extraArgc) && "more extra arguments? Change the value of extraArgc");

    moduleNameVec = OptionBase::parseOptions(arg_num, arg_value, "SVF IR", "[options] <input-bitcode...>");

    LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);

    // Instantiate an SVFIR builder
    SVFIRBuilder builder;
    cout << "Generating SVFIR(PAG), call graph and ICFG ..." << endl;

    // TODO: here, generate SVFIR(PAG), call graph and ICFG, and dump them to files
    //@{
    SVFIR *pag = builder.build(); // build() 会构造 PAG、ICFG、CG 等。
    if (!pag) {
        cerr << "Failed to build SVFIR (pag == nullptr)\n";
        LLVMModuleSet::releaseLLVMModuleSet();
        delete[] arg_value;
        return 1;
    }

    cout << "SVFIR built. Dumping graphs to .dot files ..." << endl;

    // Dump SVFIR/PAG 到 dot（通常 pag 非 const，可以直接调用）
    pag->dump("svfir_initial.dot");

    // Dump CallGraph：getCallGraph() 返回 const CallGraph*（API 可能如此），
    // 所以这里做一次 const_cast 再调用非 const 的 dump().
    // Dump CallGraph
    if (pag->getCallGraph()) {
        // getCallGraph() 返回 const CallGraph*，需要去掉 const
        CallGraph *cg = const_cast<CallGraph*>(pag->getCallGraph());
        cg->dump("callgraph_initial.dot");
    }

    // Dump ICFG
    if (pag->getICFG()) {
        ICFG *icfg = const_cast<ICFG*>(pag->getICFG());
        icfg->dump("icfg_initial.dot");
    }


    // 清理：释放 SVFIR 结构与模块集
    SVFIR::releaseSVFIR();
    LLVMModuleSet::releaseLLVMModuleSet();

    // 释放 LLVM 全局资源
    llvm::llvm_shutdown();

    // 释放我们临时申请的 argv 拷贝数组（注意仅删除数组本身，数组内指针指向 argv 或字面串，不要 delete 每个指针）
    delete[] arg_value;
    //@}

    return 0;
}
