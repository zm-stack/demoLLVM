API接口
===
#include "llvm/IR/Type.h"
---
* bool isIntegerTy() const
* bool isFloatingPointTy()
* bool isSized(): Return true if the type has known size. Things that don’t have a size are abstract types, labels and void.
***
### IntegerType (integer types of any bit width)
+ static const IntegerType* get(unsigned NumBits)
+ unsigned getBitWidth() const
### SequentialType (subclassed by ArrayType and VectorType)
+ const Type * getElementType() const
+ uint64_t getNumElements() const
### ArrayTypee (for array types)
### PointerType (for pointer types)
### VectorType (for vector types)
### StructType (for struct types)
### FunctionType (for function types)
+ bool isVarArg() const
+ const Type * getReturnType() const
+ const Type * getParamType (unsigned i)
+ const unsigned getNumParams() const

#include "llvm/IR/Module.h"
---
Top level structure present in LLVM programs  
Keeps track of a list of Functions, a list of GlobalVariables, and a SymbolTable.
***
+ Module::Module(std::string name = "")
+ Module::iterator
+ Module::const_iterator
+ begin(), end(), size(), empty()
+ Module::FunctionListType &getFunctionList()
***
+ Module::global_iterator
+ Module::const_global_iterator
+ global_begin(), global_end(), global_size(), global_empty()
+ Module::GlobalListType &getGlobalList()
***
+ SymbolTable *getSymbolTable()
***
+ Function *getFunction(StringRef Name) const
+ FunctionCallee getOrInsertFunction(const std::string &Name, const FunctionType *T)
+ std::string getTypeName(const Type *Ty)
+ bool addTypeName(const std::string &Name, const Type *Ty)

#include "llvm/IR/Value.h"
----
The Value class keeps a list of all of the Users that is using it. (represents def-use information)  
The name of any value may be missing (an empty string), so names should ONLY be used for debugging.  
Any reference to the value produced by an instruction (or the value available as an incoming argument, for example) is represented as a direct pointer to the instance of the class that represents this value.
***
+ Value::use_iterator
+ Value::const_use_iterator
+ unsigned use_size()
+ bool use_empty()
+ use_iterator use_begin()/use_end()
+ User *use_back()
+ void replaceAllUsesWith(Value *V)
***
+ Type *getType() const
+ bool hasName() const
+ std::string getName() const
+ void setName(const std::string &Name)

#include "llvm/IR/User.h"
---
The User class itself is a subclass of Value.   
Exposes a list of “Operands” that are all of the Values that the User is referring to. (represents use-def information)
***
+ Value *getOperand(unsigned i)
+ unsigned getNumOperands()
+ User::op_iterator
+ op_iterator op_begin()/op_end()

#include "llvm/IR/Instruction.h"
---
Because the Instruction class subclasses the User class, its operands can be accessed in the same way as for other Users.  
The primary data tracked by the Instruction class itself is the opcode (instruction type) and the parent BasicBlock the Instruction is embedded into.  
An important file for the Instruction class is the llvm/Instruction.def file.  
***
### main subclasses
+ BinaryOperator: represents all two operand instructions whose operands must be the same type, except for the comparison instructions.
+ CastInst: the parent of the 12 casting instructions. It provides common operations on cast instructions.
+ CmpInst: represents the two comparison instructions, ICmpInst (integer operands), and FCmpInst (floating point operands).
### main public members
+ BasicBlock *getParent()
+ bool mayWriteToMemory()
+ unsigned getOpcode()
+ Instruction *clone() const

The Constant class
---
### main subclasses
+ ConstantInt
    + const APInt& getValue() const
    + int64_t getSExtValue() const
    + uint64_t getZExtValue() const
    + static ConstantInt* get(const APInt& Val)
    + static ConstantInt* get(const Type *Ty, uint64_t Val)
+ ConstantFP
    + double getValue() const
+ ConstantArray
    + const std::vector< Use> &getValues() const
+ ConstantStruct
    + const std::vector< Use> &getValues() const
+ GlobalValue  

#include "llvm/IR/GlobalValue.h"
---
Superclasses: Constant, User, Value  
The Type of a global is always a pointer to its contents. When using the GetElementPtrInst instruction  this pointer must be dereferenced first.  
***
+ bool hasInternalLinkage()/hasExternalLinkage() const
+ void setInternalLinkage(bool HasInternalLinkage)
+ Module *getParent()  

#include "llvm/IR/Function.h"
---
Superclasses: GlobalValue, Constant, User, Value
The Function class keeps track of a list of BasicBlocks, a list of formal Arguments, and a SymbolTable.  
The first BasicBlock is the implicit entry node for the Function. It is not legal in LLVM to explicitly branch to this initial block.  
The SymbolTable is used when you have to look up a value by name, or used internally to make sure that there are not conflicts between the names of Instructions, BasicBlocks, or Arguments in the function body.
***
+ Function(const FunctionType \*Ty, LinkageTypes Linkage, const std::string &N = "", Module* Parent = 0)
+ bool isDeclaration()
+ Function::iterator
+ Function::const_iterator
+ begin(), end(), size(), empty()
+ Function::BasicBlockListType &getBasicBlockList()
***
+ Function::arg_iterator
+ Function::const_arg_iterator
+ arg_begin(), arg_end(), arg_size(), arg_empty()
+ Function::ArgumentListType &getArgumentList()
+ BasicBlock &getEntryBlock()
+ Type *getReturnType()
+ FunctionType *getFunctionType()
+ SymbolTable *getSymbolTable()  
  
#include "llvm/IR/GlobalVariable.h"
---
Superclasses: GlobalValue, Constant, User, Value
***
+ GlobalVariable(const Type \*Ty, bool isConstant, LinkageTypes &Linkage, Constant \*Initializer = 0, const std::string &Name = "", Module* Parent = 0)
+ bool isConstant() const
+ bool hasInitializer()
+ Constant *getInitializer()  
  
#include "llvm/IR/BasicBlock.h"
---
Superclass: Value  
This class represents a single entry single exit section of the code, and maintains a list of Instructions, which form the body of the block. 
***
+ BasicBlock(const std::string &Name = "", Function *Parent = 0)
+ BasicBlock::iterator
+ BasicBlock::const_iterator
+ begin(), end(), front(), back(), size(), empty()
+ BasicBlock::InstListType &getInstList()
+ Function *getParent()
+ Instruction *getTerminator()  
  
#include "llvm/IR/BasicBlock.h"
---
Subclass of Value  
A Function maintains a list of its formal arguments.  
An argument has a pointer to the parent Function.