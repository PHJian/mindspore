/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/arrays/scatter_functor_gpu_kernel.h"

namespace mindspore {
namespace kernel {
using KernelRunFunc = ScatterFunctorGPUKernelMod::KernelRunFunc;
static const std::map<std::string, ScatterFunctorType> kScatterFunctorTypeMap = {
  {"ScatterUpdate", SCATTER_FUNC_UPDATE}, {"ScatterAdd", SCATTER_FUNC_ADD}, {"ScatterSub", SCATTER_FUNC_SUB},
  {"ScatterMax", SCATTER_FUNC_MAX},       {"ScatterMin", SCATTER_FUNC_MIN},
};

bool ScatterFunctorGPUKernelMod::Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                      const std::vector<KernelTensorPtr> &outputs) {
  kernel_name_ = base_operator->name();
  auto iter = kScatterFunctorTypeMap.find(kernel_name_);
  if (iter == kScatterFunctorTypeMap.end()) {
    MS_LOG(EXCEPTION)
      << "Only support these scatter functors: ScatterUpdate, ScatterAdd, ScatterSub, ScatterMax, ScatterMin."
      << " currently, but got " << kernel_name_;
  }
  scatter_functor_type_ = iter->second;
  if (!MatchKernelFunc(base_operator, inputs, outputs)) {
    return false;
  }
  return true;
}

int ScatterFunctorGPUKernelMod::Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                       const std::vector<KernelTensorPtr> &outputs,
                                       const std::map<uint32_t, tensor::TensorPtr> &) {
  size_t input_num = inputs.size();
  const size_t correct_input_num = 3;
  if (input_num != correct_input_num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs must be 3, but got " << input_num;
  }
  size_t output_num = outputs.size();
  if (output_num != 1) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs must be 1, but got " << output_num;
  }
  auto input_shape = Convert2SizeTClipNeg(inputs[0]->GetShapeVector());
  if (input_shape.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the input can not be empty";
  }

  if (auto ret = KernelMod::Resize(base_operator, inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  first_dim_size_ = input_shape[0];
  input_size_ = 1;
  inner_size_ = 1;
  for (size_t i = 1; i < input_shape.size(); i++) {
    inner_size_ *= input_shape[i];
  }
  input_size_ = input_shape[0] * inner_size_;
  auto indices_shape = inputs[1]->GetShapeVector();
  indices_size_ = SizeOf(indices_shape);
  updates_size_ = indices_size_ * inner_size_;
  return KRET_OK;
}

template <typename T, typename S>
bool ScatterFunctorGPUKernelMod::LaunchKernel(const std::vector<AddressPtr> &inputs,
                                              const std::vector<AddressPtr> &workspace,
                                              const std::vector<AddressPtr> &outputs) {
  T *input = GetDeviceAddress<T>(inputs, 0);
  S *indices = GetDeviceAddress<S>(inputs, 1);
  T *updates = GetDeviceAddress<T>(inputs, 2);
  T *output = GetDeviceAddress<T>(outputs, 0);
  S size_limit = static_cast<S>(first_dim_size_);
  ScatterFunc(scatter_functor_type_, size_limit, inner_size_, indices_size_, indices, updates, input,
              reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaMemcpyAsync(output, input, inputs[0]->size, cudaMemcpyDeviceToDevice,
                                                     reinterpret_cast<cudaStream_t>(cuda_stream_)),
                                     "cudaMemcpyAsync output failed");
  return true;
}

#define DTYPE_REGISTER(INPUT, INDICES, UPDATES, OUTPUT, T, S)                                                          \
  {                                                                                                                    \
    KernelAttr().AddInputAttr(INPUT).AddInputAttr(INDICES).AddInputAttr(UPDATES).AddOutputAttr(OUTPUT).AddOutInRef(0,  \
                                                                                                                   0), \
      &ScatterFunctorGPUKernelMod::LaunchKernel<T, S>                                                                  \
  }

const std::vector<std::pair<KernelAttr, KernelRunFunc>> &ScatterFunctorGPUKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> max_min_func_list = {
    // Data type: double
    DTYPE_REGISTER(kNumberTypeFloat64, kNumberTypeInt32, kNumberTypeFloat64, kNumberTypeFloat64, double, int),
    DTYPE_REGISTER(kNumberTypeFloat64, kNumberTypeInt64, kNumberTypeFloat64, kNumberTypeFloat64, double, int64_t),
    // Data type: float
    DTYPE_REGISTER(kNumberTypeFloat32, kNumberTypeInt32, kNumberTypeFloat32, kNumberTypeFloat32, float, int),
    DTYPE_REGISTER(kNumberTypeFloat32, kNumberTypeInt64, kNumberTypeFloat32, kNumberTypeFloat32, float, int64_t),
    // Data type: half
    DTYPE_REGISTER(kNumberTypeFloat16, kNumberTypeInt32, kNumberTypeFloat16, kNumberTypeFloat16, half, int),
    DTYPE_REGISTER(kNumberTypeFloat16, kNumberTypeInt64, kNumberTypeFloat16, kNumberTypeFloat16, half, int64_t),
    // Data type: int64
    DTYPE_REGISTER(kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt64, int64_t, int),
    DTYPE_REGISTER(kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, int64_t, int64_t),
    // Data type: int32_t
    DTYPE_REGISTER(kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, int32_t, int),
    DTYPE_REGISTER(kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt32, int32_t, int64_t),
  };

  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> other_func_list = {
    // Data type: float
    DTYPE_REGISTER(kNumberTypeFloat32, kNumberTypeInt32, kNumberTypeFloat32, kNumberTypeFloat32, float, int),
    DTYPE_REGISTER(kNumberTypeFloat32, kNumberTypeInt64, kNumberTypeFloat32, kNumberTypeFloat32, float, int64_t),
    // Data type: half
    DTYPE_REGISTER(kNumberTypeFloat16, kNumberTypeInt32, kNumberTypeFloat16, kNumberTypeFloat16, half, int),
    DTYPE_REGISTER(kNumberTypeFloat16, kNumberTypeInt64, kNumberTypeFloat16, kNumberTypeFloat16, half, int64_t),
    // Data type: int32_t
    DTYPE_REGISTER(kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, int32_t, int),
    DTYPE_REGISTER(kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt32, int32_t, int64_t),
    // Data type: int8_t
    DTYPE_REGISTER(kNumberTypeInt8, kNumberTypeInt32, kNumberTypeInt8, kNumberTypeInt8, int8_t, int),
    DTYPE_REGISTER(kNumberTypeInt8, kNumberTypeInt64, kNumberTypeInt8, kNumberTypeInt8, int8_t, int64_t),
    // Data type: uint8_t
    DTYPE_REGISTER(kNumberTypeUInt8, kNumberTypeInt32, kNumberTypeUInt8, kNumberTypeUInt8, uint8_t, int),
    DTYPE_REGISTER(kNumberTypeUInt8, kNumberTypeInt64, kNumberTypeUInt8, kNumberTypeUInt8, uint8_t, int64_t),
  };
  if (kernel_name_ == "ScatterMax" || kernel_name_ == "ScatterMin") {
    return max_min_func_list;
  }
  return other_func_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScatterUpdate, ScatterFunctorGPUKernelMod);
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScatterAdd, ScatterFunctorGPUKernelMod);
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScatterSub, ScatterFunctorGPUKernelMod);
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScatterMax, ScatterFunctorGPUKernelMod);
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScatterMin, ScatterFunctorGPUKernelMod);
}  // namespace kernel
}  // namespace mindspore
