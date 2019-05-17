; ------------------------------------------------------------------------
; Author: Carlos Chinea Perez <carlos.chinea.perez@huawei.com>
; Copyright (C) 2019 Huawei Finland Oy
;
; This file is distributed under the University of Illinois Open Source
; License. See LICENSE.TXT for details.
; ------------------------------------------------------------------------
; Test that we insert our spill/reload pseudo instruction for data pointer arguments
;
; RUN: llc -mtriple=aarch64-none-linux-gnu -mattr=v8.3a -parts-dpi -verify-machineinstrs --verify-regalloc --stop-after=greedy < %s | FileCheck %s
;

define void @spill_data_pointer(i64* %d0, i64* %d1, i64* %d2, i64 %d3, i64 %d4, i64 %d5, i64 %d6, i64 %d7, i64* %dataptronstack) {
entry:
  %0 = call i64* @llvm.parts.data.pointer.argument.p0i64(i64* %d0)
  %1 = call i64* @llvm.parts.data.pointer.argument.p0i64(i64* %d1)
  %2 = call i64* @llvm.parts.data.pointer.argument.p0i64(i64* %d2)
  %3 = call i64* @llvm.parts.data.pointer.argument.p0i64(i64* %dataptronstack)
; CHECK: PARTS_SPILL {{%[0-9]+}}, [[STACK1:%stack.[0-9]+]]
; CHECK: PARTS_SPILL {{%[0-9]+}}, [[STACK2:%stack.[0-9]+]]
  call void asm sideeffect "nop", "~{x0},~{x1},~{x2},~{x3},~{x4},~{x5},~{x6},~{x7},~{x8},~{x9},~{x10},~{x11},~{x12},~{x13},~{x14},~{x15},~{x16},~{x17},~{x18},~{x19},~{x20},~{x21},~{x22},~{x23},~{x24},~{x25},~{x26},~{x27},~{x28},~{x29},~{x30},~{x31}"() #1
  call void @func(i64* %0, i64* %1, i64* %2, i64 %d3, i64 %d4, i64 %d5, i64 %d6, i64 %d7, i64* %3)
; CHECK: PARTS_RELOAD [[STACK1]]
; CHECK: PARTS_RELOAD [[STACK2]]
  ret void
}

declare void @func(i64*, i64*, i64*, i64, i64, i64, i64, i64, i64*)

; Function Attrs: nounwind readnone
declare i64* @llvm.parts.data.pointer.argument.p0i64(i64*) #0

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
