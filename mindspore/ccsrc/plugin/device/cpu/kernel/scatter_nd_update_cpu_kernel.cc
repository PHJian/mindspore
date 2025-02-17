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

#include "plugin/device/cpu/kernel/scatter_nd_update_cpu_kernel.h"
#include <complex>
#include <string>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"

namespace mindspore {
namespace kernel {
constexpr size_t kMinIndiceRank = 2;
bool ScatterUpdateArithmeticCpuKernelMod::Init(const BaseOperatorPtr &base_operator,
                                               const std::vector<KernelTensorPtr> &inputs,
                                               const std::vector<KernelTensorPtr> &outputs) {
  kernel_name_ = base_operator->name();
  if (kernel_name_ != "ScatterNdUpdate" && kernel_name_ != "TensorScatterUpdate") {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the current operator does not support this operation.";
  }
  return MatchKernelFunc(base_operator, inputs, outputs);
}

int ScatterUpdateArithmeticCpuKernelMod::Resize(const BaseOperatorPtr &base_operator,
                                                const std::vector<KernelTensorPtr> &inputs,
                                                const std::vector<KernelTensorPtr> &outputs,
                                                const std::map<uint32_t, tensor::TensorPtr> &) {
  if (int ret = KernelMod::Resize(base_operator, inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  auto shape = inputs[0]->GetShapeVector();
  auto indices_shape_ori = inputs[1]->GetShapeVector();
  auto updates_shape_ori = inputs[2]->GetShapeVector();
  dtype_value_ = inputs[0]->GetDtype();
  dtype_shape_ = inputs[1]->GetDtype();
  auto indices_shape = Convert2SizeT(indices_shape_ori);
  auto updates_shape = Convert2SizeT(updates_shape_ori);
  auto indices_unit_rank = indices_shape.back();
  if (indices_unit_rank > shape.size()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the value of last dimension of 'indices' must be less than "
                         "or equal to the dimension of 'shape', but got the value of last dimension of 'indices': "
                      << indices_unit_rank << " and the dimension of 'shape': " << shape.size();
  }
  if (indices_shape.size() < kMinIndiceRank) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of 'indices' must be at least 2, but got "
                      << indices_shape.size();
  }
  if (updates_shape.size() != indices_shape.size() - 1 + shape.size() - indices_unit_rank) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dimension of 'update' and 'shape', 'indices' are not "
                         "satisfy the equivalence relationship: "
                         "'updates_shape.size() == indices_shape.size() - 1 + shape.size() - indices_unit_rank'";
  }
  for (size_t i = 0; i < indices_shape.size() - 1; ++i) {
    if (updates_shape[i] != indices_shape[i]) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_
                        << "', the shape of 'updates' and 'indices' are different in dimension i=" << i
                        << ". The 'updates_shape[i]' is " << updates_shape[i] << " and the 'indices_shape[i]' is "
                        << indices_shape[i];
    }
  }
  indices_unit_rank_ = indices_unit_rank;
  unit_size_ = 1;
  for (size_t i = indices_shape.size() - 1; i < updates_shape.size(); ++i) {
    unit_size_ *= updates_shape[i];
  }
  num_units_ = 1;
  num_units_ *= updates_shape[indices_shape.size() - 2];
  for (int i = SizeToInt(indices_shape.size()) - 3; i >= 0; i--) {
    num_units_ *= updates_shape[i];
  }
  size_t out_stride = 1;
  out_strides_.push_back(out_stride);
  for (int64_t i = SizeToLong(indices_unit_rank_) - 2; i >= 0; i--) {
    out_stride *= LongToSize(shape[LongToSize(i + 1)]);
    out_strides_.push_back(out_stride);
  }
  reverse(out_strides_.begin(), out_strides_.end());
  return KRET_OK;
}

template <typename T, typename S>
bool ScatterUpdateArithmeticCpuKernelMod::LaunchKernel(const std::vector<kernel::AddressPtr> &inputs,
                                                       const std::vector<kernel::AddressPtr> &workspace,
                                                       const std::vector<kernel::AddressPtr> &outputs) {
  T *x = nullptr;
  if (kernel_name_ == "ScatterNdUpdate") {
    x = reinterpret_cast<T *>(inputs[0]->addr);
  } else {
    x = reinterpret_cast<T *>(outputs[0]->addr);
    auto ret = memcpy_s(x, outputs[0]->size, inputs[0]->addr, inputs[0]->size);
    if (ret != 0) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', memcpy_s error. Error no: " << ret;
    }
  }
  size_t start = 0;
  S *indices = reinterpret_cast<S *>(inputs[1]->addr);
  T *updates = reinterpret_cast<T *>(inputs[2]->addr);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(indices);
  MS_EXCEPTION_IF_NULL(updates);
  size_t x_mem_size = inputs[0]->size;
  for (size_t i = start; i < num_units_; ++i) {
    size_t offset = 0;
    std::vector<size_t> local_indices;
    for (size_t j = 0; j < indices_unit_rank_; ++j) {
      auto index = indices[i * indices_unit_rank_ + j];
      (void)local_indices.emplace_back(IntToSize(index));
      if (index < 0) {
        MS_LOG(ERROR) << "For '" << kernel_name_
                      << "', each element in 'indices' must be greater than or equal to 0, but got " << index;
        return false;
      }
      offset += IntToSize(index) * out_strides_[j] * unit_size_;
    }
    if (offset * sizeof(T) > x_mem_size) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', indices out of range for input_x. Please check the indices which is " << local_indices;
      return false;
    }
    auto ret = memcpy_s(x + offset, x_mem_size - offset * sizeof(T), updates + unit_size_ * i, unit_size_ * sizeof(T));
    if (ret != 0) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', memcpy_s error. Error no: " << ret;
      return false;
    }
  }
  if (memcpy_s(outputs[0]->addr, outputs[0]->size, x, inputs[0]->size) != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does memory copy fail.";
  }
  return true;
}

using complex64 = std::complex<float>;
using complex128 = std::complex<double>;

#define SCATTER_ND_UPDATE_CPU_REGISTER(IN_DT0, IN_DT1, IN_DT2, OUT_DT0, T, S)                         \
  KernelAttr().AddInputAttr(IN_DT0).AddInputAttr(IN_DT1).AddInputAttr(IN_DT2).AddOutputAttr(OUT_DT0), \
    &ScatterUpdateArithmeticCpuKernelMod::LaunchKernel<T, S>

const ScatterUpdateArithmeticCpuKernelMod::SupportListType &ScatterUpdateArithmeticCpuKernelMod::GetFuncList() const {
  static const ScatterUpdateArithmeticCpuKernelMod::SupportListType scatter_nd_update_func_list = {
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat16, kNumberTypeInt32, kNumberTypeFloat16, kNumberTypeFloat16,
                                    float16, int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat32, kNumberTypeInt32, kNumberTypeFloat32, kNumberTypeFloat32, float,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat64, kNumberTypeInt32, kNumberTypeFloat64, kNumberTypeFloat64,
                                    double, int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt8, kNumberTypeInt32, kNumberTypeInt8, kNumberTypeInt8, int8_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt16, kNumberTypeInt32, kNumberTypeInt16, kNumberTypeInt16, int16_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, int32_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt64, int64_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt8, kNumberTypeInt32, kNumberTypeUInt8, kNumberTypeUInt8, uint8_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt16, kNumberTypeInt32, kNumberTypeUInt16, kNumberTypeUInt16, uint16_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt32, kNumberTypeInt32, kNumberTypeUInt32, kNumberTypeUInt32, uint32_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt64, kNumberTypeInt32, kNumberTypeUInt64, kNumberTypeUInt64, uint64_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeComplex64, kNumberTypeInt32, kNumberTypeComplex64, kNumberTypeComplex64,
                                    complex64, int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeComplex128, kNumberTypeInt32, kNumberTypeComplex128,
                                    kNumberTypeComplex128, complex128, int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat16, kNumberTypeInt64, kNumberTypeFloat16, kNumberTypeFloat16,
                                    float16, int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat32, kNumberTypeInt64, kNumberTypeFloat32, kNumberTypeFloat32, float,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat64, kNumberTypeInt64, kNumberTypeFloat64, kNumberTypeFloat64,
                                    double, int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt8, kNumberTypeInt64, kNumberTypeInt8, kNumberTypeInt8, int8_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt16, kNumberTypeInt64, kNumberTypeInt16, kNumberTypeInt16, int16_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt32, int32_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, int64_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt8, kNumberTypeInt64, kNumberTypeUInt8, kNumberTypeUInt8, uint8_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt16, kNumberTypeInt64, kNumberTypeUInt16, kNumberTypeUInt16, uint16_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt32, kNumberTypeInt64, kNumberTypeUInt32, kNumberTypeUInt32, uint32_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeUInt64, kNumberTypeInt64, kNumberTypeUInt64, kNumberTypeUInt64, uint64_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeComplex64, kNumberTypeInt64, kNumberTypeComplex64, kNumberTypeComplex64,
                                    complex64, int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeComplex128, kNumberTypeInt64, kNumberTypeComplex128,
                                    kNumberTypeComplex128, complex128, int64_t)},
  };

  static const ScatterUpdateArithmeticCpuKernelMod::SupportListType tensor_scatter_update_func_list = {
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat32, kNumberTypeInt32, kNumberTypeFloat32, kNumberTypeFloat32, float,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat64, kNumberTypeInt32, kNumberTypeFloat64, kNumberTypeFloat64,
                                    double, int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, kNumberTypeInt32, int32_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt64, int64_t,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeBool, kNumberTypeInt32, kNumberTypeBool, kNumberTypeBool, bool,
                                    int32_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat32, kNumberTypeInt64, kNumberTypeFloat32, kNumberTypeFloat32, float,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeFloat64, kNumberTypeInt64, kNumberTypeFloat64, kNumberTypeFloat64,
                                    double, int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt32, kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt32, int32_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, int64_t,
                                    int64_t)},
    {SCATTER_ND_UPDATE_CPU_REGISTER(kNumberTypeBool, kNumberTypeInt64, kNumberTypeBool, kNumberTypeBool, bool,
                                    int64_t)},
  };

  if (kernel_name_ == "TensorScatterUpdate") {
    return tensor_scatter_update_func_list;
  } else {
    return scatter_nd_update_func_list;
  }
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ScatterNdUpdate, ScatterUpdateArithmeticCpuKernelMod);
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TensorScatterUpdate, ScatterUpdateArithmeticCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
