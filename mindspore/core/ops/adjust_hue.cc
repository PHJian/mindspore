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

#include "ops/adjust_hue.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr AdjustHueInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  auto input_shape_images = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->BuildShape())[kShape];
  const int64_t min_dim = 3;
  (void)CheckAndConvertUtils::CheckInteger("dimension of image", SizeToLong(input_shape_images.size()), kGreaterEqual,
                                           min_dim, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("last dimension of image", input_shape_images[input_shape_images.size() - 1],
                                           kEqual, min_dim, prim_name);
  auto input_shape_delta = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->BuildShape())[kShape];
  const int64_t delta_dim = 0;
  (void)CheckAndConvertUtils::CheckInteger("dimension of delta", SizeToLong(input_shape_delta.size()), kEqual,
                                           delta_dim, prim_name);
  return std::make_shared<abstract::Shape>(input_shape_images);
}

TypePtr AdjustHueInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  auto input_type_images = input_args[0]->BuildType();
  MS_EXCEPTION_IF_NULL(input_type_images);
  const std::set valid_types = {kFloat16, kFloat32, kFloat64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("images", input_type_images, valid_types, prim_name);
  auto input_type_delta = input_args[1]->BuildType();
  MS_EXCEPTION_IF_NULL(input_type_delta);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("delta", input_type_delta, {kFloat32}, prim_name);
  return input_type_images;
}
}  // namespace

MIND_API_OPERATOR_IMPL(AdjustHue, BaseOperator);
AbstractBasePtr AdjustHueInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
  auto infer_type = AdjustHueInferType(primitive, input_args);
  auto infer_shape = AdjustHueInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
REGISTER_PRIMITIVE_EVAL_IMPL(AdjustHue, prim::kPrimAdjustHue, AdjustHueInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
