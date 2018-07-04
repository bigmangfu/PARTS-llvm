//===- PtrTypeMD.cpp - Code For Pointer Type Metadata ---------------------===/
//
//                     The LLVM Compiler Infrastructure
//
// This code is released under Apache 2.0 license.
// Author: Zaheer Ahmed Gauhar
// Copyright: Secure Systems Group, Aalto University https://ssg.aalto.fi/
//
//===----------------------------------------------------------------------===//
//
// This file implements Pointer type extraction for load and store 
// instructions and differentiates between pointers to data and functions.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "MDclass.h"
using namespace llvm;

#define DEBUG_TYPE "PtrTypeMDPass"
namespace {
 
  struct PtrTypeMDPass : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    MDclass md=MDclass();
    PtrTypeMDPass() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override {
        //errs() << "Hello: ";
        //errs().write_escaped(F.getName()) << '\n';
      for (auto &BB:F){
	//errs() << "Basic Block: ";
	//errs().write_escaped(F.getName()) << '\n';
          for (auto &I: BB){
                //errs() << "Instruction: ";
               //I.dump();
              if (I.getOpcode()==Instruction::Load || I.getOpcode()==Instruction::Store ){
                 //dumpInsts(I);            //Dump Instructions
                 Type* Ty1;
              if (I.getOpcode()==Instruction::Store ){
                 Ty1 = I.getOperand(0)->getType();
                     }
              if (I.getOpcode()==Instruction::Load ){
                 Ty1 = I.getType();
                     }

	 			 //insertNops(F,I);           //Add nop instructions

				 if(Ty1->isPointerTy()){
					 md.setFPtrType(false);                   
					 assertPtrType(I);
				     bool fty=md.getFPtrType();
                     md_fn(F,I,md.getPtrType(),fty);
                        }
                     }
				dumpInsts(I);                //Dump instructions
                 }
 	    }
      return true;
      }

//Function to dump the instruction 
  void dumpInsts(Instruction &I){
    errs() << "Instruction: ";
    I.dump();
    errs().write_escaped(I.getOpcodeName(I.getOpcode()));
    errs() << "\n";
  }

//Function to assert the types of instruction operands to be equal  
  void assertPtrType(Instruction &I){
    int ops=I.getNumOperands();
    Type* Ty=I.getOperand(0)->getType();
    Type* found_ty=isFnType(Ty);
    for (int i=1; i<ops; i++){
        Type* next_ty=I.getOperand(i)->getType();
        Type* next_found_ty=isFnType(next_ty);
        if (found_ty!=next_found_ty){
                errs()<<"Exception: Type mismatch between operands\n";
                break;
                }
        else{
                found_ty=next_found_ty;
                }
        }
     md.setPtrType(found_ty);
  }

//Function to set metadata on an instruction
  void md_fn(Function &F, Instruction &I, Type* ptrTy, bool fty){
      std::string type_str;
      llvm::raw_string_ostream rso(type_str);
      ptrTy->print(rso);
      auto &C = F.getContext();
      Metadata* vals[2]={MDString::get(C, rso.str()), MDString::get(C, std::string (std::to_string(fty)))};
      MDNode *N = MDNode::get(C,vals);
      I.setMetadata("PAData", N);
      errs() << "Metadata:"<<"\n";
      errs() << cast<MDString>(I.getMetadata("PAData")->getOperand(0))->getString()<<"\n";
      errs() << cast<MDString>(I.getMetadata("PAData")->getOperand(1))->getString()<<"\n";
      errs()<<"\n";
  }

//Function to check if the pointer of an instruction operand points to a function instead of data  
  Type* isFnType(Type* Ty){
      if (PointerType * PT = dyn_cast<PointerType>(Ty)) {
         Type* ty=PT;
         //bool fty=false; //is function pointer type or not
           if (PointerType* pt = dyn_cast<PointerType>(ty)) {
                  int i=0;
            do {
              Type* pointedType = pt->getPointerElementType();
                          //errs()<<"Iteration:";
                          //errs()<<i<<"\n";
                          ++i;
                      if (pointedType->isFunctionTy()) {
                          //errs()<<"Found function pointer type"<<"\n";
                         md.setFPtrType(pointedType->isFunctionTy());
                      }
                    // This may be a pointer to a pointer to ...
                     ty = pointedType;
                    } while ((pt = dyn_cast<PointerType>(ty)));
                }
	    return ty;
           }
	return Ty;
    }

//Function to insert NOP instructions
	void insertNops(Function &F, Instruction &I){
		auto &C= F.getContext();
	    Value* zero = ConstantInt::get(Type::getInt32Ty(C),0);
	    auto* newInst = BinaryOperator::Create(Instruction::Add, zero, zero, "nop", I.getNextNode());
		MDNode *N= MDNode::get(C, MDString::get(C, "Nop Instruction"));
		newInst->setMetadata("PAData", N);
	}

  };
}
char PtrTypeMDPass::ID = 0;
static RegisterPass<PtrTypeMDPass> X("ptr-type-md-pass", "Pointer Type Metadata Pass");

