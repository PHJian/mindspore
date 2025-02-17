/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "src/extendrt/subgraph_kernel.h"
namespace mindspore::kernel {
bool SubgraphKernel::Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                            const std::vector<AddressPtr> &outputs, void *stream_ptr) {
  // TODO(zhaizhiqiang): Construct input/output tensor::Tensor for graph executor.
  std::vector<tensor::Tensor> in;
  std::vector<tensor::Tensor> out;
  std::map<string, string> compile_options;
  executor_->RunGraph(subgraph_, in, &out, compile_options);
  return true;
}
bool SubgraphKernel::Init(const BaseOperatorPtr &opdef, const std::vector<KernelTensorPtr> &inputs,
                          const std::vector<KernelTensorPtr> &outputs) {
  std::map<string, string> compile_options;
  return executor_->CompileGraph(subgraph_, compile_options);
}
int SubgraphKernel::Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                           const std::vector<KernelTensorPtr> &outputs,
                           const std::map<uint32_t, tensor::TensorPtr> &inputsOnHost) {
  return 0;
}
}  // namespace mindspore::kernel
