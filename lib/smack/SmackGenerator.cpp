//
// Copyright (c) 2008 Zvonimir Rakamaric (zvonimir@cs.utah.edu)
// This file is distributed under the MIT License. See LICENSE for details.
//
#include "SmackGenerator.h"

using namespace smack;
using namespace std;

RegisterPass<SmackGenerator> X("smack", "SMACK generator pass");
char SmackGenerator::ID = 0;

bool SmackGenerator::runOnModule(Module &m) {
  DataLayout& targetData = getAnalysis<DataLayout>();
  module = new SmackModule();

  DEBUG(errs() << "Analyzing globals...\n");
  for (Module::const_global_iterator g = m.global_begin(), e = m.global_end(); g != e; ++g) {
    if (isa<GlobalVariable>(g)) {
      string globalName = translateName(g);
      module->addGlobalVariable(globalName);
    }
  }

  DEBUG(errs() << "Analyzing functions...\n");
  SmackInstVisitor smackVisitor(&targetData);

  for (Module::iterator func = m.begin(), e = m.end(); func != e; ++func) {
    if (func->isDeclaration() || func->getName().str().find("__SMACK") != string::npos ) {
      continue;
    }
    DEBUG(errs() << "Analyzing function: " << func->getName().str() << "\n");

    Procedure* procedure = new Procedure(func->getName().str(), func->getReturnType());

    // set return variable name
    if ( !procedure->isVoid() )
      procedure->setReturnVar(new VarExpr("$r"));

    // add arguments
    for (Function::const_arg_iterator i = func->arg_begin(), e = func->arg_end(); i != e; ++i) {
      procedure->addArgument(i);
    }

    module->addProcedure(procedure);

    if ( !func->isDeclaration() && !func->empty() && !func->getEntryBlock().empty() ) {    
      map<const BasicBlock*, Block*> knownBlocks;
      stack<BasicBlock*> workStack;    
      int bn = 0;

      BasicBlock& entryBlock = func->getEntryBlock();
      Block* smackEntryBlock = new Block(&entryBlock,bn++);
      procedure->setEntryBlock(smackEntryBlock);
      knownBlocks[&entryBlock] = smackEntryBlock;
      workStack.push(&entryBlock);

      // invariant: knownBlocks.CONTAINS(b) iff workStack.CONTAINS(b) or  
      // workStack.CONTAINED(b) at some point in time.
      while (!workStack.empty()) {      
        BasicBlock *basicBlock = workStack.top(); workStack.pop();
        Block *smackBlock = knownBlocks[basicBlock];

        procedure->addBlock(smackBlock);
        smackVisitor.setBlock(smackBlock);
        smackVisitor.visit(basicBlock);
        
        for (succ_iterator i = succ_begin(basicBlock), e = succ_end(basicBlock); i != e; ++i) {
          BasicBlock* succ = *i;
          Block* smackSucc;
          
          if (knownBlocks.count(succ) == 0) {
            smackSucc = new Block(succ, bn++);
            knownBlocks[succ] = smackSucc;
            workStack.push(succ);
          } else {
              smackSucc = knownBlocks[succ];
          }
          smackVisitor.addSuccBlock(smackSucc);
        }
      }
    }


    DEBUG(errs() << "Finished analyzing function: " << func->getName().str() << "\n\n");
  }
  return false;
}
