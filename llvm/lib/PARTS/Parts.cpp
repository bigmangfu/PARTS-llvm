//===----------------------------------------------------------------------===//
//
// Author: Hans Liljestrand <hans.liljestrand@pm.me>
// Copyright: Secure Systems Group, Aalto University https://ssg.aalto.fi/
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <llvm/PARTS/Parts.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<bool> EnablePartsBeCfi("parts-becfi", cl::Hidden,
                                      cl::desc("PARTS backward-edge CFI"),
                                      cl::init(false));

static cl::opt<bool> EnablePartsFeCfi("parts-fecfi", cl::Hidden,
                                      cl::desc("PARTS backward-edge CFI"),
                                      cl::init(false));

static cl::opt<bool> EnablePartsDpi("parts-dpi", cl::Hidden,
                                    cl::desc("PARTS backward-edge CFI"),
                                    cl::init(false));

bool llvm::PARTS::useBeCfi() {
  return EnablePartsBeCfi;
}

bool llvm::PARTS::useFeCfi() {
  return EnablePartsFeCfi;
}

bool llvm::PARTS::useDpi() {
  return EnablePartsDpi;
}

bool llvm::PARTS::useAny() {
  return EnablePartsDpi || EnablePartsFeCfi || EnablePartsBeCfi;
}
