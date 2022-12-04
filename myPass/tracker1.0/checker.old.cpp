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
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/StringRef.h"

using namespace llvm;

#define MAX_BB (1 << 10)        // function中最大basicblock数
#define MAX_INST (1 << 20)      // function中最大instruction数
#define MAX_GLOBAL (1 << 20)    // module中最大global_variable数

#define V_NT  (1)   // 未被污染的值变量 	
#define P_NT  (2)   // 未被污染的指针变量	 	
#define VAR   (3)	// 被污染的值变量
#define PTR   (4)   // 被污染的指针变量
#define N_F   (0)   // 不存在此变量

//记录function的所有信息
struct funVal
{
	BasicBlock *BB[MAX_BB];	            // 基本块数组
	Value *ins[MAX_INST];			    // 指令数组
	unsigned char insType[MAX_INST];    // 指令元素污点类型		
	int InsNum;					        // 指令数
	int GloNum;				            // 全局变量数
};


// Pass要声明在llvm命名空间内
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
        bool FPL11(Function *F){
            StringRef shim = "i8* %stub.chunk0";
            StringRef get_transient = "i32 28";
            StringRef put_private = "i32 33";
            bool is_shim = 0, has_put_private = 0, has_get_transient = 0;

            for (BasicBlock &B : *F) {
                for (Instruction &I: B) {
                    if (I.getOpcode() == Instruction :: GetElementPtr) {
                        for (Use &U : I.operands()) {
                            if (Value *v =U.get()) {
                                std::string arg;
                                raw_string_ostream(arg) << *v;
                                // 定位getPrrivateData
                                if (arg == shim) {
                                    is_shim = 1;
                                } else if (arg == get_transient) {
                                    has_get_transient = 1;
                                } else if (arg == put_private) {
                                    has_put_private = 1;
                                }
                            }
                        }
                    }
                    if (is_shim && is_private) {
                        // 通过llvm.memcpy定位getPrrivateData返回值
                        if (auto *CB = dyn_cast<CallBase>(&I)) {
                            // call指令的第一个参数
                            auto *bb = CB->operands().begin()->get();
                            auto *end = CB->operands().end();
                            // call指令的最后一个参数
                            auto *ee = (--end)->get();
                            if (ee->getName() == targetFunc) {
                                // errs()<< "find privacy call in block: " << B.getName() << "\n";
                                errs() << "found private leak in function: ";
                                errs().write_escaped(F->getName()) << "\n";
                                errs()<< "private data: " << bb->getName() << "\n";
                                is_shim = 0; is_private = 0;
                                // 修改该指针变量的污点类型
                                for(unsigned int i = 0; i < f->InsNum; i++) {
                                    if(f->val[i]->getName() == bb->getName()) {
                                        f->valType[i] = PS;
                                    }
                                }
                            }
                        }
                    } else if (faw == 1) {
                        // 通过llvm.memcpy定位getstate返回值
                        if (auto *CB = dyn_cast<CallBase>(&I)) {
                            // call指令的第一个参数
                            auto *bb = CB->operands().begin()->get();
                            auto *end = CB->operands().end();
                            // call指令的最后一个参数
                            auto *ee = (--end)->get();
                            if (ee->getName() == targetFunc) {
                                // errs()<< "find privacy call in block: " << B.getName() << "\n";
                                errs() << "found read after write in function: ";
                                errs().write_escaped(F->getName()) << "\n";
                                errs()<< "affected data: " << bb->getName() << "\n";
                                faw = 0;
                                // 修改该指针变量的污点类型
                                for(unsigned int i = 0; i < f->InsNum; i++) {
                                    if(f->val[i]->getName() == bb->getName()) {
                                        f->valType[i] = PS;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (has_put_private && !has_get_transient){
                return true;
            } else {
                return false;
            }
        }


           
            // // 收集全局变量并记录污点类型
            // for (Module::global_iterator i = F->getParent()->global_begin(), e = F->getParent()->global_end(); i != e; ++i) {
            //     f->val[f->InsNum] = &*i;
            //     // errs() << *i << "\t";
            //     f->valType[f->InsNum] = P_NT;
            //     // errs() << "全局变量" << "\n";
            //     f->InsNum++;
            // }
            // f->GloNum = f->InsNum - f->InsNum;

            // // 收集function中的所有变量并记录污点类型
            // for (BasicBlock &B : *F){
            //     for (Instruction &I: B){
            //         f->val[f->InsNum] = &I;
            //         // errs() << I << "\t";
            //         if (I.getType()->isPointerTy()) {
            //             f->valType[f->InsNum] = P_NT;
            //             // errs() << "指针变量" << "\n";
            //         } else {
            //             f->valType[f->InsNum] = V_NT;
            //             // errs() << "值变量" << "\n";
            //         }
            //         f->InsNum++;
            //     }
            // }
        

        // 标记污点
        void Stain(Function *F, funVal *f)
        {
            /*
                定位privacy leak
            */
            StringRef shim = "i8* %stub.chunk0";
            StringRef get_private = "i64 96";
            StringRef query = "i64 80";
            StringRef get_state = "i64 168";
            StringRef put_state = "i64 264";
            StringRef targetFunc = "llvm.memcpy.p0i8.p0i8.i64";
            bool is_shim = 0, is_private = 0, faw = 0, is_put = 0;
            // errs() << "---------------locate private API call in ";
            // 遍历function
            // errs() << "Function name: ";
            // errs().write_escaped(F->getName()) << "---------------\n";
            for (BasicBlock &B : *F) {
                // 遍历block
                for (Instruction &I: B) {
                    if (I.getOpcode() == Instruction :: GetElementPtr) {
                        for (Use &U : I.operands()) {
                            if (Value *v =U.get()) {
                                std::string arg;
                                raw_string_ostream(arg) << *v;
                                // 定位getPrrivateData
                                if (arg == shim) {
                                    is_shim = 1;
                                } else if (arg == get_private) {
                                    is_private = 1;
                                } else if (arg == put_state) {
                                    is_put = 1;
                                } else if (arg == get_state &&  is_put == 1) {
                                    faw = 1;
                                }
                            }
                        }
                    }
                    if (is_shim && is_private) {
                        // 通过llvm.memcpy定位getPrrivateData返回值
                        if (auto *CB = dyn_cast<CallBase>(&I)) {
                            // call指令的第一个参数
                            auto *bb = CB->operands().begin()->get();
                            auto *end = CB->operands().end();
                            // call指令的最后一个参数
                            auto *ee = (--end)->get();
                            if (ee->getName() == targetFunc) {
                                // errs()<< "find privacy call in block: " << B.getName() << "\n";
                                errs() << "found private leak in function: ";
                                errs().write_escaped(F->getName()) << "\n";
                                errs()<< "private data: " << bb->getName() << "\n";
                                is_shim = 0; is_private = 0;
                                // 修改该指针变量的污点类型
                                // for(unsigned int i = 0; i < f->InsNum; i++) {
                                //     if(f->val[i]->getName() == bb->getName()) {
                                //         f->valType[i] = PS;
                                //     }
                                // }
                            }
                        }
                    } else if (faw == 1) {
                        // 通过llvm.memcpy定位getstate返回值
                        if (auto *CB = dyn_cast<CallBase>(&I)) {
                            // call指令的第一个参数
                            auto *bb = CB->operands().begin()->get();
                            auto *end = CB->operands().end();
                            // call指令的最后一个参数
                            auto *ee = (--end)->get();
                            if (ee->getName() == targetFunc) {
                                // errs()<< "find privacy call in block: " << B.getName() << "\n";
                                errs() << "found read after write in function: ";
                                errs().write_escaped(F->getName()) << "\n";
                                errs()<< "affected data: " << bb->getName() << "\n";
                                faw = 0;
                                // 修改该指针变量的污点类型
                                // for(unsigned int i = 0; i < f->InsNum; i++) {
                                //     if(f->val[i]->getName() == bb->getName()) {
                                //         f->valType[i] = PS;
                                //     }
                                // }
                            }
                        }
                    }
                }
            }
        }
        int FindVal(Value *v, funVal *f)
		{
			int index;
			for (index = 0; index < f->InsNum; index++) {
				if (f->val[index] == v)
					return index;
			}
			return -1;
		}

		Value *UserToInst(Value *v)
		{
			if (dyn_cast<Instruction>(v)) {
                return v;
            } else if (dyn_cast<ConstantExpr>(v)) {
				for (User *u : v->users()) {
					UserToInst(u);
				}
			}
			return NULL;
		}

        // function内污点传播
        bool UpdateFun(Function *F, funVal *f)
        {
            bool changed = false;

            // 遍历函数内所有value
            for (int i = 0; i < f->InsNum; i++) {
                Value *v = f->val[i];

                if (f->valType[i] == PS || f->valType[i] == VS) {
                    // 遍历所有用到该value的指令
                    for (User *u : v->users()) {
                        // 由于funVal中包含全局变量，因此需要过滤掉其中的ConstantExpr
                        if (UserToInst(u)) {
                            Instruction *Inst = dyn_cast<Instruction>(UserToInst(u));
                            int index = FindVal(Inst, f);
                            
                            // 对于生成新的值的指令，修改结果的污点类型
                            if (f->valType[index] == V_NT || f->valType[index] == P_NT) {
                                f->valType[index] = Inst->getType()->isPointerTy()? PS : VS;
                                changed = true;
                            }

                            // 对于store指令，如果value是污点，则修改地址为污点
                            if (Inst->getOpcode() == Instruction::Store && f->valType[i] == VS) {
                                Instruction *ptr_arg = dyn_cast<Instruction>(Inst->getOperand(1));
                                int arg = FindVal(ptr_arg, f);
                                if (f->valType[arg] == P_NT) {
                                    f->valType[arg] = PS;
                                    changed = true;
                                }
                            }

                            // 对于ret指令，如果参数是污点，则RetType是污点
                            if (Inst->getOpcode() == Instruction::Store) {
                                f->RetType = f->valType[i] == PS? PS : VS;
                                changed = true;
                            }
                        }              
                    }
                }
            }
            return changed;
        }

        // bool UpdateMod(Function *F, funVal *f)
        // {
        // }

        // 检测漏洞
        void Check()
        {

        }

        void PrintStain(funVal *f)
        {
            for (int i = 0; i < f->InsNum; i++) {
                errs() << "值：" << *f->val[i] << "\t" << "类型：" << Print_Type(f->valType[i]) << "\n";
            }
        }

        StringRef Print_Type(int i)
		{
			StringRef type;
            if (i == P_NT)
				type = "指针";
			else if (i == V_NT)
				type = "值";
			else if (i == PS)
				type = "污点指针";
			else if (i == VS)
				type = "污点值";
            return type;
		}

        bool runOnModule(Module &M) {
            bool isChaincode = false;
            for (Function &F:M) {
                if (F.getName().contains("Invoke")) {
                    isChaincode = true;
                    errs() << "------Detection start------\n";
                }
                if (FPL11(&F)){
                    errs() << "FPL1.1 detected in function: \t";
                    errs() << F.getName() << "\n";
                    errs() << "please get private argument via getTransient \n";

                }
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