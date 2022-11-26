#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h" 
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;
namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct GetLoopInfo2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    GetLoopInfo2() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      errs() <<"Function "<< F.getName() << '\n';
      for(Loop *L : *LI)
      {
          countBlocksInLoop(L, 0);
      }
      return false;
    }

    // We don't modify the program, so we preserve all analyses.
    void countBlocksInLoop(Loop* L, unsigned nest) {
      unsigned num_Blocks = 0;
      Loop::block_iterator bb;
      for(bb = L->block_begin(); bb != L->block_end(); ++bb)
      {
          num_Blocks ++;
      }
      errs() << "Loop Level "<< nest << " has "<< num_Blocks<< " Blocks\n";
      std::vector<Loop*> subLoops =L->getSubLoops();
      Loop::iterator j, f;
      for(j = subLoops.begin(), f= subLoops.end();j!=f;++j)
          countBlocksInLoop(*j, nest+1);
    }
    
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<LoopInfoWrapperPass>();
      //AU.setPreservesAll();
    }
  };
  
}
char GetLoopInfo2::ID = 0;
static RegisterPass<GetLoopInfo2> Y("getLoopInfo2", "Get LoopInfo2");