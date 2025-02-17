/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "ir/dtype/tensor_type.h"
#include "utils/log_adapter.h"
#include "utils/ms_utils.h"

namespace mindspore {
TypePtr UndeterminedType::DeepCopy() const {
  MS_EXCEPTION_IF_NULL(element_type_);
  if (IsGeneric()) {
    return std::make_shared<UndeterminedType>();
  }
  return std::make_shared<UndeterminedType>(element_type_->DeepCopy());
}

std::string UndeterminedType::ToReprString() const {
  if (element_type_ == nullptr) {
    return "Undetermined";
  }
  return "Undetermined[" + element_type_->ToReprString() + "]";
}

std::string UndeterminedType::ToString() const {
  if (element_type_ == nullptr) {
    return "Undetermined";
  }
  return "Undetermined[" + element_type_->ToString() + "]";
}

std::string UndeterminedType::DumpText() const {
  if (element_type_ == nullptr) {
    return "Undetermined";
  }
  return "Undetermined[" + element_type_->DumpText() + "]";
}

bool UndeterminedType::operator==(const Type &other) const {
  if (!IsSameObjectType(*this, other)) {
    return false;
  }
  const auto &other_type = static_cast<const UndeterminedType &>(other);
  return common::IsEqual(element_type_, other_type.element_type_);
}

size_t UndeterminedType::hash() const {
  size_t hash_value = hash_combine(static_cast<size_t>(kMetaTypeObject), static_cast<size_t>(object_type()));
  if (element_type_ != nullptr) {
    hash_value = hash_combine(hash_value, element_type_->hash());
  }
  return hash_value;
}

TypePtr TensorType::DeepCopy() const {
  if (element_type_ == nullptr) {
    return std::make_shared<TensorType>();
  }
  if (IsGeneric()) {
    return std::make_shared<TensorType>();
  }
  return std::make_shared<TensorType>(element_type_->DeepCopy());
}

std::string TensorType::ToReprString() const {
  if (element_type_ == nullptr) {
    return "tensor";
  }
  return "tensor[" + element_type_->ToReprString() + "]";
}

std::string TensorType::ToString() const {
  if (element_type_ == nullptr) {
    return "Tensor";
  }
  return "Tensor[" + element_type_->ToString() + "]";
}

std::string TensorType::DumpText() const {
  if (element_type_ == nullptr) {
    return "Tensor";
  }
  return "Tensor(" + element_type_->DumpText() + ")";
}

bool TensorType::operator==(const Type &other) const {
  if (!IsSameObjectType(*this, other)) {
    return false;
  }
  const auto &other_type = static_cast<const TensorType &>(other);
  return common::IsEqual(element_type_, other_type.element_type_);
}

size_t TensorType::hash() const {
  size_t hash_value = hash_combine(static_cast<size_t>(kMetaTypeObject), static_cast<size_t>(object_type()));
  if (element_type_ != nullptr) {
    hash_value = hash_combine(hash_value, element_type_->hash());
  }
  return hash_value;
}

std::string SparseTensorType::ElementsDtypeStr(const StringType str_type) const {
  std::ostringstream oss;
  for (const TypePtr &elem : elements_) {
    if (str_type == kToString) {
      oss << elem->ToString();
    } else if (str_type == kDumpText) {
      oss << elem->DumpText();
    } else if (str_type == kReprString) {
      oss << elem->ToReprString();
    }
    oss << ",";
  }
  return oss.str();
}

std::string SparseTensorType::ToString() const {
  if (elements_.empty()) {
    return GetSparseTensorTypeName();
  }
  return GetSparseTensorTypeName() + "[" + ElementsDtypeStr(kToString) + "]";
}

std::string SparseTensorType::DumpText() const {
  if (elements_.empty()) {
    return GetSparseTensorTypeName();
  }
  return GetSparseTensorTypeName() + "[" + ElementsDtypeStr(kDumpText) + "]";
}

std::string SparseTensorType::ToReprString() const {
  if (elements_.empty()) {
    return GetSparseTensorTypeName();
  }
  return GetSparseTensorTypeName() + "[" + ElementsDtypeStr(kReprString) + "]";
}

const TypePtrList SparseTensorType::ElementsClone() const {
  TypePtrList elems;
  (void)std::transform(elements_.begin(), elements_.end(), std::back_inserter(elems),
                       [](const TypePtr &ele) { return ele->DeepCopy(); });
  return elems;
}

TypePtr SparseTensorType::DeepCopy() const {
  if (IsGeneric()) {
    return std::make_shared<SparseTensorType>();
  }
  return std::make_shared<SparseTensorType>(ElementsClone());
}

const TypePtr SparseTensorType::operator[](std::size_t dim) const {
  if (dim >= size()) {
    MS_LOG(EXCEPTION) << "Index " << dim << " is out range of the SparseTensorType size " << size() << ".";
  }
  return elements_[dim];
}

bool SparseTensorType::operator==(const Type &other) const {
  if (!IsSameObjectType(*this, other)) {
    return false;
  }
  const auto &other_type = static_cast<const SparseTensorType &>(other);
  return TypeListEqual()(elements_, other_type.elements_);
}

size_t SparseTensorType::hash() const {
  size_t hash_value = hash_combine(static_cast<size_t>(kMetaTypeObject), static_cast<size_t>(object_type()));
  return hash_combine(hash_value, TypeListHasher()(elements_));
}

TypePtr RowTensorType::DeepCopy() const {
  MS_EXCEPTION_IF_NULL(element_type_);
  if (IsGeneric()) {
    return std::make_shared<RowTensorType>();
  }
  return std::make_shared<RowTensorType>(element_type_->DeepCopy());
}

std::string RowTensorType::ToReprString() const {
  if (element_type_ == nullptr) {
    return "RowTensor";
  }
  return "RowTensor[" + element_type_->ToReprString() + "]";
}

std::string RowTensorType::ToString() const {
  if (element_type_ == nullptr) {
    return "RowTensor";
  }
  return "RowTensor[" + element_type_->ToString() + "]";
}

std::string RowTensorType::DumpText() const {
  if (element_type_ == nullptr) {
    return "RowTensor";
  }
  return "RowTensor[" + element_type_->DumpText() + "]";
}

bool RowTensorType::operator==(const Type &other) const {
  if (!IsSameObjectType(*this, other)) {
    return false;
  }
  const auto &other_type = static_cast<const RowTensorType &>(other);
  return common::IsEqual(element_type_, other_type.element_type_);
}

size_t RowTensorType::hash() const {
  size_t hash_value = hash_combine(static_cast<size_t>(kMetaTypeObject), static_cast<size_t>(object_type()));
  if (element_type_ != nullptr) {
    hash_value = hash_combine(hash_value, element_type_->hash());
  }
  return hash_value;
}

TypePtr COOTensorType::DeepCopy() const {
  if (IsGeneric()) {
    return std::make_shared<COOTensorType>();
  }
  return std::make_shared<COOTensorType>(ElementsClone());
}

TypePtr CSRTensorType::DeepCopy() const {
  if (IsGeneric()) {
    return std::make_shared<CSRTensorType>();
  }
  return std::make_shared<CSRTensorType>(ElementsClone());
}
}  // namespace mindspore
