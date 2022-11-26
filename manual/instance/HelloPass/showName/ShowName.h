#ifndef _PLLVM_PROJECT_SHOWNAME_H
#define _PLLVM_PROJECT_SHOWNAME_H

#include "llvm/Pass.h"

namespace llvm {
    Pass *createShowName();
    void initializeShowNamePass(PassRegistry &Registry);
}


#endif //_PLLVM_PROJECT_SHOWNAME_H