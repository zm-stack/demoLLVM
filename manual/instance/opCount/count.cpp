// https://zhuanlan.zhihu.com/p/118664682

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace//这个所谓的匿名namespace，实际就是一个子类，下面写了继承自FunctionPass,说明咱们搞的这个类输入是一个Function
{
	// OpcodeCounter - The first implementation, without getAnalysisUsage.
	struct OpcodeCounter : public FunctionPass 
	{
		static char ID; // Pass identification, replacement for typeid
		OpcodeCounter() : FunctionPass(ID) {}//构造函数

		bool runOnFunction(Function &F) override
		{
			std::map<std::string,int> opcode_map;//一个类似python中dict的类型，不了解python就理解成结构体

			errs() << "Function name: ";
			errs().write_escaped(F.getName()) << '\n';//显示遍历的函数名字
			for(Function::iterator bb = F.begin(),e = F.end();bb!=e;bb++)//在funciton中遍历basicblock
			{
				errs() << "BasicBlock name: "<< bb->getName() <<"\n";
				errs() << "BasicBlock size: "<< bb->size() <<"\n";//显示basicblock的名字和大小
				for(BasicBlock::iterator i = bb->begin(),i2 = bb->end();i!=i2;i++)//在bb中遍历statement
				{
					errs() << "              "<< *i<<"\n";
					if(opcode_map.find(i->getOpcodeName())==opcode_map.end())//找statement中的运算符，为了数ll文件中各个运算符出现的次数
					{
						opcode_map[i->getOpcodeName()] = 1;//第一次见到某个运算符置1，以后再见到就+1
					}
					else
					{
						opcode_map[i->getOpcodeName()]+=1;
					}
					//examples of user and use
					Instruction *inst = dyn_cast<Instruction>(i); 
					if(inst->getOpcode() == Instruction :: Add)//如果见到运算符是add，下面先显示用到它的语句，再显示它所调用的语句，注意一个用指针，一个用地址
					{
						for(User *U :inst ->users())
						{
							if(Instruction *Inst = dyn_cast<Instruction>(U))
							{
								errs()<<"result of add used in : "<<*Inst <<"\n";
							}
						}
						for(Use &U :inst ->operands())
						{
							Value *v =U.get();
							errs()<<"input of add originate from : " <<*v<<"\n";
						}
					}
				}

			}
			errs()<<"------------------------------\n";//这里把前面数出来的各个运算符的个数显示出来
			errs()<<"summary:\n";
			std::map<std::string,int>::iterator i = opcode_map.begin();
			std::map<std::string,int>::iterator e = opcode_map.end();
			while(i!=e)
			{
				errs()<<"number of "<< i->first << ":"<< i->second <<"\n";
				i++;
			}
			errs()<<"\n";
			opcode_map.clear();
			return false;
		}
	};
}

char OpcodeCounter::ID = 0;
static RegisterPass<OpcodeCounter> X("OpcodeCounter", "OpcodeCounter Pass");//在RegisterPass中注册我们自定义的这个pass