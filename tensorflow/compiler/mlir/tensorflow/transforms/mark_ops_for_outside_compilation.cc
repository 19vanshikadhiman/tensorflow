/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <memory>
#include <string>
#include <utility>

#include "mlir/IR/TypeUtilities.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Pass/PassRegistry.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_device.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"

namespace mlir {
namespace TFDevice {

namespace {

constexpr char kXlaOutsideCompilationAttr[] = "_xla_outside_compilation";

// This pass marks unsupported ops in a device cluster with
// `_xla_outside_compilation` attribute so the operations will run on the host
// instead of the device.  Unsupported ops are ops that can not be code
// generated to run on the device for the cluster.
struct MarkOpsForOutsideCompilation
    : public PassWrapper<MarkOpsForOutsideCompilation,
                         OperationPass<ModuleOp>> {
  void runOnOperation() override;
};

bool HasStringOperand(Operation& op) {
  for (auto operand : op.getOperands()) {
    if (getElementTypeOrSelf(operand).isa<TF::StringType>()) return true;
  }
  return false;
}

bool HasStringResult(Operation& op) {
  for (auto result : op.getResults()) {
    if (getElementTypeOrSelf(result).isa<TF::StringType>()) return true;
  }
  return false;
}

// Checks if the op is supported inside of a device cluster.
bool IsSupportedOp(Operation& op) {
  if (HasStringOperand(op) || HasStringResult(op)) {
    return false;
  }
  return true;
}

LogicalResult MarkUncompilableOps(Block* block) {
  for (Operation& op : *block) {
    if (!IsSupportedOp(op)) {
      op.setAttr(kXlaOutsideCompilationAttr,
                 StringAttr::get("auto", op.getContext()));
    }
  }
  return success();
}

void MarkOpsForOutsideCompilation::runOnOperation() {
  auto module = getOperation();

  module.walk([&](tf_device::ClusterOp cluster) {
    MarkUncompilableOps(&cluster.GetBody());
  });
}

}  // namespace

std::unique_ptr<OperationPass<ModuleOp>>
CreateMarkOpsForOutsideCompilationPass() {
  return std::make_unique<MarkOpsForOutsideCompilation>();
}

static PassRegistration<MarkOpsForOutsideCompilation> pass(
    "tf-mark-ops-for-outside-compilation",
    "Marks unsupported ops a device cluster for outside compilation.");

}  // namespace TFDevice
}  // namespace mlir
