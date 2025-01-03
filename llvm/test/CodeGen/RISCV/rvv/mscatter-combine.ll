; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=riscv32 -mattr=+m,+v -target-abi=ilp32d \
; RUN:     -verify-machineinstrs < %s | FileCheck %s --check-prefixes=RV32
; RUN: llc -mtriple=riscv64 -mattr=+m,+v -target-abi=lp64d \
; RUN:     -verify-machineinstrs < %s | FileCheck %s --check-prefixes=RV64

%struct = type { i64, i64, ptr, i32, i32, i32, [4 x i32] }

define void @complex_gep(ptr %p, <vscale x 2 x i64> %vec.ind, <vscale x 2 x i1> %m) {
; RV32-LABEL: complex_gep:
; RV32:       # %bb.0:
; RV32-NEXT:    vsetvli a1, zero, e32, m1, ta, ma
; RV32-NEXT:    vnsrl.wi v10, v8, 0
; RV32-NEXT:    li a1, 48
; RV32-NEXT:    addi a0, a0, 28
; RV32-NEXT:    vmul.vx v8, v10, a1
; RV32-NEXT:    vmv.v.i v9, 0
; RV32-NEXT:    vsoxei32.v v9, (a0), v8, v0.t
; RV32-NEXT:    ret
;
; RV64-LABEL: complex_gep:
; RV64:       # %bb.0:
; RV64-NEXT:    li a1, 56
; RV64-NEXT:    addi a0, a0, 32
; RV64-NEXT:    vsetvli a2, zero, e64, m2, ta, ma
; RV64-NEXT:    vmul.vx v8, v8, a1
; RV64-NEXT:    vsetvli zero, zero, e32, m1, ta, ma
; RV64-NEXT:    vmv.v.i v10, 0
; RV64-NEXT:    vsoxei64.v v10, (a0), v8, v0.t
; RV64-NEXT:    ret
  %gep = getelementptr inbounds %struct, ptr %p, <vscale x 2 x i64> %vec.ind, i32 5
  call void @llvm.masked.scatter.nxv2i32.nxv2p0(<vscale x 2 x i32> zeroinitializer, <vscale x 2 x ptr> %gep, i32 8, <vscale x 2 x i1> %m)
  ret void
}

define void @strided_store_zero_start(i64 %n, ptr %p) {
; RV32-LABEL: strided_store_zero_start:
; RV32:       # %bb.0:
; RV32-NEXT:    vsetvli a0, zero, e64, m1, ta, ma
; RV32-NEXT:    vid.v v8
; RV32-NEXT:    li a0, 48
; RV32-NEXT:    addi a1, a2, 32
; RV32-NEXT:    vsetvli zero, zero, e32, mf2, ta, ma
; RV32-NEXT:    vnsrl.wi v8, v8, 0
; RV32-NEXT:    vmul.vx v8, v8, a0
; RV32-NEXT:    vsetvli zero, zero, e64, m1, ta, ma
; RV32-NEXT:    vmv.v.i v9, 0
; RV32-NEXT:    vsoxei32.v v9, (a1), v8
; RV32-NEXT:    ret
;
; RV64-LABEL: strided_store_zero_start:
; RV64:       # %bb.0:
; RV64-NEXT:    addi a0, a1, 36
; RV64-NEXT:    vsetvli a1, zero, e64, m1, ta, ma
; RV64-NEXT:    vmv.v.i v8, 0
; RV64-NEXT:    li a1, 56
; RV64-NEXT:    vsse64.v v8, (a0), a1
; RV64-NEXT:    ret
  %step = tail call <vscale x 1 x i64> @llvm.stepvector.nxv1i64()
  %gep = getelementptr inbounds %struct, ptr %p, <vscale x 1 x i64> %step, i32 6
  tail call void @llvm.masked.scatter.nxv1i64.nxv1p0(<vscale x 1 x i64> zeroinitializer, <vscale x 1 x ptr> %gep, i32 8, <vscale x 1 x i1> splat (i1 true))
  ret void
}

define void @strided_store_offset_start(i64 %n, ptr %p) {
; RV32-LABEL: strided_store_offset_start:
; RV32:       # %bb.0:
; RV32-NEXT:    vsetvli a1, zero, e64, m1, ta, ma
; RV32-NEXT:    vid.v v8
; RV32-NEXT:    vadd.vx v8, v8, a0
; RV32-NEXT:    li a0, 48
; RV32-NEXT:    addi a1, a2, 32
; RV32-NEXT:    vsetvli zero, zero, e32, mf2, ta, ma
; RV32-NEXT:    vnsrl.wi v8, v8, 0
; RV32-NEXT:    vmul.vx v8, v8, a0
; RV32-NEXT:    vsetvli zero, zero, e64, m1, ta, ma
; RV32-NEXT:    vmv.v.i v9, 0
; RV32-NEXT:    vsoxei32.v v9, (a1), v8
; RV32-NEXT:    ret
;
; RV64-LABEL: strided_store_offset_start:
; RV64:       # %bb.0:
; RV64-NEXT:    slli a2, a0, 3
; RV64-NEXT:    slli a0, a0, 6
; RV64-NEXT:    vsetvli a3, zero, e64, m1, ta, ma
; RV64-NEXT:    vmv.v.i v8, 0
; RV64-NEXT:    sub a0, a0, a2
; RV64-NEXT:    add a0, a1, a0
; RV64-NEXT:    addi a0, a0, 36
; RV64-NEXT:    li a1, 56
; RV64-NEXT:    vsse64.v v8, (a0), a1
; RV64-NEXT:    ret
  %step = tail call <vscale x 1 x i64> @llvm.stepvector.nxv1i64()
  %.splatinsert = insertelement <vscale x 1 x i64> poison, i64 %n, i64 0
  %.splat = shufflevector <vscale x 1 x i64> %.splatinsert, <vscale x 1 x i64> poison, <vscale x 1 x i32> zeroinitializer
  %add = add <vscale x 1 x i64> %step, %.splat
  %gep = getelementptr inbounds %struct, ptr %p, <vscale x 1 x i64> %add, i32 6
  tail call void @llvm.masked.scatter.nxv1i64.nxv1p0(<vscale x 1 x i64> zeroinitializer, <vscale x 1 x ptr> %gep, i32 8, <vscale x 1 x i1> splat (i1 true))
  ret void
}

define void @stride_one_store(i64 %n, ptr %p) {
; RV32-LABEL: stride_one_store:
; RV32:       # %bb.0:
; RV32-NEXT:    vsetvli a0, zero, e64, m1, ta, ma
; RV32-NEXT:    vid.v v8
; RV32-NEXT:    vsetvli zero, zero, e32, mf2, ta, ma
; RV32-NEXT:    vnsrl.wi v8, v8, 0
; RV32-NEXT:    vsll.vi v8, v8, 3
; RV32-NEXT:    vsetvli zero, zero, e64, m1, ta, ma
; RV32-NEXT:    vmv.v.i v9, 0
; RV32-NEXT:    vsoxei32.v v9, (a2), v8
; RV32-NEXT:    ret
;
; RV64-LABEL: stride_one_store:
; RV64:       # %bb.0:
; RV64-NEXT:    vsetvli a0, zero, e64, m1, ta, ma
; RV64-NEXT:    vmv.v.i v8, 0
; RV64-NEXT:    vs1r.v v8, (a1)
; RV64-NEXT:    ret
  %step = tail call <vscale x 1 x i64> @llvm.stepvector.nxv1i64()
  %gep = getelementptr inbounds i64, ptr %p, <vscale x 1 x i64> %step
  tail call void @llvm.masked.scatter.nxv1i64.nxv1p0(<vscale x 1 x i64> zeroinitializer, <vscale x 1 x ptr> %gep, i32 8, <vscale x 1 x i1> splat (i1 true))
  ret void
}

declare <vscale x 1 x i64> @llvm.stepvector.nxv1i64()
declare void @llvm.masked.scatter.nxv2i32.nxv2p0(<vscale x 2 x i32>, <vscale x 2 x ptr>, i32, <vscale x 2 x i1>)
declare void @llvm.masked.scatter.nxv1i64.nxv1p0(<vscale x 1 x i64>, <vscale x 1 x ptr>, i32, <vscale x 1 x i1>)
