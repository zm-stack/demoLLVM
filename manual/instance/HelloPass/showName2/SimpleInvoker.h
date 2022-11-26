#ifndef _PLLVM_PROJECT_SIMPLEINVOKER_H
#define _PLLVM_PROJECT_SIMPLEINVOKER_H

#include "llvm/Pass.h"

namespace llvm {
    Pass *createSimpleInvoker();
    void initializeSimpleInvokerPass(PassRegistry &Registry);
}

#endif //_PLLVM_PROJECT_SIMPLEINVOKER_H