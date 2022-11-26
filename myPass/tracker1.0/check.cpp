/*
*   检测链码中的隐私泄露
*   主要步骤如下
*   1. 定位API调用语句（getelementptr参数）
*   2. 同一block中标记返回结果(llvm.memcpy参数) // 污点需要更加精确提前
*   3. 执行污点传播
*   4. 检测传播情况（不能判断，不能传入其他函数） 
*/

// fix！！！！！！
// 污点应当更加提前切精确：距离getprivate最接近的call的第一个参数%sret.actual.2
// 指令处理包括memcopy，从参数到参数的污点传递
// 函数见的污点传播

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
#define MAX_CALL_DEEP (10)	    // 最大函数调用深度

#define N_VS  (1)   // 未被污染的值变量 	
#define N_PS  (2)   // 未被污染的指针变量	 	
#define VS    (3)	// 被污染的值变量
#define PS    (4)   // 被污染的指针变量
#define NF    (0)  // 不存在此变量

//记录function的所有信息
struct funVal
{
	BasicBlock *BB[MAX_BB];	            // basicblock数组
	Value *val[MAX_INST];			    // value数组
	unsigned char valType[MAX_INST];    // value数组对应的污点类型
	unsigned char RetType;              // function返回值污点类型			
	int valNum;					        // 指令数
	int argNum;					        // 参数个数
	int gloNum;				            // 全局变量数
};


// Pass要声明在llvm命名空间内
namespace {
    struct checker : public FunctionPass {
        static char ID;
        checker() : FunctionPass(ID) {}

        funVal main, sub;       // 分别用于记录主函数、子函数信息
        ushort subdeep;         // 记录子函数调用深度

        // 初始化funVal数据成员
        void Init(funVal *f)
        {
            f->valNum = 0;
            f->argNum = 0;
            f->gloNum = 0;
            for (unsigned int i = 0; i < MAX_INST; i++) {
                f->val[i] = NULL;
                f->valType[i] = NF;
            }
            f->RetType = NF;
        }

        // 获取每个函数的funVal信息
        void Collect(Function *F, funVal *f)
        {
            // 收集全局变量并记录污点类型
            for (Module::global_iterator i = F->getParent()->global_begin(), e = F->getParent()->global_end(); i != e; ++i) {
                f->val[f->valNum] = &*i;
                // errs() << *i << "\t";
                f->valType[f->valNum] = N_PS;
                // errs() << "全局变量" << "\n";
                f->valNum++;
            }
            f->gloNum = f->valNum - f->argNum;

            // 收集function中的所有变量并记录污点类型
            for (BasicBlock &B : *F){
                for (Instruction &I: B){
                    f->val[f->valNum] = &I;
                    // errs() << I << "\t";
                    if (I.getType()->isPointerTy()) {
                        f->valType[f->valNum] = N_PS;
                        // errs() << "指针变量" << "\n";
                    } else {
                        f->valType[f->valNum] = N_VS;
                        // errs() << "值变量" << "\n";
                    }
                    f->valNum++;
                }
            }
        }

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
                                // for(unsigned int i = 0; i < f->valNum; i++) {
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
                                // for(unsigned int i = 0; i < f->valNum; i++) {
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
			for (index = 0; index < f->valNum; index++) {
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
            for (int i = 0; i < f->valNum; i++) {
                Value *v = f->val[i];

                if (f->valType[i] == PS || f->valType[i] == VS) {
                    // 遍历所有用到该value的指令
                    for (User *u : v->users()) {
                        // 由于funVal中包含全局变量，因此需要过滤掉其中的ConstantExpr
                        if (UserToInst(u)) {
                            Instruction *Inst = dyn_cast<Instruction>(UserToInst(u));
                            int index = FindVal(Inst, f);
                            
                            // 对于生成新的值的指令，修改结果的污点类型
                            if (f->valType[index] == N_VS || f->valType[index] == N_PS) {
                                f->valType[index] = Inst->getType()->isPointerTy()? PS : VS;
                                changed = true;
                            }

                            // 对于store指令，如果value是污点，则修改地址为污点
                            if (Inst->getOpcode() == Instruction::Store && f->valType[i] == VS) {
                                Instruction *ptr_arg = dyn_cast<Instruction>(Inst->getOperand(1));
                                int arg = FindVal(ptr_arg, f);
                                if (f->valType[arg] == N_PS) {
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
            for (int i = 0; i < f->valNum; i++) {
                errs() << "值：" << *f->val[i] << "\t" << "类型：" << Print_Type(f->valType[i]) << "\n";
            }
        }

        StringRef Print_Type(int i)
		{
			StringRef type;
            if (i == N_PS)
				type = "指针";
			else if (i == N_VS)
				type = "值";
			else if (i == PS)
				type = "污点指针";
			else if (i == VS)
				type = "污点值";
            return type;
		}

        bool runOnFunction(Function &F) override {

            Init(&main);
            Collect(&F, &main);
            Stain(&F, &main);
            // while(UpdateFun(&F, &main)) {
            //     ;
            // }

            // PrintStain(&main);
            return false;
        }

        



    }; // end of struct Hello
}  // end of anonymous namespace

char checker::ID = 0;

// Register for opt
static RegisterPass<checker> X("check", "chaincode checker",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);