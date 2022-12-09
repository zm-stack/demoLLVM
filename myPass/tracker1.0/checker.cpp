/*
*  检测链码中的隐私泄露，参见read.me
*  污点源：
*  1. 定位API调用语
*       - getelementptr指令，参数格式"i8* %stub.chunk0, i64 96"
*  2. 将返回结果标记为污点
*       - 同一个block最近的call语句的参数，记录为S
*       - 针对S的getelementptr的i64 0, i32 0的访问，记录为污点
*  3. 执行污点传播
*       - 可能会有指令需要特殊处理
*  4. 检测逻辑
*       1.1 存在PutPrivateData(33)不存在GetTransient(28)
*       1.2 存在更新API，污点影响到分支语句
*       1.3 存在更新API，污点影响到shim.error或shim.success
*       2.1 污点影响到全局变量
*       2.2 污点影响到InvokeChaincode(31)
*/ 

#include "llvm/Pass.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/StringRef.h"

#define MAX_BB (1 << 10)        // function中最大basicblock数
#define MAX_INST (1 << 20)      // function中最大instruction数
#define MAX_GLOBAL (1 << 20)    // module中最大global_variable数

#define V_NT  (1)   // 未被污染的值变量 	
#define P_NT  (2)   // 未被污染的指针变量	 	
#define VAR   (3)	// 被污染的值变量
#define PTR   (4)   // 被污染的指针变量
#define N_F   (0)   // 不存在此变量

// Pass要声明在llvm命名空间内
using namespace llvm;

//记录function的所有信息
struct funVal
{
	BasicBlock *BB[MAX_BB];	            // 基本块数组
	Value *ins[MAX_INST];			    // 指令数组
	unsigned char insType[MAX_INST];    // 指令元素污点类型		
	int InsNum;					        // 指令数
	int GloNum;				            // 全局变量数
};

namespace {
    struct checker : public ModulePass {
        
        static char ID;
        checker() : ModulePass(ID) {}

        // 初始化Invoke函数的数据成员
        void Init(funVal *f)
        {
            f->InsNum = 0;
            f->GloNum = 0;
            for (unsigned int i = 0; i < MAX_INST; i++){
                f->ins[i] = NULL;
                f->insType[i] = N_F;
            }
        }

        // 从module中获取全局变量
        void GetGlobal(Module *M, funVal *f)
        {
            for (auto i = M->global_begin(); i != M->global_end(); ++i){
                GlobalVariable *gv = &*i;
            }

        }

        // 从function中获取所有指令
        void GetInstruction(Function *F, funVal *f)
        {

        }

        // detect FPL1.1
        void FPL11(Function *F){
            StringRef const shim = "i8* %stub.chunk0";
            StringRef const get_private = "i32 12";
            StringRef const get_transient = "i32 28";
            StringRef const put_private = "i32 33";
            bool is_shim = 0, has_put_private = 0, has_get_transient = 0;

            for (BasicBlock &B : *F) {
                for (Instruction &I: B) {
                    if (I.getOpcode() == Instruction :: GetElementPtr) {
                        Value *arg = I.getOperand(I.getNumOperands() - 1);
                        std::string api;
                        raw_string_ostream(api) << *arg;
                        if (api == get_private) {
                            errs()<< "find privacy read: " << I << "\n";
                        }
                    }
                }
            }
            // if (has_put_private && !has_get_transient){
            //     return true;
            // } else {
            //     return false;
            // }
        }

        bool runOnModule(Module &M) {
            bool isChaincode = false;
            for (Function &F:M) {
                if (F.getName().contains("getPrivate")) {
                    isChaincode = true;
                    errs() << "------Detection start------\n";
                    FPL11(&F);
                }
                // else {
                //     errs() << "FPL1.1 detected in function: \t";
                //     errs() << F.getName() << "\n";
                //     errs() << "please get private argument via getTransient \n";

                // }
            }
            if (!isChaincode) { 
                errs() << "------Detection end, Invoke function not found------\n";
            }
            return false;
        }
    }; // end of struct Hello
}  // end of anonymous namespace

char checker::ID = 0;

// Register for opt
static RegisterPass<checker> X("checker", "chaincode checker",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);