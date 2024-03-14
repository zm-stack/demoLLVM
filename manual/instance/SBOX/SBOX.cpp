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
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;
#define DEBUG_TYPE "SBOX"
#define MAX_BASICBLOCK (1<<10) //
#define MAX_VAL_Fun (1<<20)//
#define MAX_VAL_GLOBAL (1<<20)//
#define MAX_SUB_FUN_DEEP (10)

#define No_state         (1)//
#define G_ROM_N          (2)//
#define G_ROM_S          (3)//
#define State            (4)// 
#define VAL_Not_Found    (-1)// refer to data which relate to function input
char flg = 0;
char print_flg;
struct funvalst
{
	void *Function_Basics[MAX_BASICBLOCK];
	Value *FunInst[MAX_VAL_Fun];
	unsigned char FunInstVal[MAX_VAL_Fun];
	unsigned char RetType;
	int functionval_num;
	int functionarg_num;
	int functionglo_num;
};
namespace 
{// SBOX - The first implementation, without getAnalysisUsage.
  struct SBOX : public FunctionPass 
	{
    	static char ID;// Pass identification, replacement for typeid
		funvalst mainst;
		funvalst subfst[MAX_SUB_FUN_DEEP];
		int subdeep;

    	SBOX() : FunctionPass(ID) 	
		{
		}
		void Clean_st(funvalst * fst)
		{
			fst->functionval_num = 0;
			fst->functionarg_num = 0;
			fst->functionglo_num = 0;
			for(unsigned int i=0;i<MAX_VAL_Fun;i++)fst->FunInst[i]=NULL;
			for(unsigned int i=0;i<MAX_VAL_Fun;i++)fst->FunInstVal[i]=No_state;		
			fst->RetType = No_state;
		}
		bool IS_ROM(GlobalVariable * g)
		{
			int l=0;
			unsigned int witwidth;
			Constant *CT;
			if(CT =	g->getInitializer()->getAggregateElement(l)){l++;}
			else return 0;
			if(dyn_cast<ConstantInt>(CT)){witwidth = dyn_cast<ConstantInt>(CT)->getBitWidth();}
			else return 0;	
			l++;
			while(CT =	g->getInitializer()->getAggregateElement(l))
			{
				if(dyn_cast<ConstantInt>(CT))
				{
					if(witwidth != dyn_cast<ConstantInt>(CT)->getBitWidth())
					{
						return 0;
					}
				}
				else return 0;	
				l++;
			}
			return 1;
		}

		Value* Find_Used_Inst(Value * v)
		{
			for(User *u: v -> users())
			{
				if(dyn_cast<Instruction>(u))
				{
					//					errs()<< dyn_cast<Instruction>(u)->getOpcodeName()<<" ";
					return u;
				}
				else if(dyn_cast<ConstantExpr>(u))
				{
					Find_Used_Inst(u);
				}
			}
			return NULL;
		}

		Value* Used_to_Inst(Value * v)
		{
			if(dyn_cast<Instruction>(v)){return v;}	
			if(dyn_cast<ConstantExpr>(v))
			{		
				for(User *u: v -> users())
				{
					if(dyn_cast<Instruction>(u))
					{
						return u;
					}
					else if(dyn_cast<ConstantExpr>(u))
					{
						Find_Used_Inst(u);
					}
				}
			}
			return NULL;
		}	

		void Pinter_Find_WR(Value * v)
		{
			for(User *u: v -> users())
			{
				if(dyn_cast<Instruction>(u))
				{
					if(dyn_cast<Instruction>(u)->getOpcode() == Instruction::Store||dyn_cast<Instruction>(u)->getOpcode() == Instruction::Load)
					{
						errs()<<"   OpCode "<< dyn_cast<Instruction>(u)->getOpcodeName() <<" used \n";
					}
					else Pinter_Find_WR(u);
				}
				else if(dyn_cast<ConstantExpr>(u))
				{
					Pinter_Find_WR(u);
				}		
			}			
		}

		void Find_All_GloabalVariable(Module *M,funvalst * fst)
		{
			int l=0;
			for(Module::global_iterator i = M->global_begin(),e = M->global_end();i!=e;++i)
			{
				errs()<<fst->functionval_num;
				fst->FunInst[fst->functionval_num] = &*i;
				l=0;
				if(print_flg)errs()<<i->getName()<<" ";
				while(i->getInitializer()->getAggregateElement(l))l++;
				if(l==0)l=1;//have only single value;
				errs()<<"1";
				if(IS_ROM(&*i))
				{
					errs()<<"2";
					Constant *CT = i->getInitializer()->getAggregateElement(l-1);
					fst->FunInstVal[fst->functionval_num] = G_ROM_N;
					if(print_flg && dyn_cast<ConstantInt>(CT))errs()<<" "<<dyn_cast<ConstantInt>(CT)->getBitWidth()/8<<"Byte X "<<"["<<l<<"] G_ROM\n";
				}
				else 
				{
					errs()<<"3";
					fst->FunInstVal[fst->functionval_num] = No_state;
					if(print_flg && dyn_cast<ConstantInt>(i->getInitializer()))errs()<< dyn_cast<ConstantInt>(i->getInitializer())->getBitWidth()/8<<"Byte X "<<"["<<l<<"] State\n";
				}
				errs()<<"4";
				fst->functionval_num++;
			}
			fst->functionglo_num = fst->functionval_num - fst->functionarg_num;
			if(print_flg)errs()<<"------------------------------------------\n";
		}

		void Find_All_FunctionArg(Function *F,funvalst * fst)
		{
			Argument *arg;
			unsigned int l;
			//find all of the global variable
			
			for(Function::arg_iterator i = F->arg_begin(),e = F->arg_end();i!=e;++i)
			{
				arg = &*i;
				fst->FunInst[fst->functionval_num] = arg;
				if(i->getType()->getTypeID() != Type::PointerTyID)fst->FunInstVal[fst->functionval_num] = State;
				else fst->FunInstVal[fst->functionval_num] = State;//which don't know the data struct
				if(print_flg)errs()<<"arg: %"<<fst->functionval_num<<"\n";
				fst->functionval_num++;
			}
			fst->functionarg_num = fst->functionval_num;
			if(print_flg)errs()<<"------------------------------------------\n";
		}

		void Find_First_FunctionArg(Function *F,funvalst * fst)
		{
			Argument *arg;
			unsigned int l;
			//find all of the global variable
			
			for(Function::arg_iterator i = F->arg_begin(),e = F->arg_end();i!=e;++i)
			{
				arg = &*i;
				fst->FunInst[fst->functionval_num] = arg;
				//				if(i->getType()->getTypeID() != Type::PointerTyID)fst->FunInstVal[fst->functionval_num] = State;
				//				else fst->FunInstVal[fst->functionval_num] = State;//which don't know the data struct
				if(print_flg)errs()<<"arg: %"<<fst->functionval_num<<"\n";
				fst->functionval_num++;
			}
			if(fst->functionval_num >= 3)
			{
				fst->FunInstVal[0] = G_ROM_S;
				fst->FunInstVal[1] = G_ROM_N;
				fst->FunInstVal[2] = G_ROM_N;
			}
			fst->functionarg_num = fst->functionval_num;
			if(print_flg)errs()<<"------------------------------------------\n";
		}

		int Find_Val(Value* v,funvalst * fst)
		{
			int i;
			for(i=0;i<fst->functionval_num;i++)
			{
				if(fst->FunInst[i]==v)
				{
					return i;
				}
			}
			return -1;
		}

		void Serch_Blocks(BasicBlock *BB_c,funvalst * fst)
		{
			Instruction *Inst;
			int i;
			for(BasicBlock::iterator i2=BB_c->begin(),e2=BB_c->end();i2!=e2;++i2)//
			{
				Inst = &*i2;

				if(Inst->hasNUsesOrMore(1))//[store] or [br] will not be used
				{
					if(Inst->getOpcode() == Instruction::Alloca)
					{
						fst->FunInst[fst->functionval_num] = Inst;
						fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						fst->functionval_num++;
					}
					else
					{
						fst->FunInst[fst->functionval_num] = Inst;
						if(Inst->getType()->isPointerTy())fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						else fst->FunInstVal[fst->functionval_num] = No_state;
						fst->functionval_num++;						
					}
				}
				else if(Inst->getOpcode() == Instruction::Ret)
				{
					if(Inst->getNumOperands())
					{
						fst->FunInst[fst->functionval_num] = Inst;
						if(Inst->getOperand(0)->getType()->isPointerTy())fst->FunInstVal[fst->functionval_num] = G_ROM_N;
						else fst->FunInstVal[fst->functionval_num] = No_state;
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

		void Find_All_FunctionVal(Function *F,funvalst * fst)
		{
			BasicBlock *BB_c;
			for(Function::iterator i = F->begin(),e = F->end();i!=e;++i)
			{
				BB_c = &*i;
				Serch_Blocks(BB_c,fst);
			}
		}

		unsigned char Find_Val_Type(Value *u,funvalst * fst)
		{
			int i;
			for(i=0;i<fst->functionval_num;i++)
			{
				if(fst->FunInst[i]==u)return fst->FunInstVal[i];
			}
			return VAL_Not_Found;
		}

		int Update_Val(Function *F,funvalst * fst)
		{
			int i;
			int change = 0;
			Instruction * FInst;
			ConstantExpr *CE;
			Value *v;
			Instruction * Inst;
			Instruction *SI;

			//errs()<<"Update_Val\n";
			for(i=0;i<fst->functionval_num;i++)
			{
				//errs()<<" update "<<i<<" use: ";
				v = fst->FunInst[i];
				for(User *u: fst->FunInst[i] -> users())
				{	
					//errs()<<" In: "; 
					if(Used_to_Inst(u))
					{
						Inst = (Instruction *)Used_to_Inst(u);
						if(Inst->getParent()->getParent() == F)
						{
							if(Inst->getOpcode()== llvm::Instruction::Load)
							{
								if(Find_Val(Inst,fst)!=VAL_Not_Found)
								{
									if(fst->FunInstVal[i] == G_ROM_S)
									{
										if(fst->FunInstVal[Find_Val(Inst,fst)] == No_state)
										{
											fst->FunInstVal[Find_Val(Inst,fst)] = State;
											//errs()<<"1\n";
											change++;
										}
										else if(fst->FunInstVal[Find_Val(Inst,fst)] == G_ROM_N)
										{
											fst->FunInstVal[Find_Val(Inst,fst)] = G_ROM_S;
											//errs()<<"2\n";
											change++;
										}
									}
									else if(fst->FunInstVal[i] == State)
									{
										//errs()<<" change type load\n";
										fst->FunInstVal[i] = G_ROM_S;							
										change++;
									}
									else if(fst->FunInstVal[i] == No_state)
									{
										//errs()<<" change type load\n";
										fst->FunInstVal[i] = G_ROM_N;							
										change++;										
									}
								}
							}
							else if(Inst->getOpcode()== llvm::Instruction::Store)
							{
								int target_index = VAL_Not_Found;
								if(Find_Val(Inst->getOperand(1),fst)!= VAL_Not_Found)target_index = Find_Val(Inst->getOperand(1),fst);
								else
								{
									if(dyn_cast<ConstantExpr>(Inst->getOperand(1)))
									{
										CE = dyn_cast<ConstantExpr>(Inst->getOperand(1));
										SI = CE->getAsInstruction();
										for(int jj=0;jj<SI->getNumOperands();jj++)
										{
											if(Find_Val(SI->getOperand(jj),fst)!=VAL_Not_Found)
											{
												target_index = Find_Val(SI->getOperand(jj),fst);
												break;
											}
										}
										SI->deleteValue();
									}
								}

								if(target_index!= VAL_Not_Found)
								{
									if(Find_Val(Inst->getOperand(0),fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == G_ROM_S)
									{
										if(fst->FunInstVal[target_index] != G_ROM_S)
										{
											fst->FunInstVal[target_index] = G_ROM_S;
											//errs()<<"3\n";
											change++;
										}
									}
									else if(Find_Val(Inst->getOperand(0),fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == State)
									{
										if(fst->FunInstVal[target_index] != G_ROM_S)
										{
											fst->FunInstVal[target_index] = G_ROM_S;
											//errs()<<"4\n";
											change++;
										}
									}
									else if (fst->FunInstVal[target_index]==State)
									{
										errs()<<" change type store\n";
										fst->FunInstVal[target_index] = G_ROM_S;
										change++;
									}
									else if (fst->FunInstVal[target_index]==No_state)
									{
										errs()<<" change type store\n";
										fst->FunInstVal[target_index] = G_ROM_N;
										change++;
									}
									
									if(fst->FunInstVal[target_index] == G_ROM_S && Find_Val(Inst->getOperand(0),fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == G_ROM_N)
									{
										fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] = G_ROM_S;
									}
								}	
							}
							else if(Inst->getOpcode()!= llvm::Instruction::Call)
							{
								if(Find_Val(Inst,fst)!=VAL_Not_Found)
								{
									if(fst->FunInstVal[Find_Val(Inst,fst)] == No_state)
									{
										if(fst->FunInstVal[Find_Val(Inst,fst)] != fst->FunInstVal[i])
										{
											fst->FunInstVal[Find_Val(Inst,fst)] = fst->FunInstVal[i];
											//errs()<<"5\n";
											change++;
										}
									}
									else if(fst->FunInstVal[Find_Val(Inst,fst)] == G_ROM_N)
									{
										if(fst->FunInstVal[i] == G_ROM_S || fst->FunInstVal[i] == State)
										{
											fst->FunInstVal[Find_Val(Inst,fst)] = G_ROM_S;
											//errs()<<"6\n";
											change++;
										}
									}
								}
							}
						}
					}
				}

				if(fst->FunInst[i]->getType()->isPointerTy())
				{
					if(fst->FunInstVal[i] == State) {fst->FunInstVal[i]=G_ROM_S;change++;}
					if(fst->FunInstVal[i] == No_state) {fst->FunInstVal[i]=G_ROM_N;change++;}
				}


				//errs()<<"Inst";
				if(dyn_cast<Instruction>(fst->FunInst[i]))
				{	
					FInst = dyn_cast<Instruction>(fst->FunInst[i]);
					if(FInst->getOpcode()== llvm::Instruction::Ret)
					{
						if(FInst->getNumOperands() && fst->RetType!=G_ROM_S && fst->RetType!=State)
						{
							fst->RetType = Find_Val_Type(FInst->getOperand(0),fst);
						}
						
					}
					else if(FInst->getOpcode()!= llvm::Instruction::Call && FInst->getOpcode()!= llvm::Instruction::Store && FInst->getOpcode()!= llvm::Instruction::Load && FInst->getOpcode()!= llvm::Instruction::Alloca)
					{
						if(Find_Val(FInst,fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst,fst)] == G_ROM_S)
						{
							for(int ii=0;ii<FInst->getNumOperands();ii++)
							{
								if(Find_Val(FInst->getOperand(ii),fst) != VAL_Not_Found && fst->FunInstVal[Find_Val(FInst->getOperand(ii),fst)] == G_ROM_N)
								{
									//errs()<<"7\n";
									fst->FunInstVal[Find_Val(FInst->getOperand(ii),fst)] = G_ROM_S;
									change++;
								}
							}
						}
					}
					else if(FInst->getOpcode() == llvm::Instruction::Load)
					{
						for(int ii=0;ii<FInst->getNumOperands();ii++)
						{
							if(Find_Val(FInst->getOperand(ii),fst)!=VAL_Not_Found)
							{
								if(fst->FunInstVal[Find_Val(FInst->getOperand(ii),fst)] == G_ROM_N)
								{
									if(Find_Val(FInst,fst)!=VAL_Not_Found)
									{
										if(fst->FunInstVal[Find_Val(FInst,fst)] == G_ROM_S || fst->FunInstVal[Find_Val(FInst,fst)] == State )
										{
											fst->FunInstVal[Find_Val(FInst->getOperand(ii),fst)] = G_ROM_S;
											change++;
										}
									}
								}
							}
							else 
							{
								if(dyn_cast<ConstantExpr>(FInst->getOperand(ii)))
								{
									CE = dyn_cast<ConstantExpr>(FInst->getOperand(ii));
									SI = CE->getAsInstruction();
									{
										///*
										for(int jj=0;jj<SI->getNumOperands();jj++)
										{
											if(Find_Val(SI->getOperand(jj),fst)!=VAL_Not_Found)
											{
												if(fst->FunInstVal[Find_Val(SI->getOperand(jj),fst)] == G_ROM_S)
												{
													if(Find_Val(FInst,fst)!=VAL_Not_Found && fst->FunInstVal[Find_Val(FInst,fst)]==No_state)
													{
														fst->FunInstVal[Find_Val(FInst,fst)] = State;
														change++;
													}
													else if(Find_Val(FInst,fst)!=VAL_Not_Found && fst->FunInstVal[Find_Val(FInst,fst)]==G_ROM_N)
													{
														fst->FunInstVal[Find_Val(FInst,fst)] = G_ROM_S;
														change++;
													}
												}
												else
												{
													if(Find_Val(FInst,fst)!=VAL_Not_Found && fst->FunInstVal[Find_Val(FInst,fst)]==State)
													{
														fst->FunInstVal[Find_Val(SI->getOperand(jj),fst)] = G_ROM_S;
														change++;
													}
													else if(Find_Val(FInst,fst)!=VAL_Not_Found && fst->FunInstVal[Find_Val(FInst,fst)]==G_ROM_S)
													{
														fst->FunInstVal[Find_Val(SI->getOperand(jj),fst)] = G_ROM_S;
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
//*/
		void Print_user(funvalst * fst)
		{
			int i;
			Instruction * Inst;
			for(i=0;i<fst->functionval_num;i++)
			{
				if(dyn_cast<Instruction>(fst->FunInst[i]))errs()<<"val: "<<dyn_cast<Instruction>(fst->FunInst[i])->getOpcodeName()<<"\n";
				if(dyn_cast<Argument>(fst->FunInst[i]))errs()<<"arg: "<<dyn_cast<Argument>(fst->FunInst[i])<<"\n";
				

				if(fst->FunInst[i]->hasOneUse())//arg or val
				{
					for(User *u: fst->FunInst[i] -> users())
					{
						errs()<<"     usr: "<<dyn_cast<Instruction>(u)->getOpcodeName()<<"\n";
					}
				}
				
			}
			//update store code,global store;
		}		

		void Print_Type(int i)
		{
			if(i==No_state)errs()<<"NS";
			else if(i==G_ROM_N)errs()<<"NP";
			else if(i==G_ROM_S)errs()<<"SP";
			else if(i==State)errs()<<"ST";

		}
		void Print_Function(Function *F,funvalst * fst)
		{
			unsigned int br_num,op_num,rom_num;
			br_num = op_num = rom_num = 0;
			errs()<<"Function "<<F->getName()<<'\n';
			BasicBlock *BB_c;
			int j;
			errs()<<"arg\n";
			for(int valindex = 0;valindex < fst->functionarg_num;valindex++)
			{
				errs()<<"["<<valindex<<"]";
				Print_Type(fst->FunInstVal[valindex]);
				errs()<<"\n";
			}			
			errs()<<"glo\n";
			for(int valindex = fst->functionarg_num;valindex < fst->functionglo_num;valindex++)
			{
				errs()<<"["<<valindex<<"]";
				Print_Type(fst->FunInstVal[valindex]);
				errs()<<"\n";
			}	
			errs()<<"body\n";
			for(Function::iterator i = F->begin(),e = F->end();i!=e;++i)
			{
				BB_c = &*i;
				Instruction *Inst;
				for(BasicBlock::iterator i2=BB_c->begin(),e2=BB_c->end();i2!=e2;++i2)//
				{
					char block_split = 0;
					Inst = &*i2;
					errs()<<Inst->getOpcodeName();

					if(Inst->getOpcode()== llvm::Instruction::Call)
					{
						if(dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands()-1)))
						{
							errs()<<" Function "<<Inst->getNumOperands()<<" ";
						}
					}
					if(llvm::Instruction::GetElementPtr == Inst->getOpcode() && Find_Val(Inst->getOperand(0),fst)!=VAL_Not_Found && (fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == G_ROM_N || fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == G_ROM_S))
					{
						for(j=1;j<Inst->getNumOperands();j++)
						{
							if(Find_Val(Inst->getOperand(j),fst)!=VAL_Not_Found && fst->FunInstVal[Find_Val(Inst->getOperand(j),fst)]==State)
							{
								if(dyn_cast<GlobalVariable>(fst->FunInst[Find_Val(Inst->getOperand(0),fst)]))
								{
									GlobalVariable * g = dyn_cast<GlobalVariable>(fst->FunInst[Find_Val(Inst->getOperand(0),fst)]);
									if(g->isConstant())
									{
										unsigned int l=0;
										while(g->getInitializer()->getAggregateElement(l))l++;
										if(l==0)l=1;//have only single value;
										if(IS_ROM(g))
										{
											Constant *CT = g->getInitializer()->getAggregateElement(l-1);
											if(dyn_cast<ConstantInt>(CT))
												errs()<<" [const ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<" word size: "<<dyn_cast<ConstantInt>(CT)->getBitWidth()/8<<"*"<<l<<">";
											else if(dyn_cast<ConstantData>(CT))
												errs()<<" [const ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<" Byte size: "<<l<<">";
										}
										else errs()<<" [const ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<" Byte size: "<<" UN "<<">";
									
										rom_num++;
										break;
									}
									else
									{
										unsigned int l=0;
										while(g->getInitializer()->getAggregateElement(l))l++;

										if(l!=0)
										{
											Constant *CT = g->getInitializer()->getAggregateElement(l-1);
											if(dyn_cast<ConstantInt>(CT))
												errs()<<" [ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<" word size: "<<dyn_cast<ConstantInt>(CT)->getBitWidth()/8<<"*"<<l<<">";
											else if(dyn_cast<ConstantData>(CT))
												errs()<<" [ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<" Byte size: "<<l<<">";
										}
										else
										{
											errs()<<" [ROM cache atack may be!]"<<" <"<<g->getName()<<" : ["<<rom_num<<"]"<<">";
										}
										

										rom_num++;										
									}	
								}								

								break;
							}
						}
					}
					else if(llvm::Instruction::Br == Inst->getOpcode() && Find_Val(Inst->getOperand(0),fst)!=VAL_Not_Found && ((fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == State)||(fst->FunInstVal[Find_Val(Inst->getOperand(0),fst)] == G_ROM_S)))
					{
						errs()<<" [Branch statements are affected! ] <"<<br_num<<">";
						br_num ++;
					}
					else if(llvm::Instruction::Mul == Inst->getOpcode() || llvm::Instruction::FMul == Inst->getOpcode())
					{
						for(int j=0;j<Inst->getNumOperands();j++)
						{
							if(Find_Val(Inst->getOperand(j),fst)!=VAL_Not_Found && (fst->FunInstVal[Find_Val(Inst->getOperand(j),fst)] == State))
							{
								errs()<<" [operation time atack may be! ] <"<<op_num<<">";
								op_num++;
								break;
							}
						}

					}
					else if(llvm::Instruction::UDiv == Inst->getOpcode() || llvm::Instruction::SDiv == Inst->getOpcode()|| llvm::Instruction::FDiv == Inst->getOpcode())
					{
						for(int j=0;j<Inst->getNumOperands();j++)
						{
							if(Find_Val(Inst->getOperand(j),fst)!=VAL_Not_Found && (fst->FunInstVal[Find_Val(Inst->getOperand(j),fst)] == State))
							{
								errs()<<" [operation time atack may be! ] <"<<op_num<<">";
								op_num++;
								break;
							}
						}
	
					}
					else if(Inst->getType()->isHalfTy()||Inst->getType()->isFloatTy()||Inst->getType()->isDoubleTy())
					{
						for(int j=0;j<Inst->getNumOperands();j++)
						{
							if(Find_Val(Inst->getOperand(j),fst)!=VAL_Not_Found && (fst->FunInstVal[Find_Val(Inst->getOperand(j),fst)] == State))
							{
								errs()<<" [Float operation time atack may be! ] <"<<op_num<<">";
								op_num++;
								break;
							}
						}
					}				

					if(Find_Val(Inst,fst)!=VAL_Not_Found)
					{
						errs()<<" ( "<<Find_Val(Inst,fst)<<" ";
						Print_Type((int)fst->FunInstVal[Find_Val(Inst,fst)]);
						errs()<<" "<<(int)Inst->getType()->getTypeID()<<" ) ";
					}

					for(int j=0;j<Inst->getNumOperands();j++)
					{
						if(Find_Val(Inst->getOperand(j),fst)!=VAL_Not_Found)
						{
							errs()<<" [ "<<Find_Val(Inst->getOperand(j),fst)<<" ";
							Print_Type((int)fst->FunInstVal[Find_Val(Inst->getOperand(j),fst)]);
							errs()<<" ] ";
						}
						else
						{
							errs()<<" [ CT ] ";
						}
						
					}
					errs()<<"\n";
					if(block_split==1)break;
				}
				errs()<<"block end\n";
				
			}	
			//errs()<<"Function "<<F->getName()<<" cache attack :"<<rom_num<<'\n';		
			errs()<<"Function "<<F->getName()<<"Branch statements are affected: "<<br_num<<'\n';		
			//errs()<<"Function "<<F->getName()<<"    op attack :"<<op_num<<'\n';		
		}

		int Update_Function(Function *F,funvalst * fst)
		{
			if(print_flg)errs()<<"Function "<<F->getName()<<'\n';
			BasicBlock *BB_c;
			int change = 0;
			unsigned char ret_type;
			int j;
			for(Function::iterator i = F->begin(),e = F->end();i!=e;++i)
			{
				BB_c = &*i;
				Instruction *Inst;
				for(BasicBlock::iterator i2=BB_c->begin(),e2=BB_c->end();i2!=e2;++i2)//
				{
					char block_split = 0;
					Inst = &*i2;
					if(Inst->getOpcode()== llvm::Instruction::Call)
					{
						if(dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands()-1)))
						{
							Function *subf = dyn_cast<Function>(Inst->getOperand(Inst->getNumOperands()-1));
							Clean_st(&subfst[subdeep]);
							//							errs()<<"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
							//							errs()<<"<< deep >>"<<subdeep<<"\n";
							Find_All_FunctionArg(subf,&subfst[subdeep]);
							//							/*
							for(int jj=0;jj<subfst[subdeep].functionval_num;jj++)
							{
								if(Find_Val(Inst->getOperand(jj),fst)!=VAL_Not_Found)
								{
									subfst[subdeep].FunInstVal[jj] = Find_Val_Type(Inst->getOperand(jj),fst);
								}
								else
								{
									subfst[subdeep].FunInstVal[jj] = No_state;
								}
							}
							//							*/

							Find_All_GloabalVariable(subf->getParent(),&subfst[subdeep]);
							//							/*
							for(int jj=0;jj<subfst[subdeep].functionval_num;jj++)
							{
								if(Find_Val(subfst[subdeep].FunInst[jj],fst)!=VAL_Not_Found)
								{
									subfst[subdeep].FunInstVal[jj] = Find_Val_Type(subfst[subdeep].FunInst[jj],fst);
								}
							}
							//							*/
							Find_All_FunctionVal(subf,&subfst[subdeep]);	


							subdeep++;
							if(subdeep<MAX_SUB_FUN_DEEP)
							{
								if(Find_Val(Inst,fst) != VAL_Not_Found) ret_type = Find_Val_Type(Inst,fst);

								while((Update_Val(subf,&subfst[subdeep-1]) + Update_Function(subf,&subfst[subdeep-1]))!=0)
								{
									//									Update_Val(subf,&subfst[subdeep-1]);
								}

								if(print_flg)
								{
									errs()<<"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
									Print_Function(subf,&subfst[subdeep-1]);
									errs()<<"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n";
								}

								if(Find_Val(Inst,fst)!= VAL_Not_Found)//return change
								{
									//									fst->FunInstVal[Find_Val(Inst,fst)] = State;
									if(ret_type == No_state && subfst[subdeep-1].RetType != No_state)
									{
										fst->FunInstVal[Find_Val(Inst,fst)] = subfst[subdeep-1].RetType;
										change++;
									}
								}

								for(int ii=0;ii<fst->functionval_num;ii++)//global val change
								{
									for(int jj=0;jj<subfst[subdeep-1].functionval_num;jj++)
									{
										if(fst->FunInst[ii] == subfst[subdeep-1].FunInst[jj])
										{
											if(fst->FunInstVal[ii] != subfst[subdeep-1].FunInstVal[jj])
											{
												fst->FunInstVal[ii] = subfst[subdeep-1].FunInstVal[jj];
												change++;
											}
										}
									}
								}
								///*
								for(int jj=0;jj<subfst[subdeep-1].functionarg_num;jj++)//arg change
								{
									if(Find_Val(Inst->getOperand(jj),fst)!=VAL_Not_Found)
									{
										if(subfst[subdeep-1].FunInstVal[jj] == G_ROM_S && Find_Val_Type(Inst->getOperand(jj),fst) == G_ROM_N)
										{
											fst->FunInstVal[Find_Val(Inst->getOperand(jj),fst)] = G_ROM_S;
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
			subdeep=0;
			//			if(flg==0)
			if(F.getName().contains("work_parse"))
			{
				errs()<<"###################Function str###################\n";
				errs()<<"Function "<<F.getName()<<'\n';
				Clean_st(&mainst);

				print_flg=1;
				Find_First_FunctionArg(&F,&mainst);
				Find_All_GloabalVariable(F.getParent(),&mainst);
				errs()<<"Find_All_FunctionVal";
				Find_All_FunctionVal(&F,&mainst);	
				while((Update_Val(&F,&mainst)+Update_Function(&F,&mainst))!=0);
				errs()<<"###################Function end###################\n";
				Print_Function(&F,&mainst);
			}
			return false;
		}
  };
}
char SBOX::ID = 0;
static RegisterPass<SBOX> X("sbox", "SBOX Pass");