; Check that we insert the needed functionality to authenticate a global nested struct with data pointers
; RUN: opt -load LLVMPtrTypeMDPass.so -parts-dpi -pauth-markglobals -S < %s  | FileCheck %s

%struct.extra_data = type { %struct.data, i32 }
%struct.data = type { %struct.data*, i32 }

@global_struct_data = global %struct.extra_data { %struct.data { %struct.data* getelementptr inbounds (%struct.extra_data, %struct.extra_data* @global_struct_data, i32 0, i32 0), i32 -559038737 }, i32 -623899219 }, align 8

define i32 @main() {
entry:
; CHECK: call void @__pauth_pac_globals()
  %retval = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = load i32, i32* getelementptr inbounds (%struct.extra_data, %struct.extra_data* @global_struct_data, i32 0, i32 1), align 8
  ret i32 %0
}

; CHECK: define void @__pauth_pac_globals() #0 {
; CHECK: entry:
; CHECK:   %0 = load %struct.data*, %struct.data** getelementptr inbounds (%struct.extra_data, %struct.extra_data* @global_struct_data, i32 0, i32 0, i32 0)
; CHECK:   %1 = call %struct.data* @llvm.pa.pacda.p0s_struct.datas(%struct.data* %0, i64 -1989659990953377005)
; CHECK:   store %struct.data* %1, %struct.data** getelementptr inbounds (%struct.extra_data, %struct.extra_data* @global_struct_data, i32 0, i32 0, i32 0)
; CHECK:   ret void
; CHECK: }

; CHECK: ; Function Attrs: nounwind readnone
; CHECK: declare %struct.data* @llvm.pa.pacda.p0s_struct.datas(%struct.data*, i64) #1

; CHECK: attributes #0 = { "no-parts"="true" }
; CHECK: attributes #1 = { nounwind readnone }
