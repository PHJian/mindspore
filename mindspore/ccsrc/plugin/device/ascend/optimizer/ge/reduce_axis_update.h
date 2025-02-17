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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_GE_REDUCE_AXIS_UPDATE_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_GE_REDUCE_AXIS_UPDATE_H_

#include "backend/common/optimizer/optimizer.h"

namespace mindspore {
namespace opt {
class ReduceAxisUpdate : public PatternProcessPass {
 public:
  explicit ReduceAxisUpdate(bool multigraph = true) : PatternProcessPass("reduce_axis_update", multigraph) {}
  ~ReduceAxisUpdate() override = default;

  const BaseRef DefinePattern() const override;
  const AnfNodePtr Process(const FuncGraphPtr &, const AnfNodePtr &, const EquivPtr &) const override;

 private:
  bool IsAxisEmpty(const ValueNodePtr &axis_node) const;
  static bool IsReduce(const BaseRef &ref);
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_GE_REDUCE_AXIS_UPDATE_H_
