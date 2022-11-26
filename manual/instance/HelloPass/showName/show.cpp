//https://www.leadroyal.cn/p/719/

#include <string>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

#include "ShowName.h"

using namespace std;
using namespace llvm;

/*
 * 简单的用来练手的，在每个函数被调用时，先打印函数名，分别在栈、data、rodata 存放函数名，测试用的。
 * */
namespace llvm {
    class ShowName : public FunctionPass {
    public:
        static char ID;

        ShowName() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override;

        virtual StringRef getPassName() const override {
            return "Show Name";
        }
    };

    Pass *createShowName() {
        return new ShowName();
    }

    char ShowName::ID = 0;
}

void declarePrintf(Function &F);

void insertRodata(Function &F);

void insertData(Function &F);

void insertStack(Function &F);

bool ShowName::runOnFunction(Function &F) {
    errs() << "enter ShowName " << F.getName() << "n";
    declarePrintf(F);
    insertRodata(F);
    insertData(F);
    insertStack(F);
    return true;
}

void declarePrintf(Function &F) {
    Function *func_printf = F.getParent()->getFunction("printf");
    if (!func_printf) {
        FunctionType *FT = FunctionType::get(Type::getInt8PtrTy(F.getContext()), true);
        Function::Create(FT, Function::ExternalLinkage, "printf", F.getParent());
    }
}

void insertRodata(Function &F) {
    // get entry block
    BasicBlock *entryBlock = &F.getEntryBlock();
    // insert printfBlock before entry block
    BasicBlock *printfBlock = BasicBlock::Create(F.getContext(), "printfRodataBlock", &F, entryBlock);
    // create function_string
    std::string function_string = "Function ";
    function_string += F.getName();
    function_string += " is invoked! @rodatan";
    // insert at end of printfBlock
    IRBuilder<> IRB(printfBlock);
    // find printf
    Function *func_printf = F.getParent()->getFunction("printf");
    if (!func_printf)
        assert(false && "printf not found");
    // create string ptr
    Value *str = IRB.CreateGlobalStringPtr(function_string);
    // create printf(funcName)
    IRB.CreateCall(func_printf, {str});
    // create branch to entryBlock
    IRB.CreateBr(entryBlock);
}

void insertData(Function &F) {
    // get entry block
    BasicBlock *entryBlock = &F.getEntryBlock();
    // insert printfBlock before entry block
    BasicBlock *printfBlock = BasicBlock::Create(F.getContext(), "printfDataBlock", &F, entryBlock);
    // create function_string
    std::string function_string = "Function ";
    function_string += F.getName();
    function_string += " is invoked! @datan";
    // insert at end of printfBlock
    IRBuilder<> IRB(printfBlock);
    // find printf
    Function *func_printf = F.getParent()->getFunction("printf");
    if (!func_printf)
        assert(false && "printf not found");
    // create string ptr
    GlobalVariable *GV = IRB.CreateGlobalString(function_string);
    // set writable
    GV->setConstant(false);
    // i32 0
    Constant *Zero = ConstantInt::get(Type::getInt32Ty(F.getContext()), 0);
    // for index
    Constant *Indices[] = {Zero, Zero};
    // get ptr buffer[0]
    Value *str = ConstantExpr::getInBoundsGetElementPtr(GV->getValueType(), GV, Indices);
    // create printf(funcName)
    IRB.CreateCall(func_printf, {str});
    // create branch to entryBlock
    IRB.CreateBr(entryBlock);
}

void insertStack(Function &F) {
    // get entry block
    BasicBlock *entryBlock = &F.getEntryBlock();
    // insert printfBlock before entry block
    BasicBlock *printfBlock = BasicBlock::Create(F.getContext(), "printfStackBlock", &F, entryBlock);
    // create function_string
    std::string function_string = "Function ";
    function_string += F.getName();
    function_string += " is invoked! @stackn";
    // insert at end of printfBlock
    IRBuilder<> IRB(printfBlock);
    // find printf
    Function *func_printf = F.getParent()->getFunction("printf");
    if (!func_printf)
        assert(false && "printf not found");
    // i8 0
    Value *Zero = ConstantInt::get(Type::getInt8Ty(F.getContext()), 0);
    // i64 size
    Value *Size = ConstantInt::get(Type::getInt64Ty(F.getContext()), function_string.size() + 1);
    // i1 bool
    Value *Bool = ConstantInt::get(Type::getInt1Ty(F.getContext()), 0);
    // type: i8[size]
    ArrayType *arrayType = ArrayType::get(IntegerType::getInt8Ty(F.getContext()), function_string.size() + 1);
    // find llvm.memset.p0i8.i64
    Function *func_memset = Intrinsic::getDeclaration(F.getParent(), Intrinsic::memset, {IntegerType::getInt8PtrTy(F.getContext()), IntegerType::getInt64Ty(F.getContext())});
    // new i8[size]
    AllocaInst *alloc = IRB.CreateAlloca(arrayType);
    // align 16
    alloc->setAlignment(MaybeAlign(16));
    // get ptr buffer[0]
    Value *str = IRB.CreateGEP(alloc, {Zero, Zero});
    IRB.CreateCall(func_memset, {str, Zero, Size, Bool});

    for (int i = 0; i < function_string.size(); i++) {
        // i64 index
        Value *Index = ConstantInt::get(Type::getInt64Ty(F.getContext()), i);
        // i8 char
        Value *CDATA = ConstantInt::get(Type::getInt8Ty(F.getContext()), function_string[i]);
        // get ptr  bufferr[i]
        Value *GEP = IRB.CreateGEP(alloc, {Zero, Index});
        // store data
        IRB.CreateStore(CDATA, GEP);
    }
    // create printf(funcName)
    IRB.CreateCall(func_printf, {str});
    // create branch to entryBlock
    IRB.CreateBr(entryBlock);
}