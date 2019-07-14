//===----------------------------------------------------------------------===//
//
// Author: Hans Liljestrand <hans.liljestrand@pm.me>
// Copyright: Secure Systems Group, Aalto University https://ssg.aalto.fi/
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "AArch64.h"
#include "AArch64Subtarget.h"
#include "AArch64RegisterInfo.h"
#include "llvm/PARTS/Parts.h"
#include "llvm/PARTS/PartsTypeMetadata.h"
#include "llvm/PARTS/PartsEventCount.h"
#include "PartsUtils.h"

#define DEBUG_TYPE "aarch64-parts-intrinsics"
#define TAG "PartsPasSIntrinsics"

using namespace llvm;
using namespace llvm::PARTS;

namespace {
class PartsPassIntrinsics : public MachineFunctionPass {
public:
  static char ID;

  PartsPassIntrinsics() :
      MachineFunctionPass(ID),
      log(PARTS::PartsLog::getLogger(DEBUG_TYPE))
  {
    DEBUG_PA(log->enable());
  }

  StringRef getPassName() const override { return "parts-intrinsics"; }

  bool doInitialization(Module &M) override;
  bool runOnMachineFunction(MachineFunction &) override;

private:

  PartsLog_ptr log;

  //const TargetMachine *TM = nullptr;
  const AArch64Subtarget *STI = nullptr;
  const AArch64InstrInfo *TII = nullptr;
  const AArch64RegisterInfo *TRI = nullptr;
  PartsUtils_ptr partsUtils;

  Function *funcCountDataStr = nullptr;
  Function *funcCountNonleafCall = nullptr;
  Function *funcCountLeafCall = nullptr;
};
} // end anonymous namespace

FunctionPass *llvm::createPartsPassIntrinsics() {
  return new PartsPassIntrinsics();
}

char PartsPassIntrinsics::ID = 0;

bool PartsPassIntrinsics::doInitialization(Module &M) {
  funcCountDataStr = PartsEventCount::getFuncDataStr(M);
  funcCountNonleafCall = PartsEventCount::getFuncNonleafCall(M);
  funcCountLeafCall = PartsEventCount::getFuncLeafCall(M);
  return true;
}

bool PartsPassIntrinsics::runOnMachineFunction(MachineFunction &MF) {
  bool found = false;

  STI = &MF.getSubtarget<AArch64Subtarget>();
  TII = STI->getInstrInfo();
  //TM = &MF.getTarget();;
  TRI = STI->getRegisterInfo();
  partsUtils = PartsUtils::get(TRI, TII);

  bool foundReturnSign = false;

  for (auto &MBB : MF) {
    for (auto MIi = MBB.instr_begin(); MIi != MBB.instr_end(); MIi++) {
      DEBUG_PA(log->debug(MF.getName()) << MF.getName() << "->" << MBB.getName() << "->" << MIi);

      const auto MIOpcode = MIi->getOpcode();

      switch(MIOpcode) {
        default:
          break;
        case AArch64::PACIB: {
          // FIXME: This return address signing counting should probably be properly put somewhere else...
          // Should however work as long as we only use PACIB for return address signing.
          if (PARTS::useRuntimeStats()) {
            const auto &DL = MIi->getDebugLoc();
            partsUtils->addEventCallFunction(MBB, *MIi, DL, funcCountNonleafCall);
            foundReturnSign = true;
            found = true;
          }
          break;
        }
        case AArch64::PARTS_PACDA:
        case AArch64::PARTS_AUTDA:
          const auto &DL = MIi->getDebugLoc();
          const unsigned dst = MIi->getOperand(0).getReg();
          const unsigned src = MIi->getOperand(1).getReg();
          unsigned mod = MIi->getOperand(2).getReg();

          // Save the mod register if it is marked as killable!
          if (MIi->getOperand(2).isKill()) {
            unsigned oldMod = mod;
            mod = PARTS::getModifierReg();
            BuildMI(MBB, MIi, DL, TII->get(AArch64::ADDXri), mod).addReg(oldMod).addImm(0).addImm(0);
          }
          // Move the pointer to destination register
          BuildMI(MBB, MIi, DL, TII->get(AArch64::ADDXri), dst).addReg(src).addImm(0).addImm(0);

          // Insert appropriate PA instruction
          if (MIOpcode == AArch64::PARTS_PACDA) {
            log->inc(TAG ".pacda", true) << "converting PARTS_PACDA\n";
            partsUtils->insertPAInstr(MBB, MIi, dst, mod, TII->get(AArch64::PACDA), DL);
            partsUtils->addEventCallFunction(MBB, *MIi, DL, funcCountDataStr);
          } else if (MIOpcode == AArch64::PARTS_AUTDA) {
            assert(false && "this isn't currently used, and should be updated if its gonna be");
            log->inc(TAG ".autda", true) << "converting PARTS_AUTDA\n";
            partsUtils->insertPAInstr(MBB, MIi, dst, mod, TII->get(AArch64::AUTDA), DL);
          }

          // And finally, remove the intrinsic
          auto tmp = MIi;
          MIi--;
          tmp->removeFromParent();

          found = true; // make sure we return true when we modify stuff

          break;
      }

    }
  }

  if (PARTS::useRuntimeStats()) {
    if (!foundReturnSign) {
      // if this function was instrumented we should already have found the PACIB earlier
      // Instrumenting the return instead, since instrumenting the entry prooved unreliable (guessing
      // empty functions, weird entry basic blocks, etc)
      for (auto &MBB : MF) {
        for (auto MIi = MBB.instr_begin(); MIi != MBB.instr_end(); MIi++) {
          if (MIi->isReturn()) {
            partsUtils->addEventCallFunction(MBB, *MIi, MIi->getDebugLoc(), funcCountLeafCall);
            found = true;
          }
        }
      }
    }
  }

  return found;
}
