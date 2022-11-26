#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include <map>

using namespace llvm;
#define MAX_BASICBLOCK (1 << 10) 	// 一个function中最大的basicblock数
#define MAX_VAL_Fun (1 << 20)	 	// 一个function中最大的指令数
#define MAX_VAL_GLOBAL (1 << 20) 	// module中最大全局变量数
#define MAX_SUB_FUN_DEEP (10)		// 最大函数调用深度

#define No_state (1) 	// 未被污染的值变量
#define G_ROM_N (2)	 	// 未污染指针变量
#define G_ROM_S (3)	 	// 被污染指针变量
#define State (4)	 	// 被污染的值变量 
#define VAL_Not_Found (-1)	// 不存在此变量

char print_flg;
//记录function的所有信息
struct funvalst
{
	void *Function_Basics[MAX_BASICBLOCK];	// 指向funtion中basicblock的指针数组
	Value *FunInst[MAX_VAL_Fun];			// 指向function中指令的指针数组
	unsigned char FunInstVal[MAX_VAL_Fun];	// 记录function中指令的污点类型
	unsigned char RetType;					// ？？？
	int functionval_num;					// 指令数
	int functionarg_num;					// 参数个数
	int functionglo_num;					// 全局变量数
};

namespace
{	// SBOX - The first implementation, without getAnalysisUsage.
	struct stain : public FunctionPass
	{
		static char ID;
		funvalst mainst;				   //用于记录主函数指令信息
		funvalst subfst[MAX_SUB_FUN_DEEP]; //用于记录子函数指令信息
		int subdeep;					   //子函数调用深度
		stain() : FunctionPass(ID) {}

		//初始化funvalst实例的数据成员
		void Clean_st(funvalst *fst)
		{
			fst->functionval_num = 0;
			fst->functionarg_num = 0;
			fst->functionglo_num = 0;
			for (unsigned int i = 0; i < MAX_VAL_Fun; i++)
				fst->FunInst[i] = NULL;
			for (unsigned int i = 0; i < MAX_VAL_Fun; i++)
				fst->FunInstVal[i] = No_state;
			fst->RetType = No_state;
		}

		void Stain_Set(Function *F, funvalst *fst)
		{
			Argument *arg;

			for (Function::arg_iterator i = F->arg_begin(), e = F->arg_end(); i != e; ++i)
			{
				arg = &*i;
				fst->FunInst[fst->functionval_num] = arg;
				//if(i->getType()->getTypeID() != Type::PointerTyID)fst->FunInstVal[fst->functionval_num] = State;
				//else fst->FunInstVal[fst->functionval_num] = State;//which don't know the data struct
				if (print_flg)
					errs() << "arg: %" << fst->functionval_num << "\n";
				fst->functionval_num++;
			}
			fst->FunInstVal[0] = G_ROM_S;
			fst->functionarg_num = fst->functionval_num;
			if (print_flg)
				errs() << "------------------------------------------\n";
		}

		// 判断g是否为全局变量，也有可能是常量
		// 初始化结构中每个元素具有相同的位宽
		bool IS_ROM(GlobalVariable *g)
		{
			int l = 0;
			unsigned int witwidth;
			Constant *CT;
			// Constant *getInitializer(): Returns the initial value for a GlobalVariable
			if (CT = g->getInitializer()->getAggregateElement(l))
				l++;
			else
				return 0;
			if (dyn_cast<ConstantInt>(CT))
				witwidth = dyn_cast<ConstantInt>(CT)->getBitWidth();
			else
				return 0;
			l++;
			while (CT = g->getInitializer()->getAggregateElement(l))
			{
				if (dyn_cast<ConstantInt>(CT))
				{
					if (witwidth != dyn_cast<ConstantInt>(CT)->getBitWidth())
						return 0;
				}
				else
					return 0;
				l++;
			}
			return 1;
		}

		//获取module中所有全局变量并对被污染情况进行初始化
		void Find_All_GloabalVariable(Module *M, funvalst *fst)
		{
			int l = 0;
			for (Module::global_iterator i = M->global_begin(), e = M->global_end(); i != e; ++i)
			{
				// 将全局变量顺序加入FunInst数组
				errs() << fst->functionval_num;
				fst->FunInst[fst->functionval_num] = &*i;
				l = 0;
				if (print_flg)
					errs() << i->getName() << " ";
				while (i->getInitializer()->getAggregateElement(l))
					l++;
				if (l == 0)
					l = 1; //have only single value;
				errs() << "1";
				// 若该全局变量是变量类型，则标记为G_ROM_N
				if (IS_ROM(&*i))
				{
					errs() << "2";
					Constant *CT = i->getInitializer()->getAggregateElement(l - 1);
					fst->FunInstVal[fst->functionval_num] = G_ROM_N;
					if (print_flg && dyn_cast<ConstantInt>(CT))
						errs() << " " << dyn_cast<ConstantInt>(CT)->getBitWidth() / 8 << "Byte X "
							   << "[" << l << "] G_ROM\n";
				}
				// 若该全局变量不是变量类型（常量），则标记为No_state
				else
				{
					errs() << "3";
					fst->FunInstVal[fst->functionval_num] = No_state;
					if (print_flg && dyn_cast<ConstantInt>(i->getInitializer()))
						errs() << dyn_cast<ConstantInt>(i->getInitializer())->getBitWidth() / 8 << "Byte X "
							   << "[" << l << "] State\n";
				}
				errs() << "4";
				fst->functionval_num++;
			}
			fst->functionglo_num = fst->functionval_num - fst->functionarg_num;
			if (print_flg)
				errs() << "------------------------------------------\n";
		}

		// 遍历basicblock中指令并初始化其污点类型
		// 除了store等（user为0的）的指令
		void Serch_Blocks(BasicBlock *BB_c, funvalst *fst)
		{
			Instruction *Inst;
			int i;
			for (BasicBlock::iterator i2 = BB_c->begin(), e2 = BB_c->end(); i2 != e2; ++i2)
			{
				Inst = &*i2;
				// store和br指令并没有value实例，will not be used
				// 推测Inst这里指的就是指令的执行结果或者返回值
				if (Inst->hasNUsesOrMore(1))
				{                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
					if (Inst->getOpcode() == Instruction::Alloca)
					{
						fst->FunInst[fst->functionval_num] = Inst;
						fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						fst->functionval_num++;
					}
					else
					{
						fst->FunInst[fst->functionval_num] = Inst;
						if (Inst->getType()->isPointerTy())
							fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						else
							fst->FunInstVal[fst->functionval_num] = No_state;
						fst->functionval_num++;
					}
				}
				else if (Inst->getOpcode() == Instruction::Ret)
				{
					// 如果返回值非空
					if (Inst->getNumOperands())
					{
						fst->FunInst[fst->functionval_num] = Inst;
						if (Inst->getOperand(0)->getType()->isPointerTy())
							fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						else
							fst->FunInstVal[fst->functionval_num] = No_state;
						fst->functionval_num++;
					}
					else
					{
						fst->FunInst[fst->functionval_num] = Inst;
						fst->FunInstVal[fst->functionval_num] = No_state;
						fst->functionval_num++;
					}
				}
			}
		}

		// 遍历function中的所有basicblock，调用Serch_Blocks初始化其污点类型
		void Find_All_FunctionVal(Function *F, funvalst *fst)
		{
			BasicBlock *BB_c;
			for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i)
			{
				BB_c = &*i;
				Serch_Blocks(BB_c, fst);
			}
		}

		// 返回所有使用v的指令
		Value *Find_Used_Inst(Value *v)
		{
			for (User *u : v->users())
			{
				if (dyn_cast<Instruction>(u))
					return u;
				// ConstantExpr: 由使用v初始化的常量值。
				// https://lists.llvm.org/pipermail/llvm-dev/2013-April/060949.html
				else if (dyn_cast<ConstantExpr>(u))
					Find_Used_Inst(u);
			}
			return NULL;
		}

		// 确保v（如果是常量表达式则继续追溯def-use）是Instruction类型
		Value *Used_to_Inst(Value *v)
		{
			if (dyn_cast<Instruction>(v))
				return v;
			if (dyn_cast<ConstantExpr>(v))
			{
				for (User *u : v->users())
				{
					if (dyn_cast<Instruction>(u))
						return u;
					else if (dyn_cast<ConstantExpr>(u))
						Find_Used_Inst(u);
				}
			}
			return NULL;
		}

		// 找出指定value在functionval_num中的序号
		int Find_Val(Value *v, funvalst *fst)
		{
			int i;
			for (i = 0; i < fst->functionval_num; i++)
			{
				if (fst->FunInst[i] == v)
					return i;
			}
			return -1;
		}

		// 找出指定value的污点类型
		unsigned char Find_Val_Type(Value *u, funvalst *fst)
		{
			int i;
			for (i = 0; i < fst->functionval_num; i++)
			{
				if (fst->FunInst[i] == u)
					return fst->FunInstVal[i];
			}
			return VAL_Not_Found;
		}

		// 遍历函数参数，污染了一个指针类型的参数变量
		void Find_All_FunctionArg(Function *F, funvalst *fst)
		{
			Argument *arg;
			unsigned int l;
			//find all of the global variable
			for (Function::arg_iterator i = F->arg_begin(), e = F->arg_end(); i != e; ++i)
			{
				arg = &*i;
				fst->FunInst[fst->functionval_num] = arg;
				if (i->getType()->getTypeID() != Type::PointerTyID)
					fst->FunInstVal[fst->functionval_num] = State;
				else
					fst->FunInstVal[fst->functionval_num] = State; //which don't know the data struct
				if (print_flg)
					errs() << "arg: %" << fst->functionval_num << "\n";
				fst->functionval_num++;
			}
			fst->functionarg_num = fst->functionval_num;
			if (print_flg)
				errs() << "------------------------------------------\n";
		}

		void Print_Type(int i)
		{
			if (i == No_state)
				errs() << "NS";
			else if (i == G_ROM_N)
				errs() << "GRN";
			else if (i == G_ROM_S)
				errs() << "GRS";
			else if (i == State)
				errs() << "ST";
		}

		// 输出函数内所有指令及污点类型
		void Print_Function(Function *F, funvalst *fst)
		{
			unsigned int br_num, op_num, rom_num;
			br_num = op_num = rom_num = 0;
			errs() << "Function " << F->getName() << '\n';
			BasicBlock *BB_c;
			int j;
			// functionarg按照参数，全局，局部变量的顺序计数
			errs() << "arg\n";
			for (int valindex = 0; valindex < fst->functionarg_num; valindex++)
			{
				errs() << "[" << valindex << "]";
				Print_Type(fst->FunInstVal[valindex]);
				errs() << "\n";
			}
			errs() << "glo\n";
			for (int valindex = fst->functionarg_num; valindex < fst->functionglo_num; valindex++)
			{
				errs() << "[" << valindex << "]";
				Print_Type(fst->FunInstVal[valindex]);
				errs() << "\n";
			}
			errs() << "body\n";
			for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i)
			{
				BB_c = &*i;
				Instruction *Inst;
				for (BasicBlock::iterator i2 = BB_c->begin(), e2 = BB_c->end(); i2 != e2; ++i2) 
				{
					char block_split = 0;
					Inst = &*i2;
					errs() << Inst->getOpcodeName();
					
					// 如果是call指令，则输出跳转函数名称（最后一个参数）
					if (Inst->getOpcode() == llvm::Instruction::Call)
					{
						if (dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands() - 1)))
							errs() << " Function " << Inst->getNumOperands() << " ";
					}

					// 如果是br指令且指令参数为污点类型，则说明存在风险
					if (llvm::Instruction::Br == Inst->getOpcode() && 
						Find_Val(Inst->getOperand(0), fst) != VAL_Not_Found && 
						((fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] == State) || 
						(fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] == G_ROM_S)))
					{
						errs() << " [Br time atack may be! ] <" << br_num << ">";
						br_num++;
					}

					// 如果是一般指令则输出指令名称和污点类型，以及指令参数（显示为序号）的污点类型
					// 若指令参数并未被标记为污点或非污点，则显示为常量（CT）
					if (Find_Val(Inst, fst) != VAL_Not_Found)
					{
						errs() << " ( " << Find_Val(Inst, fst) << " ";
						Print_Type((int)fst->FunInstVal[Find_Val(Inst, fst)]);
						errs() << " " << (int)Inst->getType()->getTypeID() << " ) ";
					}

					for (int j = 0; j < Inst->getNumOperands(); j++)
					{
						if (Find_Val(Inst->getOperand(j), fst) != VAL_Not_Found)
						{
							errs() << " [ " << Find_Val(Inst->getOperand(j), fst) << " ";
							Print_Type((int)fst->FunInstVal[Find_Val(Inst->getOperand(j), fst)]);
							errs() << " ] ";
						}
						else
							errs() << " [ CT ] ";
					}
					errs() << "\n";
					if (block_split == 1)
						break;
				}
				errs() << "block end\n";
			}
			errs() << "Function " << F->getName() << " br attack :" << br_num << '\n';
		}

		int Update_Val(Function *F, funvalst *fst)
		{
			int i;
			int change = 0;
			Instruction *FInst;
			ConstantExpr *CE;
			Value *v;
			Instruction *Inst;
			Instruction *SI;

			// 遍历functionval中每一个指令的使用者
			for (i = 0; i < fst->functionval_num; i++)
			{
				v = fst->FunInst[i];
				for (User *u : fst->FunInst[i]->users())
				{
					if (Used_to_Inst(u))
					{
						Inst = (Instruction *)Used_to_Inst(u);
						// 这里只处理函数内传播
						if (Inst->getParent()->getParent() == F)
						{
							// load指令，如果当前指令i是污点，那么所有使用者是污点
							if (Inst->getOpcode() == llvm::Instruction::Load)
							{
								if (Find_Val(Inst, fst) != VAL_Not_Found)
								{
									// 如果i是load的指针参数
									if (fst->FunInstVal[i] == G_ROM_S)
									{
										if (fst->FunInstVal[Find_Val(Inst, fst)] == No_state)
										{
											fst->FunInstVal[Find_Val(Inst, fst)] = State;
											change++;
										}
										else if (fst->FunInstVal[Find_Val(Inst, fst)] == G_ROM_N)
										{
											fst->FunInstVal[Find_Val(Inst, fst)] = G_ROM_S;
											change++;
										}
									}
									// 如果i是load的值参数，则被认为和内存污染情况相关
									else if (fst->FunInstVal[i] == State)
									{
										fst->FunInstVal[i] = G_ROM_S;
										change++;
									}
									else if (fst->FunInstVal[i] == No_state)
									{
										fst->FunInstVal[i] = G_ROM_N;
										change++;
									}
								}
							}
							// store指令，如果当前指令i是污点，那么所有使用者是污点
							else if (Inst->getOpcode() == llvm::Instruction::Store)
							{
								int target_index = VAL_Not_Found;
								// 如果是写入已被统计过的指针变量的地址，target_index为对应的序号
								if (Find_Val(Inst->getOperand(1), fst) != VAL_Not_Found)
									target_index = Find_Val(Inst->getOperand(1), fst);
								else
								{
									if (dyn_cast<ConstantExpr>(Inst->getOperand(1)))
									{
										CE = dyn_cast<ConstantExpr>(Inst->getOperand(1));
										SI = CE->getAsInstruction();
										for (int jj = 0; jj < SI->getNumOperands(); jj++)
										{
											if (Find_Val(SI->getOperand(jj), fst) != VAL_Not_Found)
											{
												target_index = Find_Val(SI->getOperand(jj), fst);
												break;
											}
										}
										SI->deleteValue();
									}
								}

								if (target_index != VAL_Not_Found)
								{
									if (Find_Val(Inst->getOperand(0), fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] == G_ROM_S)
									{
										if (fst->FunInstVal[target_index] != G_ROM_S)
										{
											fst->FunInstVal[target_index] = G_ROM_S;
											//errs()<<"3\n";
											change++;
										}
									}
									else if (Find_Val(Inst->getOperand(0), fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] == State)
									{
										if (fst->FunInstVal[target_index] != G_ROM_S)
										{
											fst->FunInstVal[target_index] = G_ROM_S;
											//errs()<<"4\n";
											change++;
										}
									}
									else if (fst->FunInstVal[target_index] == State)
									{
										errs() << " change type store\n";
										fst->FunInstVal[target_index] = G_ROM_S;
										change++;
									}
									else if (fst->FunInstVal[target_index] == No_state)
									{
										errs() << " change type store\n";
										fst->FunInstVal[target_index] = G_ROM_N;
										change++;
									}

									if (fst->FunInstVal[target_index] == G_ROM_S && Find_Val(Inst->getOperand(0), fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] == G_ROM_N)
									{
										fst->FunInstVal[Find_Val(Inst->getOperand(0), fst)] = G_ROM_S;
									}
								}
							}
							else if (Inst->getOpcode() != llvm::Instruction::Call)
							{
								if (Find_Val(Inst, fst) != VAL_Not_Found)
								{
									if (fst->FunInstVal[Find_Val(Inst, fst)] == No_state)
									{
										if (fst->FunInstVal[Find_Val(Inst, fst)] != fst->FunInstVal[i])
										{
											fst->FunInstVal[Find_Val(Inst, fst)] = fst->FunInstVal[i];
											//errs()<<"5\n";
											change++;
										}
									}
									else if (fst->FunInstVal[Find_Val(Inst, fst)] == G_ROM_N)
									{
										if (fst->FunInstVal[i] == G_ROM_S || fst->FunInstVal[i] == State)
										{
											fst->FunInstVal[Find_Val(Inst, fst)] = G_ROM_S;
											//errs()<<"6\n";
											change++;
										}
									}
								}
							}
						}
					}
				}

				if (fst->FunInst[i]->getType()->isPointerTy())
				{
					if (fst->FunInstVal[i] == State)
					{
						fst->FunInstVal[i] = G_ROM_S;
						change++;
					}
					if (fst->FunInstVal[i] == No_state)
					{
						fst->FunInstVal[i] = G_ROM_N;
						change++;
					}
				}

				//errs()<<"Inst";
				if (dyn_cast<Instruction>(fst->FunInst[i]))
				{
					FInst = dyn_cast<Instruction>(fst->FunInst[i]);
					if (FInst->getOpcode() == llvm::Instruction::Ret)
					{
						if (FInst->getNumOperands() && fst->RetType != G_ROM_S && fst->RetType != State)
						{
							fst->RetType = Find_Val_Type(FInst->getOperand(0), fst);
						}
					}
					else if (FInst->getOpcode() != llvm::Instruction::Call && FInst->getOpcode() != llvm::Instruction::Store && FInst->getOpcode() != llvm::Instruction::Load && FInst->getOpcode() != llvm::Instruction::Alloca)
					{
						if (Find_Val(FInst, fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst, fst)] == G_ROM_S)
						{
							for (int ii = 0; ii < FInst->getNumOperands(); ii++)
							{
								if (Find_Val(FInst->getOperand(ii), fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst->getOperand(ii), fst)] == G_ROM_N)
								{
									//errs()<<"7\n";
									fst->FunInstVal[Find_Val(FInst->getOperand(ii), fst)] = G_ROM_S;
									change++;
								}
							}
						}
					}
					else if (FInst->getOpcode() == llvm::Instruction::Load)
					{
						for (int ii = 0; ii < FInst->getNumOperands(); ii++)
						{
							if (Find_Val(FInst->getOperand(ii), fst) != VAL_Not_Found)
							{
								if (fst->FunInstVal[Find_Val(FInst->getOperand(ii), fst)] == G_ROM_N)
								{
									if (Find_Val(FInst, fst) != VAL_Not_Found)
									{
										if (fst->FunInstVal[Find_Val(FInst, fst)] == G_ROM_S || fst->FunInstVal[Find_Val(FInst, fst)] == State)
										{
											fst->FunInstVal[Find_Val(FInst->getOperand(ii), fst)] = G_ROM_S;
											change++;
										}
									}
								}
							}
							else
							{
								if (dyn_cast<ConstantExpr>(FInst->getOperand(ii)))
								{
									CE = dyn_cast<ConstantExpr>(FInst->getOperand(ii));
									SI = CE->getAsInstruction();
									{
										///*
										for (int jj = 0; jj < SI->getNumOperands(); jj++)
										{
											if (Find_Val(SI->getOperand(jj), fst) != VAL_Not_Found)
											{
												if (fst->FunInstVal[Find_Val(SI->getOperand(jj), fst)] == G_ROM_S)
												{
													if (Find_Val(FInst, fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst, fst)] == No_state)
													{
														fst->FunInstVal[Find_Val(FInst, fst)] = State;
														change++;
													}
													else if (Find_Val(FInst, fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst, fst)] == G_ROM_N)
													{
														fst->FunInstVal[Find_Val(FInst, fst)] = G_ROM_S;
														change++;
													}
												}
												else
												{
													if (Find_Val(FInst, fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst, fst)] == State)
													{
														fst->FunInstVal[Find_Val(SI->getOperand(jj), fst)] = G_ROM_S;
														change++;
													}
													else if (Find_Val(FInst, fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst, fst)] == G_ROM_S)
													{
														fst->FunInstVal[Find_Val(SI->getOperand(jj), fst)] = G_ROM_S;
														change++;
													}
												}
											}
										}
									}
									SI->deleteValue();
								}
							}
						}
					}
				}
			}
			return change;
		}

		int Update_Function(Function *F, funvalst *fst)
		{
			if (print_flg)
				errs() << "Function " << F->getName() << '\n';
			BasicBlock *BB_c;
			int change = 0;
			unsigned char ret_type;
			int j;
			for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i)
			{
				BB_c = &*i;
				Instruction *Inst;
				for (BasicBlock::iterator i2 = BB_c->begin(), e2 = BB_c->end(); i2 != e2; ++i2) //
				{
					char block_split = 0;
					Inst = &*i2;
					if (Inst->getOpcode() == llvm::Instruction::Call)
					{
						if (dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands() - 1)))
						{
							Function *subf = dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands() - 1));
							Clean_st(&subfst[subdeep]);
							//							errs()<<"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
							//							errs()<<"<< deep >>"<<subdeep<<"\n";
							Find_All_FunctionArg(subf, &subfst[subdeep]);
							//							/*
							for (int jj = 0; jj < subfst[subdeep].functionval_num; jj++)
							{
								if (Find_Val(Inst->getOperand(jj), fst) != VAL_Not_Found)
								{
									subfst[subdeep].FunInstVal[jj] = Find_Val_Type(Inst->getOperand(jj), fst);
								}
								else
								{
									subfst[subdeep].FunInstVal[jj] = No_state;
								}
							}
							//							*/
							Find_All_GloabalVariable(subf->getParent(), &subfst[subdeep]);
							//							/*
							for (int jj = 0; jj < subfst[subdeep].functionval_num; jj++)
							{
								if (Find_Val(subfst[subdeep].FunInst[jj], fst) != VAL_Not_Found)
								{
									subfst[subdeep].FunInstVal[jj] = Find_Val_Type(subfst[subdeep].FunInst[jj], fst);
								}
							}
							//							*/
							Find_All_FunctionVal(subf, &subfst[subdeep]);

							subdeep++;
							if (subdeep < MAX_SUB_FUN_DEEP)
							{
								if (Find_Val(Inst, fst) != VAL_Not_Found)
									ret_type = Find_Val_Type(Inst, fst);
								while ((Update_Val(subf, &subfst[subdeep - 1]) + Update_Function(subf, &subfst[subdeep - 1])) != 0)
								{
									//									Update_Val(subf,&subfst[subdeep-1]);
								}
								if (print_flg)
								{
									errs() << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
									Print_Function(subf, &subfst[subdeep - 1]);
									errs() << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
								}
								if (Find_Val(Inst, fst) != VAL_Not_Found) //return change
								{
									//									fst->FunInstVal[Find_Val(Inst,fst)] = State;
									if (ret_type == No_state && subfst[subdeep - 1].RetType != No_state)
									{
										fst->FunInstVal[Find_Val(Inst, fst)] = subfst[subdeep - 1].RetType;
										change++;
									}
								}
								for (int ii = 0; ii < fst->functionval_num; ii++) //global val change
								{
									for (int jj = 0; jj < subfst[subdeep - 1].functionval_num; jj++)
									{
										if (fst->FunInst[ii] == subfst[subdeep - 1].FunInst[jj])
										{
											if (fst->FunInstVal[ii] != subfst[subdeep - 1].FunInstVal[jj])
											{
												fst->FunInstVal[ii] = subfst[subdeep - 1].FunInstVal[jj];
												change++;
											}
										}
									}
								}
								///*
								for (int jj = 0; jj < subfst[subdeep - 1].functionarg_num; jj++) //arg change
								{
									if (Find_Val(Inst->getOperand(jj), fst) != VAL_Not_Found)
									{
										if (subfst[subdeep - 1].FunInstVal[jj] == G_ROM_S && Find_Val_Type(Inst->getOperand(jj), fst) == G_ROM_N)
										{
											fst->FunInstVal[Find_Val(Inst->getOperand(jj), fst)] = G_ROM_S;
											change++;
										}
									}
								}
								//*/
							}
							subdeep--;
						}
					}
				}
			}
			return change;
		}

		bool runOnFunction(Function &F) override
		{
			subdeep = 0;
			if (F.getName().contains("pay")) //Invoke作为入口函数进行分析
			{
				errs() << "###################Function str###################\n";
				errs() << "Function " << F.getName() << '\n';
				Clean_st(&mainst);
				print_flg=1;
				Stain_Set(&F, &mainst);
				Find_All_GloabalVariable(F.getParent(), &mainst);
				errs() << "Find_All_FunctionVal";
				Find_All_FunctionVal(&F, &mainst);
				Print_Function(&F,&mainst);
				while((Update_Val(&F, &mainst) + Update_Function(&F, &mainst)) != 0);
				errs()<<"###################Function end###################\n";
				Print_Function(&F, &mainst);
			}
			return false;
		}
	}; // end of struct Hello
} // end of anonymous namespace

char stain::ID = 0;
static RegisterPass<stain> X("stain", "stain Analysis Pass");