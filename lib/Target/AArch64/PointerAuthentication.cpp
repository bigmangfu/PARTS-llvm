/*
 * PointerAuthentication.cpp
 * Copyright (C) 2018 Hans Liljestrand <hans.liljestrand@pm.me>
 *
 * Distributed under terms of the MIT license.
 */

#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Metadata.h>
#include "PointerAuthentication.h"
#include "AArch64Subtarget.h"

using namespace llvm;
using namespace llvm::PA;

const MDNode *llvm::PA::getPAData(MachineInstr &MI) {
  // FIXME: Don't just check the last operand
  // FIXME: Verify the found data is correct type
  auto op = MI.getOperand(MI.getNumOperands()-1);

  if (op.isMetadata()) {
    return op.getMetadata();
  }
  return nullptr;
}

bool llvm::PA::isInstrPointer(const MDNode *paData) {
  return cast<MDString>(paData->getOperand(1))->getString().equals("1");
}

bool llvm::PA::isStore(MachineInstr &MI) {
  switch(MI.getOpcode()) {
    default:
      return false;
    case AArch64::STRWpost:
    case AArch64::STURQi:
    case AArch64::STURXi:
    case AArch64::STURDi:
    case AArch64::STURWi:
    case AArch64::STURSi:
    case AArch64::STURHi:
    case AArch64::STURHHi:
    case AArch64::STURBi:
    case AArch64::STURBBi:
    case AArch64::STPQi:
    case AArch64::STNPQi:
    case AArch64::STRQui:
    case AArch64::STPXi:
    case AArch64::STPDi:
    case AArch64::STNPXi:
    case AArch64::STNPDi:
    case AArch64::STRXui:
    case AArch64::STRDui:
    case AArch64::STPWi:
    case AArch64::STPSi:
    case AArch64::STNPWi:
    case AArch64::STNPSi:
    case AArch64::STRWui:
    case AArch64::STRSui:
    case AArch64::STRHui:
    case AArch64::STRHHui:
    case AArch64::STRBui:
    case AArch64::STRBBui:
      return true;
  }
}

bool llvm::PA::isLoad(MachineInstr &MI) {
  switch(MI.getOpcode()) {
    default:
      return false;
    case AArch64::LDPXi:
    case AArch64::LDPDi:
    case AArch64::LDRWpost:
    case AArch64::LDURQi:
    case AArch64::LDURXi:
    case AArch64::LDURDi:
    case AArch64::LDURWi:
    case AArch64::LDURSi:
    case AArch64::LDURSWi:
    case AArch64::LDURHi:
    case AArch64::LDURHHi:
    case AArch64::LDURSHXi:
    case AArch64::LDURSHWi:
    case AArch64::LDURBi:
    case AArch64::LDURBBi:
    case AArch64::LDURSBXi:
    case AArch64::LDURSBWi:
    case AArch64::LDPQi:
    case AArch64::LDNPQi:
    case AArch64::LDRQui:
    case AArch64::LDNPXi:
    case AArch64::LDNPDi:
    case AArch64::LDRXui:
    case AArch64::LDRDui:
    case AArch64::LDPWi:
    case AArch64::LDPSi:
    case AArch64::LDNPWi:
    case AArch64::LDNPSi:
    case AArch64::LDRWui:
    case AArch64::LDRSui:
    case AArch64::LDRSWui:
    case AArch64::LDRHui:
    case AArch64::LDRHHui:
    case AArch64::LDRBui:
    case AArch64::LDRBBui:
      return true;
  }
}

void llvm::PA::buildPAC(const TargetInstrInfo &TII,
                        MachineBasicBlock &MBB, MachineBasicBlock::iterator iter,
                        const DebugLoc &DL, unsigned ctxReg, unsigned ptrReg) {
  errs() << "\t\t#################Adding PACIA instruction\n";

  BuildMI(MBB, iter, DebugLoc(), TII.get(AArch64::MOVZWi))
      .addReg(ctxReg)
      .addImm(0)
      .addImm(0);
  BuildMI(MBB, iter, DebugLoc(), TII.get(AArch64::PACIA))
      .addReg(ptrReg)
      .addReg(ctxReg);
}

void llvm::PA::instrumentEpilogue(const TargetInstrInfo *TII,
                                  MachineBasicBlock &MBB, MachineBasicBlock::iterator &MBBI,
                                  const DebugLoc &DL, const bool IsTailCallReturn) {
  if (!IsTailCallReturn) {
    assert(MBBI != MBB.end());
    BuildMI(MBB, MBBI, DebugLoc(), TII->get(AArch64::RETAA));
    MBB.erase(MBBI);
  } else {
    BuildMI(MBB, MBBI, DebugLoc(), TII->get(AArch64::AUTIASP));
  }
}

pauth_type_id PA::createPauthTypeId(const Type *Ty)
{
  if (!Ty->isPointerTy())
    return 0;

  pauth_type_id type_id = type_id_mask_ptr;

  if (Ty->getPointerElementType()->isFunctionTy()) {
    type_id = type_id | type_id_mask_instr;
  }

  return type_id;
}

pauth_type_id PA::getPauthTypeId(const Constant *C)
{
  return (pauth_type_id) C->getUniqueInteger().getLimitedValue(1UL << 63);
}

pauth_type_id PA::getPauthTypeId(const MDNode *PAMDNode)
{
  return getPauthTypeId(getPauthTypeIdConstant(PAMDNode));
}

Constant *PA::getPauthTypeIdConstant(const MDNode *PAMDNode)
{
  assert(PAMDNode->getNumOperands() == 1);
  assert(isa<MDNode>(PAMDNode->getOperand(0)));

  const auto *MDN = dyn_cast<MDNode>(PAMDNode->getOperand(0));

  assert(MDN->getNumOperands() == 1);
  assert(isa<ConstantAsMetadata>(MDN->getOperand(0)));

  return dyn_cast<ConstantAsMetadata>(MDN->getOperand(0))->getValue();
}


MDNode *PA::createPauthMDNode(LLVMContext &C, const Type *Ty)
{
  const pauth_type_id type_id = createPauthTypeId(Ty);

  Metadata* vals[1] = { ConstantAsMetadata::get(Constant::getIntegerValue(Type::getInt32Ty(C), APInt(64, type_id))) };
  return MDNode::get(C, vals);
}
