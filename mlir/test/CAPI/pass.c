//===- pass.c - Simple test of C APIs -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/* RUN: mlir-capi-pass-test 2>&1 | FileCheck %s
 */

#include "mlir-c/Pass.h"
#include "mlir-c/Dialect/Func.h"
#include "mlir-c/IR.h"
#include "mlir-c/RegisterEverything.h"
#include "mlir-c/Transforms.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void registerAllUpstreamDialects(MlirContext ctx) {
  MlirDialectRegistry registry = mlirDialectRegistryCreate();
  mlirRegisterAllDialects(registry);
  mlirContextAppendDialectRegistry(ctx, registry);
  mlirDialectRegistryDestroy(registry);
}

void testRunPassOnModule(void) {
  MlirContext ctx = mlirContextCreate();
  registerAllUpstreamDialects(ctx);

  const char *funcAsm = //
      "func.func @foo(%arg0 : i32) -> i32 {   \n"
      "  %res = arith.addi %arg0, %arg0 : i32 \n"
      "  return %res : i32                    \n"
      "}                                      \n";
  MlirOperation func =
      mlirOperationCreateParse(ctx, mlirStringRefCreateFromCString(funcAsm),
                               mlirStringRefCreateFromCString("funcAsm"));
  if (mlirOperationIsNull(func)) {
    fprintf(stderr, "Unexpected failure parsing asm.\n");
    exit(EXIT_FAILURE);
  }

  // Run the print-op-stats pass on the top-level module:
  // CHECK-LABEL: Operations encountered:
  // CHECK: arith.addi        , 1
  // CHECK: func.func      , 1
  // CHECK: func.return        , 1
  {
    MlirPassManager pm = mlirPassManagerCreate(ctx);
    MlirPass printOpStatPass = mlirCreateTransformsPrintOpStats();
    mlirPassManagerAddOwnedPass(pm, printOpStatPass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, func);
    if (mlirLogicalResultIsFailure(success)) {
      fprintf(stderr, "Unexpected failure running pass manager.\n");
      exit(EXIT_FAILURE);
    }
    mlirPassManagerDestroy(pm);
  }
  mlirOperationDestroy(func);
  mlirContextDestroy(ctx);
}

void testRunPassOnNestedModule(void) {
  MlirContext ctx = mlirContextCreate();
  registerAllUpstreamDialects(ctx);

  const char *moduleAsm = //
      "module {                                   \n"
      "  func.func @foo(%arg0 : i32) -> i32 {     \n"
      "    %res = arith.addi %arg0, %arg0 : i32   \n"
      "    return %res : i32                      \n"
      "  }                                        \n"
      "  module {                                 \n"
      "    func.func @bar(%arg0 : f32) -> f32 {   \n"
      "      %res = arith.addf %arg0, %arg0 : f32 \n"
      "      return %res : f32                    \n"
      "    }                                      \n"
      "  }                                        \n"
      "}                                          \n";
  MlirOperation module =
      mlirOperationCreateParse(ctx, mlirStringRefCreateFromCString(moduleAsm),
                               mlirStringRefCreateFromCString("moduleAsm"));
  if (mlirOperationIsNull(module))
    exit(1);

  // Run the print-op-stats pass on functions under the top-level module:
  // CHECK-LABEL: Operations encountered:
  // CHECK: arith.addi        , 1
  // CHECK: func.func      , 1
  // CHECK: func.return        , 1
  {
    MlirPassManager pm = mlirPassManagerCreate(ctx);
    MlirOpPassManager nestedFuncPm = mlirPassManagerGetNestedUnder(
        pm, mlirStringRefCreateFromCString("func.func"));
    MlirPass printOpStatPass = mlirCreateTransformsPrintOpStats();
    mlirOpPassManagerAddOwnedPass(nestedFuncPm, printOpStatPass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsFailure(success))
      exit(2);
    mlirPassManagerDestroy(pm);
  }
  // Run the print-op-stats pass on functions under the nested module:
  // CHECK-LABEL: Operations encountered:
  // CHECK: arith.addf        , 1
  // CHECK: func.func      , 1
  // CHECK: func.return        , 1
  {
    MlirPassManager pm = mlirPassManagerCreate(ctx);
    MlirOpPassManager nestedModulePm = mlirPassManagerGetNestedUnder(
        pm, mlirStringRefCreateFromCString("builtin.module"));
    MlirOpPassManager nestedFuncPm = mlirOpPassManagerGetNestedUnder(
        nestedModulePm, mlirStringRefCreateFromCString("func.func"));
    MlirPass printOpStatPass = mlirCreateTransformsPrintOpStats();
    mlirOpPassManagerAddOwnedPass(nestedFuncPm, printOpStatPass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsFailure(success))
      exit(2);
    mlirPassManagerDestroy(pm);
  }

  mlirOperationDestroy(module);
  mlirContextDestroy(ctx);
}

static void printToStderr(MlirStringRef str, void *userData) {
  (void)userData;
  fwrite(str.data, 1, str.length, stderr);
}

static void dontPrint(MlirStringRef str, void *userData) {
  (void)str;
  (void)userData;
}

void testPrintPassPipeline(void) {
  MlirContext ctx = mlirContextCreate();
  MlirPassManager pm = mlirPassManagerCreateOnOperation(
      ctx, mlirStringRefCreateFromCString("any"));
  // Populate the pass-manager
  MlirOpPassManager nestedModulePm = mlirPassManagerGetNestedUnder(
      pm, mlirStringRefCreateFromCString("builtin.module"));
  MlirOpPassManager nestedFuncPm = mlirOpPassManagerGetNestedUnder(
      nestedModulePm, mlirStringRefCreateFromCString("func.func"));
  MlirPass printOpStatPass = mlirCreateTransformsPrintOpStats();
  mlirOpPassManagerAddOwnedPass(nestedFuncPm, printOpStatPass);

  // Print the top level pass manager
  //      CHECK: Top-level: any(
  // CHECK-SAME:   builtin.module(func.func(print-op-stats{json=false}))
  // CHECK-SAME: )
  fprintf(stderr, "Top-level: ");
  mlirPrintPassPipeline(mlirPassManagerGetAsOpPassManager(pm), printToStderr,
                        NULL);
  fprintf(stderr, "\n");

  // Print the pipeline nested one level down
  // CHECK: Nested Module: builtin.module(func.func(print-op-stats{json=false}))
  fprintf(stderr, "Nested Module: ");
  mlirPrintPassPipeline(nestedModulePm, printToStderr, NULL);
  fprintf(stderr, "\n");

  // Print the pipeline nested two levels down
  // CHECK: Nested Module>Func: func.func(print-op-stats{json=false})
  fprintf(stderr, "Nested Module>Func: ");
  mlirPrintPassPipeline(nestedFuncPm, printToStderr, NULL);
  fprintf(stderr, "\n");

  mlirPassManagerDestroy(pm);
  mlirContextDestroy(ctx);
}

void testParsePassPipeline(void) {
  MlirContext ctx = mlirContextCreate();
  MlirPassManager pm = mlirPassManagerCreate(ctx);
  // Try parse a pipeline.
  MlirLogicalResult status = mlirParsePassPipeline(
      mlirPassManagerGetAsOpPassManager(pm),
      mlirStringRefCreateFromCString(
          "builtin.module(func.func(print-op-stats{json=false}))"),
      printToStderr, NULL);
  // Expect a failure, we haven't registered the print-op-stats pass yet.
  if (mlirLogicalResultIsSuccess(status)) {
    fprintf(
        stderr,
        "Unexpected success parsing pipeline without registering the pass\n");
    exit(EXIT_FAILURE);
  }
  // Try again after registrating the pass.
  mlirRegisterTransformsPrintOpStats();
  status = mlirParsePassPipeline(
      mlirPassManagerGetAsOpPassManager(pm),
      mlirStringRefCreateFromCString(
          "builtin.module(func.func(print-op-stats{json=false}))"),
      printToStderr, NULL);
  // Expect a failure, we haven't registered the print-op-stats pass yet.
  if (mlirLogicalResultIsFailure(status)) {
    fprintf(stderr,
            "Unexpected failure parsing pipeline after registering the pass\n");
    exit(EXIT_FAILURE);
  }

  // CHECK: Round-trip: builtin.module(func.func(print-op-stats{json=false}))
  fprintf(stderr, "Round-trip: ");
  mlirPrintPassPipeline(mlirPassManagerGetAsOpPassManager(pm), printToStderr,
                        NULL);
  fprintf(stderr, "\n");

  // Try appending a pass:
  status = mlirOpPassManagerAddPipeline(
      mlirPassManagerGetAsOpPassManager(pm),
      mlirStringRefCreateFromCString("func.func(print-op-stats{json=false})"),
      printToStderr, NULL);
  if (mlirLogicalResultIsFailure(status)) {
    fprintf(stderr, "Unexpected failure appending pipeline\n");
    exit(EXIT_FAILURE);
  }
  //      CHECK: Appended: builtin.module(
  // CHECK-SAME:   func.func(print-op-stats{json=false}),
  // CHECK-SAME:   func.func(print-op-stats{json=false})
  // CHECK-SAME: )
  fprintf(stderr, "Appended: ");
  mlirPrintPassPipeline(mlirPassManagerGetAsOpPassManager(pm), printToStderr,
                        NULL);
  fprintf(stderr, "\n");

  mlirPassManagerDestroy(pm);
  mlirContextDestroy(ctx);
}

void testParseErrorCapture(void) {
  // CHECK-LABEL: testParseErrorCapture:
  fprintf(stderr, "\nTEST: testParseErrorCapture:\n");

  MlirContext ctx = mlirContextCreate();
  MlirPassManager pm = mlirPassManagerCreate(ctx);
  MlirOpPassManager opm = mlirPassManagerGetAsOpPassManager(pm);
  MlirStringRef invalidPipeline = mlirStringRefCreateFromCString("invalid");

  // CHECK: mlirParsePassPipeline:
  // CHECK: expected pass pipeline to be wrapped with the anchor operation type
  fprintf(stderr, "mlirParsePassPipeline:\n");
  if (mlirLogicalResultIsSuccess(
          mlirParsePassPipeline(opm, invalidPipeline, printToStderr, NULL)))
    exit(EXIT_FAILURE);
  fprintf(stderr, "\n");

  // CHECK: mlirOpPassManagerAddPipeline:
  // CHECK: 'invalid' does not refer to a registered pass or pass pipeline
  fprintf(stderr, "mlirOpPassManagerAddPipeline:\n");
  if (mlirLogicalResultIsSuccess(mlirOpPassManagerAddPipeline(
          opm, invalidPipeline, printToStderr, NULL)))
    exit(EXIT_FAILURE);
  fprintf(stderr, "\n");

  // Make sure all output is going through the callback.
  // CHECK: dontPrint: <>
  fprintf(stderr, "dontPrint: <");
  if (mlirLogicalResultIsSuccess(
          mlirParsePassPipeline(opm, invalidPipeline, dontPrint, NULL)))
    exit(EXIT_FAILURE);
  if (mlirLogicalResultIsSuccess(
          mlirOpPassManagerAddPipeline(opm, invalidPipeline, dontPrint, NULL)))
    exit(EXIT_FAILURE);
  fprintf(stderr, ">\n");

  mlirPassManagerDestroy(pm);
  mlirContextDestroy(ctx);
}

struct TestExternalPassUserData {
  int constructCallCount;
  int destructCallCount;
  int initializeCallCount;
  int cloneCallCount;
  int runCallCount;
};
typedef struct TestExternalPassUserData TestExternalPassUserData;

void testConstructExternalPass(void *userData) {
  ++((TestExternalPassUserData *)userData)->constructCallCount;
}

void testDestructExternalPass(void *userData) {
  ++((TestExternalPassUserData *)userData)->destructCallCount;
}

MlirLogicalResult testInitializeExternalPass(MlirContext ctx, void *userData) {
  ++((TestExternalPassUserData *)userData)->initializeCallCount;
  return mlirLogicalResultSuccess();
}

MlirLogicalResult testInitializeFailingExternalPass(MlirContext ctx,
                                                    void *userData) {
  ++((TestExternalPassUserData *)userData)->initializeCallCount;
  return mlirLogicalResultFailure();
}

void *testCloneExternalPass(void *userData) {
  ++((TestExternalPassUserData *)userData)->cloneCallCount;
  return userData;
}

void testRunExternalPass(MlirOperation op, MlirExternalPass pass,
                         void *userData) {
  ++((TestExternalPassUserData *)userData)->runCallCount;
}

void testRunExternalFuncPass(MlirOperation op, MlirExternalPass pass,
                             void *userData) {
  ++((TestExternalPassUserData *)userData)->runCallCount;
  MlirStringRef opName = mlirIdentifierStr(mlirOperationGetName(op));
  if (!mlirStringRefEqual(opName,
                          mlirStringRefCreateFromCString("func.func"))) {
    mlirExternalPassSignalFailure(pass);
  }
}

void testRunFailingExternalPass(MlirOperation op, MlirExternalPass pass,
                                void *userData) {
  ++((TestExternalPassUserData *)userData)->runCallCount;
  mlirExternalPassSignalFailure(pass);
}

MlirExternalPassCallbacks makeTestExternalPassCallbacks(
    MlirLogicalResult (*initializePass)(MlirContext ctx, void *userData),
    void (*runPass)(MlirOperation op, MlirExternalPass, void *userData)) {
  return (MlirExternalPassCallbacks){testConstructExternalPass,
                                     testDestructExternalPass, initializePass,
                                     testCloneExternalPass, runPass};
}

void testExternalPass(void) {
  MlirContext ctx = mlirContextCreate();
  registerAllUpstreamDialects(ctx);

  const char *moduleAsm = //
      "module {                                 \n"
      "  func.func @foo(%arg0 : i32) -> i32 {   \n"
      "    %res = arith.addi %arg0, %arg0 : i32 \n"
      "    return %res : i32                    \n"
      "  }                                      \n"
      "}";
  MlirOperation module =
      mlirOperationCreateParse(ctx, mlirStringRefCreateFromCString(moduleAsm),
                               mlirStringRefCreateFromCString("moduleAsm"));
  if (mlirOperationIsNull(module)) {
    fprintf(stderr, "Unexpected failure parsing module.\n");
    exit(EXIT_FAILURE);
  }

  MlirStringRef description = mlirStringRefCreateFromCString("");
  MlirStringRef emptyOpName = mlirStringRefCreateFromCString("");

  MlirTypeIDAllocator typeIDAllocator = mlirTypeIDAllocatorCreate();

  // Run a generic pass
  {
    MlirTypeID passID = mlirTypeIDAllocatorAllocateTypeID(typeIDAllocator);
    MlirStringRef name = mlirStringRefCreateFromCString("TestExternalPass");
    MlirStringRef argument =
        mlirStringRefCreateFromCString("test-external-pass");
    TestExternalPassUserData userData = {0};

    MlirExternalPass externalPass = mlirExternalPassCreate(
        passID, name, argument, description, emptyOpName, 0, NULL,
        makeTestExternalPassCallbacks(NULL, testRunExternalPass), &userData);

    MlirPass pass = mlirExternalPassGetPass(externalPass);

    if (userData.constructCallCount != 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    MlirPassManager pm = mlirPassManagerCreate(ctx);
    mlirPassManagerAddOwnedPass(pm, pass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsFailure(success)) {
      fprintf(stderr, "Unexpected failure running external pass.\n");
      exit(EXIT_FAILURE);
    }

    if (userData.runCallCount != 1) {
      fprintf(stderr, "Expected runCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    mlirPassManagerDestroy(pm);

    if (userData.destructCallCount != userData.constructCallCount) {
      fprintf(stderr, "Expected destructCallCount to be equal to "
                      "constructCallCount\n");
      exit(EXIT_FAILURE);
    }
  }

  // Run a func operation pass
  {
    MlirTypeID passID = mlirTypeIDAllocatorAllocateTypeID(typeIDAllocator);
    MlirStringRef name = mlirStringRefCreateFromCString("TestExternalFuncPass");
    MlirStringRef argument =
        mlirStringRefCreateFromCString("test-external-func-pass");
    TestExternalPassUserData userData = {0};
    MlirDialectHandle funcHandle = mlirGetDialectHandle__func__();
    MlirStringRef funcOpName = mlirStringRefCreateFromCString("func.func");

    MlirExternalPass externalPass = mlirExternalPassCreate(
        passID, name, argument, description, funcOpName, 1, &funcHandle,
        makeTestExternalPassCallbacks(NULL, testRunExternalFuncPass),
        &userData);

    MlirPass pass = mlirExternalPassGetPass(externalPass);

    if (userData.constructCallCount != 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    MlirPassManager pm = mlirPassManagerCreate(ctx);
    MlirOpPassManager nestedFuncPm =
        mlirPassManagerGetNestedUnder(pm, funcOpName);
    mlirOpPassManagerAddOwnedPass(nestedFuncPm, pass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsFailure(success)) {
      fprintf(stderr, "Unexpected failure running external operation pass.\n");
      exit(EXIT_FAILURE);
    }

    // Since this is a nested pass, it can be cloned and run in parallel
    if (userData.cloneCallCount != userData.constructCallCount - 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    // The pass should only be run once this there is only one func op
    if (userData.runCallCount != 1) {
      fprintf(stderr, "Expected runCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    mlirPassManagerDestroy(pm);

    if (userData.destructCallCount != userData.constructCallCount) {
      fprintf(stderr, "Expected destructCallCount to be equal to "
                      "constructCallCount\n");
      exit(EXIT_FAILURE);
    }
  }

  // Run a pass with `initialize` set
  {
    MlirTypeID passID = mlirTypeIDAllocatorAllocateTypeID(typeIDAllocator);
    MlirStringRef name = mlirStringRefCreateFromCString("TestExternalPass");
    MlirStringRef argument =
        mlirStringRefCreateFromCString("test-external-pass");
    TestExternalPassUserData userData = {0};

    MlirExternalPass externalPass = mlirExternalPassCreate(
        passID, name, argument, description, emptyOpName, 0, NULL,
        makeTestExternalPassCallbacks(testInitializeExternalPass,
                                      testRunExternalPass),
        &userData);

    MlirPass pass = mlirExternalPassGetPass(externalPass);

    if (userData.constructCallCount != 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    MlirPassManager pm = mlirPassManagerCreate(ctx);
    mlirPassManagerAddOwnedPass(pm, pass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsFailure(success)) {
      fprintf(stderr, "Unexpected failure running external pass.\n");
      exit(EXIT_FAILURE);
    }

    if (userData.initializeCallCount != 1) {
      fprintf(stderr, "Expected initializeCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    if (userData.runCallCount != 1) {
      fprintf(stderr, "Expected runCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    mlirPassManagerDestroy(pm);

    if (userData.destructCallCount != userData.constructCallCount) {
      fprintf(stderr, "Expected destructCallCount to be equal to "
                      "constructCallCount\n");
      exit(EXIT_FAILURE);
    }
  }

  // Run a pass that fails during `initialize`
  {
    MlirTypeID passID = mlirTypeIDAllocatorAllocateTypeID(typeIDAllocator);
    MlirStringRef name =
        mlirStringRefCreateFromCString("TestExternalFailingPass");
    MlirStringRef argument =
        mlirStringRefCreateFromCString("test-external-failing-pass");
    TestExternalPassUserData userData = {0};

    MlirExternalPass externalPass = mlirExternalPassCreate(
        passID, name, argument, description, emptyOpName, 0, NULL,
        makeTestExternalPassCallbacks(testInitializeFailingExternalPass,
                                      testRunExternalPass),
        &userData);

    MlirPass pass = mlirExternalPassGetPass(externalPass);

    if (userData.constructCallCount != 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    MlirPassManager pm = mlirPassManagerCreate(ctx);
    mlirPassManagerAddOwnedPass(pm, pass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsSuccess(success)) {
      fprintf(
          stderr,
          "Expected failure running pass manager on failing external pass.\n");
      exit(EXIT_FAILURE);
    }

    if (userData.initializeCallCount != 1) {
      fprintf(stderr, "Expected initializeCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    if (userData.runCallCount != 0) {
      fprintf(stderr, "Expected runCallCount to be 0\n");
      exit(EXIT_FAILURE);
    }

    mlirPassManagerDestroy(pm);

    if (userData.destructCallCount != userData.constructCallCount) {
      fprintf(stderr, "Expected destructCallCount to be equal to "
                      "constructCallCount\n");
      exit(EXIT_FAILURE);
    }
  }

  // Run a pass that fails during `run`
  {
    MlirTypeID passID = mlirTypeIDAllocatorAllocateTypeID(typeIDAllocator);
    MlirStringRef name =
        mlirStringRefCreateFromCString("TestExternalFailingPass");
    MlirStringRef argument =
        mlirStringRefCreateFromCString("test-external-failing-pass");
    TestExternalPassUserData userData = {0};

    MlirExternalPass externalPass = mlirExternalPassCreate(
        passID, name, argument, description, emptyOpName, 0, NULL,
        makeTestExternalPassCallbacks(NULL, testRunFailingExternalPass),
        &userData);

    MlirPass pass = mlirExternalPassGetPass(externalPass);

    if (userData.constructCallCount != 1) {
      fprintf(stderr, "Expected constructCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    MlirPassManager pm = mlirPassManagerCreate(ctx);
    mlirPassManagerAddOwnedPass(pm, pass);
    MlirLogicalResult success = mlirPassManagerRunOnOp(pm, module);
    if (mlirLogicalResultIsSuccess(success)) {
      fprintf(
          stderr,
          "Expected failure running pass manager on failing external pass.\n");
      exit(EXIT_FAILURE);
    }

    if (userData.runCallCount != 1) {
      fprintf(stderr, "Expected runCallCount to be 1\n");
      exit(EXIT_FAILURE);
    }

    mlirPassManagerDestroy(pm);

    if (userData.destructCallCount != userData.constructCallCount) {
      fprintf(stderr, "Expected destructCallCount to be equal to "
                      "constructCallCount\n");
      exit(EXIT_FAILURE);
    }
  }

  mlirTypeIDAllocatorDestroy(typeIDAllocator);
  mlirOperationDestroy(module);
  mlirContextDestroy(ctx);
}

int main(void) {
  testRunPassOnModule();
  testRunPassOnNestedModule();
  testPrintPassPipeline();
  testParsePassPipeline();
  testParseErrorCapture();
  testExternalPass();
  return 0;
}
