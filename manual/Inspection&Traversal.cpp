// Iterating over the BasicBlock in a Function
Function &Func = ...
for (BasicBlock &BB : Func)
  // Print out the name of the basic block if it has one, and then the
  // number of instructions that it contains
  errs() << "Basic block (name=" << BB.getName() << ") has "
             << BB.size() << " instructions.\n";

// Iterating over the Instruction in a BasicBlock
BasicBlock& BB = ...
// errs() << BB << "\n";.
for (Instruction &I : BB)
   // The next statement works since operator<<(ostream&,...)
   // is overloaded for Instruction&
   errs() << I << "\n";

// Iterating over the Instruction in a Function
#include "llvm/IR/InstIterator.h"
// F is a pointer to a Function instance
for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
  errs() << *I << "\n";
// or use InstIterators to fill a work list with its initial contents
std::set<Instruction*> worklist;
// or better yet, SmallPtrSet<Instruction*, 64> worklist;
for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
  worklist.insert(&*I);

// Turning an iterator into a class pointer (and vice-versa)
// The iterators in LLVM will automatically convert to a ptr-to-instance type whenever they need to.
// Just simply assign the iterator to the proper pointer type and get the dereference 
// and address-of operation as a result of the assignment.
// Assuming that i is a BasicBlock::iterator and j is a BasicBlock::const_iterator:
Instruction& inst = *i;   // Grab reference to instruction reference
Instruction* pinst = &*i; // Grab pointer to instruction reference, semantically equivalent to:
// Instruction *pinst = i;
const Instruction& inst = *j;

// turn a class pointer into the corresponding iterator
void printNextInstruction(Instruction* inst) {
  BasicBlock::iterator it(inst);
  ++it; // After this line, it refers to the instruction after *inst
  if (it != inst->getParent()->end()) errs() << *it << "\n";
}
// However, these implicit conversions prevent the following code, where B is a BasicBlock, for example: 
llvm::SmallVector<llvm::Instruction *, 16>(B->begin(), B->end());
// these implicit conversions may be removed some day, and operator* changed to return a pointer instead of a reference.

// Finding call sites
// Count all the locations in the entire module where a certain function is already in scope.
// Use an InstVisitor to accomplish this in a much more straight-forward manner.
initialize callCounter to zero
for each Function f in the Module
  for each BasicBlock b in f
    for each Instruction i in b
      if (i a Call and calls the given function)
        increment callCounter
// the actual code
Function* targetFunc = ...;
class OurFunctionPass : public FunctionPass {
  public:
    OurFunctionPass(): callCounter(0) { }
    virtual runOnFunction(Function& F) {
      for (BasicBlock &B : F) {
        for (Instruction &I: B) {
          if (auto *CB = dyn_cast<CallBase>(&I)) {
            // We know we've encountered some kind of call instruction (call,
            // invoke, or callbr), so we need to determine if it's a call to
            // the function pointed to by m_func or not.
            if (CB->getCalledFunction() == targetFunc)
              ++callCounter;
          }
        }
      }
    }
  private:
    unsigned callCounter;
};

// Iterating over def-use & use-def chains
// The list of all Users of a particular Value is called a def-use chain.
// Finding all of the instructions that use foo -> iterating over the def-use chain of F.
Function *F = ...;
for (User *U : F->users()) {
  if (Instruction *Inst = dyn_cast<Instruction>(U)) {
    errs() << "F is used in instruction:\n";
    errs() << *Inst << "\n";
}
// The list of all Values used by a User is known as a use-def chain.
// iterate over all of the values that a particular instruction uses (that is, the operands)
Instruction *pi = ...;
for (Use &U : pi->operands()) {
  Value *v = U.get();
  // ...
}

// Iterating over predecessors & successors of blocks
#include "llvm/IR/CFG.h"
BasicBlock *BB = ...;
for (BasicBlock *Pred : predecessors(BB)) {
  // ...
}
// Similarly, to iterate over successors use successors.

