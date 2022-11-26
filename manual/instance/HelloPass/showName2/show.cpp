// https://www.leadroyal.cn/p/719/

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

#include "SimpleInvoker.h"

using namespace std;
using namespace llvm;

namespace llvm {
    class SimpleInvoker : public ModulePass {
    public:
        static char ID;

        SimpleInvoker() : ModulePass(ID) {}

        bool runOnModule(Module &M) override;

        virtual StringRef getPassName() const override {
            return "Simple Invoker";
        }
    };

    Pass *createSimpleInvoker() {
        return new SimpleInvoker();
    }

    char SimpleInvoker::ID = 0;
}

bool SimpleInvoker::runOnModule(Module &M) {
    Function *F = M.getFunction("main");
    if (F) {
        errs() << "Main Function Found! So return.n";
        return true;
    }
    errs() << "Main Function Not Found! So create.n";
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(M.getContext()), false);
    F = Function::Create(FT, GlobalValue::LinkageTypes::ExternalLinkage, "main", &M);
    BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "EntryBlock", F);
    IRBuilder<> IRB(EntryBB);
    for (Function &FF:M) {
        if(FF.getName() == "main")
            continue;
        if(FF.empty())
            continue;
        IRB.CreateCall(&FF);
    }
    IRB.CreateRet(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0));
    return true;
}