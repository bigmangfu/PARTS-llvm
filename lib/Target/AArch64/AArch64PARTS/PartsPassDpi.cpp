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
// LLVM includes
#include "AArch64.h"
#include "AArch64Subtarget.h"
#include "AArch64RegisterInfo.h"
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
// PARTS includes
#include "llvm/PARTS/PartsTypeMetadata.h"
#include "llvm/PARTS/PartsLog.h"
#include "llvm/PARTS/Parts.h"
#include "PartsUtils.h"

#define DEBUG_TYPE "aarch64-parts-dpi"

using namespace llvm;
using namespace llvm::PARTS;

#define skipIfB(ifx, fName, stat, b, string) do {  \
    if ((ifx)) {                            \
      log->inc(stat, b, fName) << string;          \
      return false;                         \
    }                                       \
} while(false)

#define skipIfN(ifx, fName, stat, string) do {     \
    if ((ifx)) {                            \
      log->inc(stat, fName) << string;             \
      return false;                         \
    }                                       \
} while (false)

namespace {
 class PartsPassDpi : public MachineFunctionPass {

 public:
   static char ID;

   PartsPassDpi() :
       MachineFunctionPass(ID),
       log(PARTS::PartsLog::getLogger(DEBUG_TYPE))
   {
     DEBUG_PA(log->enable());
   }

   StringRef getPassName() const override { return DEBUG_TYPE; }

   bool doInitialization(Module &M) override;
   bool runOnMachineFunction(MachineFunction &) override;
   bool instrumentLoadStore(MachineFunction &MF, MachineBasicBlock &MBB, MachineBasicBlock::instr_iterator &MIi);
   bool instrumentBranches(MachineFunction &MF, MachineBasicBlock &MBB, MachineBasicBlock::instr_iterator &MIi);

 private:

   PartsLog_ptr log;

   const TargetMachine *TM = nullptr;
   const AArch64Subtarget *STI = nullptr;
   const AArch64InstrInfo *TII = nullptr;
   const AArch64RegisterInfo *TRI = nullptr;
   PartsUtils_ptr  partsUtils = nullptr;
   int m_PACed_me_a_live_one = false; // FIXME: horrible hack!
   MachineOperand *m_the_live_one = nullptr;
 };
} // end anonymous namespace

FunctionPass *llvm::createPartsPassDpi() {
  return new PartsPassDpi();
}

char PartsPassDpi::ID = 0;

bool PartsPassDpi::doInitialization(Module &M) {
  return false;
}

bool PartsPassDpi::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << getPassName() << ", function " << MF.getName() << '\n');
  TM = &MF.getTarget();;
  STI = &MF.getSubtarget<AArch64Subtarget>();
  TII = STI->getInstrInfo();
  TRI = STI->getRegisterInfo();
  partsUtils = PartsUtils::get(TRI, TII);

  for (auto &MBB : MF) {
    for (auto MIi = MBB.instr_begin(); MIi != MBB.instr_end(); MIi++) {
      DEBUG_PA(log->debug(MF.getName()) << MF.getName() << "->" << MBB.getName() << "->" << MIi);

      if (partsUtils->isLoadOrStore(*MIi)) {
        instrumentLoadStore(MF, MBB, MIi);
      }

      if (m_PACed_me_a_live_one > 1) {
        // llvm_unreachable("didn't immediately get rid of un-killed PACed store");
        m_PACed_me_a_live_one = 0;
        m_the_live_one = nullptr;
      } else if (m_PACed_me_a_live_one > 0) {
        m_PACed_me_a_live_one++;
      }
    }
  }

  return true;
}

bool PartsPassDpi::instrumentLoadStore(MachineFunction &MF, MachineBasicBlock &MBB,
                                         MachineBasicBlock::instr_iterator &MIi) {
  assert(partsUtils->isLoadOrStore(*MIi));

  auto partsType = PartsTypeMetadata::retrieve(*MIi);
  auto &C = MF.getFunction().getContext();
  const auto fName = MF.getName();

  const auto MIOpcode = MIi->getOpcode();
  const auto MIName = TII->getName(MIOpcode).str();

  DEBUG_PA(log->debug(fName) << "found a load/store (" << TII->getName(MIOpcode) << ")\n");

  if (partsType == nullptr) {
    DEBUG_PA(log->debug(fName) << "trying to figure out type_id\n");
    auto Op = MIi->getOperand(0);
    const auto targetReg = Op.getReg();

    if (!partsUtils->checkIfRegInstrumentable(targetReg)) {
      partsType = PartsTypeMetadata::getIgnored();
    } else {
      if (partsUtils->isStore(*MIi)) {
        partsType = partsUtils->inferPauthTypeIdRegBackwards(MF, MBB, *MIi, targetReg);
      } else {
        // FIXME: this only supports loads of type load reg [reg, imm]
        if (MIi->getOperand(2).isImm()) {
          partsType = partsUtils->inferPauthTypeIdStackBackwards(MF, MBB, *MIi, targetReg,
                                                                 MIi->getOperand(1).getReg(),
                                                                 MIi->getOperand(2).getImm());
        } else {
          log->error() << __FUNCTION__ << ": OMG! unexpected operands, is this a pair store thingy?\n";
          partsType = PartsTypeMetadata::getUnknown();
        }
      }
    }
    partsUtils->attach(MF.getFunction().getContext(), partsType, &*MIi);

    MIi->addOperand(MachineOperand::CreateMetadata(partsType->getMDNode(C)));
    log->inc("StoreLoad.Inferred") << "      storing type_id " << partsType->toString() << ") in current MI\n";
  }

  skipIfN(partsType->isIgnored(), fName, "StoreLoad.Ignored_" + MIName, "marked as ignored, skipping!\n");
  skipIfB(!partsType->isKnown(), fName, "StoreLoad.Unknown_" + MIName, false, "type_id is unknown!\n");
  skipIfN(!partsType->isPointer(), fName, "StoreLoad.NotAPointer_" + MIName, "not a pointer, skipping!\n");
  skipIfN(partsType->isCodePointer(), fName, "PartsPassDpi.StoreLoad.IgnoringCodePointer_" + MIName, "ignoring code pointer\n");

  auto reg = MIi->getOperand(0).getReg();
  const auto modReg = PARTS::getModifierReg();
  const auto type_id = partsType->getTypeId();

  if (partsUtils->isStore(*MIi)) {
    if (PARTS::useDpi()) {
      log->inc("StoreLoad.InstrumentedDataStore", true) << "instrumenting store" << partsType->toString() << "\n";

      // FIXME: Horrible hack for double define!
      if (m_PACed_me_a_live_one == 0) {
        partsUtils->pacDataPointer(MBB, MIi, reg, modReg, type_id, MIi->getDebugLoc());
      } else {
        assert(m_the_live_one->getReg() == MIi->getOperand(0).getReg());
      }

      if (MIi->getOperand(0).isKill()) {
        m_PACed_me_a_live_one = 0;
      } else {
        m_PACed_me_a_live_one = 1;
        m_the_live_one = &(MIi->getOperand(0));
      }

      return true;
    }
  } else {
    auto loc = MIi;
    MIi->getDebugLoc();
    loc++;
    if (PARTS::useDpi()) {
      log->inc("StoreLoad.InstrumentedDataLoad", true, fName) << "instrumenting load with " << partsType->toString() << "\n";
      partsUtils->autDataPointer(MBB, loc, reg, modReg, type_id, MIi->getDebugLoc());
      return true;
    }
  }

  return false;
}
