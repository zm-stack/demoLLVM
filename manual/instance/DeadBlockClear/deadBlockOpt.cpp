// https://zhuanlan.zhihu.com/p/63568031

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/IR/CFG.h"
#include <set>
using namespace llvm;
namespace {
struct DeadBlock : public FunctionPass
{
    static char ID;
    DeadBlock() : FunctionPass(ID) {}
    
    virtual bool runOnFunction(llvm::Function& F) override {
        bool changed = false;
        // visitedSet 用于存放已经被访问过的BaseBlock
        // unreachableSet 则在最后用于存放无法被访问到的block
        std::set<BasicBlock*> visitedSet;
        std::set<BasicBlock*> unreachableSet;
        // 从EntryBlock开始深度优先遍历整个函数内可以访问的BaseBlock
        // 将已被访问过的BaseBlock存放在visitedSet中
        for (auto i = df_ext_begin<BasicBlock*,std::set<BasicBlock*>>(&F.getEntryBlock(), visitedSet),
            e = df_ext_end<BasicBlock*,std::set<BasicBlock*>>(&F.getEntryBlock(), visitedSet);
            i != e; i++);
        // 遍历函数内所有BaseBlock，将不在vistitedSet中的BaseBlock添加到unreachableSet中
        for (BasicBlock & BB : F) {
            if (visitedSet.find(&BB) == visitedSet.end()) {
                unreachableSet.insert(&BB);
            }
        }
        // 标记目标函数是否会被修改
        if (!unreachableSet.empty()) {
            changed = true;
        }
        // 遍历unreachableSet，通知其successor移除多余的phi node
        for (BasicBlock* BB : unreachableSet) {
            for (auto i = succ_begin(BB); i != succ_end(BB); i++) {
                i->removePredecessor(BB);
            }
            BB->eraseFromParent();
        }
    
        return changed;
    };
};
}
// LLVM会利用pass的地址来为这个id赋值，所以初始值并不重要
char DeadBlock::ID = 0;
// 注册pass，这个pass可能会改变CFG，所以将第三个参数设为true
static RegisterPass<DeadBlock> X("deadblock", "Dead blocks elimination pass", true, false);