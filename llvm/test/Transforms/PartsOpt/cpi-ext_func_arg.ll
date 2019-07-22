; ------------------------------------------------------------------------
; Author: Hans Liljestrand <hans.liljestrand@pm.me>
; Copyright (C) 2018 Secure Systems Group, Aalto University <ssg.aalto.fi>
;
; This file is distributed under the University of Illinois Open Source
; License. See LICENSE.TXT for details.
; ------------------------------------------------------------------------
; RUN: opt -parts-fecfi -parts-opt-cpi -S < %s | FileCheck %s
;
; Because the stdlib qsort is uninstrumented it cannot use a signed code pointer
; for the compare function. Instead, the instrumentation should realize the it
; is calling an external function and leave the pointer unsigned.
;

@arr = hidden global [5 x i32] [i32 9, i32 8, i32 4, i32 2, i32 1], align 4

declare i32 @cmpfunc(i8* nocapture readonly %a, i8* nocapture readonly %b) #0;

; CHECK-LABEL: @sorter
; CHECK-NOT: pacia
; CHECK-NOT: autia
; CHECK: tail call void @qsort
; CHECK: ret void
define hidden void @sorter(i8* %ptr, i64 %len, i64 %el_size) local_unnamed_addr #1 {
entry:
  tail call void @qsort(i8* %ptr, i64 %len, i64 %el_size, i32 (i8*, i8*)* nonnull @cmpfunc) #3
  ret void
}

declare void @qsort(i8*, i64, i64, i32 (i8*, i8*)* nocapture) local_unnamed_addr #2

attributes #0 = { norecurse nounwind readonly }
; "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
; "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+neon" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }
