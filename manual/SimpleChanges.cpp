// Instantiating Instructions & Naming values
auto *pa = new AllocaInst(Type::Int32Ty, 0, "indexLoc");
// Instruction subclass https://llvm.org/doxygen/classllvm_1_1Instruction.html
// indexLoc is the logical name of the instruction’s execution value, a pointer to an integer on the run time stack.

// Inserting instructions
// 1. Insertion into an explicit instruction list
// Inserts newInst before pi in pb
BasicBlock *pb = ...;
Instruction *pi = ...;
auto *newInst = new Instruction(...);
pb->getInstList().insert(pi, newInst); 
// Appends newInst to pb
BasicBlock *pb = ...;
auto *newInst = new Instruction(...);
pb->getInstList().push_back(newInst);

// 2. Insertion into an implicit instruction list
Instruction *pi = ...;
auto *newInst = new Instruction(...);
pi->getParent()->getInstList().insert(pi, newInst);
//Instruction constructors can insert the newly-created instance 
// into the BasicBlock of a provided instruction, immediately before that instruction.
Instruction* pi = ...;
auto *newInst = new Instruction(..., pi);

// 3. Insertion using an instance of IRBuilder
// The IRBuilder is a convenience class that can be used to add several instructions
// to the end of a BasicBlock or before a particular Instruction.
Instruction *pi = ...;      // BasicBlock *pb = ...;
IRBuilder<> Builder(pi);    // IRBuilder<> Builder(pb);
CallInst* callOne = Builder.CreateCall(...);
CallInst* callTwo = Builder.CreateCall(...);
Value* result = Builder.CreateMul(callOne, callTwo);

// Deleting Instructions or GlobalVariables
Instruction *I = .. ;   // GlobalVariable *GV = .. ;
I->eraseFromParent();   // GV->eraseFromParent();

// Replacing an Instruction with another Value
// Including “llvm/Transforms/Utils/BasicBlockUtils.h” permits use of
// ReplaceInstWithValue and ReplaceInstWithInst.
// The replacement of the result of a particular AllocaInst that allocates memory 
// for a single integer with a null pointer to an integer.
AllocaInst* instToReplace = ...;
BasicBlock::iterator ii(instToReplace);
ReplaceInstWithValue(instToReplace->getParent()->getInstList(), ii,
                     Constant::getNullValue(PointerType::getUnqual(Type::Int32Ty)));
// The replacement of one AllocaInst with another.
AllocaInst* instToReplace = ...;
BasicBlock::iterator ii(instToReplace);
ReplaceInstWithInst(instToReplace->getParent()->getInstList(), ii,
                    new AllocaInst(Type::Int32Ty, 0, "ptrToReplacedInt"));

// Replacing multiple uses of Users and Values
// use Value::replaceAllUsesWith and User::replaceUsesOfWith to change more than one use at a time.
// https://llvm.org/doxygen/classllvm_1_1Value.html https://llvm.org/doxygen/classllvm_1_1User.html

