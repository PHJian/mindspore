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

#include "ops/grad/sparse_segment_sqrt_n_grad.h"
#include "abstract/dshape.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "utils/tensor_construct_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr SparseSegmentSqrtNGradInferShape(const PrimitivePtr &prim,
                                                    const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->BuildShape())[kShape];
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->BuildShape())[kShape];
  auto segment_ids_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->BuildShape())[kShape];
  auto output_dim0_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->BuildShape())[kShape];
  if (x_shape.size() < kInputIndex1) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor x's rank less than 1.";
  }
  if (output_dim0_shape.size() != kInputIndex0) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor outputdim0 should be a scalar.";
  }
  if (indices_shape[kInputIndex0] != segment_ids_shape[kInputIndex0]) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor indices & segment_ids's ranks mismatch.";
  }
  if (!input_args[kInputIndex3]->BuildValue()->isa<AnyValue>() &&
      !input_args[kInputIndex3]->BuildValue()->isa<None>()) {
    auto output_dim0_value = input_args[kInputIndex3]->cast<abstract::AbstractTensorPtr>();
    MS_EXCEPTION_IF_NULL(output_dim0_value);
    auto output_dim0_value_ptr = output_dim0_value->BuildValue();
    MS_EXCEPTION_IF_NULL(output_dim0_value_ptr);
    auto output_dim0_value_ptr_tensor =
      CheckAndConvertUtils::CheckTensorIntValue("output_dim0", output_dim0_value_ptr, prim_name);
    size_t dim_zero = static_cast<size_t>(output_dim0_value_ptr_tensor[kInputIndex0]);
    if (dim_zero <= kInputIndex0) {
      MS_EXCEPTION(ValueError) << "Input output_dim0 must > 0!";
    } else {
      ShapeVector y_shape = x_shape;
      y_shape[kInputIndex0] = static_cast<int64_t>(dim_zero);
      return std::make_shared<abstract::Shape>(y_shape);
    }
  } else {
    std::vector<int64_t> output_shape = {-2};
    return std::make_shared<abstract::Shape>(output_shape);
  }
}

TypePtr SparseSegmentSqrtNGradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto x_type = input_args[kInputIndex0]->BuildType();
  auto indices_type = input_args[kInputIndex1]->BuildType();
  auto segment_ids_type = input_args[kInputIndex2]->BuildType();
  auto output_dim0_type = input_args[kInputIndex3]->BuildType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, {kFloat16, kFloat32, kFloat64}, prim->name());
  std::map<std::string, TypePtr> types;
  (void)types.emplace("indices", indices_type);
  (void)types.emplace("segment_ids", segment_ids_type);
  (void)types.emplace("output_dim0", output_dim0_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, {kInt32}, prim->name());
  return input_args[kInputIndex0]->BuildType();
}
}  // namespace

MIND_API_OPERATOR_IMPL(SparseSegmentSqrtNGrad, BaseOperator);
AbstractBasePtr SparseSegmentSqrtNGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &prim,
                                            const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  const int64_t input_num = static_cast<int64_t>(kInputIndex4);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, prim_name);
  auto types = SparseSegmentSqrtNGradInferType(prim, input_args);
  auto shapes = SparseSegmentSqrtNGradInferShape(prim, input_args);
  return abstract::MakeAbstract(shapes, types);
}
REGISTER_HOST_DEPENDS(kNameSparseSegmentSqrtNGrad, {3});
REGISTER_PRIMITIVE_EVAL_IMPL(SparseSegmentSqrtNGrad, prim::kPrimSparseSegmentSqrtNGrad, SparseSegmentSqrtNGradInfer,
                             nullptr, true);
}  // namespace ops
}  // namespace mindspore
